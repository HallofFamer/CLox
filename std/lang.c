#include <stdio.h>
#include <stdlib.h>

#include "lang.h"
#include "../assert.h"
#include "../native.h"
#include "../object.h"
#include "../vm.h"

LOX_METHOD(Bool, clone) {
	assertArgCount(vm, "Bool::clone()", 0, argCount);
	RETURN_BOOL(receiver);
}

LOX_METHOD(Bool, init) {
	assertError(vm, "Cannot instantiate from class Bool.");
	RETURN_NIL;
}

LOX_METHOD(Bool, toString) {
	assertArgCount(vm, "Bool::toString()", 0, argCount);
	if (AS_BOOL(receiver)) RETURN_STRING("true");
	else RETURN_STRING("false");
}

LOX_METHOD(Nil, clone) {
	assertArgCount(vm, "Nil::clone()", 0, argCount);
	RETURN_NIL;
}

LOX_METHOD(Nil, init) {
	assertError(vm, "Cannot instantiate from class Nil.");
	RETURN_NIL;
}

LOX_METHOD(Nil, toString) {
	assertArgCount(vm, "Nil::toString()", 0, argCount);
	RETURN_STRING("nil");
}

LOX_METHOD(Number, clone) {
	assertArgCount(vm, "Number::clone()", 0, argCount);
	RETURN_NUMBER((double)receiver);
}

LOX_METHOD(Number, init) {
	assertError(vm, "Cannot instantiate from class Number.");
	RETURN_NIL;
}

LOX_METHOD(Number, toString) {
	assertArgCount(vm, "Number::toString()", 0, argCount);
	char chars[24];
	int length = sprintf_s(chars, 24, "%.14g", AS_NUMBER(receiver));
	RETURN_STRING(chars);
}

LOX_METHOD(Object, clone) {
	assertArgCount(vm, "Object::clone()", 0, argCount);
	ObjInstance* thisObject = AS_INSTANCE(receiver);
	ObjInstance* thatObject = newInstance(vm, thisObject->klass);
	tableAddAll(vm, &thisObject->fields, &thatObject->fields);
	RETURN_OBJ(thatObject);
}

LOX_METHOD(Object, equals) {
	assertArgCount(vm, "Object::equals(value)", 1, argCount);
	RETURN_BOOL(valuesEqual(receiver, args[0]));
}

LOX_METHOD(Object, getClass) {
	assertArgCount(vm, "Object::getClass()", 0, argCount);
	RETURN_OBJ(getObjClass(vm, receiver));
}

LOX_METHOD(Object, getClassName) {
	assertArgCount(vm, "Object::getClassName()", 0, argCount);
	RETURN_OBJ(getObjClass(vm, receiver)->name);
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
	ObjClass* thisClass = getObjClass(vm, receiver);
	ObjClass* thatClass = AS_CLASS(args[0]);
	if (thisClass == thatClass) RETURN_TRUE;

	ObjClass* superclass = thisClass->superclass;
	while (superclass != NULL) {
		if (superclass == thatClass) RETURN_TRUE;
		superclass = superclass->superclass;
	}
	RETURN_FALSE;
}

LOX_METHOD(Object, memberOf) {
	assertArgCount(vm, "Object::memberOf(class)", 1, argCount);
	if (!IS_CLASS(args[0])) RETURN_FALSE;
	ObjClass* thisClass = getObjClass(vm, receiver);
	ObjClass* thatClass = AS_CLASS(args[0]);
	RETURN_BOOL(thisClass == thatClass);
}

LOX_METHOD(Object, toString) {
	assertArgCount(vm, "Object::toString()", 0, argCount);
	RETURN_STRING("object");
}

void registerLangPackage(VM* vm){
	vm->objectClass = defineNativeClass(vm, "Object");
	DEF_METHOD(vm->objectClass, Object, clone);
	DEF_METHOD(vm->objectClass, Object, equals);
	DEF_METHOD(vm->objectClass, Object, getClass);
	DEF_METHOD(vm->objectClass, Object, getClassName);
	DEF_METHOD(vm->objectClass, Object, hasField);
	DEF_METHOD(vm->objectClass, Object, instanceOf);
	DEF_METHOD(vm->objectClass, Object, memberOf);
	DEF_METHOD(vm->objectClass, Object, toString);

	vm->nilClass = defineNativeClass(vm, "Nil");
	bindSuperclass(vm, vm->nilClass, vm->objectClass);
	DEF_METHOD(vm->nilClass, Nil, clone);
	DEF_METHOD(vm->nilClass, Nil, init);
	DEF_METHOD(vm->nilClass, Nil, toString);

	vm->boolClass = defineNativeClass(vm, "Bool");
	bindSuperclass(vm, vm->boolClass, vm->objectClass);
	DEF_METHOD(vm->boolClass, Bool, clone);
	DEF_METHOD(vm->boolClass, Bool, init);
	DEF_METHOD(vm->boolClass, Bool, toString);

	vm->numberClass = defineNativeClass(vm, "Number");
	bindSuperclass(vm, vm->numberClass, vm->objectClass);
	DEF_METHOD(vm->numberClass, Number, clone);
	DEF_METHOD(vm->numberClass, Number, init);
	DEF_METHOD(vm->numberClass, Number, toString);
}