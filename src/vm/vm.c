#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "debug.h"
#include "memory.h"
#include "native.h"
#include "string.h"
#include "vm.h"
#include "../inc/ini.h"
#include "../std/lang.h"
#include "../std/util.h"

static void resetStack(VM* vm) {
    vm->stackTop = vm->stack;
    vm->frameCount = 0;
    vm->openUpvalues = NULL;
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
    vm->currentCompiler = NULL;
    vm->currentClass = NULL;
    vm->objects = NULL;
    vm->bytesAllocated = 0;
    vm->nextGC = vm->config.gcHeapSize;

    vm->grayCount = 0;
    vm->grayCapacity = 0;
    vm->grayStack = NULL;

    initTable(&vm->globals);
    initTable(&vm->strings);
    vm->initString = NULL;
    vm->initString = copyString(vm, "init", 4);

    registerLangPackage(vm);
    registerUtilPackage(vm);
    registerNativeFunctions(vm);
}

void freeVM(VM* vm) {
    freeTable(vm, &vm->globals);
    freeTable(vm, &vm->strings);
    vm->initString = NULL;
    freeObjects(vm);
}

void push(VM* vm, Value value) {
    *vm->stackTop = value;
    vm->stackTop++;
}

Value pop(VM* vm) {
    vm->stackTop--;
    return *vm->stackTop;
}

static Value peek(VM* vm, int distance) {
    return vm->stackTop[-1 - distance];
}

static bool isFalsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
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

static void makeList(VM* vm, uint8_t elementCount) {
    ObjList* list = newList(vm);
    push(vm, OBJ_VAL(list));
    for (int i = elementCount; i > 0; i--) {
        valueArrayWrite(vm, &list->elements, peek(vm, i));
    }
    pop(vm);

    while (elementCount > 0) {
        elementCount--;
        pop(vm);
    }
    push(vm, OBJ_VAL(list));
}

static bool makeDictionary(VM* vm, uint8_t entryCount) {
    ObjDictionary* dictionary = newDictionary(vm);
    push(vm, OBJ_VAL(dictionary));
    for (int i = 1; i <= entryCount; i++) {
        Value key = peek(vm, 2 * i);
        Value value = peek(vm, 2 * i - 1);
        if (!IS_STRING(key)) return false;
        tableSet(vm, &dictionary->table, AS_STRING(key), value);
    }
    pop(vm);

    while (entryCount > 0) {
        entryCount--;
        pop(vm);
        pop(vm);
    }
    push(vm, OBJ_VAL(dictionary));
    return true;
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
        makeList(vm, argCount);
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
    vm->stackTop -= argCount + 1;
    push(vm, result);
    return true;
}

static bool callMethod(VM* vm, Value method, int argCount) {
    if (IS_NATIVE_METHOD(method)) {
        return callNativeMethod(vm, AS_NATIVE_METHOD(method)->method, argCount);
    }
    else return callClosure(vm, AS_CLOSURE(method), argCount);
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
        runtimeError(vm, "Undefined property '%s'.", name->chars);
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
    return invokeFromClass(vm, getObjClass(vm, receiver), name, argCount);
}

static bool bindMethod(VM* vm, ObjClass* klass, ObjString* name) {
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        runtimeError(vm, "Undefined property '%s'.", name->chars);
        return false;
    }

    ObjBoundMethod* bound = newBoundMethod(vm, peek(vm, 0), AS_CLOSURE(method));
    pop(vm);
    push(vm, OBJ_VAL(bound));
    return true;
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

static void defineMethod(VM* vm, ObjString* name) {
    Value method = peek(vm, 0);
    ObjClass* klass = AS_CLASS(peek(vm, 1));
    tableSet(vm, &klass->methods, name, method);
    pop(vm);
}

void bindSuperclass(VM* vm, ObjClass* subclass, ObjClass* superclass) {
    if (superclass == NULL) {
        runtimeError(vm, "Superclass cannot be null for class %s", subclass->name);
        return;
    }
    subclass->superclass = superclass;
    tableAddAll(vm, &superclass->methods, &subclass->methods);
}

static InterpretResult run(VM* vm) {
    CallFrame* frame = &vm->frames[vm->frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op) \
    do { \
        if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
            runtimeError(vm, "Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(pop(vm)); \
        double a = AS_NUMBER(pop(vm)); \
        push(vm, valueType(a op b)); \
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
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value;
                if (!tableGet(&vm->globals, name, &value)) {
                    runtimeError(vm, "Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(vm, value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                tableSet(vm, &vm->globals, name, peek(vm, 0));
                pop(vm);
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
                if (!IS_INSTANCE(peek(vm, 0))) {
                    runtimeError(vm, "Only instances can have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjInstance* instance = AS_INSTANCE(peek(vm, 0));
                ObjString* name = READ_STRING();
                Value value;

                if (tableGet(&instance->fields, name, &value)) {
                    pop(vm);
                    push(vm, value);
                    break;
                }
                
                if (!bindMethod(vm, instance->obj.klass, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_PROPERTY: {
                if (!IS_INSTANCE(peek(vm, 1))) {
                    runtimeError(vm, "Only instances can have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjInstance* instance = AS_INSTANCE(peek(vm, 1));
                tableSet(vm, &instance->fields, READ_STRING(), peek(vm, 0));

                Value value = pop(vm);
                pop(vm);
                push(vm, value);
                break;
            }
            case OP_GET_SUBSCRIPT: {
                if (IS_INT(peek(vm, 0))) {
                    if (!IS_LIST(peek(vm, 1))) {
                        runtimeError(vm, "Only List can have integer subscripts.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    int index = AS_INT(pop(vm));
                    ObjList* list = AS_LIST(pop(vm));
                    if (index < 0 || index > list->elements.count) {
                        runtimeError(vm, "List index is out of bound.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    Value element = list->elements.values[index];
                    push(vm, element);
                }
                else if (IS_STRING(peek(vm, 0))) {
                    if (!IS_DICTIONARY(peek(vm, 1))) {
                        runtimeError(vm, "Only Dictionary can have string subscripts.");
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    ObjString* key = AS_STRING(pop(vm));
                    ObjDictionary* dictionary = AS_DICTIONARY(pop(vm));
                    Value value;
                    if (tableGet(&dictionary->table, key, &value)) push(vm, value);
                    else push(vm, NIL_VAL);
                }
                else {
                    runtimeError(vm, "Subscript must be integer or string.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_SUBSCRIPT: {
                if (IS_INT(peek(vm, 1))) {
                    if (!IS_LIST(peek(vm, 2))) {
                        runtimeError(vm, "Only List can have integer subscripts.");
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    Value element = pop(vm);
                    int index = AS_INT(pop(vm));
                    ObjList* list = AS_LIST(pop(vm));
                    if (index < 0 || index > list->elements.count) {
                        runtimeError(vm, "List index is out of bound.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    valueArrayInsert(vm, &list->elements, index, element);
                    push(vm, OBJ_VAL(list));
                }
                else if (IS_STRING(peek(vm, 1))) {
                    if (!IS_DICTIONARY(peek(vm, 2))) {
                        runtimeError(vm, "Only Dictionary can have string subscripts.");
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    Value value = pop(vm);
                    ObjString* key = AS_STRING(pop(vm));
                    ObjDictionary* dictionary = AS_DICTIONARY(pop(vm));
                    tableSet(vm, &dictionary->table, key, value);
                    push(vm, OBJ_VAL(dictionary));
                }
                else {
                    runtimeError(vm, "Subscript must be integer or string.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_SUPER: {
                ObjString* name = READ_STRING();
                ObjClass* superclass = AS_CLASS(pop(vm));
                if (!bindMethod(vm, superclass, name)) {
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
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
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
                    runtimeError(vm, "Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
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
            case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
            case OP_NOT:
                push(vm, BOOL_VAL(isFalsey(pop(vm))));
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(vm, 0))) {
                    runtimeError(vm, "Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if(IS_INT(peek(vm, 0))) push(vm, INT_VAL(-AS_INT(pop(vm))));
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
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                int argCount = READ_BYTE();
                if (!callValue(vm, peek(vm, argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_INVOKE: {
                ObjString* method = READ_STRING();
                int argCount = READ_BYTE();
                if (!invoke(vm, method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }
            case OP_SUPER_INVOKE: {
                ObjString* method = READ_STRING();
                int argCount = READ_BYTE();
                ObjClass* superclass = AS_CLASS(pop(vm));
                if (!invokeFromClass(vm, superclass, method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
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
            case OP_CLASS:
                push(vm, OBJ_VAL(newClass(vm, READ_STRING())));
                break;
            case OP_INHERIT: {
                Value superclass = peek(vm, 1);
                if (!IS_CLASS(superclass)) {
                    runtimeError(vm, "Superclass must be a class.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjClass* subclass = AS_CLASS(peek(vm, 0));
                bindSuperclass(vm, subclass, AS_CLASS(superclass));
                pop(vm);
                break;
            }
            case OP_METHOD:
                defineMethod(vm, READ_STRING());
                break;
            case OP_LIST: {
                int elementCount = READ_BYTE();
                makeList(vm, elementCount);
                break;
            }
            case OP_DICTIONARY: {
                int entryCount = READ_BYTE();
                if (!makeDictionary(vm, entryCount)) { 
                    runtimeError(vm, "keys for dictionary literal must be strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
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

void hack(VM* vm, bool b) {
    run(vm);
    if (b) hack(vm, false);
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