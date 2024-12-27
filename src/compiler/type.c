#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "type.h"
#include "../vm/object.h"

#define TYPE_TABLE_MAX_LOAD 0.75
DEFINE_BUFFER(TypeInfoArray, TypeInfo*)

BehaviorTypeInfo* newBehaviorInfo(TypeInfo* superclassType) {
    BehaviorTypeInfo* behavior = (BehaviorTypeInfo*)malloc(sizeof(BehaviorTypeInfo));
    if (behavior != NULL) {
        behavior->superclassType = superclassType;
        behavior->traitTypes = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        if(behavior->traitTypes != NULL) TypeInfoArrayInit(behavior->traitTypes);
        behavior->methods = newTypeTable();
    }
    return behavior;
}

BehaviorTypeInfo* newBehaviorInfoWithTraits(TypeInfo* superclassType, int numTraits, ...) {
    BehaviorTypeInfo* behavior = (BehaviorTypeInfo*)malloc(sizeof(BehaviorTypeInfo));
    if (behavior != NULL) {
        behavior->superclassType = superclassType;
        behavior->traitTypes = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        behavior->methods = newTypeTable();

        if (behavior->traitTypes != NULL) {
            TypeInfoArrayInit(behavior->traitTypes);
            va_list args;
            va_start(args, numTraits);

            for (int i = 0; i < numTraits; i++) {
                TypeInfo* type = va_arg(args, TypeInfo*);
                TypeInfoArrayAdd(behavior->traitTypes, type);
            }
            va_end(args);
        }
    }
    return behavior;
}

BehaviorTypeInfo* newBehaviorInfoWithMethods(TypeInfo* superclassType, TypeTable* methods) {
    BehaviorTypeInfo* behavior = (BehaviorTypeInfo*)malloc(sizeof(BehaviorTypeInfo));
    if (behavior != NULL) {
        behavior->superclassType = superclassType;
        behavior->traitTypes = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        TypeInfoArrayInit(behavior->traitTypes);
        behavior->methods = methods;
    }
    return behavior;
}

void freeBehaviorTypeInfo(BehaviorTypeInfo* behavior) {
    if (behavior->traitTypes != NULL) {
        TypeInfoArrayFree(behavior->traitTypes);
    }
    freeTypeTable(behavior->methods);
    free(behavior);
}

FunctionTypeInfo* newFunctionInfo(TypeInfo* returnType) {
    FunctionTypeInfo* function = (FunctionTypeInfo*)malloc(sizeof(FunctionTypeInfo));
    if (function != NULL) {
        function->returnType = returnType;
        function->paramTypes = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        if (function->paramTypes != NULL) TypeInfoArrayInit(function->paramTypes);
    }
    return function;
}

FunctionTypeInfo* newFunctionInfoWithParams(TypeInfo* returnType, int numParams, ...) {
    FunctionTypeInfo* function = (FunctionTypeInfo*)malloc(sizeof(FunctionTypeInfo));
    if (function != NULL) {
        function->returnType = returnType;
        function->paramTypes = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        if (function->paramTypes != NULL) {
            TypeInfoArrayInit(function->paramTypes);
            va_list args;
            va_start(args, numParams);
            for (int i = 0; i < numParams; i++) {
                TypeInfo* type = va_arg(args, TypeInfo*);
                TypeInfoArrayAdd(function->paramTypes, type);
            }
            va_end(args);
        }
    }
    return function;
}

void freeFunctionTypeInfo(FunctionTypeInfo* function) {
    if (function->paramTypes != NULL) {
        TypeInfoArrayFree(function->paramTypes);
    }
    free(function);
}

static void initAdditionalInfo(TypeInfo* type, void* additionalInfo) {
    switch (type->category) {
        case TYPE_CATEGORY_CLASS:
        case TYPE_CATEGORY_TRAIT:
            type->behavior = (BehaviorTypeInfo*)additionalInfo;
            break;
        case TYPE_CATEGORY_FUNCTION:
        case TYPE_CATEGORY_METHOD:
            type->function = (FunctionTypeInfo*)additionalInfo;
            break;
        default:
            break;
    }
}

TypeInfo* newTypeInfo(int id, TypeCategory category, ObjString* shortName, ObjString* fullName, void* additionalInfo) {
    TypeInfo* type = (TypeInfo*)malloc(sizeof(TypeInfo));
    if (type != NULL) {
        type->id = id;
        type->category = category;
        type->shortName = shortName;
        type->fullName = fullName;
        type->behavior = NULL;
        type->function = NULL;
        if (additionalInfo != NULL) initAdditionalInfo(type, additionalInfo);
    }
    return type;
}

void freeTypeInfo(TypeInfo* type) {
    if (type != NULL) {
        if (type->behavior != NULL) freeBehaviorTypeInfo(type->behavior);
        if (type->function != NULL) freeFunctionTypeInfo(type->function);
        free(type);
    }
}

TypeTable* newTypeTable() {
    TypeTable* typetab = (TypeTable*)malloc(sizeof(TypeTable));
    if (typetab != NULL) {
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

static void typeTableOutputCategory(TypeCategory category) {
    switch (category) {
        case TYPE_CATEGORY_CLASS:
            printf("class");
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
        typeTableOutput(behavior->methods);
    }
}

static void typeTableOutputFunction(FunctionTypeInfo* function) {
    printf("    return: %s\n", (function->returnType != NULL) ? function->returnType->fullName->chars : "dynamic");
    if (function->paramTypes != NULL && function->paramTypes->count > 0) {
        printf("    params:\n      %i: %s\n", 1, function->paramTypes->elements[0]->fullName->chars);
        for (int i = 1; i < function->paramTypes->count; i++) {
            printf("      %i: %s\n", i + 1, function->paramTypes->elements[i]->fullName->chars);
        }
    } 
}

static void typeTableOutputEntry(TypeEntry* entry) {
    printf("  %s(%s)\n    id: %d\n    category: ", entry->value->shortName->chars, entry->value->fullName->chars, entry->value->id);
    typeTableOutputCategory(entry->value->category);
    if (entry->value->behavior != NULL) typeTableOutputBehavior(entry->value->behavior);
    if (entry->value->function != NULL) typeTableOutputFunction(entry->value->function);
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