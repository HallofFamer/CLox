#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "type.h"
#include "../common/buffer.h"
#include "../vm/object.h"

#define TYPE_TABLE_MAX_LOAD 0.75
DEFINE_BUFFER(TypeInfoArray, TypeInfo*)

BehaviorTypeInfo* newBehaviorInfo(TypeInfo* superclassType, int numTraits, ...) {
    BehaviorTypeInfo* behavior = (BehaviorTypeInfo*)malloc(sizeof(BehaviorTypeInfo));
    if (behavior != NULL) {
        behavior->superclassType = superclassType;
        behavior->traitTypes = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
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

void freeBehaviorTypeInfo(BehaviorTypeInfo* behavior) {
    if (behavior->traitTypes != NULL) {
        TypeInfoArrayFree(behavior->traitTypes);
    }
    free(behavior);
}

FunctionTypeInfo* newFunctionInfo(TypeInfo* returnType, int numParams, ...) {
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

TypeInfo* newTypeInfo(int id, TypeCategory category, ObjString* shortName, ObjString* fullName, BehaviorTypeInfo* behavior, FunctionTypeInfo* function) {
    TypeInfo* type = (TypeInfo*)malloc(sizeof(TypeInfo));
    if (type != NULL) {
        type->id = id;
        type->category = category;
        type->shortName = shortName;
        type->fullName = fullName;
        type->behavior = behavior;
        type->function = function;
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
}

static void typeTableOutputEntry(TypeEntry* entry) {
    printf("  %s(%s) -> id: %d, category: ", entry->value->shortName->chars, entry->value->fullName->chars, entry->value->id);
    typeTableOutputCategory(entry->value->category);
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