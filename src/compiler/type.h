#pragma once
#ifndef clox_type_h
#define clox_type_h

#include "../common/buffer.h"
#include "../common/common.h"
#include "../vm/value.h"

typedef struct TypeInfo TypeInfo;
typedef struct TypeTable TypeTable;
DECLARE_BUFFER(TypeInfoArray, TypeInfo*)

#define IS_BEHAVIOR_TYPE(type) (type->category == TYPE_CATEGORY_CLASS || type->category == TYPE_CATEGORY_TRAIT)
#define IS_FUNCTION_TYPE(type) (type->category == TYPE_CATEGORY_FUNCTION || type->category == TYPE_CATEGORY_METHOD)

#define AS_BEHAVIOR_TYPE(type) ((BehaviorTypeInfo*)type)
#define AS_FUNCTION_TYPE(type) ((FunctionTypeInfo*)type)

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
};

typedef struct {
    TypeInfo baseType;
    TypeInfo* superclassType;
    TypeInfoArray* traitTypes;
    TypeTable* methods;
} BehaviorTypeInfo;

typedef struct {
    TypeInfo baseType;
    TypeInfo* returnType;
    TypeInfoArray* paramTypes;
} FunctionTypeInfo;

typedef struct {
    ObjString* key;
    TypeInfo* value;
} TypeEntry;

struct TypeTable {
    int id;
    int count;
    int capacity;
    TypeEntry* entries;
};

TypeInfo* newTypeInfo(int id, size_t size, TypeCategory category, ObjString* shortName, ObjString* fullName);
BehaviorTypeInfo* newBehaviorInfo(int id, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType);
BehaviorTypeInfo* newBehaviorInfoWithTraits(int id, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType, int numTraits, ...);
BehaviorTypeInfo* newBehaviorInfoWithMethods(int id, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType, TypeTable* methods);
FunctionTypeInfo* newFunctionInfo(int id, TypeCategory category, ObjString* name, TypeInfo* returnType);
FunctionTypeInfo* newFunctionInfoWithParams(int id, TypeCategory category, ObjString* name, TypeInfo* returnType, int numParams, ...);
void freeTypeInfo(TypeInfo* type);
TypeTable* newTypeTable(int id);
void freeTypeTable(TypeTable* typeTable);
TypeInfo* typeTableGet(TypeTable* typetab, ObjString* key);
bool typeTableSet(TypeTable* typetab, ObjString* key, TypeInfo* value);
BehaviorTypeInfo* insertBehaviorTypeTable(TypeTable* typetab, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType);
FunctionTypeInfo* insertFunctionTypeTable(TypeTable* typetab, TypeCategory category, ObjString* name, TypeInfo* returnType);
void typeTableOutput(TypeTable* typetab);

#endif // !clox_type_h