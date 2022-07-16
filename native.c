#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "assert.h"
#include "memory.h"
#include "native.h"
#include "string.h"
#include "vm.h"

LOX_FUNCTION(clock){
    assertArgCount(vm, "clock()", 0, argCount);
    RETURN_NUMBER((double)clock() / CLOCKS_PER_SEC);
}

LOX_FUNCTION(error){
    assertArgCount(vm, "error(message)", 1, argCount);
    assertArgIsString(vm, "error(message)", args, 0);
    runtimeError(vm, AS_CSTRING(args[0]));
    exit(70);
    RETURN_NIL;
}

LOX_FUNCTION(gc) {
    assertArgCount(vm, "gc()", 0, argCount);
    collectGarbage(vm);
    RETURN_NIL;
}

LOX_FUNCTION(print){
    assertArgCount(vm, "print(message)", 1, argCount);
    printValue(args[0]);
    RETURN_NIL;
}

LOX_FUNCTION(println){
    assertArgCount(vm, "println(message)", 1, argCount);
    printValue(args[0]);
    printf("\n");
    RETURN_NIL;
}

ObjClass* defineNativeClass(VM* vm, const char* name) {
    ObjString* className = copyString(vm, name, (int)strlen(name));
    push(vm, OBJ_VAL(className));
    ObjClass* nativeClass = newClass(vm, className);
    nativeClass->isNative = true;
    push(vm, OBJ_VAL(nativeClass));
    tableSet(vm, &vm->globals, AS_STRING(vm->stack[0]), vm->stack[1]);
    pop(vm);
    pop(vm);
    return nativeClass;
}

void defineNativeFunction(VM* vm, const char* name, int arity, NativeFn function) {
    ObjString* functionName = copyString(vm, name, (int)strlen(name));
    push(vm, OBJ_VAL(functionName));
    push(vm, OBJ_VAL(newNativeFunction(vm, functionName, arity, function)));
    tableSet(vm, &vm->globals, AS_STRING(vm->stack[0]), vm->stack[1]);
    pop(vm);
    pop(vm);
}

void defineNativeMethod(VM* vm, ObjClass* klass, const char* name, int arity, NativeMethod method) {
    ObjString* methodName = copyString(vm, name, (int)strlen(name));
    push(vm, OBJ_VAL(methodName));
    ObjNativeMethod* nativeMethod = newNativeMethod(vm, klass, methodName, arity, method);
    push(vm, OBJ_VAL(nativeMethod));
    tableSet(vm, &klass->methods, methodName, OBJ_VAL(nativeMethod));
    pop(vm);
    pop(vm);
}

void registerNativeFunctions(VM* vm){
    DEF_FUNCTION(clock, 0);
    DEF_FUNCTION(error, 1);
    DEF_FUNCTION(gc, 0);
    DEF_FUNCTION(print, 1);
    DEF_FUNCTION(println, 1);
}