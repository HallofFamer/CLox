#include <stdio.h>
#include <stdlib.h>

#include "lang.h"
#include "../native.h"
#include "../object.h"
#include "../vm.h"

static Value equalsNativeMethod(VM* vm, Value receiver, int argCount, Value* args) {
	if (argCount != 1) {
		runtimeError(vm, "method Object::equals() expects 1 argument but got %d.", argCount);
		exit(70);
	}
	return BOOL_VAL(valuesEqual(receiver, args[0]));
}

static Value getClassNativeMethod(VM* vm, Value receiver, int argCount, Value* args) {
	if (argCount != 0) {
		runtimeError(vm, "method Object::getClass() expects 0 argument but got %d.", argCount);
		exit(70);
	}

	return OBJ_VAL(AS_INSTANCE(receiver)->klass);
}

static Value getClassNameNativeMethod(VM* vm, Value receiver, int argCount, Value* args) {
	if (argCount != 0) {
		runtimeError(vm, "method Object::getClassName() expects 0 argument but got %d.", argCount);
		exit(70);
	}

	return OBJ_VAL(AS_INSTANCE(receiver)->klass->name);
}

static Value hasFieldNativeMethod(VM* vm, Value receiver, int argCount, Value* args) {
	if (argCount != 1) {
		runtimeError(vm, "method Object::hasField() expects 1 argument but got %d.", argCount);
		exit(70);
	}

	if (!IS_STRING(args[0])) {
		runtimeError(vm, "method Object::hasField() expects argument 1 to be string.");
		exit(70);
	}

	if (!IS_INSTANCE(receiver)) return false;
	ObjInstance* instance = AS_INSTANCE(receiver);
	Value value;
	return BOOL_VAL(tableGet(&instance->fields, AS_STRING(args[0]), &value));
}

static Value toStringNativeMethod(VM* vm, Value receiver, int argCount, Value* args) {
	if (argCount > 0) {
		runtimeError(vm, "method Object::equals() expects 1 argument but got %d.", argCount);
		exit(70);
	}

	if (IS_BOOL(receiver)) {
		return OBJ_VAL(AS_BOOL(args[0]) ? copyString(vm, "true", 4) : copyString(vm, "false", 5));
	}
	else if (IS_NIL(receiver)) {
		return OBJ_VAL(copyString(vm, "nil", 3));
	}
	else if (IS_NUMBER(receiver)) {
		char chars[24];
		int length = sprintf_s(chars, 24, "%.14g", AS_NUMBER(args[0]));
		return OBJ_VAL(copyString(vm, chars, length));
	}
	else {
		return OBJ_VAL(copyString(vm, "Object", 6));
	}
}

void registerLangPackage(VM* vm){
	ObjClass* objectClass = defineNativeClass(vm, "Object");
	defineNativeMethod(vm, objectClass, "equals", equalsNativeMethod);
	defineNativeMethod(vm, objectClass, "getClass", getClassNativeMethod);
	defineNativeMethod(vm, objectClass, "getClassName", getClassNameNativeMethod);
	defineNativeMethod(vm, objectClass, "hasField", hasFieldNativeMethod);
	defineNativeMethod(vm, objectClass, "toString", toStringNativeMethod);
}