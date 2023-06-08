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
#define IS_METHOD(value)            isObjType(value, OBJ_METHOD)
#define IS_NAMESPACE(value)         isObjType(value, OBJ_NAMESPACE)
#define IS_NATIVE_FUNCTION(value)   isObjType(value, OBJ_NATIVE_FUNCTION)
#define IS_NATIVE_METHOD(value)     isObjType(value, OBJ_NATIVE_METHOD)
#define IS_NODE(value)              isObjType(value, OBJ_NODE)
#define IS_RANGE(value)             isObjType(value, OBJ_RANGE)    
#define IS_RECORD(value)            isObjType(value, OBJ_RECORD)
#define IS_STRING(value)            isObjType(value, OBJ_STRING)

#define AS_ARRAY(value)             ((ObjArray*)AS_OBJ(value))
#define AS_BOUND_METHOD(value)      ((ObjBoundMethod*)AS_OBJ(value))
#define AS_CLASS(value)             ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)           ((ObjClosure*)AS_OBJ(value))
#define AS_DICTIONARY(value)        ((ObjDictionary*)AS_OBJ(value))
#define AS_ENTRY(value)             ((ObjEntry*)AS_OBJ(value))
#define AS_FILE(value)              ((ObjFile*)AS_OBJ(value))
#define AS_FUNCTION(value)          ((ObjFunction*)AS_OBJ(value))
#define AS_INSTANCE(value)          ((ObjInstance*)AS_OBJ(value))
#define AS_METHOD(value)            ((ObjMethod*)AS_OBJ(value))
#define AS_NAMESPACE(value)         ((ObjNamespace*)AS_OBJ(value))
#define AS_NATIVE_FUNCTION(value)   ((ObjNativeFunction*)AS_OBJ(value))
#define AS_NATIVE_METHOD(value)     ((ObjNativeMethod*)AS_OBJ(value))
#define AS_NODE(value)              ((ObjNode*)AS_OBJ(value))
#define AS_RANGE(value)             ((ObjRange*)AS_OBJ(value))
#define AS_RECORD(value)            ((ObjRecord*)AS_OBJ(value))
#define AS_CRECORD(value, type)     ((type*)((ObjRecord*)AS_OBJ(value)->data))
#define AS_STRING(value)            ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)           (((ObjString*)AS_OBJ(value))->chars)

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
    OBJ_METHOD,
    OBJ_NAMESPACE,
    OBJ_NATIVE_FUNCTION,
    OBJ_NATIVE_METHOD,
    OBJ_NODE,
    OBJ_RANGE,
    OBJ_RECORD,
    OBJ_STRING,
    OBJ_UPVALUE,
    OBJ_VALUE
} ObjType;

typedef enum {
    BEHAVIOR_CLASS,
    BEHAVIOR_METACLASS,
    BEHAVIOR_TRAIT
} BehaviorType;

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
typedef void (*MarkFunction)(void* data);
typedef void (*FreeFunction)(void* data);

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

struct ObjNode {
    Obj obj;
    Value element;
    struct ObjNode* prev;
    struct ObjNode* next;
};

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
    ObjString* name;
    ObjString* path;
    Table values;
} ObjNamespace;

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
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalueCount;
} ObjClosure;

typedef struct {
    Obj obj;
    ObjClass* behavior;
    ObjClosure* closure;
} ObjMethod;

typedef struct {
    Obj obj;
    Value receiver;
    ObjClosure* method;
} ObjBoundMethod;

struct ObjClass {
    Obj obj;
    ObjString* name;
    BehaviorType behavior;
    struct ObjClass* superclass;
    ValueArray traits;
    bool isNative;
    Table fields;
    Table methods;
};

typedef struct {
    Obj obj;
    Table fields;
} ObjInstance;

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
ObjNamespace* newNamespace(VM* vm, ObjString* name, ObjString* path);
ObjMethod* newMethod(VM* vm, ObjClass* behavior, ObjClosure* closure);
ObjNativeFunction* newNativeFunction(VM* vm, ObjString* name, int arity, NativeFunction function);
ObjNativeMethod* newNativeMethod(VM* vm, ObjClass* klass, ObjString* name, int arity, NativeMethod method);
ObjNode* newNode(VM* vm, Value element, ObjNode* prev, ObjNode* next);
ObjRange* newRange(VM* vm, int from, int to);
ObjRecord* newRecord(VM* vm, void* data);
ObjUpvalue* newUpvalue(VM* vm, Value* slot);

ObjClass* createClass(VM* vm, ObjString* name, ObjClass* metaclass, BehaviorType behavior);
ObjClass* createTrait(VM* vm, ObjString* name);
ObjClass* getObjClass(VM* vm, Value value);
bool isObjInstanceOf(VM* vm, Value value, ObjClass* klass);
bool isClassExtendingSuperclass(ObjClass* klass, ObjClass* superclass);
bool isClassImplementingTrait(ObjClass* trait, ObjClass* klass);
void inheritSuperclass(VM* vm, ObjClass* subclass, ObjClass* superclass);
void bindSuperclass(VM* vm, ObjClass* subclass, ObjClass* superclass);
void implementTraits(VM* vm, ObjClass* klass, ValueArray* traits);
void bindTrait(VM* vm, ObjClass* klass, ObjClass* trait);
void bindTraits(VM* vm, int numTraits, ObjClass* klass, ...);
Value getObjProperty(VM* vm, ObjInstance* object, char* name);
void setObjProperty(VM* vm, ObjInstance* object, char* name, Value value);
void copyObjProperty(VM* vm, ObjInstance* object, ObjInstance* object2, char* name);
void copyObjProperties(VM* vm, ObjInstance* fromObject, ObjInstance* toObject);
Value getClassProperty(VM* vm, ObjClass* klass, char* name);
void setClassProperty(VM* vm, ObjClass* klass, char* name, Value value);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && (AS_OBJ(value)->type == type);
}

#endif // !clox_object_h