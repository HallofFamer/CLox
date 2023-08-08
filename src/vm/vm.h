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

struct Module {
    char* filePath;
    char* source;
    Table values;
};

struct VM {
    ObjClass* objectClass;
    ObjClass* classClass;
    ObjClass* metaclassClass;
    ObjClass* nilClass;
    ObjClass* boolClass;
    ObjClass* numberClass;
    ObjClass* intClass;
    ObjClass* floatClass;
    ObjClass* stringClass;
    ObjClass* functionClass;
    ObjClass* methodClass;
    ObjClass* boundMethodClass;
    ObjClass* namespaceClass;
    ObjClass* traitClass;
    ObjClass* arrayClass;
    ObjClass* dictionaryClass;
    ObjClass* rangeClass;
    ObjClass* nodeClass;
    ObjClass* entryClass;
    ObjClass* fileClass;

    ObjNamespace* rootNamespace;
    ObjNamespace* cloxNamespace;
    ObjNamespace* stdNamespace;
    ObjNamespace* langNamespace;
    ObjNamespace* currentNamespace;

    CallFrame frames[FRAMES_MAX];
    int frameCount;
 
    Value stack[STACK_MAX];
    Value* stackTop;
    int apiStackDepth;

    Configuration config;
    Module* currentModule;
    Compiler* currentCompiler;
    ClassCompiler* currentClass;

    Table globals;
    Table namespaces;
    Table modules;
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
void initModule(VM* vm, Module* module, const char* filePath);
void freeModule(VM* vm, Module* module);
void push(VM* vm, Value value);
Value pop(VM* vm);
bool isFalsey(Value value);
bool callClosure(VM* vm, ObjClosure* closure, int argCount);
bool callMethod(VM* vm, Value method, int argCount);
Value callReentrant(VM* vm, Value receiver, Value callee, ...);
bool loadGlobal(VM* vm, ObjString* name, Value* value);
void runtimeError(VM* vm, const char* format, ...);
char* readFile(const char* path);
InterpretResult run(VM* vm);
InterpretResult interpret(VM* vm, const char* source);

#endif // !clox_vm_h
