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
    ASSERT_ARG_COUNT("Bool::clone()", 0);
    RETURN_BOOL(receiver);
}

LOX_METHOD(Bool, init) {
    raiseError(vm, "Cannot instantiate from class Bool.");
    RETURN_NIL;
}

LOX_METHOD(Bool, toString) {
    ASSERT_ARG_COUNT("Bool::toString()", 0);
    if (AS_BOOL(receiver)) RETURN_STRING("true", 4);
    else RETURN_STRING("false", 5);
}

LOX_METHOD(Class, clone) {
    ASSERT_ARG_COUNT("Class::clone()", 0);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Class, getClass) {
    ASSERT_ARG_COUNT("Class::getClass()", 0);
    RETURN_OBJ(vm->classClass);
}

LOX_METHOD(Class, getClassName) {
    ASSERT_ARG_COUNT("Class::getClassName()", 0);
    RETURN_OBJ(vm->classClass->name);
}

LOX_METHOD(Class, hasMethod) {
    ASSERT_ARG_COUNT("Class::hasMethod(method)", 1);
    ASSERT_ARG_TYPE("Class::hasMethod(method)", 0, String);
    ObjClass* self = AS_CLASS(receiver);
    Value value;
    RETURN_BOOL(tableGet(&self->methods, AS_STRING(args[0]), &value));
}

LOX_METHOD(Class, init) {
    ASSERT_ARG_COUNT("Class::init(name, superclass)", 2);
    ASSERT_ARG_TYPE("Class::init(name, superclass)", 0, String);
    ASSERT_ARG_TYPE("Class::init(name, superclass)", 1, Class);
    ObjClass* klass = newClass(vm, AS_STRING(args[0]));
    bindSuperclass(vm, klass, AS_CLASS(args[1]));
    RETURN_OBJ(klass);
}

LOX_METHOD(Class, instanceOf) {
    ASSERT_ARG_COUNT("Class::instanceOf(class)", 1);
    if (!IS_CLASS(args[0])) RETURN_FALSE;
    ObjClass* klass = AS_CLASS(args[0]);
    if (klass == vm->classClass) RETURN_TRUE;
    else RETURN_FALSE;
}

LOX_METHOD(Class, memberOf) {
    ASSERT_ARG_COUNT("Class::memberOf(class)", 1);
    if (!IS_CLASS(args[0])) RETURN_FALSE;
    ObjClass* klass = AS_CLASS(args[0]);
    if (klass == vm->classClass) RETURN_TRUE;
    else RETURN_FALSE;
}

LOX_METHOD(Class, name) {
    ASSERT_ARG_COUNT("Class::name()", 0);
    RETURN_OBJ(AS_CLASS(receiver)->name);
}

LOX_METHOD(Class, superclass) {
    ASSERT_ARG_COUNT("Class::superclass()", 0);
    ObjClass* klass = AS_CLASS(receiver);
    if (klass->superclass == NULL) RETURN_NIL;
    RETURN_OBJ(klass->superclass);
}

LOX_METHOD(Class, toString) {
    ASSERT_ARG_COUNT("Class::toString()", 0);
    RETURN_STRING_FMT("<class %s>", AS_CLASS(receiver)->name->chars);
}

LOX_METHOD(Float, clone) {
    ASSERT_ARG_COUNT("Float::clone()", 0);
    RETURN_NUMBER((double)receiver);
}

LOX_METHOD(Float, init) {
    raiseError(vm, "Cannot instantiate from class Float.");
    RETURN_NIL;
}

LOX_METHOD(Float, toString) {
    ASSERT_ARG_COUNT("Float::toString()", 0);
    RETURN_STRING_FMT("%g", AS_FLOAT(receiver));
}

LOX_METHOD(Function, arity) {
    ASSERT_ARG_COUNT("Function::arity()", 0);
    if (IS_NATIVE_FUNCTION(receiver)) {
         RETURN_INT(AS_NATIVE_FUNCTION(receiver)->arity);
    }
    RETURN_INT(AS_CLOSURE(receiver)->function->arity);
}

LOX_METHOD(Function, call) {
    ObjClosure* self = AS_CLOSURE(receiver);
    if (callClosure(vm, self, argCount)) {
        int i = 0;
        while (i < argCount) {
            push(vm, args[i]);
            i++;
        }
        RETURN_VAL(args[argCount - 1]);
    }
    RETURN_NIL;
}

LOX_METHOD(Function, call0) {
    ASSERT_ARG_COUNT("Function::call0()", 0);
    if (callClosure(vm, AS_CLOSURE(receiver), argCount)) {
        RETURN_VAL(args[0]);
    }
    RETURN_NIL;
}

LOX_METHOD(Function, call1) {
    ASSERT_ARG_COUNT("Function::call(arg)", 1);
    if (callClosure(vm, AS_CLOSURE(receiver), argCount)) {
        push(vm, args[0]);
        RETURN_VAL(args[0]);
    }
    RETURN_NIL;
}

LOX_METHOD(Function, call2) {
    ASSERT_ARG_COUNT("Function::call2(arg1, arg2)", 2);
    if (callClosure(vm, AS_CLOSURE(receiver), argCount)) {
        push(vm, args[0]);
        push(vm, args[1]);
        RETURN_VAL(args[1]);
    }
    RETURN_NIL;
}

LOX_METHOD(Function, clone) {
    ASSERT_ARG_COUNT("Function::clone()", 0);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Function, init) {
    raiseError(vm, "Cannot instantiate from class Function.");
    RETURN_NIL;
}

LOX_METHOD(Function, isAnonymous) {
    ASSERT_ARG_COUNT("Function::isAnonymous()", 0);
    if (IS_NATIVE_FUNCTION(receiver)) RETURN_FALSE;
    RETURN_BOOL(AS_CLOSURE(receiver)->function->name->length == 0);
}

LOX_METHOD(Function, isNative) {
    ASSERT_ARG_COUNT("Function::isNative()", 0);
    RETURN_BOOL(IS_NATIVE_FUNCTION(receiver));
}

LOX_METHOD(Function, isVariadic) {
    ASSERT_ARG_COUNT("Function::isVariadic()", 0);
    RETURN_BOOL(AS_CLOSURE(receiver)->function->arity == -1);
}

LOX_METHOD(Function, name) {
    ASSERT_ARG_COUNT("Function::name()", 0);
    if (IS_NATIVE_FUNCTION(receiver)) {
        RETURN_OBJ(AS_NATIVE_FUNCTION(receiver)->name);
    }

    RETURN_OBJ(AS_CLOSURE(receiver)->function->name);
}

LOX_METHOD(Function, toString) {
    ASSERT_ARG_COUNT("Function::toString()", 0);
    if (IS_NATIVE_FUNCTION(receiver)) {
        RETURN_STRING_FMT("<fn %s>", AS_NATIVE_FUNCTION(receiver)->name->chars);
    }
    RETURN_STRING_FMT("<fn %s>", AS_CLOSURE(receiver)->function->name->chars);
}

LOX_METHOD(Function, upvalueCount) {
    ASSERT_ARG_COUNT("Function::upvalueCount()", 0);
    RETURN_INT(AS_CLOSURE(receiver)->upvalueCount);
}

LOX_METHOD(Int, abs) {
    ASSERT_ARG_COUNT("Int::abs()", 0);
    RETURN_INT(abs(AS_INT(receiver)));
}

LOX_METHOD(Int, clone) {
    ASSERT_ARG_COUNT("Int::clone()", 0);
    RETURN_INT((int)receiver);
}

LOX_METHOD(Int, factorial) {
    ASSERT_ARG_COUNT("Int::factorial()", 0);
    int self = AS_INT(receiver);
    assertNumberNonNegative(vm, "Int::factorial()", self, -1);
    RETURN_INT(factorial(self));
}

LOX_METHOD(Int, gcd) {
    ASSERT_ARG_COUNT("Int::gcd(other)", 1);
    ASSERT_ARG_TYPE("Int::gcd(other)", 0, Int);
    RETURN_INT(gcd(abs(AS_INT(receiver)), abs(AS_INT(args[0]))));
}

LOX_METHOD(Int, init) {
    raiseError(vm, "Cannot instantiate from class Int.");
    RETURN_NIL;
}

LOX_METHOD(Int, isEven) {
    ASSERT_ARG_COUNT("Int::isEven()", 0);
    RETURN_BOOL(AS_INT(receiver) % 2 == 0);
}

LOX_METHOD(Int, isOdd) {
    ASSERT_ARG_COUNT("Int::isOdd()", 0);
    RETURN_BOOL(AS_INT(receiver) % 2 != 0);
}

LOX_METHOD(Int, lcm) {
    ASSERT_ARG_COUNT("Int::lcm(other)", 1);
    ASSERT_ARG_TYPE("Int::lcm(other)", 0, Int);
    RETURN_INT(lcm(abs(AS_INT(receiver)), abs(AS_INT(args[0]))));
}

LOX_METHOD(Int, toBinary) {
    ASSERT_ARG_COUNT("Int::toBinary()", 0);
    char buffer[32];
    int length = 32;
    _itoa_s(AS_INT(receiver), buffer, length, 2);
    RETURN_STRING(buffer, length);
}

LOX_METHOD(Int, toFloat) {
    ASSERT_ARG_COUNT("Int::toFloat()", 0);
    RETURN_NUMBER((double)AS_INT(receiver));
}

LOX_METHOD(Int, toHexadecimal) {
    ASSERT_ARG_COUNT("Int::toHexadecimal()", 0);
    char buffer[8];
    int length = 8;
    _itoa_s(AS_INT(receiver), buffer, length, 16);
    RETURN_STRING(buffer, length);
}

LOX_METHOD(Int, toString) {
    ASSERT_ARG_COUNT("Int::toString()", 0);
    RETURN_STRING_FMT("%d", AS_INT(receiver));
}

LOX_METHOD(Method, arity) {
    ASSERT_ARG_COUNT("Method::arity()", 0);
    RETURN_INT(AS_BOUND_METHOD(receiver)->method->function->arity);
}

LOX_METHOD(Method, clone) {
    ASSERT_ARG_COUNT("Method::clone()", 0);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Method, init) {
    raiseError(vm, "Cannot instantiate from class Method.");
    RETURN_NIL;
}

LOX_METHOD(Method, name) {
    ASSERT_ARG_COUNT("Method::name()", 0);
    ObjBoundMethod* bound = AS_BOUND_METHOD(receiver);
    RETURN_STRING_FMT("%s::%s", getObjClass(vm, bound->receiver)->name->chars, bound->method->function->name->chars);
}

LOX_METHOD(Method, receiver) {
    ASSERT_ARG_COUNT("Method::receiver()", 0);
    RETURN_VAL(AS_BOUND_METHOD(receiver)->receiver);
}

LOX_METHOD(Method, toString) {
    ASSERT_ARG_COUNT("Method::toString()", 0);
    ObjBoundMethod* bound = AS_BOUND_METHOD(receiver);
    RETURN_STRING_FMT("<method %s::%s>", getObjClass(vm, bound->receiver)->name->chars, bound->method->function->name->chars);
}

LOX_METHOD(Method, upvalueCount) {
    ASSERT_ARG_COUNT("Method::upvalueCount()", 0);
    RETURN_INT(AS_BOUND_METHOD(receiver)->method->upvalueCount);
}

LOX_METHOD(Nil, clone) {
    ASSERT_ARG_COUNT("Nil::clone()", 0);
    RETURN_NIL;
}

LOX_METHOD(Nil, init) {
    raiseError(vm, "Cannot instantiate from class Nil.");
    RETURN_NIL;
}

LOX_METHOD(Nil, toString) {
    ASSERT_ARG_COUNT("Nil::toString()", 0);
    RETURN_STRING("nil", 3);
}

LOX_METHOD(Number, abs) {
    ASSERT_ARG_COUNT("Number::abs()", 0);
    RETURN_NUMBER(fabs(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, acos) {
    ASSERT_ARG_COUNT("Number::acos()", 0);
    RETURN_NUMBER(acos(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, asin) {
    ASSERT_ARG_COUNT("Number::asin()", 0);
    RETURN_NUMBER(asin(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, atan) {
    ASSERT_ARG_COUNT("Number::atan()", 0);
    RETURN_NUMBER(atan(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, cbrt) {
    ASSERT_ARG_COUNT("Number::cbrt()", 0);
    RETURN_NUMBER(cbrt(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, ceil) {
    ASSERT_ARG_COUNT("Number::ceil()", 0);
    RETURN_NUMBER(ceil(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, clone) {
    ASSERT_ARG_COUNT("Number::clone()", 0);
    RETURN_NUMBER((double)receiver);
}

LOX_METHOD(Number, cos) {
    ASSERT_ARG_COUNT("Number::cos()", 0);
    RETURN_NUMBER(cos(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, exp) {
    ASSERT_ARG_COUNT("Number::exp()", 0);
    RETURN_NUMBER(exp(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, floor) {
    ASSERT_ARG_COUNT("Number::floor()", 0);
    RETURN_NUMBER(floor(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, hypot) {
    ASSERT_ARG_COUNT("Number::hypot(other)", 1);
    ASSERT_ARG_TYPE("Number::hypot(other)", 0, Number);
    RETURN_NUMBER(hypot(AS_NUMBER(receiver), AS_NUMBER(args[0])));
}

LOX_METHOD(Number, init) {
    raiseError(vm, "Cannot instantiate from class Number.");
    RETURN_NIL;
}

LOX_METHOD(Number, log) {
    ASSERT_ARG_COUNT("Number::log()", 0);
    double self = AS_NUMBER(receiver);
    assertNumberPositive(vm, "Number::log2()", self, -1);
    RETURN_NUMBER(log(self));
}

LOX_METHOD(Number, log10) {
    ASSERT_ARG_COUNT("Number::log10()", 0);
    double self = AS_NUMBER(receiver);
    assertNumberPositive(vm, "Number::log10()", self, -1);
    RETURN_NUMBER(log10(self));
}

LOX_METHOD(Number, log2) {
    ASSERT_ARG_COUNT("Number::log2()", 0);
    double self = AS_NUMBER(receiver);
    assertNumberPositive(vm, "Number::log2()", self, -1);
    RETURN_NUMBER(log2(self));
}

LOX_METHOD(Number, max) {
    ASSERT_ARG_COUNT("Number::max(other)", 1);
    ASSERT_ARG_TYPE("Number::max(other)", 0, Number);
    RETURN_NUMBER(fmax(AS_NUMBER(receiver), AS_NUMBER(args[0])));
}

LOX_METHOD(Number, min) {
    ASSERT_ARG_COUNT("Number::min(other)", 1);
    ASSERT_ARG_TYPE("Number::min(other)", 0, Number);
    RETURN_NUMBER(fmin(AS_NUMBER(receiver), AS_NUMBER(args[0])));
}

LOX_METHOD(Number, pow) {
    ASSERT_ARG_COUNT("Number::pow(exponent)", 1);
    ASSERT_ARG_TYPE("Number::pow(exponent)", 0, Number);
    RETURN_NUMBER(pow(AS_NUMBER(receiver), AS_NUMBER(args[0])));
}

LOX_METHOD(Number, round) {
    ASSERT_ARG_COUNT("Number::round()", 0);
    RETURN_NUMBER(round(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, sin) {
    ASSERT_ARG_COUNT("Number::sin()", 0);
    RETURN_NUMBER(sin(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, sqrt) {
    ASSERT_ARG_COUNT("Number::sqrt()", 0);
    double self = AS_NUMBER(receiver);
    assertNumberPositive(vm, "Number::sqrt()", self, -1);
    RETURN_NUMBER(sqrt(self));
}

LOX_METHOD(Number, tan) {
    ASSERT_ARG_COUNT("Number::tan()", 0);
    RETURN_NUMBER(tan(AS_NUMBER(receiver)));
}

LOX_METHOD(Number, toInt) {
    ASSERT_ARG_COUNT("Number::toInt()", 0);
    RETURN_INT((int)AS_NUMBER(receiver));
}

LOX_METHOD(Number, toString) {
    ASSERT_ARG_COUNT("Number::toString()", 0);
    char chars[24];
    int length = sprintf_s(chars, 24, "%.14g", AS_NUMBER(receiver));
    RETURN_STRING(chars, length);
}

LOX_METHOD(Object, clone) {
    ASSERT_ARG_COUNT("Object::clone()", 0);
    ObjInstance* thisObject = AS_INSTANCE(receiver);
    ObjInstance* thatObject = newInstance(vm, OBJ_KLASS(receiver));
    push(vm, OBJ_VAL(thatObject));
    tableAddAll(vm, &thisObject->fields, &thatObject->fields);
    pop(vm);
    RETURN_OBJ(thatObject);
}

LOX_METHOD(Object, equals) {
    ASSERT_ARG_COUNT("Object::equals(value)", 1);
    RETURN_BOOL(valuesEqual(receiver, args[0]));
}

LOX_METHOD(Object, getClass) {
    ASSERT_ARG_COUNT("Object::getClass()", 0);
    RETURN_OBJ(getObjClass(vm, receiver));
}

LOX_METHOD(Object, getClassName) {
    ASSERT_ARG_COUNT("Object::getClassName()", 0);
    RETURN_OBJ(getObjClass(vm, receiver)->name);
}

LOX_METHOD(Object, hasField) {
    ASSERT_ARG_COUNT("Object::hasField(field)", 1);
    ASSERT_ARG_TYPE("Object::hasField(field)", 0, String);
    if (!IS_INSTANCE(receiver)) return false;
    ObjInstance* instance = AS_INSTANCE(receiver);
    Value value;
    RETURN_BOOL(tableGet(&instance->fields, AS_STRING(args[0]), &value));
}

LOX_METHOD(Object, hashCode) {
    ASSERT_ARG_COUNT("Object::hashCode()", 0);
    RETURN_INT(hashValue(receiver));
}

LOX_METHOD(Object, instanceOf) {
    ASSERT_ARG_COUNT("Object::instanceOf(class)", 1);
    if (!IS_CLASS(args[0])) RETURN_FALSE;
    RETURN_BOOL(isObjInstanceOf(vm, receiver, AS_CLASS(args[0])));
}

LOX_METHOD(Object, memberOf) {
    ASSERT_ARG_COUNT("Object::memberOf(class)", 1);
    if (!IS_CLASS(args[0])) RETURN_FALSE;
    ObjClass* thisClass = getObjClass(vm, receiver);
    ObjClass* thatClass = AS_CLASS(args[0]);
    RETURN_BOOL(thisClass == thatClass);
}

LOX_METHOD(Object, toString) {
    ASSERT_ARG_COUNT("Object::toString()", 0);
    RETURN_STRING_FMT("<object %s>", AS_OBJ(receiver)->klass->name->chars);
}

LOX_METHOD(String, capitalize) {
    ASSERT_ARG_COUNT("String::capitalize()", 0);
    RETURN_OBJ(capitalizeString(vm, AS_STRING(receiver)));
}

LOX_METHOD(String, clone) {
    ASSERT_ARG_COUNT("String::clone()", 0);
    RETURN_OBJ(receiver);
}

LOX_METHOD(String, contains) {
    ASSERT_ARG_COUNT("String::contains(chars)", 1);
    ASSERT_ARG_TYPE("String::contains(chars)", 0, String);
    ObjString* haystack = AS_STRING(receiver);
    ObjString* needle = AS_STRING(args[0]);
    RETURN_BOOL(searchString(vm, haystack, needle, 0) != -1);
}

LOX_METHOD(String, decapitalize) {
    ASSERT_ARG_COUNT("String::decapitalize()", 0);
    RETURN_OBJ(decapitalizeString(vm, AS_STRING(receiver)));
}

LOX_METHOD(String, endsWith) {
    ASSERT_ARG_COUNT("String::endsWith(chars)", 1);
    ASSERT_ARG_TYPE("String::endsWith(chars)", 0, String);
    ObjString* haystack = AS_STRING(receiver);
    ObjString* needle = AS_STRING(args[0]);
    if (needle->length > haystack->length) RETURN_FALSE;
    RETURN_BOOL(memcmp(haystack->chars + haystack->length - needle->length, needle->chars, needle->length) == 0);
}

LOX_METHOD(String, getChar) {
    ASSERT_ARG_COUNT("String::getChar(index)", 1);
    ASSERT_ARG_TYPE("String::getChar(index)", 0, Int);
    
    ObjString* self = AS_STRING(receiver);
    int index = AS_INT(args[0]);
    assertIntWithinRange(vm, "String::getChar(index)", index, 0, self->length, 0);

    char chars[2] = { self->chars[index], '\0' };
    RETURN_STRING(chars, 1);
}

LOX_METHOD(String, indexOf) {
    ASSERT_ARG_COUNT("String::indexOf(chars)", 1);
    ASSERT_ARG_TYPE("String::indexOf(chars)", 0, String);
    ObjString* haystack = AS_STRING(receiver);
    ObjString* needle = AS_STRING(args[0]);
    RETURN_INT(searchString(vm, haystack, needle, 0));
}

LOX_METHOD(String, init) {
    ASSERT_ARG_COUNT("String::init(chars)", 1);
    ASSERT_ARG_TYPE("String::init(chars)", 0, String);
    RETURN_OBJ(args[0]);
}

LOX_METHOD(String, length) {
    ASSERT_ARG_COUNT("String::length()", 0);
    RETURN_INT(AS_STRING(receiver)->length);
}

LOX_METHOD(String, next) {
    ASSERT_ARG_COUNT("String::next(index)", 1);
    ObjString* self = AS_STRING(receiver);
    if (IS_NIL(args[0])) {
        if (self->length == 0) RETURN_FALSE;
        RETURN_INT(0);
    }

    ASSERT_ARG_TYPE("String::next(index)", 0, Int);
    int index = AS_INT(args[0]);
    if (index < 0 || index < self->length - 1) RETURN_INT(index + 1);
    RETURN_NIL;
}

LOX_METHOD(String, nextValue) {
    ASSERT_ARG_COUNT("String::nextValue(index)", 1);
    ASSERT_ARG_TYPE("String::nextValue(index)", 0, Int);
    ObjString* self = AS_STRING(receiver);
    int index = AS_INT(args[0]);
    if (index > -1 && index < self->length) {
        char chars[2] = { self->chars[index], '\0' };
        RETURN_STRING(chars, 1);
    }
    RETURN_NIL;
}

LOX_METHOD(String, replace) {
    ASSERT_ARG_COUNT("String::replace(target, replacement)", 2);
    ASSERT_ARG_TYPE("String::replace(target, replacement)", 0, String);
    ASSERT_ARG_TYPE("String::replace(target, replacement)", 1, String);
    RETURN_OBJ(replaceString(vm, AS_STRING(receiver), AS_STRING(args[0]), AS_STRING(args[1])));
}

LOX_METHOD(String, reverse) {
    ASSERT_ARG_COUNT("String::reverse()", 0);
    ObjString* self = AS_STRING(receiver);
    if (self->length <= 1) RETURN_OBJ(receiver);
    RETURN_STRING(_strrev(self->chars), self->length);
}

LOX_METHOD(String, split) {
    ASSERT_ARG_COUNT("String::split(delimiter)", 1);
    ASSERT_ARG_TYPE("String::split(delimiter)", 0, String);
    ObjString* self = AS_STRING(receiver);
    ObjString* delimiter = AS_STRING(args[0]);

    ObjArray* list = newArray(vm);
    push(vm, OBJ_VAL(list));
    char* string = _strdup(self->chars);
    char* next = NULL;
    char* token = strtok_s(string, delimiter->chars, &next);
    while (token != NULL) {
        valueArrayWrite(vm, &list->elements, OBJ_VAL(newString(vm, token)));
        token = strtok_s(NULL, delimiter->chars, &next);
    }
    free(string);
    pop(vm);
    RETURN_OBJ(list);
}

LOX_METHOD(String, startsWith) {
    ASSERT_ARG_COUNT("String::startsWith(chars)", 1);
    ASSERT_ARG_TYPE("String::startsWith(chars)", 0, String);
    ObjString* haystack = AS_STRING(receiver);
    ObjString* needle = AS_STRING(args[0]);
    if (needle->length > haystack->length) RETURN_FALSE;
    RETURN_BOOL(memcmp(haystack->chars, needle->chars, needle->length) == 0);
}

LOX_METHOD(String, subString) {
    ASSERT_ARG_COUNT("String::subString(from, to)", 2);
    ASSERT_ARG_TYPE("String::subString(from, to)", 0, Int);
    ASSERT_ARG_TYPE("String::subString(from, to)", 1, Int);
    RETURN_OBJ(subString(vm, AS_STRING(receiver), AS_INT(args[0]), AS_INT(args[1])));
}

LOX_METHOD(String, toLowercase) {
    ASSERT_ARG_COUNT("String::toLowercase()", 0);
    RETURN_OBJ(toLowerString(vm, AS_STRING(receiver)));
}

LOX_METHOD(String, toString) {
    ASSERT_ARG_COUNT("String::toString()", 0);
    RETURN_OBJ(receiver);
}

LOX_METHOD(String, toUppercase) {
    ASSERT_ARG_COUNT("String::toLowercase()", 0);
    RETURN_OBJ(toUpperString(vm, AS_STRING(receiver)));
}

LOX_METHOD(String, trim) {
    ASSERT_ARG_COUNT("String::trim()", 0);
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
    DEF_METHOD(vm->classClass, Class, hasMethod, 1);
    DEF_METHOD(vm->classClass, Class, init, 2);
    DEF_METHOD(vm->classClass, Class, instanceOf, 1);
    DEF_METHOD(vm->classClass, Class, memberOf, 1);
    DEF_METHOD(vm->classClass, Class, name, 0);
    DEF_METHOD(vm->classClass, Class, superclass, 0);
    DEF_METHOD(vm->classClass, Class, toString, 0);
    vm->objectClass->obj.klass = vm->classClass;

    initNativePackage(vm, "src/std/lang.lox");

    vm->nilClass = getNativeClass(vm, "Nil");
    DEF_METHOD(vm->nilClass, Nil, clone, 0);
    DEF_METHOD(vm->nilClass, Nil, init, 0);
    DEF_METHOD(vm->nilClass, Nil, toString, 0);
    
    vm->boolClass = getNativeClass(vm, "Bool");
    DEF_METHOD(vm->boolClass, Bool, clone, 0);
    DEF_METHOD(vm->boolClass, Bool, init, 0);
    DEF_METHOD(vm->boolClass, Bool, toString, 0);
    
    vm->numberClass = getNativeClass(vm, "Number");
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

    vm->intClass = getNativeClass(vm, "Int");
    bindSuperclass(vm, vm->intClass, vm->numberClass);
    DEF_METHOD(vm->intClass, Int, abs, 0);
    DEF_METHOD(vm->intClass, Int, clone, 0);
    DEF_METHOD(vm->intClass, Int, factorial, 0);
    DEF_METHOD(vm->intClass, Int, gcd, 1);
    DEF_METHOD(vm->intClass, Int, init, 0);
    DEF_METHOD(vm->intClass, Int, isEven, 0);
    DEF_METHOD(vm->intClass, Int, isOdd, 0);
    DEF_METHOD(vm->intClass, Int, lcm, 1);
    DEF_METHOD(vm->intClass, Int, toBinary, 0);
    DEF_METHOD(vm->intClass, Int, toFloat, 0);
    DEF_METHOD(vm->intClass, Int, toHexadecimal, 0);
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
    DEF_METHOD(vm->stringClass, String, next, 1);
    DEF_METHOD(vm->stringClass, String, nextValue, 1);
    DEF_METHOD(vm->stringClass, String, replace, 2);
    DEF_METHOD(vm->stringClass, String, reverse, 0);
    DEF_METHOD(vm->stringClass, String, split, 1);
    DEF_METHOD(vm->stringClass, String, startsWith, 1);
    DEF_METHOD(vm->stringClass, String, subString, 2);
    DEF_METHOD(vm->stringClass, String, toLowercase, 0);
    DEF_METHOD(vm->stringClass, String, toString, 0);
    DEF_METHOD(vm->stringClass, String, toUppercase, 0);
    DEF_METHOD(vm->stringClass, String, trim, 0);
    for (int i = 0; i < vm->strings.capacity; i++) {
        Entry* entry = &vm->strings.entries[i];
        if (entry->key == NULL) continue;
        entry->key->obj.klass = vm->stringClass;
    }

    vm->functionClass = defineNativeClass(vm, "Function");
    bindSuperclass(vm, vm->functionClass, vm->objectClass);
    DEF_METHOD(vm->functionClass, Function, arity, 0);
    DEF_METHOD(vm->functionClass, Function, call, -1);
    DEF_METHOD(vm->functionClass, Function, call0, 0);
    DEF_METHOD(vm->functionClass, Function, call1, 1);
    DEF_METHOD(vm->functionClass, Function, call2, 2);
    DEF_METHOD(vm->functionClass, Function, clone, 0);
    DEF_METHOD(vm->functionClass, Function, init, 0);
    DEF_METHOD(vm->functionClass, Function, isAnonymous, 0);
    DEF_METHOD(vm->functionClass, Function, isNative, 0);
    DEF_METHOD(vm->functionClass, Function, isVariadic, 0);
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