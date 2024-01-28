#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interceptor.h"
#include "klass.h"
#include "vm.h"

bool interceptUndefinedGet(VM* vm, ObjClass* klass, ObjString* name) {
    Value interceptor;
    if (tableGet(&klass->methods, newString(vm, "__undefinedGet__"), &interceptor)) {
        push(vm, OBJ_VAL(name));
        return callMethod(vm, interceptor, 1);
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