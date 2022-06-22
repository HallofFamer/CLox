#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "native.h"
#include "object.h"
#include "vm.h"

static Value clockNativeFunction(VM* vm, int argCount, Value* args) {
	if (argCount > 0) {
		runtimeError(vm, "native function clock() expects 0 argument but got %d.", argCount);
		return NIL_VAL;
	}
	return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static Value errorNativeFunction(VM* vm, int argCount, Value* args) {
	if (argCount != 1) {
		runtimeError(vm, "native function error() expects 1 argument but got %d.", argCount);
	}
	else if (!IS_STRING(args[0])) {
		runtimeError(vm, "native function error() expects argument 1 to be a string.");
	}
	else {
		runtimeError(vm, AS_CSTRING(args[0]));
		exit(70);
	}
	return NIL_VAL;
}

static Value printNativeFunction(VM* vm, int argCount, Value* args) {
	if (argCount != 1) {
		runtimeError(vm, "native function print() expects 1 argument but got %d.", argCount);
		return NIL_VAL;
	}

	printValue(args[0]);
	return NIL_VAL;
}

static Value printlnNativeFunction(VM* vm, int argCount, Value* args) {
	if (argCount != 1) {
		runtimeError(vm, "native function print() expects 1 argument but got %d.", argCount);
		return NIL_VAL;
	}

	printValue(args[0]);
	printf("\n");
	return NIL_VAL;
}

static Value toStringNativeFunction(VM* vm, int argCount, Value* args) {
	if (IS_BOOL(args[0])) {
		return OBJ_VAL(AS_BOOL(args[0]) ? copyString(vm, "true", 4) : copyString(vm, "false", 5));
	}
	else if (IS_NIL(args[0])) {
		return OBJ_VAL(copyString(vm, "nil", 3));
	}
	else if (IS_NUMBER(args[0])) {
		char chars[24];
		int length = sprintf_s(chars, 24, "%.14g", AS_NUMBER(args[0]));
		return OBJ_VAL(copyString(vm, chars, length));
	}
	else {
		return OBJ_VAL(copyString(vm, "object", 6));
	}
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

void defineNativeFunction(VM* vm, const char* name, NativeFn function) {
	push(vm, OBJ_VAL(copyString(vm, name, (int)strlen(name))));
	push(vm, OBJ_VAL(newNativeFunction(vm, function)));
	tableSet(vm, &vm->globals, AS_STRING(vm->stack[0]), vm->stack[1]);
	pop(vm);
	pop(vm);
}

void defineNativeMethod(VM* vm, ObjClass* klass, const char* name, NativeMethod method) {
	ObjNativeMethod* nativeMethod = newNativeMethod(vm, method);
	push(vm, OBJ_VAL(nativeMethod));
	ObjString* methodName = copyString(vm, name, (int)strlen(name));
	push(vm, OBJ_VAL(methodName));
	tableSet(vm, &klass->methods, methodName, OBJ_VAL(nativeMethod));
	pop(vm);
	pop(vm);
}

void registerNativeFunctions(VM* vm){
	defineNativeFunction(vm, "clock", clockNativeFunction);
	defineNativeFunction(vm, "error", errorNativeFunction);
	defineNativeFunction(vm, "print", printNativeFunction);
	defineNativeFunction(vm, "println", printlnNativeFunction);
	defineNativeFunction(vm, "toString", toStringNativeFunction);
}