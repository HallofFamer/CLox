#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value)            (AS_OBJ(value)->type)

#define IS_BOUND_METHOD(value)     isObjType(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value)            isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)          isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)         isObjType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value)         isObjType(value, OBJ_INSTANCE)
#define IS_NATIVE_FUNCTION(value)  isObjType(value, OBJ_NATIVE_FUNCTION)
#define IS_NATIVE_METHOD(value)    isObjType(value, OBJ_NATIVE_METHOD)
#define IS_STRING(value)           isObjType(value, OBJ_STRING)

#define AS_BOUND_METHOD(value)     ((ObjBoundMethod*)AS_OBJ(value))
#define AS_CLASS(value)            ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)          ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)         ((ObjFunction*)AS_OBJ(value))
#define AS_INSTANCE(value)         ((ObjInstance*)AS_OBJ(value))
#define AS_NATIVE_FUNCTION(value)  (((ObjNativeFunction*)AS_OBJ(value))->function)
#define AS_NATIVE_METHOD(value)    (((ObjNativeMethod*)AS_OBJ(value))->method)
#define AS_STRING(value)           ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)          (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_BOUND_METHOD,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
    OBJ_NATIVE_FUNCTION,
    OBJ_NATIVE_METHOD,
    OBJ_STRING,
    OBJ_UPVALUE
} ObjType;

struct Obj {
    ObjType type;
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
    NativeFn function;
} ObjNativeFunction;

typedef struct {
    Obj obj;
    NativeMethod method;
} ObjNativeMethod;

struct ObjString {
    Obj obj;
    int length;
    uint32_t hash;
    char chars[];
};

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
    ObjClass* klass;
    Table fields;
} ObjInstance;

typedef struct {
    Obj obj;
    Value receiver;
    ObjClosure* method;
} ObjBoundMethod;

ObjBoundMethod* newBoundMethod(VM* vm, Value receiver, ObjClosure* method);
ObjClass* newClass(VM* vm, ObjString* name);
ObjClosure* newClosure(VM* vm, ObjFunction* function);
ObjFunction* newFunction(VM* vm);
ObjInstance* newInstance(VM* vm, ObjClass* klass);
ObjNativeFunction* newNativeFunction(VM* vm, NativeFn function);
ObjNativeMethod* newNativeMethod(VM* vm, NativeMethod method);
ObjString* takeString(VM* vm, char* chars, int length);
ObjString* copyString(VM* vm, const char* chars, int length);
ObjString* formattedString(VM* vm, const char* format, ...);
ObjString* formattedLongString(VM* vm, const char* format, ...);
ObjUpvalue* newUpvalue(VM* vm, Value* slot);
ObjClass* getObjClass(VM* vm, Value value);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif