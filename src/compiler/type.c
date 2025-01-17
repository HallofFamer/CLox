#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "type.h"
#include "../vm/object.h"

#define TYPE_TABLE_MAX_LOAD 0.75
DEFINE_BUFFER(TypeInfoArray, TypeInfo*)

TypeInfo* newTypeInfo(int id, size_t size, TypeCategory category, ObjString* shortName, ObjString* fullName) {
    TypeInfo* type = (TypeInfo*)malloc(size);
    if (type != NULL) {
        type->id = id;
        type->category = category;
        type->shortName = shortName;
        type->fullName = fullName;
    }
    return type;
}

BehaviorTypeInfo* newBehaviorInfo(int id, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType) {
    BehaviorTypeInfo* behaviorType = (BehaviorTypeInfo*)newTypeInfo(id, sizeof(BehaviorTypeInfo), category, shortName, fullName);
    if (behaviorType != NULL) {
        behaviorType->superclassType = superclassType;
        behaviorType->traitTypes = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        if(behaviorType->traitTypes != NULL) TypeInfoArrayInit(behaviorType->traitTypes);
        behaviorType->methods = newTypeTable(id);
    }
    return behaviorType;
}

BehaviorTypeInfo* newBehaviorInfoWithTraits(int id, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType, int numTraits, ...) {
    BehaviorTypeInfo* behaviorType = (BehaviorTypeInfo*)newTypeInfo(id, sizeof(BehaviorTypeInfo), category, shortName, fullName);
    if (behaviorType != NULL) {
        behaviorType->superclassType = superclassType;
        behaviorType->traitTypes = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        behaviorType->methods = newTypeTable(id);

        if (behaviorType->traitTypes != NULL) {
            TypeInfoArrayInit(behaviorType->traitTypes);
            va_list args;
            va_start(args, numTraits);

            for (int i = 0; i < numTraits; i++) {
                TypeInfo* type = va_arg(args, TypeInfo*);
                TypeInfoArrayAdd(behaviorType->traitTypes, type);
            }
            va_end(args);
        }
    }
    return behaviorType;
}

BehaviorTypeInfo* newBehaviorInfoWithMethods(int id, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType, TypeTable* methods) {
    BehaviorTypeInfo* behaviorType = (BehaviorTypeInfo*)newTypeInfo(id, sizeof(BehaviorTypeInfo), category, shortName, fullName);
    if (behaviorType != NULL) {
        behaviorType->superclassType = superclassType;
        behaviorType->traitTypes = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        TypeInfoArrayInit(behaviorType->traitTypes);
        behaviorType->methods = methods;
    }
    return behaviorType;
}

FunctionTypeInfo* newFunctionInfo(int id, TypeCategory category, ObjString* name, TypeInfo* returnType) {
    FunctionTypeInfo* functionType = (FunctionTypeInfo*)newTypeInfo(id, sizeof(FunctionTypeInfo), category, name, name);
    if (functionType != NULL) {
        functionType->returnType = returnType;
        functionType->paramTypes = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        functionType->modifier = functionTypeInitModifier();
        if (functionType->paramTypes != NULL) TypeInfoArrayInit(functionType->paramTypes);
    }
    return functionType;
}

FunctionTypeInfo* newFunctionInfoWithParams(int id, TypeCategory category, ObjString* name, TypeInfo* returnType, int numParams, ...) {
    FunctionTypeInfo* functionType = (FunctionTypeInfo*)newTypeInfo(id, sizeof(FunctionTypeInfo), category, name, name);
    if (functionType != NULL) {
        functionType->returnType = returnType;
        functionType->paramTypes = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        functionType->modifier = functionTypeInitModifier();

        if (functionType->paramTypes != NULL) {
            TypeInfoArrayInit(functionType->paramTypes);
            va_list args;
            va_start(args, numParams);
            for (int i = 0; i < numParams; i++) {
                TypeInfo* type = va_arg(args, TypeInfo*);
                TypeInfoArrayAdd(functionType->paramTypes, type);
            }
            va_end(args);
        }
    }
    return functionType;
}

void freeTypeInfo(TypeInfo* type) {
    if (type == NULL) return;
    if (IS_BEHAVIOR_TYPE(type)) {
        BehaviorTypeInfo* behaviorType = AS_BEHAVIOR_TYPE(type);
        if (behaviorType->traitTypes != NULL) TypeInfoArrayFree(behaviorType->traitTypes);
        freeTypeTable(behaviorType->methods);
        free(behaviorType);
    }
    else if (IS_FUNCTION_TYPE(type)) {
        FunctionTypeInfo* functionType = AS_FUNCTION_TYPE(type);
        if (functionType->paramTypes != NULL) TypeInfoArrayFree(functionType->paramTypes);
        free(functionType);
    }
}

TypeTable* newTypeTable(int id) {
    TypeTable* typetab = (TypeTable*)malloc(sizeof(TypeTable));
    if (typetab != NULL) {
        typetab->id = id;
        typetab->count = 0;
        typetab->capacity = 0;
        typetab->entries = NULL;
    }
    return typetab;
}

void freeTypeTable(TypeTable* typetab) {
    for (int i = 0; i < typetab->capacity; i++) {
        TypeEntry* entry = &typetab->entries[i];
        if (entry != NULL) {
            freeTypeInfo(entry->value);
        }
    }

    if (typetab->entries != NULL) free(typetab->entries);
    free(typetab);
}

static TypeEntry* findTypeEntry(TypeEntry* entries, int capacity, ObjString* key) {
    uint32_t index = key->hash & (capacity - 1);
    for (;;) {
        TypeEntry* entry = &entries[index];
        if (entry->key == key || entry->key == NULL) {
            return entry;
        }
        index = (index + 1) & (capacity - 1);
    }
}

static void typeTableAdjustCapacity(TypeTable* typetab, int capacity) {
    int oldCapacity = typetab->capacity;
    TypeEntry* entries = (TypeEntry*)malloc(sizeof(TypeTable) * capacity);
    if (entries == NULL) exit(1);

    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NULL;
    }

    typetab->count = 0;
    for (int i = 0; i < typetab->capacity; i++) {
        TypeEntry* entry = &typetab->entries[i];
        if (entry->key == NULL) continue;

        TypeEntry* dest = findTypeEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        typetab->count++;
    }

    free(typetab->entries);
    typetab->capacity = capacity;
    typetab->entries = entries;
}

TypeInfo* typeTableGet(TypeTable* typetab, ObjString* key) {
    TypeEntry* entry = findTypeEntry(typetab->entries, typetab->capacity, key);
    if (entry->key == NULL) return NULL;
    return entry->value;
}

bool typeTableSet(TypeTable* typetab, ObjString* key, TypeInfo* value) {
    if (typetab->count + 1 > typetab->capacity * TYPE_TABLE_MAX_LOAD) {
        int capacity = bufferGrowCapacity(typetab->capacity);
        typeTableAdjustCapacity(typetab, capacity);
    }

    TypeEntry* entry = findTypeEntry(typetab->entries, typetab->capacity, key);
    if (entry->key != NULL) return false;
    typetab->count++;

    entry->key = key;
    entry->value = value;
    return true;
}

BehaviorTypeInfo* insertBehaviorTypeTable(TypeTable* typetab, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType) {
    int id = typetab->count + 1;
    BehaviorTypeInfo* behaviorType = newBehaviorInfo(id, category, shortName, fullName, superclassType);
    typeTableSet(typetab, fullName, (TypeInfo*)behaviorType);
    return behaviorType;
}

FunctionTypeInfo* insertFunctionTypeTable(TypeTable* typetab, TypeCategory category, ObjString* name, TypeInfo* returnType) {
    int id = typetab->count + 1;
    FunctionTypeInfo* functionType = newFunctionInfo(id, category, name, returnType);
    typeTableSet(typetab, name, (TypeInfo*)functionType);
    return functionType;
}

static void typeTableOutputCategory(TypeCategory category) {
    switch (category) {
        case TYPE_CATEGORY_CLASS:
            printf("class");
            break;
        case TYPE_CATEGORY_METACLASS:
            printf("metaclass");
            break;
        case TYPE_CATEGORY_TRAIT:
            printf("trait");
            break;
        case TYPE_CATEGORY_FUNCTION:
            printf("function");
            break;
        case TYPE_CATEGORY_METHOD:
            printf("method");
            break;
        default:
            printf("none");
    }
    printf("\n");
}

static void typeTableOutputBehavior(BehaviorTypeInfo* behavior) {
    if (behavior->superclassType != NULL) {
        printf("    superclass: %s\n", behavior->superclassType->fullName->chars);
    }

    if (behavior->traitTypes != NULL && behavior->traitTypes->count > 0) {
        printf("    traits: %s", behavior->traitTypes->elements[0]->fullName->chars);
        for (int i = 1; i < behavior->traitTypes->count; i++) {
            printf(", %s", behavior->traitTypes->elements[i]->fullName->chars);
        }
        printf("\n");
    }

    if (behavior->methods != NULL && behavior->methods->count > 0) {
        printf("    methods:\n");
        for (int i = 0; i < behavior->methods->capacity; i++) {
            TypeEntry* entry = &behavior->methods->entries[i];
            if (entry != NULL && entry->key != NULL) {
                FunctionTypeInfo* method = AS_FUNCTION_TYPE(entry->value);
                printf("      %s", method->modifier.isAsync ? "async " : "");

                if (method->returnType == NULL) printf("dynamic ");
                else if (memcmp(method->returnType->shortName->chars, "Nil", 3) == 0) printf("void ");
                else printf("%s ", method->returnType->shortName->chars);
                printf("%s(", entry->key->chars);

                if (method->paramTypes->count > 0) {
                    printf("%s", method->paramTypes->elements[0]->shortName->chars);
                    for (int i = 1; i < method->paramTypes->count; i++) {
                        printf(", %s", method->paramTypes->elements[i]->shortName->chars);
                    }
                }
                printf(")\n");
            }
        }
    }
}

static void typeTableOutputFunction(FunctionTypeInfo* function) {
    printf("    signature: %s %s(", (function->returnType != NULL) ? function->returnType->shortName->chars : "dynamic", function->baseType.shortName->chars);
    if (function->paramTypes != NULL && function->paramTypes->count > 0) {
        printf("%s", function->paramTypes->elements[0]->shortName->chars);
        for (int i = 1; i < function->paramTypes->count; i++) {
            printf(", %s", function->paramTypes->elements[i]->shortName->chars);
        }
    } 
    printf(")\n");
}

static void typeTableOutputEntry(TypeEntry* entry) {
    printf("  %s(%s)\n    id: %d\n    category: ", entry->value->shortName->chars, entry->value->fullName->chars, entry->value->id);
    typeTableOutputCategory(entry->value->category);
    if (IS_BEHAVIOR_TYPE(entry->value)) typeTableOutputBehavior(AS_BEHAVIOR_TYPE(entry->value));
    else if (IS_FUNCTION_TYPE(entry->value)) typeTableOutputFunction(AS_FUNCTION_TYPE(entry->value));
    printf("\n");
}

void typeTableOutput(TypeTable* typetab) {
    printf("type table(count: %d)\n", typetab->count);

    for (int i = 0; i < typetab->capacity; i++) {
        TypeEntry* entry = &typetab->entries[i];
        if (entry != NULL && entry->key != NULL) {
            typeTableOutputEntry(entry);
        }
    }

    printf("\n");
}