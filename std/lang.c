#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "lang.h"
#include "../assert.h"
#include "../hash.h"
#include "../native.h"
#include "../object.h"
#include "../vm.h"

LOX_METHOD(Bool, clone) {
	assertArgCount(vm, "Bool::clone()", 0, argCount);
	RETURN_BOOL(receiver);
}

LOX_METHOD(Bool, init) {
	raiseError(vm, "Cannot instantiate from class Bool.");
	RETURN_NIL;
}

LOX_METHOD(Bool, toString) {
	assertArgCount(vm, "Bool::toString()", 0, argCount);
	if (AS_BOOL(receiver)) RETURN_STRING("true", 4);
	else RETURN_STRING("false", 5);
}

LOX_METHOD(Nil, clone) {
	assertArgCount(vm, "Nil::clone()", 0, argCount);
	RETURN_NIL;
}

LOX_METHOD(Nil, init) {
	raiseError(vm, "Cannot instantiate from class Nil.");
	RETURN_NIL;
}

LOX_METHOD(Nil, toString) {
	assertArgCount(vm, "Nil::toString()", 0, argCount);
	RETURN_STRING("nil", 3);
}

LOX_METHOD(Number, abs) {
	assertArgCount(vm, "Number::abs()", 0, argCount);
	RETURN_NUMBER(fabs(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, cbrt) {
	assertArgCount(vm, "Number::cbrt()", 0, argCount);
	RETURN_NUMBER(cbrt(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, ceil) {
	assertArgCount(vm, "Number::ceil()", 0, argCount);
	RETURN_NUMBER(ceil(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, clone) {
	assertArgCount(vm, "Number::clone()", 0, argCount);
	RETURN_NUMBER((double)receiver);
}

LOX_METHOD(Number, exp) {
	assertArgCount(vm, "Number::exp()", 0, argCount);
	RETURN_NUMBER(exp(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, floor) {
	assertArgCount(vm, "Number::floor()", 0, argCount);
	RETURN_NUMBER(floor(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, init) {
	raiseError(vm, "Cannot instantiate from class Number.");
	RETURN_NIL;
}

LOX_METHOD(Number, log) {
	assertArgCount(vm, "Number::log()", 0, argCount);
	double self = AS_NUMBER(receiver);
	assertPositiveNumber(vm, "Number::log2()", self, -1);
	RETURN_NUMBER(log(self));
}

LOX_METHOD(Number, log10) {
	assertArgCount(vm, "Number::log10()", 0, argCount);
	double self = AS_NUMBER(receiver);
	assertPositiveNumber(vm, "Number::log10()", self, -1);
	RETURN_NUMBER(log10(self));
}

LOX_METHOD(Number, log2) {
	assertArgCount(vm, "Number::log2()", 0, argCount);
	double self = AS_NUMBER(receiver);
	assertPositiveNumber(vm, "Number::log2()", self, -1);
	RETURN_NUMBER(log2(self));
}

LOX_METHOD(Number, max) {
	assertArgCount(vm, "Number::max(other)", 1, argCount);
	assertArgIsNumber(vm, "Number::max(other)", args, 0);
	RETURN_NUMBER(fmax(AS_NUMBER(receiver), AS_NUMBER(args[0])));
}

LOX_METHOD(Number, min) {
	assertArgCount(vm, "Number::min(other)", 1, argCount);
	assertArgIsNumber(vm, "Number::min(other)", args, 0);
	RETURN_NUMBER(fmin(AS_NUMBER(receiver), AS_NUMBER(args[0])));
}

LOX_METHOD(Number, pow) {
	assertArgCount(vm, "Number::pow(exponent)", 1, argCount);
	assertArgIsNumber(vm, "Number::pow(exponent)", args, 0);
	RETURN_NUMBER(pow(AS_NUMBER(receiver), AS_NUMBER(args[0])));
}

LOX_METHOD(Number, round) {
	assertArgCount(vm, "Number::round()", 0, argCount);
	RETURN_NUMBER(round(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, sqrt) {
	assertArgCount(vm, "Number::sqrt()", 0, argCount);
	double self = AS_NUMBER(receiver);
	assertPositiveNumber(vm, "Number::sqrt()", self, -1);
	RETURN_NUMBER(sqrt(self));
}

LOX_METHOD(Number, toString) {
	assertArgCount(vm, "Number::toString()", 0, argCount);
	char chars[24];
	int length = sprintf_s(chars, 24, "%.14g", AS_NUMBER(receiver));
	RETURN_STRING(chars, length);
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

LOX_METHOD(Object, hashCode) {
	assertArgCount(vm, "Object::hashCode()", 0, argCount);
	RETURN_NUMBER(hashValue(receiver));
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
	RETURN_STRING_FMT("[object %s]", AS_INSTANCE(receiver)->klass->name->chars);
}

void registerLangPackage(VM* vm){
	vm->objectClass = defineNativeClass(vm, "Object");
	DEF_METHOD(vm->objectClass, Object, clone);
	DEF_METHOD(vm->objectClass, Object, equals);
	DEF_METHOD(vm->objectClass, Object, getClass);
	DEF_METHOD(vm->objectClass, Object, getClassName);
	DEF_METHOD(vm->objectClass, Object, hasField);
	DEF_METHOD(vm->objectClass, Object, hashCode);
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
	DEF_METHOD(vm->numberClass, Number, abs);
	DEF_METHOD(vm->numberClass, Number, cbrt);
	DEF_METHOD(vm->numberClass, Number, ceil);
	DEF_METHOD(vm->numberClass, Number, clone);
	DEF_METHOD(vm->numberClass, Number, exp);
	DEF_METHOD(vm->numberClass, Number, floor);
	DEF_METHOD(vm->numberClass, Number, init);
	DEF_METHOD(vm->numberClass, Number, log);
	DEF_METHOD(vm->numberClass, Number, log2);
	DEF_METHOD(vm->numberClass, Number, log10);
	DEF_METHOD(vm->numberClass, Number, max);
	DEF_METHOD(vm->numberClass, Number, min);
	DEF_METHOD(vm->numberClass, Number, pow);
	DEF_METHOD(vm->numberClass, Number, round);
	DEF_METHOD(vm->numberClass, Number, sqrt);
	DEF_METHOD(vm->numberClass, Number, toString);
}