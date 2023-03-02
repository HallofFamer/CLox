#pragma once
#ifndef clox_object_h
#define clox_object_h

#include <stdio.h>
#include <sys/stat.h>

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value)             (AS_OBJ(value)->type)
#define OBJ_KLASS(value)            (AS_OBJ(value)->klass)

#define IS_ARRAY(value)             isObjType(value, OBJ_ARRAY)
#define IS_BOUND_METHOD(value)      isObjType(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value)             isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)           isObjType(value, OBJ_CLOSURE)
#define IS_DICTIONARY(value)        isObjType(value, OBJ_DICTIONARY)
#define IS_ENTRY(value)             isObjType(value, OBJ_ENTRY)
#define IS_FILE(value)              isObjType(value, OBJ_FILE)
#define IS_FUNCTION(value)          isObjType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value)          isObjType(value, OBJ_INSTANCE)
#define IS_INTERNAL_INSTANCE(value) isObjType(value, OBJ_INTERNAL_INSTANCE)
#define IS_NATIVE_FUNCTION(value)   isObjType(value, OBJ_NATIVE_FUNCTION)
#define IS_NATIVE_METHOD(value)     isObjType(value, OBJ_NATIVE_METHOD)
#define IS_NODE(value)              isObjType(value, OBJ_NODE)
#define IS_RECORD(value)            isObjType(value, OBJ_RECORD)
#define IS_STRING(value)            isObjType(value, OBJ_STRING)

#define AS_ARRAY(value)             ((ObjArray*)asObj(value))
#define AS_BOUND_METHOD(value)      ((ObjBoundMethod*)asObj(value))
#define AS_CLASS(value)             ((ObjClass*)asObj(value))
#define AS_CLOSURE(value)           ((ObjClosure*)asObj(value))
#define AS_DICTIONARY(value)        ((ObjDictionary*)asObj(value))
#define AS_ENTRY(value)             ((ObjEntry*)asObj(value))
#define AS_FILE(value)              ((ObjFile*)asObj(value))
#define AS_FUNCTION(value)          ((ObjFunction*)asObj(value))
#define AS_INSTANCE(value)          ((ObjInstance*)asObj(value))
#define AS_INTERNAL_INSTANCE(value) (((ObjInstance*)asObj(value))->native)
#define AS_NATIVE_FUNCTION(value)   ((ObjNativeFunction*)asObj(value))
#define AS_NATIVE_METHOD(value)     ((ObjNativeMethod*)asObj(value))
#define AS_NODE(value)              ((ObjNode*)asObj(value))
#define AS_RECORD(value)            ((ObjRecord*)asObj(value))
#define AS_CRECORD(value, type)     ((type*)((ObjRecord*)asObj(value)->data))
#define AS_STRING(value)            ((ObjString*)asObj(value))
#define AS_CSTRING(value)           (((ObjString*)asObj(value))->chars)

typedef enum {
    OBJ_ARRAY,
    OBJ_BOUND_METHOD,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_DICTIONARY,
    OBJ_ENTRY,
    OBJ_FILE,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
    OBJ_INTERNAL_INSTANCE,
    OBJ_NATIVE_FUNCTION,
    OBJ_NATIVE_METHOD,
    OBJ_NODE,
    OBJ_RECORD,
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

typedef Value (*NativeFunction)(VM* vm, int argCount, Value* args);
typedef Value (*NativeMethod)(VM* vm, Value receiver, int argCount, Value* args);
typedef void (*FreeFunction)(void* func);

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

typedef struct ObjNode {
    Obj obj;
    Value element;
    struct ObjNode* prev;
    struct ObjNode* next;
} ObjNode;

struct ObjString {
    Obj obj;
    int length;
    uint32_t hash;
    char chars[];
};

typedef struct {
    Obj obj;
    ValueArray elements;
} ObjArray;
 
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

typedef struct {
    Obj obj;
    ObjString* name;
    ObjString* mode;
    bool isOpen;
    FILE* file;
} ObjFile;

typedef struct {
    Obj obj;
    void* data;
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
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalueCount;
} ObjClosure;

struct ObjClass {
    Obj obj;
    ObjString* name;
    struct ObjClass* superclass;
    bool isNative;
    Table methods;
    ObjType internalType;
    Table internalMethods;
};

typedef struct {
    Obj obj;
    Table fields;
    bool isInternal;
    Value internal;
} ObjInstance;

typedef struct {
    Obj obj;
    Value receiver;
    ObjClosure* method;
} ObjBoundMethod;

Obj* allocateObject(VM* vm, size_t size, ObjType type, ObjClass* klass);
ObjArray* newArray(VM* vm);
ObjBoundMethod* newBoundMethod(VM* vm, Value receiver, ObjClosure* method);
ObjClass* newClass(VM* vm, ObjString* name);
ObjClosure* newClosure(VM* vm, ObjFunction* function);
ObjDictionary* newDictionary(VM* vm);
ObjEntry* newEntry(VM* vm, Value key, Value value);
ObjFile* newFile(VM* vm, ObjString* name);
ObjFunction* newFunction(VM* vm);
ObjInstance* newInstance(VM* vm, ObjClass* klass);
ObjInstance* newInternalInstance(VM* vm, ObjClass* klass, Value native);
ObjNativeFunction* newNativeFunction(VM* vm, ObjString* name, int arity, NativeFunction function);
ObjNativeMethod* newNativeMethod(VM* vm, ObjClass* klass, ObjString* name, int arity, NativeMethod method);
ObjNode* newNode(VM* vm, Value element, ObjNode* prev, ObjNode* next);
ObjRecord* newRecord(VM* vm, void* data);
ObjUpvalue* newUpvalue(VM* vm, Value* slot);

ObjClass* getObjClass(VM* vm, Value value);
bool isObjInstanceOf(VM* vm, Value value, ObjClass* klass);
Value getObjProperty(VM* vm, ObjInstance* object, char* name);
void setObjProperty(VM* vm, ObjInstance* object, char* name, Value value);
void copyObjProperty(VM* vm, ObjInstance* object, ObjInstance* object2, char* name);
void copyObjProperties(VM* vm, ObjInstance* fromObject, ObjInstance* toObject);
void printObject(Value value);

static inline bool isInternalObjType(Obj* object, ObjType type) {
    if (object->type != OBJ_INTERNAL_INSTANCE) return false;
    Value internal = ((ObjInstance*)object)->internal;
    return IS_OBJ(internal) && AS_OBJ(internal)->type == type;
}

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && ((AS_OBJ(value)->type == type) || isInternalObjType(AS_OBJ(value), type));
}

static inline Obj* asObj(Value value) {
    Obj* object = AS_OBJ(value);
    return object->type == OBJ_INTERNAL_INSTANCE ? AS_OBJ(((ObjInstance*)object)->internal) : object;
}

#endif // !clox_object_h