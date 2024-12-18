#pragma once
#ifndef clox_type_h
#define clox_type_h

#include "../common/buffer.h"
#include "../common/common.h"
#include "../vm/value.h"

typedef struct TypeInfo TypeInfo;
typedef struct TypeTable TypeTable;
DECLARE_BUFFER(TypeInfoArray, TypeInfo*)

typedef enum {
    TYPE_CATEGORY_NONE,
    TYPE_CATEGORY_CLASS,
    TYPE_CATEGORY_TRAIT,
    TYPE_CATEGORY_FUNCTION,
    TYPE_CATEGORY_METHOD
} TypeCategory;

typedef struct {
    TypeInfo* superclassType;
    TypeInfoArray* traitTypes;
    TypeTable* methods;
} BehaviorTypeInfo;

typedef struct {
    TypeInfo* returnType;
    TypeInfoArray* paramTypes;
} FunctionTypeInfo;

struct TypeInfo {
    int id;
    TypeCategory category;
    ObjString* shortName;
    ObjString* fullName;
    BehaviorTypeInfo* behavior;
    FunctionTypeInfo* function;
};

typedef struct {
    ObjString* key;
    TypeInfo* value;
} TypeEntry;

struct TypeTable {
    int count;
    int capacity;
    TypeEntry* entries;
};

BehaviorTypeInfo* newBehaviorInfo(int id, TypeInfo* superclass, int numTraits, ...);
void freeBehaviorTypeInfo(BehaviorTypeInfo* behavior);
FunctionTypeInfo* newFunctionInfo(TypeInfo* returnType, int numParams, ...);
void freeFunctionTypeInfo(FunctionTypeInfo* function);
TypeInfo* newTypeInfo(int id, TypeCategory category, ObjString* shortName, ObjString* fullName, BehaviorTypeInfo* behavior, FunctionTypeInfo* function);
void freeTypeInfo(TypeInfo* type);
TypeTable* newTypeTable();
void freeTypeTable(TypeTable* typeTable);
TypeInfo* typeTableGet(TypeTable* typetab, ObjString* key);
bool typeTableSet(TypeTable* typetab, ObjString* key, TypeInfo* value);
void typeTableOutput(TypeTable* typetab);

#endif // !clox_type_h