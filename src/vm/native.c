#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "assert.h"
#include "memory.h"
#include "native.h"
#include "string.h"

LOX_FUNCTION(assert) {
    ASSERT_ARG_COUNT("assert(expression, message)", 2);
    ASSERT_ARG_TYPE("assert(expression, message)", 1, String);
    if (isFalsey(args[0])) {
        raiseError(vm, AS_CSTRING(args[1]));
    }
    RETURN_NIL;
}

LOX_FUNCTION(clock) {
    ASSERT_ARG_COUNT("clock()", 0);
    RETURN_NUMBER((double)clock() / CLOCKS_PER_SEC);
}

LOX_FUNCTION(error){
    ASSERT_ARG_COUNT("error(message)", 1);
    ASSERT_ARG_TYPE("error(message)", 0, String);
    runtimeError(vm, AS_CSTRING(args[0]));
    exit(70);
    RETURN_NIL;
}

LOX_FUNCTION(gc) {
    ASSERT_ARG_COUNT("gc()", 0);
    collectGarbage(vm);
    RETURN_NIL;
}

LOX_FUNCTION(print){
    ASSERT_ARG_COUNT("print(message)", 1);
    printValue(args[0]);
    RETURN_NIL;
}

LOX_FUNCTION(println){
    ASSERT_ARG_COUNT("println(message)", 1);
    printValue(args[0]);
    printf("\n");
    RETURN_NIL;
}

LOX_FUNCTION(read) {
    ASSERT_ARG_COUNT("read()", 0);
    uint64_t inputSize = 128;
    char* line = malloc(inputSize);
    if (line == NULL) exit(1);

    int c = EOF;
    uint64_t i = 0;
    while ((c = getchar()) != '\n' && c != EOF) {
        line[i++] = (char)c;
        if (inputSize == i + 1) {
            inputSize = GROW_CAPACITY(inputSize);
            char* newLine = realloc(line, inputSize);
            if (newLine == NULL) exit(1);
            line = newLine;
        }
    }

    line[i] = '\0';
    ObjString* input = newString(vm, line);
    free(line);
    RETURN_OBJ(input);
}

LOX_FUNCTION(require) {
    ASSERT_ARG_COUNT("require(filePath)", 1);
    ASSERT_ARG_TYPE("require(filePath)", 0, String);
    char* filePath = AS_CSTRING(args[0]);

    InterpretResult result = loadSourceFile(vm, filePath);
    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
    RETURN_NIL;
}

ObjClass* defineNativeClass(VM* vm, const char* name) {
    ObjString* className = newString(vm, name);
    push(vm, OBJ_VAL(className));
    ObjClass* nativeClass = newClass(vm, className);
    nativeClass->isNative = true;
    nativeClass->obj.klass->isNative = true;
    push(vm, OBJ_VAL(nativeClass));
    tableSet(vm, &vm->globalValues, AS_STRING(vm->stack[0]), vm->stack[1]);
    pop(vm);
    pop(vm);
    return nativeClass;
}

void defineNativeFunction(VM* vm, const char* name, int arity, NativeFunction function) {
    ObjString* functionName = newString(vm, name);
    push(vm, OBJ_VAL(functionName));
    push(vm, OBJ_VAL(newNativeFunction(vm, functionName, arity, function)));
    tableSet(vm, &vm->globalValues, AS_STRING(vm->stack[0]), vm->stack[1]);
    pop(vm);
    pop(vm);
}

void defineNativeMethod(VM* vm, ObjClass* klass, const char* name, int arity, NativeMethod method) {
    ObjString* methodName = newString(vm, name);
    push(vm, OBJ_VAL(methodName));
    ObjNativeMethod* nativeMethod = newNativeMethod(vm, klass, methodName, arity, method);
    push(vm, OBJ_VAL(nativeMethod));
    tableSet(vm, &klass->methods, methodName, OBJ_VAL(nativeMethod));
    pop(vm);
    pop(vm);
}

ObjClass* defineNativeTrait(VM* vm, const char* name) {
    ObjString* traitName = newString(vm, name);
    push(vm, OBJ_VAL(traitName));
    ObjClass* nativeTrait = createTrait(vm, traitName);
    nativeTrait->isNative = true;
    push(vm, OBJ_VAL(nativeTrait));
    tableSet(vm, &vm->globalValues, AS_STRING(vm->stack[0]), vm->stack[1]);
    pop(vm);
    pop(vm);
    return nativeTrait;
}

ObjClass* defineSpecialClass(VM* vm, const char* name, BehaviorType behavior) {
    ObjString* className = newString(vm, name);
    push(vm, OBJ_VAL(className));
    ObjClass* nativeClass = createClass(vm, className, NULL, behavior);
    nativeClass->isNative = true;
    push(vm, OBJ_VAL(nativeClass));
    tableSet(vm, &vm->globalValues, AS_STRING(vm->stack[0]), vm->stack[1]);
    pop(vm);
    pop(vm);
    return nativeClass;
}

ObjClass* getNativeClass(VM* vm, const char* name) {
    Value klass;
    tableGet(&vm->globalValues, newString(vm, name), &klass);
    if (!IS_CLASS(klass)) {
        runtimeError(vm, "Native class %s is undefined.", name);
        exit(70);
    }
    return AS_CLASS(klass);
}

ObjNativeFunction* getNativeFunction(VM* vm, const char* name) {
    Value function;
    tableGet(&vm->globalValues, newString(vm, name), &function);
    if (!IS_NATIVE_FUNCTION(function)) {
        runtimeError(vm, "Native function %s is undefined.", name);
        exit(70);
    }
    return AS_NATIVE_FUNCTION(function);
}

ObjNativeMethod* getNativeMethod(VM* vm, ObjClass* klass, const char* name) {
    Value method;
    tableGet(&klass->methods, newString(vm, name), &method);
    if (!IS_NATIVE_METHOD(method)) {
        runtimeError(vm, "Native method %s::%s is undefined.", klass->name->chars, name);
        exit(70);
    }
    return AS_NATIVE_METHOD(method);
}

InterpretResult loadSourceFile(VM* vm, const char* filePath) {
    char* source = readFile(filePath);
    InterpretResult result = interpret(vm, source);
    free(source);
    return result;
}

void registerNativeFunctions(VM* vm){
    DEF_FUNCTION(assert, 2);
    DEF_FUNCTION(clock, 0);
    DEF_FUNCTION(error, 1);
    DEF_FUNCTION(gc, 0);
    DEF_FUNCTION(print, 1);
    DEF_FUNCTION(println, 1);
    DEF_FUNCTION(read, 0);
    DEF_FUNCTION(require, 1);
}