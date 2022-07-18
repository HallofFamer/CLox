#pragma once
#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value)            (AS_OBJ(value)->type)
#define OBJ_KLASS(value)           (AS_OBJ(value)->klass)

#define IS_BOUND_METHOD(value)     isObjType(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value)            isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)          isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)         isObjType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value)         isObjType(value, OBJ_INSTANCE)
#define IS_LIST(value)             isObjType(value, OBJ_LIST)
#define IS_NATIVE_FUNCTION(value)  isObjType(value, OBJ_NATIVE_FUNCTION)
#define IS_NATIVE_METHOD(value)    isObjType(value, OBJ_NATIVE_METHOD)
#define IS_STRING(value)           isObjType(value, OBJ_STRING)

#define AS_BOUND_METHOD(value)     ((ObjBoundMethod*)AS_OBJ(value))
#define AS_CLASS(value)            ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)          ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)         ((ObjFunction*)AS_OBJ(value))
#define AS_INSTANCE(value)         ((ObjInstance*)AS_OBJ(value))
#define AS_LIST(value)             ((ObjList*)AS_OBJ(value))
#define AS_NATIVE_FUNCTION(value)  ((ObjNativeFunction*)AS_OBJ(value))
#define AS_NATIVE_METHOD(value)    ((ObjNativeMethod*)AS_OBJ(value))
#define AS_STRING(value)           ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)          (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_BOUND_METHOD,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
    OBJ_LIST,
    OBJ_NATIVE_FUNCTION,
    OBJ_NATIVE_METHOD,
    OBJ_STRING,
    OBJ_UPVALUE
} ObjType;

struct Obj {
    ObjType type;
    ObjClass* klass;
    bool isMarked;
    struct Obj* next;
};

typedef struct {
    Obj obj;
    int arity;
    int upvalueCount;
    Chunk chunk;
    ObjString* name;
} ObjFunction;

typedef Value (*NativeFn)(VM* vm, int argCount, Value* args);
typedef Value (*NativeMethod)(VM* vm, Value receiver, int argCount, Value* args);

typedef struct {
    Obj obj;
    ObjString* name;
    int arity;
    NativeFn function;
} ObjNativeFunction;

typedef struct {
    Obj obj;
    ObjClass* klass;
    ObjString* name;
    int arity;
    NativeMethod method;
} ObjNativeMethod;

struct ObjString {
    Obj obj;
    int length;
    uint32_t hash;
    char chars[];
};

typedef struct ObjList {
    Obj obj;
    ValueArray elements;
} ObjList;

typedef struct ObjUpvalue {
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalueCount;
} ObjClosure;

struct ObjClass {
    Obj obj;
    ObjString* name;
    struct ObjClass* superclass;
    Table methods;
    bool isNative;
};

typedef struct {
    Obj obj;
    Table fields;
} ObjInstance;

typedef struct {
    Obj obj;
    Value receiver;
    ObjClosure* method;
} ObjBoundMethod;

Obj* allocateObject(VM* vm, size_t size, ObjType type, ObjClass* klass);
ObjBoundMethod* newBoundMethod(VM* vm, Value receiver, ObjClosure* method);
ObjClass* newClass(VM* vm, ObjString* name);
ObjClosure* newClosure(VM* vm, ObjFunction* function);
ObjFunction* newFunction(VM* vm);
ObjInstance* newInstance(VM* vm, ObjClass* klass);
ObjList* newList(VM* vm);
ObjList* copyList(VM* vm, ValueArray elements, int fromIndex, int toIndex);
ObjNativeFunction* newNativeFunction(VM* vm, ObjString* name, int arity, NativeFn function);
ObjNativeMethod* newNativeMethod(VM* vm, ObjClass* klass, ObjString* name, int arity, NativeMethod method);
ObjUpvalue* newUpvalue(VM* vm, Value* slot);
ObjClass* getObjClass(VM* vm, Value value);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif // !clox_object_h