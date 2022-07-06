#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "assert.h"
#include "native.h"
#include "object.h"
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

LOX_FUNCTION(toString){
	assertArgCount(vm, "toString(value)", 1, argCount);

	if (IS_BOOL(args[0])) {
		if (AS_BOOL(args[0])) RETURN_STRING("true");
		else RETURN_STRING("false");
	}
	else if (IS_NIL(args[0])) {
		RETURN_STRING("nil");
	}
	else if (IS_NUMBER(args[0])) {
		char chars[24];
		int length = sprintf_s(chars, 24, "%.14g", AS_NUMBER(args[0]));
		RETURN_STRING(chars);
	}
	else {
		RETURN_STRING("object");
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
	DEF_FUNCTION(clock);
	DEF_FUNCTION(error);
	DEF_FUNCTION(print);
	DEF_FUNCTION(println);
	DEF_FUNCTION(toString);
}