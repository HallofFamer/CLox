#pragma once
#ifndef clox_vm_h
#define clox_vm_h

#include "common.h"
#include "compiler.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct {
    const char* version;
    const char* script;
    const char* path;
    const char* timezone;

    const char* gcType;
    size_t gcHeapSize;
    size_t gcGrowthFactor;
    bool gcStressMode;
} Configuration;

struct VM {
    ObjClass* objectClass;
    ObjClass* classClass;
    ObjClass* nilClass;
    ObjClass* boolClass;
    ObjClass* numberClass;
    ObjClass* intClass;
    ObjClass* floatClass;
    ObjClass* stringClass;
    ObjClass* functionClass;
    ObjClass* methodClass;
    ObjClass* listClass;
    ObjClass* dictionaryClass;
    ObjClass* fileClass;

    CallFrame frames[FRAMES_MAX];
    int frameCount;
 
    Value stack[STACK_MAX];
    Value* stackTop;

    Configuration config;
    Compiler* currentCompiler;
    ClassCompiler* currentClass;

    Table globals;
    Table strings;
    ObjString* initString;
    ObjUpvalue* openUpvalues;

    size_t bytesAllocated;
    size_t nextGC;
    Obj* objects;

    int grayCount;
    int grayCapacity;
    Obj** grayStack;
};

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM(VM* vm);
void freeVM(VM* vm);
bool isFalsey(Value value);
bool callClosure(VM* vm, ObjClosure* closure, int argCount);
void bindSuperclass(VM* vm, ObjClass* subclass, ObjClass* superclass);
void runtimeError(VM* vm, const char* format, ...);
char* readFile(const char* path);
ObjEntry* findEntryKey(ObjEntry* entries, int capacity, Value key);
bool getEntryValue(ObjDictionary* dict, Value key, Value* value);
InterpretResult interpret(VM* vm, const char* source);
void push(VM* vm, Value value);
Value pop(VM* vm);

#endif // !clox_vm_h
