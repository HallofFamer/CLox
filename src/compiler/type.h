#pragma once
#ifndef clox_type_h
#define clox_type_h

#include "../common/buffer.h"
#include "../common/common.h"
#include "../vm/value.h"

typedef struct TypeInfo TypeInfo;
typedef struct TypeTable TypeTable;
DECLARE_BUFFER(TypeInfoArray, TypeInfo*)

#define IS_BEHAVIOR_TYPE(type) (type->category == TYPE_CATEGORY_CLASS || type->category == TYPE_CATEGORY_METACLASS || type->category == TYPE_CATEGORY_TRAIT)
#define IS_CALLABLE_TYPE(type) (type->category == TYPE_CATEGORY_FUNCTION || type->category == TYPE_CATEGORY_METHOD)

#define AS_BEHAVIOR_TYPE(type) ((BehaviorTypeInfo*)type)
#define AS_CALLABLE_TYPE(type) ((CallableTypeInfo*)type)

typedef enum {
    TYPE_CATEGORY_NONE,
    TYPE_CATEGORY_CLASS,
    TYPE_CATEGORY_METACLASS,
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
    bool isAsync;
    bool isClassMethod;
    bool isGenerator;
    bool isInitializer;
    bool isInstanceMethod;
    bool isLambda;
    bool isVariadic;
} CallableTypeModifier;

typedef struct {
    TypeInfo baseType;
    TypeInfo* returnType;
    TypeInfoArray* paramTypes;
    CallableTypeModifier modifier;
} CallableTypeInfo;

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

static inline CallableTypeModifier callableTypeInitModifier() {
    CallableTypeModifier modifier = {
        .isAsync = false,
        .isClassMethod = false,
        .isGenerator = false,
        .isInitializer = false,
        .isInstanceMethod = false,
        .isLambda = false,
        .isVariadic = false
    };
    return modifier;
}

TypeInfo* newTypeInfo(int id, size_t size, TypeCategory category, ObjString* shortName, ObjString* fullName);
BehaviorTypeInfo* newBehaviorTypeInfo(int id, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType);
BehaviorTypeInfo* newBehaviorTypeInfoWithTraits(int id, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType, int numTraits, ...);
BehaviorTypeInfo* newBehaviorTypeInfoWithMethods(int id, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType, TypeTable* methods);
CallableTypeInfo* newCallableTypeInfo(int id, TypeCategory category, ObjString* name, TypeInfo* returnType);
CallableTypeInfo* newCallableTypeInfoWithParams(int id, TypeCategory category, ObjString* name, TypeInfo* returnType, int numParams, ...);
void freeTypeInfo(TypeInfo* type);
TypeTable* newTypeTable(int id);
void freeTypeTable(TypeTable* typeTable);
TypeInfo* typeTableGet(TypeTable* typetab, ObjString* key);
bool typeTableSet(TypeTable* typetab, ObjString* key, TypeInfo* value);
TypeInfo* typeTableMethodLookup(TypeInfo* type, ObjString* key);
BehaviorTypeInfo* typeTableInsertBehavior(TypeTable* typetab, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType);
CallableTypeInfo* typeTableInsertCallable(TypeTable* typetab, TypeCategory category, ObjString* name, TypeInfo* returnType);
void typeTableOutput(TypeTable* typetab);

bool isEqualType(TypeInfo* type, TypeInfo* type2);
bool isSubtypeOfType(TypeInfo* type, TypeInfo* type2);

#endif // !clox_type_h