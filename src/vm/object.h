#pragma once
#ifndef clox_object_h
#define clox_object_h

#include "chunk.h"
#include "common.h"
#include "exception.h"
#include "generator.h"
#include "id.h"
#include "interceptor.h"
#include "klass.h"
#include "loop.h"
#include "promise.h"
#include "table.h"
#include "value.h"

#define ALLOCATE_OBJ(type, objectType, objectClass) (type*)allocateObject(vm, sizeof(type), objectType, objectClass)
#define ALLOCATE_CLASS(classClass) ALLOCATE_OBJ(ObjClass, OBJ_CLASS, classClass)
#define ALLOCATE_CLOSURE(closureClass) ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE, closureClass)
#define ALLOCATE_NAMESPACE(namespaceClass) ALLOCATE_OBJ(ObjNamespace, OBJ_NAMESPACE, namespaceClass)

#define OBJ_TYPE(value)             (AS_OBJ(value)->type)
#define OBJ_KLASS(value)            (AS_OBJ(value)->klass)

#define IS_ARRAY(value)             isObjType(value, OBJ_ARRAY)
#define IS_BOOL_INSTANCE(arg)       (IS_BOOL(arg) || (IS_VALUE_INSTANCE(arg) && IS_BOOL(AS_VALUE_INSTANCE(arg)->value)))
#define IS_BOUND_METHOD(value)      isObjType(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value)             isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)           isObjType(value, OBJ_CLOSURE)
#define IS_DICTIONARY(value)        isObjType(value, OBJ_DICTIONARY)
#define IS_ENTRY(value)             isObjType(value, OBJ_ENTRY)
#define IS_EXCEPTION(value)         isObjType(value, OBJ_EXCEPTION)
#define IS_FILE(value)              isObjType(value, OBJ_FILE)
#define IS_FLOAT_INSTANCE(arg)      (IS_FLOAT(arg) || (IS_VALUE_INSTANCE(arg) && IS_FLOAT(AS_VALUE_INSTANCE(arg)->value))) 
#define IS_FRAME(value)             isObjType(value, OBJ_FRAME);
#define IS_FUNCTION(value)          isObjType(value, OBJ_FUNCTION)
#define IS_GENERATOR(value)         isObjType(value, OBJ_GENERATOR)
#define IS_INSTANCE(value)          isObjType(value, OBJ_INSTANCE)
#define IS_INT_INSTANCE(arg)        (IS_INT(arg) || (IS_VALUE_INSTANCE(arg) && IS_INT(AS_VALUE_INSTANCE(arg)->value))) 
#define IS_METHOD(value)            isObjType(value, OBJ_METHOD)
#define IS_MODULE(value)            isObjType(value, OBJ_MODULE)
#define IS_NAMESPACE(value)         isObjType(value, OBJ_NAMESPACE)
#define IS_NATIVE_FUNCTION(value)   isObjType(value, OBJ_NATIVE_FUNCTION)
#define IS_NATIVE_METHOD(value)     isObjType(value, OBJ_NATIVE_METHOD)
#define IS_NIL_INSTANCE(arg)        (IS_NIL(arg) || (IS_VALUE_INSTANCE(arg) && IS_NIL(AS_VALUE_INSTANCE(arg)->value))) 
#define IS_NODE(value)              isObjType(value, OBJ_NODE)
#define IS_NUMBER_INSTANCE(arg)     (IS_NUMBER(arg) || (IS_VALUE_INSTANCE(arg) && IS_NUMBER(AS_VALUE_INSTANCE(arg)->value))) 
#define IS_PROMISE(value)           isObjType(value, OBJ_PROMISE)
#define IS_RANGE(value)             isObjType(value, OBJ_RANGE)    
#define IS_RECORD(value)            isObjType(value, OBJ_RECORD)
#define IS_STRING(value)            isObjType(value, OBJ_STRING)
#define IS_TIMER(value)             isObjType(value, OBJ_TIMER)
#define IS_UPVALUE(value)           isObjType(value, OBJ_UPVALUE)
#define IS_VALUE_INSTANCE(value)    isObjType(value, OBJ_VALUE_INSTANCE)

#define AS_ARRAY(value)             ((ObjArray*)AS_OBJ(value))
#define AS_BOOL_INSTANCE(arg)       (IS_BOOL(arg) ? AS_BOOL(arg) : AS_BOOL(AS_VALUE_INSTANCE(arg)->value))
#define AS_BOUND_METHOD(value)      ((ObjBoundMethod*)AS_OBJ(value))
#define AS_CLASS(value)             ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)           ((ObjClosure*)AS_OBJ(value))
#define AS_DICTIONARY(value)        ((ObjDictionary*)AS_OBJ(value))
#define AS_ENTRY(value)             ((ObjEntry*)AS_OBJ(value))
#define AS_EXCEPTION(value)         ((ObjException*)AS_OBJ(value))
#define AS_FILE(value)              ((ObjFile*)AS_OBJ(value))
#define AS_FLOAT_INSTANCE(arg)      (IS_FLOAT(arg) ? AS_FLOAT(arg) : AS_FLOAT(AS_VALUE_INSTANCE(arg)->value))
#define AS_FRAME(value)             ((ObjFrame*)AS_OBJ(value))
#define AS_FUNCTION(value)          ((ObjFunction*)AS_OBJ(value))
#define AS_GENERATOR(value)         ((ObjGenerator*)AS_OBJ(value))
#define AS_INSTANCE(value)          ((ObjInstance*)AS_OBJ(value))
#define AS_INT_INSTANCE(arg)        (IS_INT(arg) ? AS_INT(arg) : AS_INT(AS_VALUE_INSTANCE(arg)->value))
#define AS_METHOD(value)            ((ObjMethod*)AS_OBJ(value))
#define AS_MODULE(value)            ((ObjModule*)AS_OBJ(value))
#define AS_NAMESPACE(value)         ((ObjNamespace*)AS_OBJ(value))
#define AS_NATIVE_FUNCTION(value)   ((ObjNativeFunction*)AS_OBJ(value))
#define AS_NATIVE_METHOD(value)     ((ObjNativeMethod*)AS_OBJ(value))
#define AS_NODE(value)              ((ObjNode*)AS_OBJ(value))
#define AS_NUMBER_INSTANCE(arg)     (IS_NUMBER(arg) ? AS_NUMBER(arg) : AS_NUMBER(AS_VALUE_INSTANCE(arg)->value))
#define AS_PROMISE(value)           ((ObjPromise*)AS_OBJ(value))
#define AS_RANGE(value)             ((ObjRange*)AS_OBJ(value))
#define AS_RECORD(value)            ((ObjRecord*)AS_OBJ(value))
#define AS_STRING(value)            ((ObjString*)AS_OBJ(value))
#define AS_TIMER(value)             ((ObjTimer*)AS_OBJ(value))
#define AS_UPVALUE(value)           ((ObjUpvalue*)AS_OBJ(value));
#define AS_VALUE_INSTANCE(value)    ((ObjValueInstance*)AS_OBJ(value))

#define AS_CARRAY(value)            (((ObjArray*)AS_OBJ(value))->elements)
#define AS_CRECORD(value, type)     ((type*)((ObjRecord*)AS_OBJ(value)->data))
#define AS_CSTRING(value)           (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_ARRAY,
    OBJ_BOUND_METHOD,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_DICTIONARY,
    OBJ_ENTRY,
    OBJ_EXCEPTION,
    OBJ_FILE,
    OBJ_FRAME,
    OBJ_FUNCTION,
    OBJ_GENERATOR,
    OBJ_INSTANCE,
    OBJ_METHOD,
    OBJ_MODULE,
    OBJ_NAMESPACE,
    OBJ_NATIVE_FUNCTION,
    OBJ_NATIVE_METHOD,
    OBJ_NODE,
    OBJ_PROMISE,
    OBJ_RANGE,
    OBJ_RECORD,
    OBJ_STRING,
    OBJ_TIMER,
    OBJ_UPVALUE,
    OBJ_VALUE_INSTANCE,
    OBJ_VOID
} ObjType;

struct Obj {
    uint64_t objectID;
    int shapeID;
    ObjType type;
    ObjClass* klass;
    bool isMarked;
    struct Obj* next;
};

typedef struct {
    Obj obj;
    int arity;
    int upvalueCount;
    bool isGenerator;
    bool isAsync;
    Chunk chunk;
    ObjString* name;
} ObjFunction;

typedef Value (*NativeFunction)(VM* vm, int argCount, Value* args);
typedef Value (*NativeMethod)(VM* vm, Value receiver, int argCount, Value* args);
typedef void (*MarkFunction)(void* data);
typedef void (*FreeFunction)(void* data);

struct ObjInstance {
    Obj obj;
    ValueArray fields;
};

typedef struct {
    Obj obj;
    Value value;
    ValueArray fields;
} ObjValueInstance;

typedef struct {
    Obj obj;
    ObjString* name;
    int arity;
    NativeFunction function;
} ObjNativeFunction;

typedef struct {
    Obj obj;
    ObjClass* klass;
    ObjString* name;
    int arity;
    NativeMethod method;
} ObjNativeMethod;
 
typedef struct {
    Obj obj;
    Value key;
    Value value;
} ObjEntry;

typedef struct {
    Obj obj;
    int capacity;
    int count;
    ObjEntry* entries;
} ObjDictionary;

struct ObjFile{
    Obj obj;
    ObjString* name;
    ObjString* mode;
    bool isOpen;
    size_t offset;
    uv_fs_t* fsStat;
    uv_fs_t* fsOpen;
    uv_fs_t* fsRead;
    uv_fs_t* fsWrite;
};

typedef struct {
    Obj obj;
    int from;
    int to;
} ObjRange;

typedef struct {
    Obj obj;
    void* data;
    MarkFunction markFunction;
    FreeFunction freeFunction;
} ObjRecord;

typedef struct ObjUpvalue {
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
    Obj obj;
    ObjClass* behavior;
    ObjClosure* closure;
} ObjMethod;

typedef struct {
    Obj obj;
    Value receiver;
    Value method;
    bool isNative;
} ObjBoundMethod;

typedef struct {
    Obj obj;
    ObjClosure* closure;
    uint8_t* ip;
    Value slots[UINT8_MAX];
    uint8_t slotCount;
    uint8_t handlerCount;
    ExceptionHandler handlerStack[UINT4_MAX];
} ObjFrame;

typedef struct ObjGenerator {
    Obj obj;
    ObjFrame* frame;
    struct ObjGenerator* outer;
    struct ObjGenerator* inner;
    GeneratorState state;
    Value value;
} ObjGenerator;

typedef struct {
    Obj obj;
    uv_timer_t* timer;
    int id;
    bool isRunning;
} ObjTimer;

struct ObjArray {
    Obj obj;
    ValueArray elements;
};

struct ObjClass {
    Obj obj;
    ObjType classType;
    BehaviorType behaviorType;
    int behaviorID;
    ObjString* name;
    ObjString* fullName;
    struct ObjNamespace* namespace;
    struct ObjClass* superclass;
    ValueArray traits;
    bool isNative;
    uint16_t interceptors;
    IDMap indexes;
    ValueArray fields;
    Table methods;
};

struct ObjClosure {
    Obj obj;
    ObjFunction* function;
    ObjModule* module;
    ObjUpvalue** upvalues;
    int upvalueCount;
};

struct ObjException {
    Obj obj;
    ObjString* message;
    ObjArray* stacktrace;
};

struct ObjModule {
    Obj obj;
    ObjString* path;
    ObjClosure* closure;
    bool isNative;
    IDMap valIndexes;
    ValueArray valFields;
    IDMap varIndexes;
    ValueArray varFields;
};

struct ObjNamespace {
    Obj obj;
    ObjString* shortName;
    ObjString* fullName;
    struct ObjNamespace* enclosing;
    bool isRoot;
    Table values;
};

typedef struct ObjNode {
    Obj obj;
    Value element;
    struct ObjNode* prev;
    struct ObjNode* next;
} ObjNode;

struct ObjPromise {
    Obj obj;
    int id;
    PromiseState state;
    Value value;
    ObjDictionary* captures;
    ObjException* exception;
    Value executor;
    ValueArray handlers;
    Value onCatch;
    Value onFinally;
};

struct ObjString {
    Obj obj;
    int length;
    uint32_t hash;
    char chars[];
};

Obj* allocateObject(VM* vm, size_t size, ObjType type, ObjClass* klass);
ObjArray* newArray(VM* vm);
ObjBoundMethod* newBoundMethod(VM* vm, Value receiver, Value method);
ObjClass* newClass(VM* vm, ObjString* name, ObjType classType);
void initClosure(VM* vm, ObjClosure* closure, ObjFunction* function);
ObjClosure* newClosure(VM* vm, ObjFunction* function);
ObjDictionary* newDictionary(VM* vm);
ObjEntry* newEntry(VM* vm, Value key, Value value);
ObjException* newException(VM* vm, ObjString* message, ObjClass* klass);
ObjFile* newFile(VM* vm, ObjString* name);
ObjFrame* newFrame(VM* vm, CallFrame* callFrame);
ObjFunction* newFunction(VM* vm);
ObjGenerator* newGenerator(VM* vm, ObjFrame* frame, ObjGenerator* outer);
ObjInstance* newInstance(VM* vm, ObjClass* klass);
ObjMethod* newMethod(VM* vm, ObjClass* behavior, ObjClosure* closure);
ObjModule* newModule(VM* vm, ObjString* path);
void initNamespace(VM* vm, ObjNamespace* namespace, ObjString* shortName, ObjNamespace* enclosing);
ObjNamespace* newNamespace(VM* vm, ObjString* shortName, ObjNamespace* enclosing);
ObjNativeFunction* newNativeFunction(VM* vm, ObjString* name, int arity, NativeFunction function);
ObjNativeMethod* newNativeMethod(VM* vm, ObjClass* klass, ObjString* name, int arity, NativeMethod method);
ObjNode* newNode(VM* vm, Value element, ObjNode* prev, ObjNode* next);
ObjPromise* newPromise(VM* vm, PromiseState state, Value value, Value executor);
ObjRange* newRange(VM* vm, int from, int to);
ObjRecord* newRecord(VM* vm, void* data);
ObjTimer* newTimer(VM* vm, ObjClosure* closure, int delay, int interval);
ObjUpvalue* newUpvalue(VM* vm, Value* slot);
ObjValueInstance* newValueInstance(VM* vm, Value value, ObjClass* klass);

Value getObjProperty(VM* vm, ObjInstance* object, char* name);
Value getObjPropertyByIndex(VM* vm, ObjInstance* object, int index);
void setObjProperty(VM* vm, ObjInstance* object, char* name, Value value);
void setObjPropertyByIndex(VM* vm, ObjInstance* object, int index, Value value);
void copyObjProperty(VM* vm, ObjInstance* object, ObjInstance* object2, char* name);
void copyObjProperties(VM* vm, ObjInstance* fromObject, ObjInstance* toObject);
Value getObjMethod(VM* vm, Value object, char* name);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && (AS_OBJ(value)->type == type);
}

#endif // !clox_object_h