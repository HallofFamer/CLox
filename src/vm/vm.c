#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "debug.h"
#include "dict.h"
#include "hash.h"
#include "memory.h"
#include "namespace.h"
#include "native.h"
#include "object.h"
#include "os.h"
#include "string.h"
#include "variable.h"
#include "vm.h"
#include "../inc/ini.h"
#include "../std/collection.h"
#include "../std/io.h"
#include "../std/lang.h"
#include "../std/net.h"
#include "../std/util.h"

static void resetCallFrame(VM* vm, int index) {
    CallFrame* frame = &vm->frames[index];
    frame->closure = NULL;
    frame->ip = NULL;
    frame->slots = NULL;
    frame->handlerCount = 0;
}

static void resetCallFrames(VM* vm) {
    for (int i = 0; i < FRAMES_MAX; i++) {
        resetCallFrame(vm, i);
    }
}

static void resetStack(VM* vm) {
    vm->stackTop = vm->stack;
    vm->frameCount = 0;
    vm->apiStackDepth = 0;
    vm->runningGenerator = NULL;
    vm->openUpvalues = NULL;
    resetCallFrames(vm);
}

void runtimeError(VM* vm, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm->frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm->frames[i];
        ObjFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } 
        else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }
    resetStack(vm);
}

char* readFile(const char* path) {
    FILE* file;
    fopen_s(&file, path, "rb");
    ABORT_IFNULL(file, "Could not open file \"%s\".\n", path);

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    ABORT_IFNULL(buffer, "Not enough memory to read \"%s\".\n", path);

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    ABORT_IFTRUE(bytesRead < fileSize, "Could not read file \"%s\".\n", path);

    buffer[bytesRead] = '\0';
    fclose(file);
    return buffer;
}

static int parseConfiguration(void* data, const char* section, const char* name, const char* value) {
    Configuration* config = (Configuration*)data;
#define HAS_CONFIG(s, n) strcmp(section, (s)) == 0 && strcmp(name, (n)) == 0
    if (HAS_CONFIG("basic", "version")) {
        config->version = _strdup(value);
    }
    else if (HAS_CONFIG("basic", "script")) {
        config->script = _strdup(value);
    }
    else if (HAS_CONFIG("basic", "path")) {
        config->path = _strdup(value);
    }
    else if (HAS_CONFIG("basic", "timezone")) {
        config->timezone = _strdup(value);
    }
    else if (HAS_CONFIG("gc", "gcType")) {
        config->gcType = _strdup(value);
    }
    else if (HAS_CONFIG("gc", "gcHeapSize")) {
        config->gcHeapSize = (size_t)atol(value);
    }
    else if (HAS_CONFIG("gc", "gcGrowthFactor")) {       
        config->gcGrowthFactor = (size_t)atol(value);
    }
    else if (HAS_CONFIG("gc", "gcStressMode")) {
        config->gcStressMode = (bool)atoi(value);
    }
    else {
        return 0;
    }
    return 1;
#undef HAS_CONFIG
}

static void initConfiguration(VM* vm) {
    Configuration config;
    int iniParsed = ini_parse("clox.ini", parseConfiguration, &config);
    ABORT_IFTRUE(iniParsed < 0, "Can't load 'clox.ini' configuration file...\n");
    vm->config = config;
}

void initVM(VM* vm) {
    resetStack(vm);
    initConfiguration(vm);
    vm->currentModule = NULL;
    vm->currentCompiler = NULL;
    vm->currentClass = NULL;
    vm->objects = NULL;
    vm->objectIndex = 0;
    vm->bytesAllocated = 0;
    vm->nextGC = vm->config.gcHeapSize;

    vm->grayCount = 0;
    vm->grayCapacity = 0;
    vm->grayStack = NULL;

    vm->behaviorCount = 0;
    vm->namespaceCount = 0;
    vm->moduleCount = 1;
    vm->promiseCount = 0;

    initTable(&vm->classes);
    initTable(&vm->namespaces);
    initTable(&vm->modules);
    initTable(&vm->strings);
    initShapeTree(vm);
    initGenericIDMap(vm);
    initLoop(vm);
    vm->initString = NULL;
    vm->initString = copyString(vm, "__init__", 8);
    vm->runningGenerator = NULL;

    registerLangPackage(vm);
    registerCollectionPackage(vm);
    registerIOPackage(vm);
    registerNetPackage(vm);
    registerUtilPackage(vm);
    registerNativeFunctions(vm);
}

void freeVM(VM* vm) {
    freeTable(vm, &vm->namespaces);
    freeTable(vm, &vm->modules);
    freeTable(vm, &vm->classes);
    freeTable(vm, &vm->strings);
    freeShapeTree(vm, &vm->shapes);
    freeGenericIDMap(vm, &vm->genericIDMap);
    vm->initString = NULL;
    freeObjects(vm);
    freeLoop(vm);
}

void push(VM* vm, Value value) {
    *vm->stackTop = value;
    vm->stackTop++;
}

static void pushes(VM* vm, int count, ...) {
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++) {
        Value value = va_arg(args, Value);
        push(vm, value);
    }
    va_end(args);
}

Value pop(VM* vm) {
    vm->stackTop--;
    return *vm->stackTop;
}

static void pops(VM* vm, int count) {
    for (int i = 0; i < count; i++) {
        pop(vm);
    }
}

Value peek(VM* vm, int distance) {
    return vm->stackTop[-1 - distance];
}

static void concatenate(VM* vm) {
    ObjString* b = AS_STRING(peek(vm, 0));
    ObjString* a = AS_STRING(peek(vm, 1));

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = takeString(vm, chars, length);
    pop(vm);
    pop(vm);
    push(vm, OBJ_VAL(result));
}

static void makeArray(VM* vm, uint8_t elementCount) {
    ObjArray* array = newArray(vm);
    push(vm, OBJ_VAL(array));
    for (int i = elementCount; i > 0; i--) {
        valueArrayWrite(vm, &array->elements, peek(vm, i));
    }
    pop(vm);

    while (elementCount > 0) {
        elementCount--;
        pop(vm);
    }
    push(vm, OBJ_VAL(array));
}

static void makeDictionary(VM* vm, uint8_t entryCount) {
    ObjDictionary* dictionary = newDictionary(vm);
    push(vm, OBJ_VAL(dictionary));

    for (int i = 1; i <= entryCount; i++) {
        Value key = peek(vm, 2 * i);
        Value value = peek(vm, 2 * i - 1);
        dictSet(vm, dictionary, key, value);
    }
    pop(vm);

    while (entryCount > 0) {
        entryCount--;
        pop(vm);
        pop(vm);
    }
    push(vm, OBJ_VAL(dictionary));
}

static ObjArray* makeTraitArray(VM* vm, uint8_t behaviorCount) {
    ObjArray* traits = newArray(vm);
    push(vm, OBJ_VAL(traits));
    for (int i = 0; i < behaviorCount; i++) {
        Value trait = peek(vm, i + 1);
        if (!IS_CLASS(trait) || AS_CLASS(trait)->behaviorType != BEHAVIOR_TRAIT) {
            return NULL;
        }
        valueArrayWrite(vm, &traits->elements, trait);
    }
    pop(vm);

    while (behaviorCount > 0) {
        behaviorCount--;
        pop(vm);
    }
    push(vm, OBJ_VAL(traits));
    return traits;
}

static Value createObject(VM* vm, ObjClass* klass, int argCount) {
    switch (klass->classType) {
        case OBJ_ARRAY: return OBJ_VAL(newArray(vm));
        case OBJ_BOUND_METHOD: return OBJ_VAL(newBoundMethod(vm, NIL_VAL, NIL_VAL));
        case OBJ_CLASS: return OBJ_VAL(ALLOCATE_CLASS(klass));
        case OBJ_CLOSURE: return OBJ_VAL(ALLOCATE_CLOSURE(klass));
        case OBJ_DICTIONARY: return OBJ_VAL(newDictionary(vm));
        case OBJ_ENTRY: return OBJ_VAL(newEntry(vm, NIL_VAL, NIL_VAL));
        case OBJ_EXCEPTION: return OBJ_VAL(newException(vm, emptyString(vm), klass));
        case OBJ_FILE: return OBJ_VAL(newFile(vm, NULL));
        case OBJ_GENERATOR: return OBJ_VAL(newGenerator(vm, NULL, NULL));
        case OBJ_INSTANCE: return OBJ_VAL(newInstance(vm, klass));
        case OBJ_METHOD: return OBJ_VAL(newMethod(vm, NULL, NULL));
        case OBJ_NAMESPACE: return OBJ_VAL(ALLOCATE_NAMESPACE(klass));
        case OBJ_NODE: return OBJ_VAL(newNode(vm, NIL_VAL, NULL, NULL));
        case OBJ_PROMISE: return OBJ_VAL(newPromise(vm, PROMISE_PENDING, NIL_VAL, NIL_VAL));
        case OBJ_RANGE: return OBJ_VAL(newRange(vm, 0, 1));
        case OBJ_RECORD: return OBJ_VAL(newRecord(vm, NULL));
        case OBJ_STRING: return OBJ_VAL(ALLOCATE_STRING(0, klass));
        case OBJ_TIMER: return OBJ_VAL(newTimer(vm, NULL, 0, 0));
        case OBJ_VALUE_INSTANCE: return OBJ_VAL(newValueInstance(vm, NIL_VAL, klass));
        default: return NIL_VAL;
    }
}

static void createCallFrame(VM* vm, ObjClosure* closure, int argCount) { 
    CallFrame* frame = &vm->frames[vm->frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm->stackTop - argCount - 1;
}

static void createGeneratorFrame(VM* vm, ObjClosure* closure, int argCount) {
    CallFrame frame = {
        .closure = closure,
        .ip = closure->function->chunk.code,
        .slots = vm->stackTop - argCount - 1
    };
    ObjGenerator* generator = newGenerator(vm, newFrame(vm, &frame), vm->runningGenerator);
    vm->stackTop -= (size_t)argCount + 1;
    push(vm, OBJ_VAL(generator));
}

static bool callClosureAsync(VM* vm, ObjClosure* closure, int argCount) {
    Value run = getObjMethod(vm, OBJ_VAL(vm->generatorClass), "run");
    makeArray(vm, argCount);
    Value arguments = pop(vm);
    pop(vm);

    push(vm, OBJ_VAL(vm->generatorClass));
    push(vm, OBJ_VAL(closure));
    push(vm, arguments);
    return callMethod(vm, run, 2);
}

bool callClosure(VM* vm, ObjClosure* closure, int argCount) {
    if (closure->function->arity > 0 && argCount != closure->function->arity) {
        runtimeError(vm, "Expected %d arguments but got %d.", closure->function->arity, argCount);
        return false;
    }

    if (vm->frameCount == FRAMES_MAX) {
        runtimeError(vm, "Stack overflow.");
        return false;
    }

    if (closure->function->arity == -1) {
        makeArray(vm, argCount);
        argCount = 1;
    }

    if (closure->function->isAsync) return callClosureAsync(vm, closure, argCount);
    if (closure->function->isGenerator) createGeneratorFrame(vm, closure, argCount);
    else createCallFrame(vm, closure, argCount);
    return true;
}

static bool callNativeFunction(VM* vm, NativeFunction function, int argCount) {
    Value result = function(vm, argCount, vm->stackTop - argCount);
    vm->stackTop -= (size_t)argCount + 1;
    push(vm, result);
    return true;
}

static bool callNativeMethod(VM* vm, NativeMethod method, int argCount) {
    Value result = method(vm, vm->stackTop[-argCount - 1], argCount, vm->stackTop - argCount);
    vm->stackTop -= (size_t)argCount + 1;
    push(vm, result);
    return true;
}

bool callMethod(VM* vm, Value method, int argCount) {
    if (IS_NATIVE_METHOD(method)) {
        return callNativeMethod(vm, AS_NATIVE_METHOD(method)->method, argCount);
    }
    else return callClosure(vm, AS_CLOSURE(method), argCount);
}

static bool callBoundMethod(VM* vm, ObjBoundMethod* boundMethod, int argCount) {
    vm->stackTop[-argCount - 1] = boundMethod->receiver;
    return callMethod(vm, boundMethod->method, argCount);
}

static bool callClass(VM* vm, ObjClass* klass, int argCount) {
    vm->stackTop[-argCount - 1] = createObject(vm, klass, argCount);
    Value initializer;
    if (tableGet(&klass->methods, vm->initString, &initializer)) {
        return callMethod(vm, initializer, argCount);
    }
    else if (argCount != 0) {
        runtimeError(vm, "Expected 0 argument but got %d.", argCount);
        return false;
    }
    return true;
}

static int getCalleeArity(Value callee) {
    if (IS_CLOSURE(callee)) return AS_CLOSURE(callee)->function->arity;
    else if (IS_NATIVE_METHOD(callee)) return AS_NATIVE_METHOD(callee)->arity;
    else if (IS_NATIVE_FUNCTION(callee)) return AS_NATIVE_FUNCTION(callee)->arity;
    else if (IS_BOUND_METHOD(callee)) return getCalleeArity(AS_BOUND_METHOD(callee)->method);
    else return 0;
}

static void callReentrantClosure(VM* vm, Value callee, int argCount) {
    vm->apiStackDepth++;
    callClosure(vm, AS_CLOSURE(callee), argCount);
    InterpretResult result = run(vm);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
    vm->apiStackDepth--;
}

Value callReentrantFunction(VM* vm, Value callee, ...) {
    int argCount = getCalleeArity(callee);
    va_list args;
    va_start(args, callee);
    for (int i = 0; i < argCount; i++) {
        push(vm, va_arg(args, Value));
    }
    va_end(args);

    if (IS_CLOSURE(callee)) callReentrantClosure(vm, callee, argCount);
    else callNativeFunction(vm, AS_NATIVE_FUNCTION(callee)->function, argCount);
    return pop(vm);
}

Value callReentrantMethod(VM* vm, Value receiver, Value callee, ...) {
    push(vm, receiver);
    int argCount = getCalleeArity(callee);
    va_list args;
    va_start(args, callee);
    for (int i = 0; i < argCount; i++) {
        push(vm, va_arg(args, Value));
    }
    va_end(args);

    if (IS_CLOSURE(callee)) callReentrantClosure(vm, callee, argCount);
    else if (IS_BOUND_METHOD(callee)) callBoundMethod(vm, AS_BOUND_METHOD(callee), argCount);
    else callNativeMethod(vm, AS_NATIVE_METHOD(callee)->method, argCount);
    return pop(vm);
}

Value callGenerator(VM* vm, ObjGenerator* generator) {
    ObjGenerator* outer = vm->runningGenerator;
    vm->runningGenerator = generator;
    loadGeneratorFrame(vm, generator);
    InterpretResult result = run(vm);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
    vm->runningGenerator = outer;
    return pop(vm);
}

static bool callValue(VM* vm, Value callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_BOUND_METHOD: return callBoundMethod(vm, AS_BOUND_METHOD(callee), argCount);
            case OBJ_CLASS: return callClass(vm, AS_CLASS(callee), argCount);
            case OBJ_CLOSURE: return callClosure(vm, AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE_FUNCTION: return callNativeFunction(vm, AS_NATIVE_FUNCTION(callee)->function, argCount);
            case OBJ_NATIVE_METHOD: return callNativeMethod(vm, AS_NATIVE_METHOD(callee)->method, argCount);
            default: break;
        }
    }

    ObjClass* klass = getObjClass(vm, callee);
    ObjString* name = copyString(vm, "()", 2);
    Value method;
    if (!tableGet(&klass->methods, name, &method)) { 
        throwNativeException(vm, "clox.std.lang.MethodNotFoundException", "Undefined operator method '%s' on class %s.", name->chars, klass->fullName->chars);
        return false;
    }
    return callMethod(vm, method, argCount);
}

static bool invokeFromClass(VM* vm, ObjClass* klass, ObjString* name, int argCount) {
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        if (interceptUndefinedInvoke(vm, klass, name, argCount)) return true;
        else { 
            if (klass != vm->nilClass) runtimeError(vm, "Undefined method '%s' on class %s.", name->chars, klass->fullName->chars);
            return false;
        }
    }
    return callMethod(vm, method, argCount);
}

static bool invoke(VM* vm, ObjString* name, int argCount) {
    Value receiver = peek(vm, argCount);
    if (!IS_OBJ(receiver)) {
        return invokeFromClass(vm, getObjClass(vm, receiver), name, argCount);
    }

    if (IS_INSTANCE(receiver)) {
        ObjInstance* instance = AS_INSTANCE(receiver);
        IDMap* idMap = getShapeIndexes(vm, instance->obj.shapeID);
        int index;
        if (idMapGet(idMap, name, &index)) {
            Value value = instance->fields.values[index];
            vm->stackTop[-argCount - 1] = value;
            return callValue(vm, value, argCount);
        }
    }
    else if (IS_NAMESPACE(receiver)) { 
        ObjNamespace* namespace = AS_NAMESPACE(receiver);
        Value value;
        if (tableGet(&namespace->values, name, &value)) { 
            return callValue(vm, value, argCount);
        }
    }
    return invokeFromClass(vm, getObjClass(vm, receiver), name, argCount);
}

static bool invokeOperator(VM* vm, ObjString* op, int arity) {
    Value receiver = peek(vm, arity);
    ObjClass* klass = getObjClass(vm, receiver);
    Value method;

    if (!tableGet(&klass->methods, op, &method)) {
        throwNativeException(vm, "clox.std.lang.MethodNotFoundException", "Undefined operator method '%s' on class %s.", op->chars, klass->fullName->chars);
        return false;
    }
    return invoke(vm, op, arity);
}

static bool hasMethod(VM* vm, ObjClass* klass, ObjString* name) { 
    if (name == NULL) return false;
    Value method;
    return tableGet(&klass->methods, name, &method);
}

bool bindMethod(VM* vm, ObjClass* klass, ObjString* name) {
    Value method;
    if (!tableGet(&klass->methods, name, &method)) return false;
    ObjBoundMethod* bound = newBoundMethod(vm, peek(vm, 0), method);
    pop(vm);
    push(vm, OBJ_VAL(bound));
    return true;
}

static void defineMethod(VM* vm, ObjString* name, bool isClassMethod) {
    Value method = peek(vm, 0);
    ObjClass* klass = AS_CLASS(peek(vm, 1));

    if (isClassMethod) {
        if (klass->behaviorType != BEHAVIOR_CLASS) {
            runtimeError(vm, "Class method '%s' can only be defined in class body.", name->chars);
        }
        klass = klass->obj.klass;
    }

    tableSet(vm, &klass->methods, name, method);
    handleInterceptorMethod(vm, klass, name);
    pop(vm);
}

static ObjUpvalue* captureUpvalue(VM* vm, Value* local) {
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = vm->openUpvalues;
    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }
    ObjUpvalue* createdUpvalue = newUpvalue(vm, local);
    createdUpvalue->next = upvalue;

    if (prevUpvalue == NULL) {
        vm->openUpvalues = createdUpvalue;
    } 
    else {
        prevUpvalue->next = createdUpvalue;
    }
    return createdUpvalue;
}

static void closeUpvalues(VM* vm, Value* last) {
    while (vm->openUpvalues != NULL && vm->openUpvalues->location >= last) {
        ObjUpvalue* upvalue = vm->openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm->openUpvalues = upvalue->next;
    }
}

InterpretResult run(VM* vm) {
    CallFrame* frame = &vm->frames[vm->frameCount - 1];

#define LOAD_FRAME() (frame = &vm->frames[vm->frameCount - 1])
#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_IDENTIFIER() (frame->closure->function->chunk.identifiers.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_IDENTIFIER())

#define BINARY_INT_OP(valueType, op) \
    do { \
        int b = AS_INT(pop(vm)); \
        int a = AS_INT(pop(vm)); \
        push(vm, INT_VAL(a op b)); \
    } while (false)

#define BINARY_NUMBER_OP(valueType, op) \
    do { \
        double b = AS_NUMBER(pop(vm)); \
        double a = AS_NUMBER(pop(vm)); \
        push(vm, valueType(a op b)); \
    } while (false)


#define CAN_INTERCEPT(receiver, interceptorType, interceptorName) \
    HAS_OBJ_INTERCEPTOR(receiver, interceptorType) && !matchVariableName(frame->closure->function->name, #interceptorName, (int)strlen(#interceptorName))

#define OVERLOAD_OP(op, arity) \
    do { \
        ObjString* opName = newString(vm, #op); \
        if (!invokeOperator(vm, opName, arity)) { \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        LOAD_FRAME(); \
    } while (false)

#define RUNTIME_ERROR(...) \
    do { \
        runtimeError(vm, ##__VA_ARGS__); \
        return INTERPRET_RUNTIME_ERROR; \
    }  while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(vm, constant);
                break;
            }
            case OP_NIL: push(vm, NIL_VAL); break;
            case OP_TRUE: push(vm, BOOL_VAL(true)); break;
            case OP_FALSE: push(vm, BOOL_VAL(false)); break;
            case OP_POP: pop(vm); break;
            case OP_DUP: push(vm, peek(vm, 0)); break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(vm, frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(vm, 0);
                break;
            }
            case OP_DEFINE_GLOBAL_VAL: {
                ObjString* name = READ_STRING();
                Value value = peek(vm, 0);
                int index;
                if (idMapGet(&vm->currentModule->valIndexes, name, &index)) {
                    vm->currentModule->valFields.values[index] = value;
                }
                else {
                    idMapSet(vm, &vm->currentModule->valIndexes, name, vm->currentModule->valFields.count);
                    valueArrayWrite(vm, &vm->currentModule->valFields, value);
                }
                pop(vm);
                break;
            }
            case OP_DEFINE_GLOBAL_VAR: {
                ObjString* name = READ_STRING();
                Value value = peek(vm, 0);
                int index;
                if (idMapGet(&vm->currentModule->varIndexes, name, &index)) {
                    vm->currentModule->varFields.values[index] = value;
                }
                else {
                    idMapSet(vm, &vm->currentModule->varIndexes, name, vm->currentModule->varFields.count);
                    valueArrayWrite(vm, &vm->currentModule->varFields, value);
                }
                pop(vm);
                break;
            }
            case OP_GET_GLOBAL: {
                uint8_t byte = READ_BYTE();
                Value value;
                if (!loadGlobal(vm, &frame->closure->function->chunk, byte, &value)) {
                    ObjString* name = AS_STRING(frame->closure->function->chunk.identifiers.values[byte]);
                    RUNTIME_ERROR("Undefined variable '%s'.", name->chars);
                }
                push(vm, value);
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value = peek(vm, 0);
                int index;
                if (idMapGet(&vm->currentModule->varIndexes, name, &index)) vm->currentModule->varFields.values[index] = value;
                else RUNTIME_ERROR("Undefined variable '%s'.", name->chars);
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(vm, *frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(vm, 0);
                break;
            }
            case OP_GET_PROPERTY: {
                Value receiver = peek(vm, 0);
                uint8_t byte = READ_BYTE();

                if (CAN_INTERCEPT(receiver, INTERCEPTOR_BEFORE_GET, __beforeGet__) && hasInstanceVariable(vm, AS_OBJ(receiver), &frame->closure->function->chunk, byte)) {
                    ObjString* name = AS_STRING(frame->closure->function->chunk.identifiers.values[byte]);
                    interceptBeforeGet(vm, receiver, name);
                    LOAD_FRAME();
                }

                if (!getInstanceVariable(vm, receiver, &frame->closure->function->chunk, byte)) {
                    ObjString* name = AS_STRING(frame->closure->function->chunk.identifiers.values[byte]);
                    if (interceptUndefinedGet(vm, receiver, name)) LOAD_FRAME();
                    else RUNTIME_ERROR("Undefined property '%s'", name->chars);
                }
                else if (CAN_INTERCEPT(receiver, INTERCEPTOR_AFTER_GET, __afterGet__)) {
                    ObjString* name = AS_STRING(frame->closure->function->chunk.identifiers.values[byte]);
                    Value value = pop(vm);
                    interceptAfterGet(vm, receiver, name, value);
                    LOAD_FRAME();
                }
                break;
            }
            case OP_SET_PROPERTY: {
                Value value = pop(vm);
                Value receiver = pop(vm);
                uint8_t byte = READ_BYTE();

                if (CAN_INTERCEPT(receiver, INTERCEPTOR_BEFORE_SET, __beforeSet__) && hasInstanceVariable(vm, AS_OBJ(receiver), &frame->closure->function->chunk, byte)) {
                    ObjString* name = AS_STRING(frame->closure->function->chunk.identifiers.values[byte]);
                    interceptBeforeSet(vm, receiver, name, value);
                    value = pop(vm);
                    LOAD_FRAME();
                }

                if (!setInstanceVariable(vm, receiver, &frame->closure->function->chunk, byte, value)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                else if (CAN_INTERCEPT(receiver, INTERCEPTOR_AFTER_GET, __afterSet__)) {
                    ObjString* name = AS_STRING(frame->closure->function->chunk.identifiers.values[byte]);
                    interceptAfterSet(vm, receiver, name);
                    LOAD_FRAME();
                }
                break;
            }
            case OP_GET_PROPERTY_OPTIONAL: {
                Value receiver = peek(vm, 0);
                uint8_t byte = READ_BYTE();

                if (CAN_INTERCEPT(receiver, INTERCEPTOR_BEFORE_GET, __beforeGet__) && hasInstanceVariable(vm, AS_OBJ(receiver), &frame->closure->function->chunk, byte)) {
                    ObjString* name = AS_STRING(frame->closure->function->chunk.identifiers.values[byte]);
                    interceptBeforeGet(vm, receiver, name);
                    LOAD_FRAME();
                }

                if (IS_NIL(receiver)) {
                    pop(vm);
                    push(vm, NIL_VAL);
                }
                else if (!getInstanceVariable(vm, receiver, &frame->closure->function->chunk, byte)) {
                    ObjString* name = AS_STRING(frame->closure->function->chunk.identifiers.values[byte]);
                    if (interceptUndefinedGet(vm, receiver, name)) LOAD_FRAME();
                    else return INTERPRET_RUNTIME_ERROR;
                }
                else if (CAN_INTERCEPT(receiver, INTERCEPTOR_AFTER_GET, __afterGet__)) {
                    ObjString* name = AS_STRING(frame->closure->function->chunk.identifiers.values[byte]);
                    Value value = pop(vm);
                    interceptAfterGet(vm, receiver, name, value);
                    LOAD_FRAME();
                }
                break;
            }
            case OP_GET_SUBSCRIPT: {
                if (IS_INT(peek(vm, 0))) {
                    int index = AS_INT(peek(vm, 0));
                    if (IS_STRING(peek(vm, 0))) {
                        pop(vm);
                        ObjString* string = AS_STRING(pop(vm));
                        if (index < 0 || index > string->length) {
                            throwNativeException(vm, "clox.std.lang.IndexOutOfBoundsException", "String index is out of bound: %d.", index);
                        }
                        else {
                            char chars[2] = { string->chars[index], '\0' };
                            ObjString* element = copyString(vm, chars, 1);
                            push(vm, OBJ_VAL(element));
                        }
                    }
                    else if (IS_ARRAY(peek(vm, 0))) {
                        pop(vm);
                        ObjArray* array = AS_ARRAY(pop(vm));
                        if (index < 0 || index > array->elements.count) {
                            throwNativeException(vm, "clox.std.lang.IndexOutOfBoundsException", "Array index is out of bound: %d.", index);
                        }
                        else {
                            Value element = array->elements.values[index];
                            push(vm, element);
                        }
                    }
                    else OVERLOAD_OP([], 1);
                }
                else if (IS_DICTIONARY(peek(vm, 1))) {
                    Value key = pop(vm);
                    ObjDictionary* dictionary = AS_DICTIONARY(pop(vm));
                    Value value;
                    if (dictGet(dictionary, key, &value)) push(vm, value);
                    else push(vm, NIL_VAL);
                }
                else OVERLOAD_OP([], 1);
                break;
            }
            case OP_SET_SUBSCRIPT: {
                if (IS_INT(peek(vm, 1)) && IS_ARRAY(peek(vm, 2))) {
                    Value element = pop(vm);
                    int index = AS_INT(pop(vm));
                    ObjArray* array = AS_ARRAY(pop(vm));
                    valueArrayPut(vm, &array->elements, index, element);
                    push(vm, OBJ_VAL(array));
                }
                else if (IS_DICTIONARY(peek(vm, 2))) {
                    Value value = pop(vm);
                    Value key = pop(vm);
                    ObjDictionary* dictionary = AS_DICTIONARY(pop(vm));
                    dictSet(vm, dictionary, key, value);
                    push(vm, OBJ_VAL(dictionary));
                }
                else OVERLOAD_OP([]=, 2);
                break;
            }
            case OP_GET_SUBSCRIPT_OPTIONAL: {
                if (IS_NIL(peek(vm, 1))) {
                    pops(vm, 2);
                    push(vm, NIL_VAL);
                }
                else if (IS_INT(peek(vm, 0))) {
                    int index = AS_INT(peek(vm, 0));
                    if (IS_STRING(peek(vm, 0))) {
                        pop(vm);
                        ObjString* string = AS_STRING(pop(vm));
                        if (index < 0 || index > string->length) {
                            throwNativeException(vm, "clox.std.lang.IndexOutOfBoundsException", "String index is out of bound: %d.", index);
                        }
                        else {
                            char chars[2] = { string->chars[index], '\0' };
                            ObjString* element = copyString(vm, chars, 1);
                            push(vm, OBJ_VAL(element));
                        }
                    }
                    else if (IS_ARRAY(peek(vm, 0))) {
                        pop(vm);
                        ObjArray* array = AS_ARRAY(pop(vm));
                        if (index < 0 || index > array->elements.count) {
                            throwNativeException(vm, "clox.std.lang.IndexOutOfBoundsException", "Array index is out of bound: %d.", index);
                        }
                        else {
                            Value element = array->elements.values[index];
                            push(vm, element);
                        }
                    }
                    else OVERLOAD_OP([], 1);
                }
                else if (IS_DICTIONARY(peek(vm, 1))) {
                    Value key = pop(vm);
                    ObjDictionary* dictionary = AS_DICTIONARY(pop(vm));
                    Value value;
                    if (dictGet(dictionary, key, &value)) push(vm, value);
                    else push(vm, NIL_VAL);
                }
                else OVERLOAD_OP([], 1);
                break;
            }
            case OP_GET_SUPER: {
                ObjString* name = READ_STRING();
                ObjClass* klass = AS_CLASS(pop(vm));

                if (!bindMethod(vm, klass->superclass, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_EQUAL: {
                if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) BINARY_NUMBER_OP(BOOL_VAL, == );
                else {
                    ObjString* op = copyString(vm, "==", 2);
                    if (!invokeOperator(vm, op, 1)) {
                        Value b = pop(vm);
                        Value a = pop(vm);
                        push(vm, BOOL_VAL(a == b));
                    }
                    else LOAD_FRAME();
                }
                break;
            }
            case OP_GREATER:
                if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) BINARY_NUMBER_OP(BOOL_VAL, > );
                else OVERLOAD_OP(> , 1);
                break;
            case OP_LESS:
                if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) BINARY_NUMBER_OP(BOOL_VAL, < );
                else OVERLOAD_OP(< , 1);
                break;
            case OP_ADD: {
                if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
                    concatenate(vm);
                }
                else if (IS_INT(peek(vm, 0)) && IS_INT(peek(vm, 1))) BINARY_INT_OP(INT_VAL, +);
                else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) BINARY_NUMBER_OP(NUMBER_VAL, +);
                else OVERLOAD_OP(+, 1);
                break;
            }
            case OP_SUBTRACT: {
                if (IS_INT(peek(vm, 0)) && IS_INT(peek(vm, 1))) BINARY_INT_OP(INT_VAL, -);
                else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) BINARY_NUMBER_OP(NUMBER_VAL, -);
                else OVERLOAD_OP(-, 1);
                break;
            }
            case OP_MULTIPLY: {
                if (IS_INT(peek(vm, 0)) && IS_INT(peek(vm, 1))) BINARY_INT_OP(INT_VAL, *);
                else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) BINARY_NUMBER_OP(NUMBER_VAL, *);
                else OVERLOAD_OP(*, 1);
                break;
            }
            case OP_DIVIDE:
                if (IS_INT(peek(vm, 0)) && AS_INT(peek(vm, 0)) == 0) {
                    throwNativeException(vm, "clox.std.lang.ArithmeticException", "It is illegal to divide an integer by 0.");
                }
                else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) BINARY_NUMBER_OP(NUMBER_VAL, / );
                else OVERLOAD_OP(/ , 1);
                break;
            case OP_MODULO: {
                if (IS_INT(peek(vm, 0)) && IS_INT(peek(vm, 1))) BINARY_INT_OP(INT_VAL, %);
                else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) {
                    double b = AS_NUMBER(pop(vm));
                    double a = AS_NUMBER(pop(vm));
                    push(vm, NUMBER_VAL(fmod(a, b)));
                }
                else OVERLOAD_OP(%, 1);
                break;
            }
            case OP_NIL_COALESCING: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, IS_NIL(a) ? b : a);
                break;
            }
            case OP_ELVIS: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, isFalsey(a) ? b : a);
                break;
            }
            case OP_NOT:
                push(vm, BOOL_VAL(isFalsey(pop(vm))));
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(vm, 0))) {
                    throwNativeException(vm, "clox.std.lang.IllegalArgumentException", "Operands must be numbers for negate operator.");
                }
                else if (IS_INT(peek(vm, 0))) push(vm, INT_VAL(-AS_INT(pop(vm))));
                else push(vm, NUMBER_VAL(-AS_NUMBER(pop(vm))));
                break;
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(vm, 0))) frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_EMPTY: {
                uint16_t offset = READ_SHORT();
                if (IS_NIL(peek(vm, 0)) || IS_UNDEFINED(peek(vm, 0))) frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                uint8_t argCount = READ_BYTE();
                if (!callValue(vm, peek(vm, argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                LOAD_FRAME();
                break;
            }
            case OP_OPTIONAL_CALL: {
                uint8_t argCount = READ_BYTE();
                Value callee = peek(vm, argCount);
                if (IS_NIL(callee)) {
                    vm->stackTop -= (size_t)argCount + 1;
                    push(vm, NIL_VAL);
                }
                else if (!callValue(vm, peek(vm, argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                LOAD_FRAME();
                break;
            }
            case OP_INVOKE: {
                ObjString* method = READ_STRING();
                uint8_t argCount = READ_BYTE();
                Value receiver = peek(vm, argCount);

                if (CAN_INTERCEPT(receiver, INTERCEPTOR_ON_INVOKE, __onInvoke__) && hasMethod(vm, getObjClass(vm, receiver), method)) {
                    interceptOnInvoke(vm, receiver, method, argCount);
                    LOAD_FRAME();
                }

                if (!invoke(vm, method, argCount)) {
                    if (IS_NIL(receiver)) runtimeError(vm, "Calling undefined method '%s' on nil.", method->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                LOAD_FRAME();
                break;
            }
            case OP_SUPER_INVOKE: {
                ObjString* method = READ_STRING();
                uint8_t argCount = READ_BYTE();
                ObjClass* klass = AS_CLASS(pop(vm));

                if (!invokeFromClass(vm, klass, method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                LOAD_FRAME();
                break;
            }
            case OP_OPTIONAL_INVOKE: {
                ObjString* method = READ_STRING();
                uint8_t argCount = READ_BYTE();
                Value receiver = peek(vm, argCount);

                if (CAN_INTERCEPT(receiver, INTERCEPTOR_ON_INVOKE, __onInvoke__) && hasMethod(vm, getObjClass(vm, receiver), method)) {
                    interceptOnInvoke(vm, receiver, method, argCount);
                    LOAD_FRAME();
                }

                if (!invoke(vm, method, argCount)) {
                    if (IS_NIL(receiver)) {
                        vm->stackTop -= (size_t)argCount + 1;
                        push(vm, NIL_VAL);
                    }
                    else RUNTIME_ERROR("Undefined method '%s'.", method->chars);
                }
                LOAD_FRAME();
                break;
            }
            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_IDENTIFIER());
                ObjClosure* closure = newClosure(vm, function);
                push(vm, OBJ_VAL(closure));
                for (int i = 0; i < closure->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (isLocal) {
                        closure->upvalues[i] = captureUpvalue(vm, frame->slots + index);
                    }
                    else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OP_CLOSE_UPVALUE:
                closeUpvalues(vm, vm->stackTop - 1);
                pop(vm);
                break;
            case OP_CLASS: {
                ObjString* className = READ_STRING();
                push(vm, OBJ_VAL(newClass(vm, className, OBJ_INSTANCE)));
                tableSet(vm, &vm->currentNamespace->values, className, peek(vm, 0));
                break;
            }
            case OP_TRAIT: {
                ObjString* traitName = READ_STRING();
                push(vm, OBJ_VAL(createTrait(vm, traitName)));
                tableSet(vm, &vm->currentNamespace->values, traitName, peek(vm, 0));
                break;
            }
            case OP_ANONYMOUS: {
                uint8_t behaviorType = READ_BYTE();
                if (behaviorType == BEHAVIOR_TRAIT) {
                    push(vm, OBJ_VAL(createTrait(vm, NULL)));
                }
                else {
                    push(vm, OBJ_VAL(createClass(vm, NULL, vm->objectClass->obj.klass, behaviorType)));
                }
                break;
            }
            case OP_INHERIT: {
                ObjClass* klass = AS_CLASS(peek(vm, 1));
                if (klass->behaviorType == BEHAVIOR_CLASS) {
                    Value superclass = peek(vm, 0);
                    if (!IS_CLASS(superclass) || AS_CLASS(superclass)->behaviorType != BEHAVIOR_CLASS) {
                        RUNTIME_ERROR("Superclass must be a class.");
                    }
                    bindSuperclass(vm, klass, AS_CLASS(superclass));
                }
                else RUNTIME_ERROR("Only class can inherit from another class.");
                pop(vm);
                break;
            }
            case OP_IMPLEMENT: {
                uint8_t behaviorCount = READ_BYTE();
                ObjArray* traits = makeTraitArray(vm, behaviorCount);
                if (traits == NULL) RUNTIME_ERROR("Only traits can be implemented by class or another trait.");
                ObjClass* klass = AS_CLASS(peek(vm, behaviorCount));
                implementTraits(vm, klass, &traits->elements);
                pop(vm);
                break;
            }
            case OP_INSTANCE_METHOD:
                defineMethod(vm, READ_STRING(), false);
                break;
            case OP_CLASS_METHOD:
                defineMethod(vm, READ_STRING(), true);
                break;
            case OP_ARRAY: {
                uint8_t elementCount = READ_BYTE();
                makeArray(vm, elementCount);
                break;
            }
            case OP_DICTIONARY: {
                uint8_t entryCount = READ_BYTE();
                makeDictionary(vm, entryCount);
                break;
            }
            case OP_RANGE: {
                if (IS_INT(peek(vm, 0)) && IS_INT(peek(vm, 1))) {
                    int b = AS_INT(pop(vm));
                    int a = AS_INT(pop(vm));
                    push(vm, OBJ_VAL(newRange(vm, a, b)));
                }
                else OVERLOAD_OP(.., 1);
                break;
            }
            case OP_REQUIRE: {
                Value filePath = pop(vm);
                Value value;
                if (!IS_STRING(filePath)) {
                    throwNativeException(vm, "clox.std.lang.IllegalArgumentException", "Required file path must be a string.");
                    break;
                }
                else if (tableGet(&vm->modules, AS_STRING(filePath), &value)) {
                    break;
                }

                loadModule(vm, AS_STRING(filePath));
                LOAD_FRAME();
                break;
            }
            case OP_NAMESPACE: {
                Value namespace = READ_IDENTIFIER();
                push(vm, namespace);
                break;
            }
            case OP_DECLARE_NAMESPACE: {
                uint8_t namespaceDepth = READ_BYTE();
                vm->currentNamespace = declareNamespace(vm, namespaceDepth);
                break;
            }
            case OP_GET_NAMESPACE: {
                uint8_t namespaceDepth = READ_BYTE();
                Value value = usingNamespace(vm, namespaceDepth);
                ObjNamespace* enclosingNamespace = AS_NAMESPACE(pop(vm));
                ObjString* shortName = AS_STRING(pop(vm));

                if (!IS_NIL(value)) push(vm, value);
                else {
                    ObjString* filePath = resolveSourceFile(vm, shortName, enclosingNamespace);
                    if (sourceFileExists(filePath)) {
                        loadModule(vm, filePath);
                        if (tableGet(&enclosingNamespace->values, shortName, &value)) {
                            pop(vm);
                            push(vm, value);
                        }
                        else RUNTIME_ERROR("Undefined class/trait/namespace %s specified", shortName->chars);
                    }
                    else {
                        ObjString* directoryPath = resolveSourceDirectory(vm, shortName, enclosingNamespace);
                        if (!sourceDirectoryExists(directoryPath)) {
                            throwNativeException(vm, "clox.std.io.FileNotFoundException", "Failed to load source file for %s", filePath->chars);
                        }
                        else if (!tableGet(&enclosingNamespace->values, shortName, &value)) {
                            ObjNamespace* namespace = newNamespace(vm, shortName, enclosingNamespace);
                            push(vm, OBJ_VAL(namespace));
                            tableSet(vm, &enclosingNamespace->values, shortName, OBJ_VAL(namespace));
                        }
                    }
                }
                break;
            }
            case OP_USING_NAMESPACE: {
                Value value = pop(vm);
                if (IS_NIL(value)) RUNTIME_ERROR("Undefined class/trait/namespace specified.");
                ObjString* alias = READ_STRING();
                int index;

               if (alias->length > 0) {
                    if (idMapGet(&vm->currentModule->valIndexes, alias, &index)) {
                        vm->currentModule->valFields.values[index] = value;
                    }
                    else {
                        idMapSet(vm, &vm->currentModule->valIndexes, alias, vm->currentModule->valFields.count);
                        valueArrayWrite(vm, &vm->currentModule->valFields, value);
                    }
                }
                else if (IS_CLASS(value)) {
                    ObjClass* klass = AS_CLASS(value);
                    if (idMapGet(&vm->currentModule->valIndexes, klass->name, &index)) {
                        vm->currentModule->valFields.values[index] = value;
                    }
                    else {
                        idMapSet(vm, &vm->currentModule->valIndexes, klass->name, vm->currentModule->valFields.count);
                        valueArrayWrite(vm, &vm->currentModule->valFields, value);
                    }
                }
                else if (IS_NAMESPACE(value)) {
                    ObjNamespace* namespace = AS_NAMESPACE(value);
                    if (idMapGet(&vm->currentModule->valIndexes, namespace->shortName, &index)) {
                        vm->currentModule->valFields.values[index] = value;
                    }
                    else {
                        idMapSet(vm, &vm->currentModule->valIndexes, namespace->shortName, vm->currentModule->valFields.count);
                        valueArrayWrite(vm, &vm->currentModule->valFields, value);
                    }
                }
                else RUNTIME_ERROR("Only classes, traits and namespaces may be imported.");
                break;
            }
            case OP_THROW: {
                ObjArray* stackTrace = getStackTrace(vm);
                Value value = peek(vm, 0);

                if (!isObjInstanceOf(vm, value, vm->exceptionClass)) {
                    runtimeError(vm, "Only instances of class clox.std.lang.Exception may be thrown.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjException* exception = AS_EXCEPTION(value);
                exception->stacktrace = stackTrace;

                ObjString* name = frame->closure->function->name;
                Value receiver = peek(vm, frame->closure->function->arity + 1);
                if (CAN_INTERCEPT(receiver, INTERCEPTOR_ON_THROW, __onThrow__) && hasInterceptableMethod(vm, receiver, name)) {
                    pop(vm);
                    interceptOnThrow(vm, receiver, name, OBJ_VAL(exception));
                    LOAD_FRAME();
                }

                if (propagateException(vm)) {
                    LOAD_FRAME();
                    break;
                }
                else if (vm->runningGenerator != NULL) vm->runningGenerator->state = GENERATOR_THROW;
                return INTERPRET_RUNTIME_ERROR;
            }
            case OP_TRY: {
                uint8_t byte = READ_BYTE();
                uint16_t handlerAddress = READ_SHORT();
                uint16_t finallyAddress = READ_SHORT();
                Value value;
                if (!loadGlobal(vm, &frame->closure->function->chunk, byte, &value)) {
                    ObjString* exceptionClass = AS_STRING(frame->closure->function->chunk.identifiers.values[byte]);
                    RUNTIME_ERROR("Undefined class %s specified as exception type.", exceptionClass->chars);
                }

                ObjClass* klass = AS_CLASS(value);
                if (!isClassExtendingSuperclass(klass, vm->exceptionClass)) {
                    ObjString* exceptionClass = AS_STRING(frame->closure->function->chunk.identifiers.values[byte]);
                    RUNTIME_ERROR("Expect subclass of clox.std.lang.Exception, but got Class %s.", exceptionClass->chars);
                }
                pushExceptionHandler(vm, klass, handlerAddress, finallyAddress);
                break;
            }
            case OP_CATCH:
                frame->handlerCount--;
                break;
            case OP_FINALLY: {
                frame->handlerCount--;
                if (propagateException(vm)) {
                    LOAD_FRAME();
                    break;
                }
                return INTERPRET_RUNTIME_ERROR;
            }
            case OP_RETURN: {
                Value result = pop(vm);
                ObjString* name = frame->closure->function->name;
                Value receiver = peek(vm, frame->closure->function->arity);
                closeUpvalues(vm, frame->slots);
                if (frame->closure->function->isGenerator || frame->closure->function->isAsync) vm->runningGenerator->state = GENERATOR_RETURN;
                if (frame->closure->function->isAsync && !IS_PROMISE(result)) {
                    result = OBJ_VAL(promiseWithFulfilled(vm, result));
                }

                vm->frameCount--;
                if (vm->frameCount == 0) {
                    pop(vm);
                    return INTERPRET_OK;
                }

                if (!frame->closure->function->isGenerator && !frame->closure->function->isAsync) vm->stackTop = frame->slots;
                push(vm, result);
                if (vm->apiStackDepth > 0) return INTERPRET_OK;
                LOAD_FRAME();

                if (CAN_INTERCEPT(receiver, INTERCEPTOR_ON_RETURN, __onReturn__) && hasInterceptableMethod(vm, receiver, name)) {
                    interceptOnReturn(vm, receiver, name, result);
                    LOAD_FRAME();
                }
                break;
            }
            case OP_RETURN_NONLOCAL: {
                Value result = pop(vm);
                uint8_t depth = READ_BYTE();
                ObjString* name = frame->closure->function->name;
                Value receiver = peek(vm, frame->closure->function->arity);
                closeUpvalues(vm, frame->slots);
                if (frame->closure->function->isGenerator || frame->closure->function->isAsync) vm->runningGenerator->state = GENERATOR_RETURN;
                if (frame->closure->function->isAsync && !IS_PROMISE(result)) {
                    result = OBJ_VAL(promiseWithFulfilled(vm, result));
                }

                vm->frameCount -= depth + 1;
                if (vm->frameCount == 0) {
                    pop(vm);
                    return INTERPRET_OK;
                }

                if (!frame->closure->function->isGenerator && !frame->closure->function->isAsync) vm->stackTop = frame->slots;
                push(vm, result);
                if (vm->apiStackDepth > 0) return INTERPRET_OK;
                LOAD_FRAME();

                if (CAN_INTERCEPT(receiver, INTERCEPTOR_ON_RETURN, __onReturn__) && hasInterceptableMethod(vm, receiver, name)) {
                    interceptOnReturn(vm, receiver, name, result);
                    LOAD_FRAME();
                }
                break;
            }
            case OP_YIELD: {
                Value result = peek(vm, 0);
                ObjString* name = frame->closure->function->name;
                Value receiver = vm->runningGenerator->frame->slots[0];
                saveGeneratorFrame(vm, vm->runningGenerator, frame, result);

                if (CAN_INTERCEPT(receiver, INTERCEPTOR_ON_YIELD, __onYield__) && hasInterceptableMethod(vm, receiver, name)) {
                    interceptOnYield(vm, receiver, name, result);
                    LOAD_FRAME();
                }

                vm->frameCount--;
                if (vm->apiStackDepth > 0) return INTERPRET_OK;
                LOAD_FRAME();
                break;
            }
            case OP_YIELD_FROM: {
                Value result = peek(vm, 0);
                ObjString* name = frame->closure->function->name;
                Value receiver = vm->runningGenerator->frame->slots[0];
                saveGeneratorFrame(vm, vm->runningGenerator, frame, result);

                if (!IS_GENERATOR(result)) {
                    result = loadInnerGenerator(vm);
                }
                ObjGenerator* generator = AS_GENERATOR(result);
                yieldFromInnerGenerator(vm, generator);

                if (generator->state == GENERATOR_RETURN) vm->runningGenerator->frame->ip++;
                else {
                    vm->frameCount--;
                    if (vm->apiStackDepth > 0) return INTERPRET_OK;
                    LOAD_FRAME();
                }
                break;
            }
            case OP_AWAIT: {
                Value result = peek(vm, 0);
                ObjString* name = frame->closure->function->name;
                Value receiver = vm->runningGenerator->frame->slots[0];
                if (!IS_PROMISE(result)) {
                    result = OBJ_VAL(promiseWithFulfilled(vm, result));
                }
                saveGeneratorFrame(vm, vm->runningGenerator, frame, result);

                if (CAN_INTERCEPT(receiver, INTERCEPTOR_ON_AWAIT, __onAwait__) && hasInterceptableMethod(vm, receiver, name)) {
                    interceptOnAwait(vm, receiver, name, result);
                    LOAD_FRAME();
                }

                vm->frameCount--;
                if (vm->apiStackDepth > 0) return INTERPRET_OK;
                LOAD_FRAME();
                break;
            }
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_IDENTIFIER
#undef READ_STRING
#undef BINARY_INT_OP
#undef BINARY_NUMBER_OP
#undef CAN_INTERCEPT
#undef OVERLOAD_OP
#undef RUNTIME_ERROR
}

InterpretResult runModule(VM* vm, ObjModule* module) {
    push(vm, OBJ_VAL(module->closure));
    bool result = callClosure(vm, module->closure, 0);
    if (module->closure->function->isAsync) return result ? INTERPRET_OK : INTERPRET_RUNTIME_ERROR;
    else return run(vm);
}

InterpretResult interpret(VM* vm, const char* source) {
    ObjFunction* function = compile(vm, source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;
    push(vm, OBJ_VAL(function));
    ObjClosure* closure = newClosure(vm, function);
    vm->currentModule->closure = closure;
    pop(vm);
    return runModule(vm, vm->currentModule);
}