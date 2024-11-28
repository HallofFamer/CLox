#pragma once
#ifndef clox_vm_h
#define clox_vm_h

#include "compiler_v1.h"
#include "exception.h"
#include "object.h"
#include "shape.h"
#include "table.h"
#include "value.h"
#include "../compiler/compiler.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

#define ABORT_IFNULL(pointer, message, ...) \
    do {\
        if (pointer == NULL) { \
            fprintf(stderr, message, ##__VA_ARGS__); \
            exit(74); \
        } \
    } while (false)

#define ABORT_IFTRUE(condition, message, ...) \
    do {\
        if (condition) { \
            fprintf(stderr, message, ##__VA_ARGS__); \
            exit(74); \
        } \
    } while (false)

struct CallFrame {
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots;
    uint8_t handlerCount;
    ExceptionHandler handlerStack[UINT4_MAX];
};

typedef struct {
    const char* version;
    const char* script;
    const char* path;
    const char* timezone;

    bool debugToken;
    bool debugAst;
    bool debugSymtab;
    bool debugCode;

    uint8_t flagUnusedImport;
    uint8_t flagUnusedVariable;
    uint8_t flagMutableVariable;

    const char* gcType;
    size_t gcHeapSize;
    size_t gcGrowthFactor;
    bool gcStressMode;
} Configuration;

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
    ObjClass* exceptionClass;
    ObjClass* arrayClass;
    ObjClass* dictionaryClass;
    ObjClass* rangeClass;
    ObjClass* nodeClass;
    ObjClass* entryClass;
    ObjClass* fileClass;
    ObjClass* generatorClass;
    ObjClass* promiseClass;
    ObjClass* timerClass;

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
    ObjGenerator* runningGenerator;
    uv_loop_t* eventLoop;

    Configuration config;
    CompilerV1* currentCompiler;
    ClassCompilerV1* currentClass;
    SymbolTable* symtab;
    int numSymtabs;
    Compiler* compiler;

    int behaviorCount;
    int namespaceCount;
    int moduleCount;
    int promiseCount;

    Table classes;
    Table namespaces;
    Table modules;
    Table strings;
    ShapeTree shapes;
    GenericIDMap genericIDMap;

    ObjString* initString;
    ObjModule* currentModule;
    ObjUpvalue* openUpvalues;

    size_t bytesAllocated;
    size_t nextGC;
    Obj* objects;
    uint64_t objectIndex;

    int grayCount;
    int grayCapacity;
    Obj** grayStack;
};

enum InterpretResult {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
};

static inline bool isFalsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

void initVM(VM* vm);
void freeVM(VM* vm);
void push(VM* vm, Value value);
Value pop(VM* vm);
Value peek(VM* vm, int distance);
bool callClosure(VM* vm, ObjClosure* closure, int argCount);
bool callMethod(VM* vm, Value method, int argCount);
Value callReentrantFunction(VM* vm, Value callee, ...);
Value callReentrantMethod(VM* vm, Value receiver, Value callee, ...);
Value callGenerator(VM* vm, ObjGenerator* generator);
void runtimeError(VM* vm, const char* format, ...);
char* readFile(const char* path);
bool bindMethod(VM* vm, ObjClass* klass, ObjString* name);
InterpretResult run(VM* vm);
InterpretResult interpret(VM* vm, const char* source);

#endif // !clox_vm_h
