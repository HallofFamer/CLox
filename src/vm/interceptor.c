#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "class.h"
#include "interceptor.h"
#include "vm.h"

static bool isInterceptorMethod(ObjString* name) {
    return (name != NULL && name->length > 4 && 
        name->chars[0] == '_' && name->chars[1] == '_' && 
        name->chars[name->length - 1] == '_' && name->chars[name->length - 2] == '_');
}

void handleInterceptorMethod(VM* vm, ObjClass* klass, ObjString* name) {
    if (!isInterceptorMethod(name)) return;

    else if (strcmp(name->chars, "__init__") == 0) SET_CLASS_INTERCEPTOR(klass, INTERCEPTOR_INIT);
    else if (strcmp(name->chars, "__beforeGet__") == 0) SET_CLASS_INTERCEPTOR(klass, INTERCEPTOR_BEFORE_GET);
    else if (strcmp(name->chars, "__afterGet__") == 0) SET_CLASS_INTERCEPTOR(klass, INTERCEPTOR_AFTER_GET);
    else if (strcmp(name->chars, "__beforeSet__") == 0) SET_CLASS_INTERCEPTOR(klass, INTERCEPTOR_BEFORE_SET);
    else if (strcmp(name->chars, "__afterSet__") == 0) SET_CLASS_INTERCEPTOR(klass, INTERCEPTOR_AFTER_SET);
    else if (strcmp(name->chars, "__onInvoke__") == 0) SET_CLASS_INTERCEPTOR(klass, INTERCEPTOR_ON_INVOKE);
    else if (strcmp(name->chars, "__onReturn__") == 0) SET_CLASS_INTERCEPTOR(klass, INTERCEPTOR_ON_RETURN);
    else if (strcmp(name->chars, "__onThrow__") == 0) SET_CLASS_INTERCEPTOR(klass, INTERCEPTOR_ON_THROW);
    else if (strcmp(name->chars, "__onYield__") == 0) SET_CLASS_INTERCEPTOR(klass, INTERCEPTOR_ON_YIELD);
    else if (strcmp(name->chars, "__onAwait__") == 0) SET_CLASS_INTERCEPTOR(klass, INTERCEPTOR_ON_AWAIT);
    else if (strcmp(name->chars, "__undefinedGet__") == 0) SET_CLASS_INTERCEPTOR(klass, INTERCEPTOR_UNDEFINED_GET);
    else if (strcmp(name->chars, "__undefinedInvoke__") == 0) SET_CLASS_INTERCEPTOR(klass, INTERCEPTOR_UNDEFINED_INVOKE);
    else {
        runtimeError(vm, "Invalid interceptor method specified.");
        exit(70);
    }
}

bool hasInterceptableMethod(VM* vm, Value receiver, ObjString* name) {
    if (name == NULL || name == vm->initString) return false;
    ObjClass* klass = getObjClass(vm, receiver);
    Value method;
    return tableGet(&klass->methods, name, &method) && !isInterceptorMethod(name);
}

bool interceptBeforeGet(VM* vm, Value receiver, ObjString* name) {
    ObjClass* klass = getObjClass(vm, receiver);
    Value interceptor;
    if (tableGet(&klass->methods, newStringPerma(vm, "__beforeGet__"), &interceptor)) {
        callReentrantMethod(vm, receiver, interceptor, OBJ_VAL(name));
        return true;
    }
    return false;
}

bool interceptAfterGet(VM* vm, Value receiver, ObjString* name, Value value) {
    ObjClass* klass = getObjClass(vm, receiver);
    Value interceptor;
    if (tableGet(&klass->methods, newStringPerma(vm, "__afterGet__"), &interceptor)) {
        Value result = callReentrantMethod(vm, receiver, interceptor, OBJ_VAL(name), value);
        push(vm, result);
        return true;
    }
    return false;
}

bool interceptBeforeSet(VM* vm, Value receiver, ObjString* name, Value value) {
    ObjClass* klass = getObjClass(vm, receiver);
    Value interceptor;
    if (tableGet(&klass->methods, newStringPerma(vm, "__beforeSet__"), &interceptor)) {
        Value result = callReentrantMethod(vm, receiver, interceptor, OBJ_VAL(name), value);
        push(vm, result);
        return true;
    }
    return false;
}

bool interceptAfterSet(VM* vm, Value receiver, ObjString* name) {
    ObjClass* klass = getObjClass(vm, receiver);
    Value interceptor;
    if (tableGet(&klass->methods, newStringPerma(vm, "__afterSet__"), &interceptor)) {
        callReentrantMethod(vm, receiver, interceptor, OBJ_VAL(name));
        return true;
    }
    return false;
}

static ObjArray* loadInterceptorArguments(VM* vm, int argCount) {
    ObjArray* args = newArray(vm);
    push(vm, OBJ_VAL(args));
    for (int i = argCount; i > 0; i--) {
        valueArrayWrite(vm, &args->elements, vm->stackTop[-i - 1]);
    }
    pop(vm);
    vm->stackTop -= argCount;
    return args;
}

static void unloadInterceptorArguments(VM* vm, ObjArray* args) {
    for (int i = 0; i < args->elements.count; i++) {
        push(vm, args->elements.values[i]);
    }
}

bool interceptOnInvoke(VM* vm, Value receiver, ObjString* name, int argCount) {
    ObjClass* klass = getObjClass(vm, receiver);
    Value interceptor;
    if (tableGet(&klass->methods, newStringPerma(vm, "__onInvoke__"), &interceptor)) {
        ObjArray* args = loadInterceptorArguments(vm, argCount);
        callReentrantMethod(vm, receiver, interceptor, OBJ_VAL(name), OBJ_VAL(args));
        unloadInterceptorArguments(vm, args);
        return true;
    }
    return false;
}

bool interceptOnReturn(VM* vm, Value receiver, ObjString* name, Value result) {
    ObjClass* klass = getObjClass(vm, receiver);
    Value interceptor;
    if (tableGet(&klass->methods, newStringPerma(vm, "__onReturn__"), &interceptor)) {
        Value result2 = callReentrantMethod(vm, receiver, interceptor, OBJ_VAL(name), result);
        push(vm, result2);
        return true;
    }
    return false;
}

bool interceptOnThrow(VM* vm, Value receiver, ObjString* name, Value exception) {
    ObjClass* klass = getObjClass(vm, receiver);
    Value interceptor;
    if (tableGet(&klass->methods, newStringPerma(vm, "__onThrow__"), &interceptor)) {
        Value exception2 = callReentrantMethod(vm, receiver, interceptor, OBJ_VAL(name), exception);
        push(vm, exception2);
        return true;
    }
    return false;
}

bool interceptOnYield(VM* vm, Value receiver, ObjString* name, Value result) {
    ObjClass* klass = getObjClass(vm, receiver);
    Value interceptor;
    if (tableGet(&klass->methods, newStringPerma(vm, "__onYield__"), &interceptor)) {
        Value result2 = callReentrantMethod(vm, receiver, interceptor, OBJ_VAL(name), result);
        pop(vm);
        push(vm, result2);
        return true;
    }
    return false;
}

bool interceptOnAwait(VM* vm, Value receiver, ObjString* name, Value result) {
    ObjClass* klass = getObjClass(vm, receiver);
    Value interceptor;
    if (tableGet(&klass->methods, newStringPerma(vm, "__onAwait__"), &interceptor)) {
        Value result2 = callReentrantMethod(vm, receiver, interceptor, OBJ_VAL(name), result);
        pop(vm);
        if (!IS_PROMISE(result2)) result2 = OBJ_VAL(promiseWithFulfilled(vm, result));
        push(vm, result2);
        return true;
    }
    return false;
}

bool interceptUndefinedGet(VM* vm, Value receiver, ObjString* name) {
    ObjClass* klass = getObjClass(vm, receiver);
    Value interceptor;
    if (tableGet(&klass->methods, newStringPerma(vm, "__undefinedGet__"), &interceptor)) {
        callReentrantMethod(vm, receiver, interceptor, OBJ_VAL(name));
        return true;
    }
    return false;
}

bool interceptUndefinedInvoke(VM* vm, ObjClass* klass, ObjString* name, int argCount) {
    Value interceptor;
    if (tableGet(&klass->methods, newStringPerma(vm, "__undefinedInvoke__"), &interceptor)) {
        ObjArray* args = loadInterceptorArguments(vm, argCount);
        push(vm, OBJ_VAL(name));
        push(vm, OBJ_VAL(args));
        return callMethod(vm, interceptor, 2);
    }
    return false;
}