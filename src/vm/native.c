#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    RETURN_NUMBER(currentTimeInSec());
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
    ObjClass* nativeClass = newClass(vm, className, OBJ_INSTANCE);
    nativeClass->isNative = true;
    nativeClass->obj.klass->isNative = true;
    push(vm, OBJ_VAL(nativeClass));
    tableSet(vm, &vm->classes, nativeClass->fullName, OBJ_VAL(nativeClass));
    tableSet(vm, &vm->currentNamespace->values, AS_STRING(vm->stack[0]), vm->stack[1]); 
    pop(vm);
    pop(vm);
    return nativeClass;
}

void defineNativeFunction(VM* vm, const char* name, int arity, NativeFunction function) {
    ObjString* functionName = newString(vm, name);
    push(vm, OBJ_VAL(functionName));
    push(vm, OBJ_VAL(newNativeFunction(vm, functionName, arity, function)));
    tableSet(vm, &vm->rootNamespace->values, AS_STRING(vm->stack[0]), vm->stack[1]);
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

void defineNativeInterceptor(VM* vm, ObjClass* klass, InterceptorType type, int arity, NativeMethod method) {
    switch (type) {
        case INTERCEPTOR_INIT:
            defineNativeMethod(vm, klass, "__init__", arity, method);
            break;
        case INTERCEPTOR_BEFORE_GET:
            defineNativeMethod(vm, klass, "__beforeGet__", 1, method);
            break;
        case INTERCEPTOR_AFTER_GET:
            defineNativeMethod(vm, klass, "__afterGet__", 2, method);
            break;
        case INTERCEPTOR_BEFORE_SET:
            defineNativeMethod(vm, klass, "__beforeSet__", 2, method);
            break;
        case INTERCEPTOR_AFTER_SET: 
            defineNativeMethod(vm, klass, "__afterSet__", 2, method);
            break;
        case INTERCEPTOR_ON_INVOKE:
            defineNativeMethod(vm, klass, "__beforeInvoke__", 2, method);
            break;
        case INTERCEPTOR_ON_RETURN:
            defineNativeMethod(vm, klass, "__afterInvoke__", 3, method);
            break;
        case INTERCEPTOR_ON_THROW:
            defineNativeMethod(vm, klass, "__beforeThrow__", 2, method);
            break;
        case INTERCEPTOR_UNDEFINED_GET:
            defineNativeMethod(vm, klass, "__undefinedGet__", 1, method);
            break;
        case INTERCEPTOR_UNDEFINED_INVOKE:
            defineNativeMethod(vm, klass, "__undefinedInvoke__", 2, method);
            break;
        default: 
            runtimeError(vm, "Unknown interceptor type %d.", type);
            exit(70);
    }
    SET_CLASS_INTERCEPTOR(klass, type);
}

ObjClass* defineNativeTrait(VM* vm, const char* name) {
    ObjString* traitName = newString(vm, name);
    push(vm, OBJ_VAL(traitName));
    ObjClass* nativeTrait = createTrait(vm, traitName);
    nativeTrait->isNative = true;
    push(vm, OBJ_VAL(nativeTrait));
    tableSet(vm, &vm->classes, nativeTrait->fullName, OBJ_VAL(nativeTrait));
    tableSet(vm, &vm->currentNamespace->values, AS_STRING(vm->stack[0]), vm->stack[1]);
    pop(vm);
    pop(vm);
    return nativeTrait;
}

ObjNamespace* defineNativeNamespace(VM* vm, const char* name, ObjNamespace* enclosing) {
    ObjString* shortName = newString(vm, name);
    push(vm, OBJ_VAL(shortName));
    ObjNamespace* nativeNamespace = newNamespace(vm, shortName, enclosing);
    push(vm, OBJ_VAL(nativeNamespace));
    tableSet(vm, &vm->namespaces, nativeNamespace->fullName, OBJ_VAL(nativeNamespace));
    tableSet(vm, &enclosing->values, nativeNamespace->shortName, OBJ_VAL(nativeNamespace));
    pop(vm);
    pop(vm);
    return nativeNamespace;
}

ObjClass* defineNativeException(VM* vm, const char* name, ObjClass* superClass) {
    ObjClass* exceptionClass = defineNativeClass(vm, name);
    bindSuperclass(vm, exceptionClass, superClass);
    return exceptionClass;
}

ObjClass* getNativeClass(VM* vm, const char* fullName) {
    Value klass;
    tableGet(&vm->classes, newString(vm, fullName), &klass);
    if (!IS_CLASS(klass)) {
        runtimeError(vm, "Class %s is undefined.", fullName);
        exit(70);
    }
    return AS_CLASS(klass);
}

ObjNativeFunction* getNativeFunction(VM* vm, const char* name) {
    Value function;
    tableGet(&vm->rootNamespace->values, newString(vm, name), &function);
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

ObjNamespace* getNativeNamespace(VM* vm, const char* name) {
    Value namespace;
    tableGet(&vm->namespaces, newString(vm, name), &namespace);
    if (!IS_NAMESPACE(namespace)) {
        runtimeError(vm, "Namespace %s is undefined.", name);
        exit(70);
    }
    return AS_NAMESPACE(namespace);
}

void loadSourceFile(VM* vm, const char* filePath) {
    char* source = readFile(filePath);
    interpret(vm, source);
    free(source);
}

void registerNativeFunctions(VM* vm){
    DEF_FUNCTION(assert, 2);
    DEF_FUNCTION(clock, 0);
    DEF_FUNCTION(error, 1);
    DEF_FUNCTION(gc, 0);
    DEF_FUNCTION(print, 1);
    DEF_FUNCTION(println, 1);
    DEF_FUNCTION(read, 0);
}