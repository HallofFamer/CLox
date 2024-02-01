#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interceptor.h"
#include "klass.h"
#include "vm.h"

void handleInterceptorMethod(VM* vm, ObjClass* klass, ObjString* name) {
    if (name->length <= 2 || name->chars[0] != '_' || name->chars[1] != '_') return;

    if (strcmp(name->chars, "__beforeGet__") == 0) SET_CLASS_INTERCEPTOR(klass, INTERCEPTOR_BEFORE_GET);
    else if (strcmp(name->chars, "__afterGet__") == 0) SET_CLASS_INTERCEPTOR(klass, INTERCEPTOR_AFTER_GET);
    else if (strcmp(name->chars, "__undefinedGet__") == 0) SET_CLASS_INTERCEPTOR(klass, INTERCEPTOR_UNDEFINED_GET);
    else if (strcmp(name->chars, "__beforeSet__") == 0) SET_CLASS_INTERCEPTOR(klass, INTERCEPTOR_BEFORE_SET);
    else if (strcmp(name->chars, "__afterSet__") == 0) SET_CLASS_INTERCEPTOR(klass, INTERCEPTOR_AFTER_SET);
    else if (strcmp(name->chars, "__undefinedInvoke__") == 0) SET_CLASS_INTERCEPTOR(klass, INTERCEPTOR_UNDEFINED_INVOKE);
}

bool interceptBeforeGet(VM* vm, Value receiver, ObjString* name) {
    ObjClass* klass = getObjClass(vm, receiver);
    Value interceptor;
    if (tableGet(&klass->methods, newString(vm, "__beforeGet__"), &interceptor)) {
        callReentrant(vm, receiver, interceptor, OBJ_VAL(name));
        return true;
    }
    return false;
}

bool interceptAfterGet(VM* vm, Value receiver, ObjString* name, Value value) {
    ObjClass* klass = getObjClass(vm, receiver);
    Value interceptor;
    if (tableGet(&klass->methods, newString(vm, "__afterGet__"), &interceptor)) {
        Value result = callReentrant(vm, receiver, interceptor, OBJ_VAL(name), value);
        push(vm, result);
        return true;
    }
    return false;
}

bool interceptUndefinedGet(VM* vm, Value receiver, ObjString* name) {
    ObjClass* klass = getObjClass(vm, receiver);
    Value interceptor;
    if (tableGet(&klass->methods, newString(vm, "__undefinedGet__"), &interceptor)) {
        callReentrant(vm, receiver, interceptor, OBJ_VAL(name));
        return true;
    }
    return false;
}

bool interceptBeforeSet(VM* vm, Value receiver, ObjString* name, Value value) {
    ObjClass* klass = getObjClass(vm, receiver);
    Value interceptor;
    if (tableGet(&klass->methods, newString(vm, "__beforeSet__"), &interceptor)) {
        Value result = callReentrant(vm, receiver, interceptor, OBJ_VAL(name), value);
        push(vm, result);
        return true;
    }
    return false;
}

bool interceptAfterSet(VM* vm, Value receiver, ObjString* name) {
    ObjClass* klass = getObjClass(vm, receiver);
    Value interceptor;
    if (tableGet(&klass->methods, newString(vm, "__afterSet__"), &interceptor)) {
        callReentrant(vm, receiver, interceptor, OBJ_VAL(name));
        return true;
    }
    return false;
}

bool interceptUndefinedInvoke(VM* vm, ObjClass* klass, ObjString* name, int argCount) {
    Value interceptor;
    if (tableGet(&klass->methods, newString(vm, "__undefinedInvoke__"), &interceptor)) {
        ObjArray* args = newArray(vm);
        push(vm, OBJ_VAL(args));
        for (int i = argCount; i > 0; i--) {
            valueArrayWrite(vm, &args->elements, vm->stackTop[-i - 1]);
        }
        pop(vm);

        vm->stackTop -= argCount;
        push(vm, OBJ_VAL(name));
        push(vm, OBJ_VAL(args));
        return callMethod(vm, interceptor, 2);
    }
    return false;
}