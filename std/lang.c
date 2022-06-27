#include <stdio.h>
#include <stdlib.h>

#include "lang.h"
#include "../assert.h"
#include "../native.h"
#include "../object.h"
#include "../vm.h"

LOX_METHOD(Object, equals) {
	assertArgCount(vm, "Object::equals(value)", 1, argCount);
	RETURN_BOOL(valuesEqual(receiver, args[0]));
}

LOX_METHOD(Object, getClass) {
	assertArgCount(vm, "Object::getClass()", 0, argCount);
	RETURN_OBJ(AS_INSTANCE(receiver)->klass);
}

LOX_METHOD(Object, getClassName) {
	assertArgCount(vm, "Object::getClassName()", 0, argCount);
	RETURN_OBJ(AS_INSTANCE(receiver)->klass->name);
}

LOX_METHOD(Object, hasField) {
	assertArgCount(vm, "Object::hasField(field)", 1, argCount);
	assertArgIsString(vm, "Object::hasField(field)", args, 0);
	if (!IS_INSTANCE(receiver)) return false;
	ObjInstance* instance = AS_INSTANCE(receiver);
	Value value;
	RETURN_BOOL(tableGet(&instance->fields, AS_STRING(args[0]), &value));
}

LOX_METHOD(Object, instanceOf) {
	assertArgCount(vm, "Object::instanceOf(class)", 1, argCount);
	if (!IS_CLASS(args[0])) RETURN_FALSE;
	ObjClass* thisClass = AS_INSTANCE(receiver)->klass;
	ObjClass* thatClass = AS_CLASS(args[0]);
	RETURN_BOOL(thisClass == thatClass);
}

LOX_METHOD(Object, memberOf) {
	assertArgCount(vm, "Object::memberOf(class)", 1, argCount);
	if (!IS_CLASS(args[0])) RETURN_FALSE;
	ObjClass* thisClass = AS_INSTANCE(receiver)->klass;
	ObjClass* thatClass = AS_CLASS(args[0]);
	RETURN_BOOL(thisClass == thatClass);
}

LOX_METHOD(Object, toString) {
	assertArgCount(vm, "Object::toString()", 0, argCount);

	if (IS_BOOL(receiver)) {
		if (AS_BOOL(receiver)) RETURN_STRING("true", 4);
		else RETURN_STRING("false", 5);
	}
	else if (IS_NIL(receiver)) {
		RETURN_STRING("nil", 3);
	}
	else if (IS_NUMBER(receiver)) {
		char chars[24];
		int length = sprintf_s(chars, 24, "%.14g", AS_NUMBER(args[0]));
		RETURN_STRING(chars, length);
	}
	else {
		RETURN_STRING("object", 6);
	}
}

void registerLangPackage(VM* vm){
	ObjClass* objectClass = defineNativeClass(vm, "Object");
	DEF_METHOD(objectClass, Object, equals);
	DEF_METHOD(objectClass, Object, getClass);
	DEF_METHOD(objectClass, Object, getClassName);
	DEF_METHOD(objectClass, Object, hasField);
	DEF_METHOD(objectClass, Object, instanceOf);
	DEF_METHOD(objectClass, Object, memberOf);
	DEF_METHOD(objectClass, Object, toString);
	vm->objectClass = objectClass;
}