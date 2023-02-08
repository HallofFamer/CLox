#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "assert.h"
#include "memory.h"
#include "native.h"
#include "object.h"
#include "string.h"
#include "vm.h"

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

LOX_FUNCTION(dateNow) {
    ASSERT_ARG_COUNT("dateNow()", 0);
    time_t nowTime;
    time(&nowTime);
    struct tm now;
    localtime_s(&now, &nowTime);
    ObjInstance* date = newInstance(vm, getNativeClass(vm, "Date"));
    push(vm, OBJ_VAL(date));
    setObjProperty(vm, date, "year", INT_VAL(1900 + now.tm_year));
    setObjProperty(vm, date, "month", INT_VAL(1 + now.tm_mon));
    setObjProperty(vm, date, "day", INT_VAL(now.tm_mday));
    pop(vm);
    RETURN_OBJ(date);
}

LOX_FUNCTION(dateTimeNow) {
    ASSERT_ARG_COUNT("dateTimeNow()", 0);
    time_t nowTime;
    time(&nowTime);
    struct tm now;
    localtime_s(&now, &nowTime);
    ObjInstance* date = newInstance(vm, getNativeClass(vm, "DateTime"));
    push(vm, OBJ_VAL(date));
    setObjProperty(vm, date, "year", INT_VAL(1900 + now.tm_year));
    setObjProperty(vm, date, "month", INT_VAL(1 + now.tm_mon));
    setObjProperty(vm, date, "day", INT_VAL(now.tm_mday));
    setObjProperty(vm, date, "hour", INT_VAL(now.tm_hour));
    setObjProperty(vm, date, "minute", INT_VAL(now.tm_min));
    setObjProperty(vm, date, "second", INT_VAL(now.tm_sec));
    pop(vm);
    RETURN_OBJ(date);
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

ObjClass* defineNativeClass(VM* vm, const char* name) {
    ObjString* className = newString(vm, name);
    push(vm, OBJ_VAL(className));
    ObjClass* nativeClass = newClass(vm, className);
    nativeClass->isNative = true;
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
    tableSet(vm, &vm->globalVariables, AS_STRING(vm->stack[0]), vm->stack[1]);
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
    tableGet(&vm->globalVariables, newString(vm, name), &function);
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

void initNativePackage(VM* vm, const char* filePath) {
    char* source = readFile(filePath);
    interpret(vm, source);
    free(source);
}

void registerNativeFunctions(VM* vm){
    DEF_FUNCTION(assert, 2);
    DEF_FUNCTION(clock, 0);
    DEF_FUNCTION(dateNow, 0);
    DEF_FUNCTION(dateTimeNow, 0);
    DEF_FUNCTION(error, 1);
    DEF_FUNCTION(gc, 0);
    DEF_FUNCTION(print, 1);
    DEF_FUNCTION(println, 1);
    DEF_FUNCTION(read, 0);
}