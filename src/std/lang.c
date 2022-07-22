#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "lang.h"
#include "../vm/assert.h"
#include "../vm/hash.h"
#include "../vm/native.h"
#include "../vm/object.h"
#include "../vm/string.h"
#include "../vm/vm.h"

static int factorial(int self) {
    int result = 1;
    for (int i = 1; i <= self; i++) {
        result *= i;
    }
    return result;
}

static int gcd(int self, int other) {
    while (self != other) {
        if (self > other) self -= other;
        else other -= self;
    }
    return self;
}

static int lcm(int self, int other) {
    return (self * other) / gcd(self, other);
}

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

LOX_METHOD(Class, clone) {
    assertArgCount(vm, "Class::clone()", 0, argCount);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Class, getClass) {
    assertArgCount(vm, "Class::getClass()", 0, argCount);
    RETURN_OBJ(vm->classClass);
}

LOX_METHOD(Class, getClassName) {
    assertArgCount(vm, "Class::getClassName()", 0, argCount);
    RETURN_OBJ(vm->classClass->name);
}

LOX_METHOD(Class, init) {
    assertArgCount(vm, "Class::init(name, superclass)", 2, argCount);
    assertArgIsString(vm, "Class::init(name, superclass)", args, 0);
    assertArgIsClass(vm, "Class::init(name, superclass)", args, 1);
    ObjClass* klass = newClass(vm, AS_STRING(args[0]));
    bindSuperclass(vm, klass, AS_CLASS(args[1]));
    RETURN_OBJ(klass);
}

LOX_METHOD(Class, instanceOf) {
    assertArgCount(vm, "Class::instanceOf(class)", 1, argCount);
    if (!IS_CLASS(args[0])) RETURN_FALSE;
    ObjClass* klass = AS_CLASS(args[0]);
    if (klass == vm->classClass) RETURN_TRUE;
    else RETURN_FALSE;
}

LOX_METHOD(Class, memberOf) {
    assertArgCount(vm, "Class::memberOf(class)", 1, argCount);
    if (!IS_CLASS(args[0])) RETURN_FALSE;
    ObjClass* klass = AS_CLASS(args[0]);
    if (klass == vm->classClass) RETURN_TRUE;
    else RETURN_FALSE;
}

LOX_METHOD(Class, name) {
    assertArgCount(vm, "Class::name()", 0, argCount);
    RETURN_OBJ(AS_CLASS(receiver)->name);
}

LOX_METHOD(Class, superclass) {
    assertArgCount(vm, "Class::superclass()", 0, argCount);
    ObjClass* klass = AS_CLASS(receiver);
    if (klass->superclass == NULL) RETURN_NIL;
    RETURN_OBJ(klass->superclass);
}

LOX_METHOD(Class, toString) {
    assertArgCount(vm, "Class::toString()", 0, argCount);
    RETURN_STRING_FMT("<class %s>", AS_CLASS(receiver)->name->chars);
}

LOX_METHOD(Float, clone) {
    assertArgCount(vm, "Float::clone()", 0, argCount);
    RETURN_NUMBER((double)receiver);
}

LOX_METHOD(Float, init) {
    raiseError(vm, "Cannot instantiate from class Float.");
    RETURN_NIL;
}

LOX_METHOD(Float, toString) {
    assertArgCount(vm, "Float::toString()", 0, argCount);
    RETURN_STRING_FMT("%g", AS_FLOAT(receiver));
}

LOX_METHOD(Function, arity) {
    assertArgCount(vm, "Function::arity()", 0, argCount);
    if (IS_NATIVE_FUNCTION(receiver)) {
         RETURN_INT(AS_NATIVE_FUNCTION(receiver)->arity);
    }
    RETURN_INT(AS_CLOSURE(receiver)->function->arity);
}

LOX_METHOD(Function, clone) {
    assertArgCount(vm, "Function::clone()", 0, argCount);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Function, init) {
    raiseError(vm, "Cannot instantiate from class Function.");
    RETURN_NIL;
}

LOX_METHOD(Function, isNative) {
    assertArgCount(vm, "Function::isNative()", 0, argCount);
    RETURN_BOOL(IS_NATIVE_FUNCTION(receiver));
}

LOX_METHOD(Function, name) {
    assertArgCount(vm, "Function::name()", 0, argCount);
    if (IS_NATIVE_FUNCTION(receiver)) {
        RETURN_OBJ(AS_NATIVE_FUNCTION(receiver)->name);
    }
    RETURN_OBJ(AS_CLOSURE(receiver)->function->name);
}

LOX_METHOD(Function, toString) {
    assertArgCount(vm, "Function::toString()", 0, argCount);
    if (IS_NATIVE_FUNCTION(receiver)) {
        RETURN_STRING_FMT("<fn %s>", AS_NATIVE_FUNCTION(receiver)->name->chars);
    }
    RETURN_STRING_FMT("<fn %s>", AS_CLOSURE(receiver)->function->name->chars);
}

LOX_METHOD(Function, upvalueCount) {
    assertArgCount(vm, "Function::upvalueCount()", 0, argCount);
    RETURN_INT(AS_CLOSURE(receiver)->upvalueCount);
}

LOX_METHOD(Int, abs) {
    assertArgCount(vm, "Int::abs()", 0, argCount);
    RETURN_INT(abs(AS_INT(receiver)));
}

LOX_METHOD(Int, clone) {
    assertArgCount(vm, "Int::clone()", 0, argCount);
    RETURN_INT((int)receiver);
}

LOX_METHOD(Int, factorial) {
    assertArgCount(vm, "Int::factorial()", 0, argCount);
    int self = AS_INT(receiver);
    assertPositiveNumber(vm, "Int::factorial()", self, -1);
    RETURN_INT(factorial(self));
}

LOX_METHOD(Int, gcd) {
    assertArgCount(vm, "Int::gcd(other)", 1, argCount);
    assertArgIsInt(vm, "Int::gcd(other)", args, 0);
    RETURN_INT(gcd(abs(AS_INT(receiver)), abs(AS_INT(args[0]))));
}

LOX_METHOD(Int, init) {
    raiseError(vm, "Cannot instantiate from class Int.");
    RETURN_NIL;
}

LOX_METHOD(Int, isEven) {
    assertArgCount(vm, "Int::isEven()", 0, argCount);
    RETURN_BOOL(AS_INT(receiver) % 2 == 0);
}

LOX_METHOD(Int, isOdd) {
    assertArgCount(vm, "Int::isOdd()", 0, argCount);
    RETURN_BOOL(AS_INT(receiver) % 2 != 0);
}

LOX_METHOD(Int, lcm) {
    assertArgCount(vm, "Int::lcm(other)", 1, argCount);
    assertArgIsInt(vm, "Int::lcm(other)", args, 0);
    RETURN_INT(lcm(abs(AS_INT(receiver)), abs(AS_INT(args[0]))));
}

LOX_METHOD(Int, toFloat) {
    assertArgCount(vm, "Int::toFloat()", 0, argCount);
    RETURN_NUMBER((double)AS_INT(receiver));
}

LOX_METHOD(Int, toString) {
    assertArgCount(vm, "Int::toString()", 0, argCount);
    RETURN_STRING_FMT("%d", AS_INT(receiver));
}

LOX_METHOD(Method, arity) {
    assertArgCount(vm, "Method::arity()", 0, argCount);
    RETURN_INT(AS_BOUND_METHOD(receiver)->method->function->arity);
}

LOX_METHOD(Method, clone) {
    assertArgCount(vm, "Method::clone()", 0, argCount);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Method, init) {
    raiseError(vm, "Cannot instantiate from class Method.");
    RETURN_NIL;
}

LOX_METHOD(Method, name) {
    assertArgCount(vm, "Method::name()", 0, argCount);
    ObjBoundMethod* bound = AS_BOUND_METHOD(receiver);
    RETURN_STRING_FMT("%s::%s", getObjClass(vm, bound->receiver)->name->chars, bound->method->function->name->chars);
}

LOX_METHOD(Method, receiver) {
    assertArgCount(vm, "Method::receiver()", 0, argCount);
    RETURN_VAL(AS_BOUND_METHOD(receiver)->receiver);
}

LOX_METHOD(Method, toString) {
    assertArgCount(vm, "Method::toString()", 0, argCount);
    ObjBoundMethod* bound = AS_BOUND_METHOD(receiver);
    RETURN_STRING_FMT("<method %s::%s>", getObjClass(vm, bound->receiver)->name->chars, bound->method->function->name->chars);
}

LOX_METHOD(Method, upvalueCount) {
    assertArgCount(vm, "Function::upvalueCount()", 0, argCount);
    RETURN_INT(AS_BOUND_METHOD(receiver)->method->upvalueCount);
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

LOX_METHOD(Number, acos) {
    assertArgCount(vm, "Number::acos()", 0, argCount);
    RETURN_NUMBER(acos(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, asin) {
    assertArgCount(vm, "Number::asin()", 0, argCount);
    RETURN_NUMBER(asin(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, atan) {
    assertArgCount(vm, "Number::atan()", 0, argCount);
    RETURN_NUMBER(atan(AS_NUMBER(receiver)));
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

LOX_METHOD(Number, cos) {
    assertArgCount(vm, "Number::cos()", 0, argCount);
    RETURN_NUMBER(cos(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, exp) {
    assertArgCount(vm, "Number::exp()", 0, argCount);
    RETURN_NUMBER(exp(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, floor) {
    assertArgCount(vm, "Number::floor()", 0, argCount);
    RETURN_NUMBER(floor(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, hypot) {
    assertArgCount(vm, "Number::hypot(other)", 1, argCount);
    assertArgIsNumber(vm, "Number::hypot(other)", args, 0);
    RETURN_NUMBER(hypot(AS_NUMBER(receiver), AS_NUMBER(args[0])));
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

LOX_METHOD(Number, sin) {
    assertArgCount(vm, "Number::sin()", 0, argCount);
    RETURN_NUMBER(sin(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, sqrt) {
    assertArgCount(vm, "Number::sqrt()", 0, argCount);
    double self = AS_NUMBER(receiver);
    assertPositiveNumber(vm, "Number::sqrt()", self, -1);
    RETURN_NUMBER(sqrt(self));
}

LOX_METHOD(Number, tan) {
    assertArgCount(vm, "Number::tan()", 0, argCount);
    RETURN_NUMBER(tan(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, toInt) {
    assertArgCount(vm, "Number::toInt()", 0, argCount);
    RETURN_INT((int)AS_NUMBER(receiver));
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
    ObjInstance* thatObject = newInstance(vm, OBJ_KLASS(receiver));
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
    RETURN_INT(hashValue(receiver));
}

LOX_METHOD(Object, instanceOf) {
    assertArgCount(vm, "Object::instanceOf(class)", 1, argCount);
    if (!IS_CLASS(args[0])) RETURN_FALSE;
    RETURN_BOOL(isObjInstanceOf(vm, receiver, AS_CLASS(args[0])));
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
    RETURN_STRING_FMT("<object %s>", AS_OBJ(receiver)->klass->name->chars);
}

LOX_METHOD(String, capitalize) {
    assertArgCount(vm, "String::capitalize()", 0, argCount);
    RETURN_OBJ(capitalizeString(vm, AS_STRING(receiver)));
}

LOX_METHOD(String, clone) {
    assertArgCount(vm, "String::clone()", 0, argCount);
    RETURN_OBJ(receiver);
}

LOX_METHOD(String, contains) {
    assertArgCount(vm, "String::contains(chars)", 1, argCount);
    assertArgIsString(vm, "String::contains(chars)", args, 0);
    ObjString* haystack = AS_STRING(receiver);
    ObjString* needle = AS_STRING(args[0]);
    RETURN_BOOL(searchString(vm, haystack, needle, 0) != -1);
}

LOX_METHOD(String, decapitalize) {
    assertArgCount(vm, "String::decapitalize()", 0, argCount);
    RETURN_OBJ(decapitalizeString(vm, AS_STRING(receiver)));
}

LOX_METHOD(String, endsWith) {
    assertArgCount(vm, "String::endsWith(chars)", 1, argCount);
    assertArgIsString(vm, "String::endsWith(chars)", args, 0);
    ObjString* haystack = AS_STRING(receiver);
    ObjString* needle = AS_STRING(args[0]);
    if (needle->length > haystack->length) RETURN_FALSE;
    RETURN_BOOL(memcmp(haystack->chars + haystack->length - needle->length, needle->chars, needle->length) == 0);
}

LOX_METHOD(String, getChar) {
    assertArgCount(vm, "String::getChar(index)", 1, argCount);
    assertArgIsInt(vm, "String::getChar(index)", args, 0);
    
    ObjString* self = AS_STRING(receiver);
    int index = AS_INT(args[0]);
    assertIndexWithinRange(vm, "String::getChar(index)", index, 0, self->length, 0);

    char chars[2];
    chars[0] = self->chars[index];
    chars[1] = '\n';
    RETURN_STRING(chars, 1);
}

LOX_METHOD(String, indexOf) {
    assertArgCount(vm, "String::indexOf(chars)", 1, argCount);
    assertArgIsString(vm, "String::indexOf(chars)", args, 0);
    ObjString* haystack = AS_STRING(receiver);
    ObjString* needle = AS_STRING(args[0]);
    RETURN_INT(searchString(vm, haystack, needle, 0));
}

LOX_METHOD(String, init) {
    assertArgCount(vm, "String::init(chars)", 1, argCount);
    assertArgIsString(vm, "String::init(chars)", args, 0);
    RETURN_OBJ(args[0]);
}

LOX_METHOD(String, length) {
    assertArgCount(vm, "String::length()", 0, argCount);
    RETURN_INT(AS_STRING(receiver)->length);
}

LOX_METHOD(String, replace) {
    assertArgCount(vm, "String::replace(target, replacement)", 2, argCount);
    assertArgIsString(vm, "String::replace(target, replacement)", args, 0);
    assertArgIsString(vm, "String::replace(target, replacement)", args, 1);
    RETURN_OBJ(replaceString(vm, AS_STRING(receiver), AS_STRING(args[0]), AS_STRING(args[1])));
}

LOX_METHOD(String, reverse) {
    assertArgCount(vm, "String::reverse()", 0, argCount);
    ObjString* self = AS_STRING(receiver);
    if (self->length <= 1) RETURN_OBJ(receiver);
    RETURN_STRING(_strrev(self->chars), self->length);
}

LOX_METHOD(String, split) {
    assertArgCount(vm, "String::split(delimiter)", 1, argCount);
    assertArgIsString(vm, "String::split(delimiter)", args, 0);
    ObjString* self = AS_STRING(receiver);
    ObjString* delimiter = AS_STRING(args[0]);

    ObjList* list = newList(vm);
    char* string = _strdup(self->chars);
    char* next = NULL;
    char* token = strtok_s(string, delimiter->chars, &next);
    while (token != NULL) {
        writeValueArray(vm, &list->elements, OBJ_VAL(newString(vm, token)));
        token = strtok_s(NULL, delimiter->chars, &next);
    }
    free(string);
    RETURN_OBJ(list);
}

LOX_METHOD(String, startsWith) {
    assertArgCount(vm, "String::startsWith(chars)", 1, argCount);
    assertArgIsString(vm, "String::startsWith(chars)", args, 0);
    ObjString* haystack = AS_STRING(receiver);
    ObjString* needle = AS_STRING(args[0]);
    if (needle->length > haystack->length) RETURN_FALSE;
    RETURN_BOOL(memcmp(haystack->chars, needle->chars, needle->length) == 0);
}

LOX_METHOD(String, subString) {
    assertArgCount(vm, "String::subString(from, to)", 2, argCount);
    assertArgIsInt(vm, "String::subString(from, to)", args, 0);
    assertArgIsInt(vm, "String::subString(from, to)", args, 1);
    RETURN_OBJ(subString(vm, AS_STRING(receiver), AS_INT(args[0]), AS_INT(args[1])));
}

LOX_METHOD(String, toLowercase) {
    assertArgCount(vm, "String::toLowercase()", 0, argCount);
    RETURN_OBJ(toLowerString(vm, AS_STRING(receiver)));
}

LOX_METHOD(String, toString) {
    assertArgCount(vm, "String::toString()", 0, argCount);
    RETURN_OBJ(receiver);
}

LOX_METHOD(String, toUppercase) {
    assertArgCount(vm, "String::toLowercase()", 0, argCount);
    RETURN_OBJ(toUpperString(vm, AS_STRING(receiver)));
}

LOX_METHOD(String, trim) {
    assertArgCount(vm, "String::trim()", 0, argCount);
    RETURN_OBJ(trimString(vm, AS_STRING(receiver)));
}

void registerLangPackage(VM* vm){
    vm->objectClass = defineNativeClass(vm, "Object");
    DEF_METHOD(vm->objectClass, Object, clone, 0);
    DEF_METHOD(vm->objectClass, Object, equals, 1);
    DEF_METHOD(vm->objectClass, Object, getClass, 0);
    DEF_METHOD(vm->objectClass, Object, getClassName, 0);
    DEF_METHOD(vm->objectClass, Object, hasField, 1);
    DEF_METHOD(vm->objectClass, Object, hashCode, 0);
    DEF_METHOD(vm->objectClass, Object, instanceOf, 1);
    DEF_METHOD(vm->objectClass, Object, memberOf, 1);
    DEF_METHOD(vm->objectClass, Object, toString, 0);

    vm->classClass = defineNativeClass(vm, "Class");
    bindSuperclass(vm, vm->classClass, vm->objectClass);
    DEF_METHOD(vm->classClass, Class, clone, 0);
    DEF_METHOD(vm->classClass, Class, getClass, 0);
    DEF_METHOD(vm->classClass, Class, getClassName, 0);
    DEF_METHOD(vm->classClass, Class, init, 2);
    DEF_METHOD(vm->classClass, Class, instanceOf, 1);
    DEF_METHOD(vm->classClass, Class, memberOf, 1);
    DEF_METHOD(vm->classClass, Class, name, 0);
    DEF_METHOD(vm->classClass, Class, superclass, 0);
    DEF_METHOD(vm->classClass, Class, toString, 0);

    vm->nilClass = defineNativeClass(vm, "Nil");
    bindSuperclass(vm, vm->nilClass, vm->objectClass);
    DEF_METHOD(vm->nilClass, Nil, clone, 0);
    DEF_METHOD(vm->nilClass, Nil, init, 0);
    DEF_METHOD(vm->nilClass, Nil, toString, 0);
    
    vm->boolClass = defineNativeClass(vm, "Bool");
    bindSuperclass(vm, vm->boolClass, vm->objectClass);
    DEF_METHOD(vm->boolClass, Bool, clone, 0);
    DEF_METHOD(vm->boolClass, Bool, init, 0);
    DEF_METHOD(vm->boolClass, Bool, toString, 0);
    
    vm->numberClass = defineNativeClass(vm, "Number");
    bindSuperclass(vm, vm->numberClass, vm->objectClass);
    DEF_METHOD(vm->numberClass, Number, abs, 0);
    DEF_METHOD(vm->numberClass, Number, acos, 0);
    DEF_METHOD(vm->numberClass, Number, asin, 0);
    DEF_METHOD(vm->numberClass, Number, atan, 0);
    DEF_METHOD(vm->numberClass, Number, cbrt, 0);
    DEF_METHOD(vm->numberClass, Number, ceil, 0);
    DEF_METHOD(vm->numberClass, Number, clone, 0);
    DEF_METHOD(vm->numberClass, Number, cos, 0);
    DEF_METHOD(vm->numberClass, Number, exp, 1);
    DEF_METHOD(vm->numberClass, Number, floor, 0);
    DEF_METHOD(vm->numberClass, Number, hypot, 1);
    DEF_METHOD(vm->numberClass, Number, init, 0);
    DEF_METHOD(vm->numberClass, Number, log, 0);
    DEF_METHOD(vm->numberClass, Number, log2, 0);
    DEF_METHOD(vm->numberClass, Number, log10, 0);
    DEF_METHOD(vm->numberClass, Number, max, 1);
    DEF_METHOD(vm->numberClass, Number, min, 1);
    DEF_METHOD(vm->numberClass, Number, pow, 1);
    DEF_METHOD(vm->numberClass, Number, round, 0);
    DEF_METHOD(vm->numberClass, Number, sin, 0);
    DEF_METHOD(vm->numberClass, Number, sqrt, 0);
    DEF_METHOD(vm->numberClass, Number, tan, 0);
    DEF_METHOD(vm->numberClass, Number, toInt, 0);
    DEF_METHOD(vm->numberClass, Number, toString, 0);

    vm->intClass = defineNativeClass(vm, "Int");
    bindSuperclass(vm, vm->intClass, vm->numberClass);
    DEF_METHOD(vm->intClass, Int, abs, 0);
    DEF_METHOD(vm->intClass, Int, clone, 0);
    DEF_METHOD(vm->intClass, Int, factorial, 0);
    DEF_METHOD(vm->intClass, Int, gcd, 1);
    DEF_METHOD(vm->intClass, Int, init, 0);
    DEF_METHOD(vm->intClass, Int, isEven, 0);
    DEF_METHOD(vm->intClass, Int, isOdd, 0);
    DEF_METHOD(vm->intClass, Int, lcm, 1);
    DEF_METHOD(vm->intClass, Int, toFloat, 0);
    DEF_METHOD(vm->intClass, Int, toString, 0);

    vm->floatClass = defineNativeClass(vm, "Float");
    bindSuperclass(vm, vm->floatClass, vm->numberClass);
    DEF_METHOD(vm->floatClass, Float, clone, 0);
    DEF_METHOD(vm->floatClass, Float, init, 0);
    DEF_METHOD(vm->floatClass, Float, toString, 0);

    vm->stringClass = defineNativeClass(vm, "String");
    bindSuperclass(vm, vm->stringClass, vm->objectClass);
    DEF_METHOD(vm->stringClass, String, capitalize, 0);
    DEF_METHOD(vm->stringClass, String, clone, 0);
    DEF_METHOD(vm->stringClass, String, contains, 1);
    DEF_METHOD(vm->stringClass, String, decapitalize, 0);
    DEF_METHOD(vm->stringClass, String, endsWith, 1);
    DEF_METHOD(vm->stringClass, String, getChar, 1);
    DEF_METHOD(vm->stringClass, String, indexOf, 1);
    DEF_METHOD(vm->stringClass, String, init, 1);
    DEF_METHOD(vm->stringClass, String, length, 0);
    DEF_METHOD(vm->stringClass, String, replace, 2);
    DEF_METHOD(vm->stringClass, String, reverse, 0);
    DEF_METHOD(vm->stringClass, String, split, 1);
    DEF_METHOD(vm->stringClass, String, startsWith, 1);
    DEF_METHOD(vm->stringClass, String, subString, 2);
    DEF_METHOD(vm->stringClass, String, toLowercase, 0);
    DEF_METHOD(vm->stringClass, String, toString, 0);
    DEF_METHOD(vm->stringClass, String, toUppercase, 0);
    DEF_METHOD(vm->stringClass, String, trim, 0);

    vm->functionClass = defineNativeClass(vm, "Function");
    bindSuperclass(vm, vm->functionClass, vm->objectClass);
    DEF_METHOD(vm->functionClass, Function, arity, 0);
    DEF_METHOD(vm->functionClass, Function, clone, 0);
    DEF_METHOD(vm->functionClass, Function, init, 0);
    DEF_METHOD(vm->functionClass, Function, isNative, 0);
    DEF_METHOD(vm->functionClass, Function, name, 0);
    DEF_METHOD(vm->functionClass, Function, toString, 0);
    DEF_METHOD(vm->functionClass, Function, upvalueCount, 0);

    vm->methodClass = defineNativeClass(vm, "Method");
    bindSuperclass(vm, vm->methodClass, vm->objectClass);
    DEF_METHOD(vm->methodClass, Method, arity, 0);
    DEF_METHOD(vm->methodClass, Method, clone, 0);
    DEF_METHOD(vm->methodClass, Method, init, 0);
    DEF_METHOD(vm->methodClass, Method, name, 0);
    DEF_METHOD(vm->methodClass, Method, receiver, 0);
    DEF_METHOD(vm->methodClass, Method, toString, 0);
    DEF_METHOD(vm->methodClass, Method, upvalueCount, 0);

}