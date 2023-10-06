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
#include "native.h"
#include "object.h"
#include "os.h"
#include "string.h"
#include "vm.h"
#include "../inc/ini.h"
#include "../std/collection.h"
#include "../std/io.h"
#include "../std/lang.h"
#include "../std/network.h"
#include "../std/util.h"

static void resetCallFrames(VM* vm) {
    for (int i = 0; i < FRAMES_MAX; i++) {
        CallFrame* frame = &vm->frames[i];
        frame->closure = NULL;
        frame->ip = NULL;
        frame->slots = NULL;
        frame->handlerCount = 0;
    }
}

static void resetStack(VM* vm) {
    vm->stackTop = vm->stack;
    vm->frameCount = 0;
    vm->apiStackDepth = 0;
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

    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }

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

void initConfiguration(VM* vm) {
    Configuration config;
    if (ini_parse("clox.ini", parseConfiguration, &config) < 0) {
        printf("Can't load 'clox.ini' configuration file...\n");
        exit(70);
    }
    vm->config = config;
}

void initVM(VM* vm) {
    resetStack(vm);
    initConfiguration(vm);
    vm->currentModule = NULL;
    vm->currentCompiler = NULL;
    vm->currentClass = NULL;
    vm->objects = NULL;
    vm->bytesAllocated = 0;
    vm->nextGC = vm->config.gcHeapSize;

    vm->grayCount = 0;
    vm->grayCapacity = 0;
    vm->grayStack = NULL;

    initTable(&vm->globals);
    initTable(&vm->namespaces);
    initTable(&vm->modules);
    initTable(&vm->strings);
    vm->initString = NULL;
    vm->initString = copyString(vm, "init", 4);

    registerLangPackage(vm);
    registerCollectionPackage(vm);
    registerIOPackage(vm);
    registerNetworkPackage(vm);
    registerUtilPackage(vm);
    registerNativeFunctions(vm);
}

void freeVM(VM* vm) {
    freeTable(vm, &vm->globals);
    freeTable(vm, &vm->namespaces);
    freeTable(vm, &vm->modules);
    freeTable(vm, &vm->strings);
    vm->initString = NULL;
    freeObjects(vm);
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

static Value peek(VM* vm, int distance) {
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
        if (!IS_CLASS(trait) || AS_CLASS(trait)->behavior != BEHAVIOR_TRAIT) {
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

static ObjNamespace* declareNamespace(VM* vm, uint8_t namespaceDepth) {
    ObjNamespace* enclosingNamespace = vm->rootNamespace;
    for (int i = namespaceDepth - 1; i >= 0; i--) {
        ObjString* name = AS_STRING(peek(vm, i));
        Value value;
        if (!tableGet(&enclosingNamespace->values, name, &value)) {
            enclosingNamespace = defineNativeNamespace(vm, name->chars, enclosingNamespace);
        }
        else enclosingNamespace = AS_NAMESPACE(value);
    }

    while (namespaceDepth > 0) {
        pop(vm);
        namespaceDepth--;
    }
    return enclosingNamespace;
}

static Value usingNamespace(VM* vm, uint8_t namespaceDepth) {
    ObjNamespace* enclosingNamespace = vm->rootNamespace;
    Value value;
    for (int i = namespaceDepth - 1; i >= 1; i--) {
        ObjString* name = AS_STRING(peek(vm, i));
        if (!tableGet(&enclosingNamespace->values, name, &value)) {
            enclosingNamespace = defineNativeNamespace(vm, name->chars, enclosingNamespace);
        }
        else enclosingNamespace = AS_NAMESPACE(value);
    }

    ObjString* shortName = AS_STRING(peek(vm, 0));
    bool valueExists = tableGet(&enclosingNamespace->values, shortName, &value);
    while (namespaceDepth > 0) {
        pop(vm);
        namespaceDepth--;
    }

    push(vm, OBJ_VAL(shortName));
    push(vm, OBJ_VAL(enclosingNamespace));
    return valueExists ? value : NIL_VAL;
}

static bool sourceFileExists(ObjString* filePath) {
    struct stat fileStat;
    return stat(filePath->chars, &fileStat) == 0;
}

static ObjString* resolveSourceFile(VM* vm, ObjString* shortName, ObjNamespace* enclosingNamespace) {
    int length = enclosingNamespace->fullName->length + shortName->length + 5;
    char* heapChars = ALLOCATE(char, length + 1);
    int offset = 0;
    while (offset < enclosingNamespace->fullName->length) {
        char currentChar = enclosingNamespace->fullName->chars[offset];
        heapChars[offset] = (currentChar == '.') ? '/' : currentChar;
        offset++;
    }
    heapChars[offset++] = '/';

    int startIndex = offset;
    while (offset < startIndex + shortName->length) {
        heapChars[offset] = shortName->chars[offset - startIndex];
        offset++;
    }

    heapChars[offset++] = '.';
    heapChars[length - 3] = 'l';
    heapChars[length - 2] = 'o';
    heapChars[length - 1] = 'x';
    heapChars[length] = '\n';
    return takeString(vm, heapChars, length);
}

static bool sourceDirectoryExists(ObjString* directoryPath) {
    struct stat directoryStat;
    if (stat(directoryPath->chars, &directoryStat) == 0) return (directoryStat.st_mode & S_IFMT) == S_IFDIR;
    return false;
}

static ObjString* resolveSourceDirectory(VM* vm, ObjString* shortName, ObjNamespace* enclosingNamespace) {
    int length = enclosingNamespace->fullName->length + shortName->length + 1;
    char* heapChars = ALLOCATE(char, length + 1);
    int offset = 0;
    while (offset < enclosingNamespace->fullName->length) {
        char currentChar = enclosingNamespace->fullName->chars[offset];
        heapChars[offset] = (currentChar == '.') ? '/' : currentChar;
        offset++;
    }
    heapChars[offset++] = '/';

    int startIndex = offset;
    while (offset < startIndex + shortName->length) {
        heapChars[offset] = shortName->chars[offset - startIndex];
        offset++;
    }

    heapChars[length] = '\n';
    return takeString(vm, heapChars, length);
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

    CallFrame* frame = &vm->frames[vm->frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm->stackTop - argCount - 1;
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

Value callReentrant(VM* vm, Value receiver, Value callee, ...) {
    push(vm, receiver);
    int argCount = IS_NATIVE_METHOD(callee) ? AS_NATIVE_METHOD(callee)->arity : AS_CLOSURE(callee)->function->arity;
    va_list args;
    va_start(args, callee);
    for (int i = 0; i < argCount; i++) {
        push(vm, va_arg(args, Value));
    }
    va_end(args);

    if (IS_CLOSURE(callee)) {
        vm->apiStackDepth++;
        callClosure(vm, AS_CLOSURE(callee), argCount);
        run(vm);
        vm->apiStackDepth--;
    }
    else {
        callNativeMethod(vm, AS_NATIVE_METHOD(callee)->method, argCount);
    }
    return pop(vm);
}

static bool callValue(VM* vm, Value callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
                vm->stackTop[-argCount - 1] = bound->receiver;
                return callMethod(vm, OBJ_VAL(bound->method), argCount);
            }
            case OBJ_CLASS: {
                ObjClass* klass = AS_CLASS(callee);
                vm->stackTop[-argCount - 1] = OBJ_VAL(newInstance(vm, klass));

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
            case OBJ_CLOSURE:
                return callClosure(vm, AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE_FUNCTION: 
                return callNativeFunction(vm, AS_NATIVE_FUNCTION(callee)->function, argCount);
            case OBJ_NATIVE_METHOD: 
                return callNativeMethod(vm, AS_NATIVE_METHOD(callee)->method, argCount);
            default:
                break;
        }
    }
    runtimeError(vm, "Can only call functions and classes.");
    return false;
}

static bool invokeFromClass(VM* vm, ObjClass* klass, ObjString* name, int argCount) {
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        if(klass != vm->nilClass) runtimeError(vm, "Undefined method '%s'.", name->chars);
        return false;
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
        Value value;
        if (tableGet(&instance->fields, name, &value)) {
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

static bool bindMethod(VM* vm, ObjClass* klass, ObjString* name) {
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        runtimeError(vm, "Undefined method '%s'.", name->chars);
        return false;
    }

    ObjBoundMethod* bound = newBoundMethod(vm, peek(vm, 0), AS_CLOSURE(method));
    pop(vm);
    push(vm, OBJ_VAL(bound));
    return true;
}

bool loadGlobal(VM* vm, ObjString* name, Value* value) {
    if (tableGet(&vm->currentModule->values, name, value)) return true;
    else if (tableGet(&vm->currentNamespace->values, name, value)) return true;
    else if (tableGet(&vm->rootNamespace->values, name, value)) return true;
    else return tableGet(&vm->globals, name, value);
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

static void defineMethod(VM* vm, ObjString* name, bool isClassMethod) {
    Value method = peek(vm, 0);
    ObjClass* klass = AS_CLASS(peek(vm, 1));

    if (isClassMethod) {
        if (klass->behavior != BEHAVIOR_CLASS) {
            runtimeError(vm, "Class method '%s' can only be defined in class body.", name->chars);
        }
        klass = klass->obj.klass;
    }

    tableSet(vm, &klass->methods, name, method);
    pop(vm);
}

static bool loadModule(VM* vm, ObjString* path) {
    ObjModule* lastModule = vm->currentModule;
    vm->currentModule = newModule(vm, path);

    char* source = readFile(path->chars);
    ObjFunction* function = compile(vm, source);
    if (function == NULL) return false;
    push(vm, OBJ_VAL(function));

    ObjClosure* closure = newClosure(vm, function);
    pop(vm);
    push(vm, OBJ_VAL(closure));
    callClosure(vm, closure, 0);
    vm->currentModule = lastModule;
    free(source);

    vm->apiStackDepth++;
    run(vm);
    vm->apiStackDepth--;
    return true;
}

static bool getInstanceVariable(VM* vm, Value receiver, ObjString* name) {
    if (IS_INSTANCE(receiver)) {
        ObjInstance* instance = AS_INSTANCE(receiver);
        Value value;

        if (tableGet(&instance->fields, name, &value)) {
            pop(vm);
            push(vm, value);
            return true;
        }

        if (!bindMethod(vm, instance->obj.klass, name)) {
            return false;
        }
    }
    else if (IS_CLASS(receiver)) {
        ObjClass* klass = AS_CLASS(receiver);
        Value value;

        if (tableGet(&klass->fields, name, &value)) {
            pop(vm);
            push(vm, value);
            return true;
        }

        if (!bindMethod(vm, klass->obj.klass, name)) {
            return false;
        }
    }
    else if (IS_NAMESPACE(receiver)) {
        ObjNamespace* enclosing = AS_NAMESPACE(receiver);
        Value value;

        if (tableGet(&enclosing->values, name, &value)) {
            pop(vm);
            push(vm, value);
            return true;
        }
        else {
            ObjString* filePath = resolveSourceFile(vm, name, enclosing);
            if (!sourceFileExists(filePath)) {
                runtimeError(vm, "Undefined class '%s.%s'.", enclosing->fullName->chars, name->chars);
                return false;
            }

            loadModule(vm, filePath);
            pop(vm);
            pop(vm);
            tableGet(&enclosing->values, name, &value);
            push(vm, value);
            return true;
        }
    }
    else {
        if (IS_NIL(receiver)) runtimeError(vm, "Undefined property on nil.");
        else runtimeError(vm, "Only instances, classes and namespaces can get properties.");
        return false;
    }

    return true;
}

static bool setInstanceField(VM* vm, Value receiver, ObjString* name, Value value) {
    if (IS_INSTANCE(receiver)) {
        ObjInstance* instance = AS_INSTANCE(receiver);
        tableSet(vm, &instance->fields, name, value);
        push(vm, value);
    }
    else if (IS_CLASS(receiver)) {
        ObjClass* klass = AS_CLASS(receiver);
        tableSet(vm, &klass->fields, name, value);
        push(vm, value);
    }
    else {
        runtimeError(vm, "Only instances and classes can set properties.");
        return false;
    }
    return true;
}

static bool propagateException(VM* vm) {
    ObjInstance* exception = AS_INSTANCE(peek(vm, 0));
    while (vm->frameCount > 0) {
        CallFrame* frame = &vm->frames[vm->frameCount - 1];
        for (int i = frame->handlerCount; i > 0; i--) {
            ExceptionHandler handler = frame->handlerStack[i - 1];
            if (isObjInstanceOf(vm, OBJ_VAL(exception), handler.exceptionClass)) {
                frame->ip = &frame->closure->function->chunk.code[handler.handlerAddress];
                return true;
            }
            else if (handler.finallyAddress != UINT16_MAX){
                push(vm, TRUE_VAL);
                frame->ip = &frame->closure->function->chunk.code[handler.finallyAddress];
                return true;
            }
        }
        vm->frameCount--;
    }

    ObjString* message = AS_STRING(getObjProperty(vm, exception, "message"));
    fprintf(stderr, "Unhandled %s.%s: %s\n", exception->obj.klass->namespace->fullName->chars, exception->obj.klass->name->chars, message->chars);
    ObjArray* stackTrace = AS_ARRAY(getObjProperty(vm, exception, "stacktrace"));
    for (int i = 0; i < stackTrace->elements.count; i++) {
        Value item = stackTrace->elements.values[i];
        fprintf(stderr, "    %s.\n", AS_CSTRING(item));
    }
    fflush(stderr);
    return false;
}

static void pushExceptionHandler(VM* vm, ObjClass* exceptionClass, uint16_t handlerAddress, uint16_t finallyAddress) {
    CallFrame* frame = &vm->frames[vm->frameCount - 1];
    if (frame->handlerCount >= UINT4_MAX) {
        runtimeError(vm, "Too many nested exception handlers.");
        exit(70);
    }
    frame->handlerStack[frame->handlerCount].handlerAddress = handlerAddress;
    frame->handlerStack[frame->handlerCount].exceptionClass = exceptionClass;
    frame->handlerStack[frame->handlerCount].finallyAddress = finallyAddress;
    frame->handlerCount++;
}

ObjArray* getStackTrace(VM* vm) {
    ObjArray* stackTrace = newArray(vm);
    push(vm, OBJ_VAL(stackTrace));
    for (int i = vm->frameCount - 1; i >= 0; i--) {
        char stackTraceBuffer[UINT8_MAX];
        CallFrame* frame = &vm->frames[i];
        ObjModule* module = frame->closure->module;
        ObjFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        uint32_t line = function->chunk.lines[instruction];

        uint8_t length = snprintf(stackTraceBuffer, UINT8_MAX, "in %s() from %s at line %d",
            function->name == NULL ? "script" : function->name->chars, module->path->chars, line);
        ObjString* stackElement = copyString(vm, stackTraceBuffer, length);
        valueArrayWrite(vm, &stackTrace->elements, OBJ_VAL(stackElement));
    }
    pop(vm);
    return stackTrace;
}

ObjInstance* throwException(VM* vm, ObjClass* exceptionClass, const char* format, ...) {
    char chars[UINT8_MAX];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(chars, UINT8_MAX, format, args);
    va_end(args);
    ObjString* message = copyString(vm, chars, length);

    ObjArray* stacktrace = getStackTrace(vm);
    ObjInstance* exception = newInstance(vm, exceptionClass);
    push(vm, OBJ_VAL(exception));
    setObjProperty(vm, exception, "message", OBJ_VAL(message));
    setObjProperty(vm, exception, "stacktrace", OBJ_VAL(stacktrace));
    if (!propagateException(vm)) exit(70);
    else return exception;
}

InterpretResult run(VM* vm) {
    CallFrame* frame = &vm->frames[vm->frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op) \
    do { \
        if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
            ObjClass* exceptionClass = getNativeClass(vm, "clox.std.lang", "IllegalArgumentException"); \
            throwException(vm, exceptionClass, "Operands must be numbers."); \
        } \
        else { \
            double b = AS_NUMBER(pop(vm)); \
            double a = AS_NUMBER(pop(vm)); \
            push(vm, valueType(a op b)); \
        } \
    } while (false)

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
                tableSet(vm, &vm->rootNamespace->values, name, peek(vm, 0));
                pop(vm);
                break;
            } 
            case OP_DEFINE_GLOBAL_VAR: {
                ObjString* name = READ_STRING();
                tableSet(vm, &vm->globals, name, peek(vm, 0));
                pop(vm);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value;
                if (!loadGlobal(vm, name, &value)) {
                    runtimeError(vm, "Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(vm, value);
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                if (tableSet(vm, &vm->globals, name, peek(vm, 0))) {
                    tableDelete(&vm->globals, name);
                    runtimeError(vm, "Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
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
                ObjString* name = READ_STRING();
                
                if (!getInstanceVariable(vm, receiver, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_PROPERTY: {
                Value value = pop(vm);
                Value receiver = pop(vm);
                ObjString* name = READ_STRING();
                if (!setInstanceField(vm, receiver, name, value)) { 
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_PROPERTY_OPTIONAL: {
                Value receiver = peek(vm, 0);
                ObjString* name = READ_STRING();

                if (IS_NIL(receiver)) { 
                    pop(vm);
                    push(vm, NIL_VAL);
                }
                else if (!getInstanceVariable(vm, receiver, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_SUBSCRIPT: {
                if (IS_INT(peek(vm, 0))) {
                    int index = AS_INT(pop(vm));
                    if (IS_STRING(peek(vm, 0))) {
                        ObjString* string = AS_STRING(pop(vm));
                        if (index < 0 || index > string->length) {
                            ObjClass* exceptionClass = getNativeClass(vm, "clox.std.lang", "IndexOutOfBoundsException");
                            throwException(vm, exceptionClass, "String index is out of bound: %d.", index);
                        }
                        else {
                            char chars[2] = { string->chars[index], '\0' };
                            ObjString* element = copyString(vm, chars, 1);
                            push(vm, OBJ_VAL(element));
                        }
                    }
                    else if (IS_ARRAY(peek(vm, 0))) {
                        ObjArray* array = AS_ARRAY(pop(vm));
                        if (index < 0 || index > array->elements.count) {
                            ObjClass* exceptionClass = getNativeClass(vm, "clox.std.lang", "IndexOutOfBoundsException");
                            throwException(vm, exceptionClass, "Array index is out of bound: %d.", index);
                        }
                        else {
                            Value element = array->elements.values[index];
                            push(vm, element);
                        }
                    }
                    else {
                        runtimeError(vm, "Only String or Array can have integer subscripts.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                }
                else if (IS_DICTIONARY(peek(vm, 1))) {
                    Value key = pop(vm);
                    ObjDictionary* dictionary = AS_DICTIONARY(pop(vm));
                    Value value;
                    if (dictGet(dictionary, key, &value)) push(vm, value);
                    else push(vm, NIL_VAL);
                }
                else {
                    ObjClass* exceptionClass = getNativeClass(vm, "clox.std.lang", "IllegalArgumentException");
                    throwException(vm, exceptionClass, "Subscript must be integer or string.");
                }
                break;
            }
            case OP_SET_SUBSCRIPT: {
                if (IS_INT(peek(vm, 1)) && IS_ARRAY(peek(vm, 2))) {
                    Value element = pop(vm);
                    int index = AS_INT(pop(vm));
                    ObjArray* array = AS_ARRAY(pop(vm));
                    if (index < 0 || index > array->elements.count) {
                        ObjClass* exceptionClass = getNativeClass(vm, "clox.std.lang", "IndexOutOfBoundsException");
                        throwException(vm, exceptionClass, "Array index is out of bound: %d.", index);
                    }
                    else {
                        valueArrayInsert(vm, &array->elements, index, element);
                        push(vm, OBJ_VAL(array));
                    }
                }
                else if (IS_DICTIONARY(peek(vm, 2))) {
                    Value value = pop(vm);
                    Value key = pop(vm);
                    ObjDictionary* dictionary = AS_DICTIONARY(pop(vm));
                    dictSet(vm, dictionary, key, value);
                    push(vm, OBJ_VAL(dictionary));
                }
                else {
                    runtimeError(vm, "Only Array and Dictionary can have subscripts.");
                    return INTERPRET_RUNTIME_ERROR;
                }
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
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER: 
                BINARY_OP(BOOL_VAL, >); 
                break;
            case OP_LESS: 
                BINARY_OP(BOOL_VAL, <); 
                break;
            case OP_ADD: {
                if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
                     concatenate(vm);
                }
                else if (IS_INT(peek(vm, 0)) && IS_INT(peek(vm, 1))) {
                    int b = AS_INT(pop(vm));
                    int a = AS_INT(pop(vm));
                    push(vm, INT_VAL(a + b));
                }
                else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) {
                    double b = AS_NUMBER(pop(vm));
                    double a = AS_NUMBER(pop(vm));
                    push(vm, NUMBER_VAL(a + b));
                }
                else {
                    ObjClass* exceptionClass = getNativeClass(vm, "clox.std.lang", "IllegalArgumentException");
                    throwException(vm, exceptionClass, "Operands must be two numbers or two strings.");
                }
                break;
            }
            case OP_SUBTRACT: {
                if (IS_INT(peek(vm, 0)) && IS_INT(peek(vm, 1))) {
                    int b = AS_INT(pop(vm));
                    int a = AS_INT(pop(vm));
                    push(vm, INT_VAL(a - b));
                }
                else BINARY_OP(NUMBER_VAL, -);
                break;
            }
            case OP_MULTIPLY: {
                if (IS_INT(peek(vm, 0)) && IS_INT(peek(vm, 1))) {
                    int b = AS_INT(pop(vm));
                    int a = AS_INT(pop(vm));
                    push(vm, INT_VAL(a * b));
                }
                else BINARY_OP(NUMBER_VAL, *); 
                break;
            }
            case OP_DIVIDE: 
                if (IS_INT(peek(vm, 0)) && AS_INT(peek(vm, 0)) == 0) {
                    ObjClass* exceptionClass = getNativeClass(vm, "clox.std.lang", "ArithmeticException");
                    throwException(vm, exceptionClass, "It is illegal to divide an integer by 0.");
                }
                else BINARY_OP(NUMBER_VAL, /); 
                break;
            case OP_MODULO: {
                if (IS_INT(peek(vm, 0)) && IS_INT(peek(vm, 1))) {
                    int b = AS_INT(pop(vm));
                    int a = AS_INT(pop(vm));
                    push(vm, INT_VAL(a % b));
                }
                else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) {
                    double b = AS_NUMBER(pop(vm));
                    double a = AS_NUMBER(pop(vm));
                    push(vm, NUMBER_VAL(fmod(a, b)));
                }
                else {
                    ObjClass* exceptionClass = getNativeClass(vm, "clox.std.lang", "IllegalArgumentException");
                    throwException(vm, exceptionClass, "Operands must be two numbers.");
                }
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
                    ObjClass* exceptionClass = getNativeClass(vm, "clox.std.lang", "IllegalArgumentException"); 
                    throwException(vm, exceptionClass, "Operands must be numbers for negate operator.");
                }
                else if(IS_INT(peek(vm, 0))) push(vm, INT_VAL(-AS_INT(pop(vm))));
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
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_INVOKE: {
                Value receiver = peek(vm, 0);
                ObjString* method = READ_STRING();
                uint8_t argCount = READ_BYTE();

                if (!invoke(vm, method, argCount)) {
                    if (IS_NIL(receiver)) runtimeError(vm, "Calling undefined method '%s' on nil.", method->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_SUPER_INVOKE: {
                ObjString* method = READ_STRING();
                uint8_t argCount = READ_BYTE();
                ObjClass* klass = AS_CLASS(pop(vm));

                if (!invokeFromClass(vm, klass, method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_OPTIONAL_INVOKE: {
                Value receiver = peek(vm, 0);
                ObjString* method = READ_STRING();
                uint8_t argCount = READ_BYTE();

                if (!invoke(vm, method, argCount)) {
                    if (IS_NIL(receiver)) {
                        vm->stackTop -= (size_t)argCount + 1;
                        push(vm, NIL_VAL);
                    }
                    else {
                        runtimeError(vm, "Undefined method '%s'.", method->chars);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                }
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
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
                push(vm, OBJ_VAL(newClass(vm, className)));
                tableSet(vm, &vm->currentNamespace->values, className, peek(vm, 0));
                break;
            }
            case OP_TRAIT: { 
                ObjString* traitName = READ_STRING();
                push(vm, OBJ_VAL(createTrait(vm, traitName)));
                tableSet(vm, &vm->rootNamespace->values, traitName, peek(vm, 0));
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
                if (klass->behavior == BEHAVIOR_CLASS) {
                    Value superclass = peek(vm, 0);
                    if (!IS_CLASS(superclass) || AS_CLASS(superclass)->behavior != BEHAVIOR_CLASS) {
                        runtimeError(vm, "Superclass must be a class.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    bindSuperclass(vm, klass, AS_CLASS(superclass));
                }
                else {
                    runtimeError(vm, "Only class can inherit from another class.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                pop(vm);
                break;
            }
            case OP_IMPLEMENT: {
                uint8_t behaviorCount = READ_BYTE();
                ObjArray* traits = makeTraitArray(vm, behaviorCount);
                if (traits == NULL) {
                    runtimeError(vm, "Only traits can be implemented by class or another trait.");
                    return INTERPRET_RUNTIME_ERROR;
                }
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
                else {
                    ObjClass* exceptionClass = getNativeClass(vm, "clox.std.lang", "IllegalArgumentException");
                    throwException(vm, exceptionClass, "Operands must be two integers.");
                }
                break;
            }
            case OP_REQUIRE: {
                Value filePath = pop(vm);
                Value value;
                if (!IS_STRING(filePath)) {
                    ObjClass* exceptionClass = getNativeClass(vm, "clox.std.lang", "IllegalArgumentException");
                    throwException(vm, exceptionClass, "Required file path must be a string.");
                    break;
                }
                else if (tableGet(&vm->modules, AS_STRING(filePath), &value)) {
                    break;
                }

                loadModule(vm, AS_STRING(filePath));
                frame = &vm->frames[vm->frameCount - 1];
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
                        else {
                            runtimeError(vm, "Undefined class/trait/namespace %s specified", shortName->chars);
                            return INTERPRET_RUNTIME_ERROR;
                        }
                    }
                    else {
                        ObjString* directoryPath = resolveSourceDirectory(vm, shortName, enclosingNamespace);
                        if (!sourceDirectoryExists(directoryPath)) {
                            ObjClass* exceptionClass = getNativeClass(vm, "clox.std.io", "FileNotFoundException");
                            throwException(vm, exceptionClass, "Failed to load source file for %s", filePath->chars);
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
                if (IS_NIL(value)) {
                    runtimeError(vm, "Undefined class/trait/namespace specified.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjString* alias = READ_STRING(); 
                if (alias->length > 0) {
                    tableSet(vm, &vm->currentModule->values, alias, value);
                }
                else if (IS_CLASS(value)) {
                    ObjClass* klass = AS_CLASS(value);
                    tableSet(vm, &vm->currentModule->values, klass->name, value);
                }
                else if (IS_NAMESPACE(value)) {
                    ObjNamespace* namespace = AS_NAMESPACE(value);
                    tableSet(vm, &vm->currentModule->values, namespace->shortName, value);
                }
                else {
                    runtimeError(vm, "Only classes, traits and namespaces may be imported.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_THROW: {
                ObjArray* stackTrace = getStackTrace(vm);
                Value exception = peek(vm, 0);
                if (!isObjInstanceOf(vm, exception, vm->exceptionClass)) {
                    runtimeError(vm, "Only instances of class clox.std.lang.Exception may be thrown.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                setObjProperty(vm, AS_INSTANCE(exception), "stacktrace", OBJ_VAL(stackTrace));
                if (propagateException(vm)) {
                    frame = &vm->frames[vm->frameCount - 1];
                    break;
                }
                return INTERPRET_RUNTIME_ERROR;
            }
            case OP_TRY: {
                ObjString* exceptionClass = READ_STRING();
                uint16_t handlerAddress = READ_SHORT();
                uint16_t finallyAddress = READ_SHORT();
                Value value;
                if (!loadGlobal(vm, exceptionClass, &value)){
                    runtimeError(vm, "Undefined class %s specified as exception type.", exceptionClass->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjClass* klass = AS_CLASS(value);
                if (!isClassExtendingSuperclass(klass, vm->exceptionClass)) {
                    runtimeError(vm, "Expect subclass of clox.std.lang.Exception, but got Class %s.", exceptionClass->chars);
                    return INTERPRET_RUNTIME_ERROR;
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
                    frame = &vm->frames[vm->frameCount - 1];
                    break;
                }
                return INTERPRET_RUNTIME_ERROR;
            }
            case OP_RETURN: {
                Value result = pop(vm);
                closeUpvalues(vm, frame->slots);
                vm->frameCount--;
                if (vm->frameCount == 0) {
                    pop(vm);
                    return INTERPRET_OK;
                }

                vm->stackTop = frame->slots;
                push(vm, result);
                if (vm->apiStackDepth > 0) return INTERPRET_OK;
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_RETURN_NONLOCAL: {
                Value result = pop(vm);
                uint8_t depth = READ_BYTE();
                closeUpvalues(vm, frame->slots);
                vm->frameCount -= depth + 1;
                if (vm->frameCount == 0) {
                    pop(vm);
                    return INTERPRET_OK;
                }

                vm->stackTop = frame->slots;
                push(vm, result);
                if (vm->apiStackDepth > 0) return INTERPRET_OK;
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(VM* vm, const char* source) {
    ObjFunction* function = compile(vm, source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(vm, OBJ_VAL(function));
    ObjClosure* closure = newClosure(vm, function);
    pop(vm);
    push(vm, OBJ_VAL(closure));
    callClosure(vm, closure, 0);
    return run(vm);
}