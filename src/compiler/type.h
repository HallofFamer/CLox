#pragma once
#ifndef clox_type_h
#define clox_type_h

#include "../common/common.h"
#include "../vm/value.h"

typedef struct TypeInfo TypeInfo;

typedef enum {
    TYPE_CATEGORY_NONE,
    TYPE_CATEGORY_CLASS,
    TYPE_CATEGORY_TRAIT,
    TYPE_CATEGORY_FUNCTION,
    TYPE_CATEGORY_METHOD
} TypeCategory;

struct TypeInfo {
    int id;
    TypeCategory category;
    ObjString* shortName;
    ObjString* fullName;
    TypeInfo* superclass;
};

typedef struct {
    ObjString* key;
    TypeInfo* value;
} TypeEntry;

typedef struct {
    int count;
    int capacity;
    TypeEntry* entries;
} TypeTable;

TypeInfo* newTypeInfo(int id, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclass);
void freeTypeInfo(TypeInfo* type);
TypeTable* newTypeTable();
void freeTypeTable(TypeTable* typeTable);
TypeInfo* typeTableGet(TypeTable* typetab, ObjString* key);
bool typeTableSet(TypeTable* typetab, ObjString* key, TypeInfo* value);

#endif // !clox_type_h