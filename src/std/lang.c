#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lang.h"
#include "../vm/assert.h"
#include "../vm/dict.h"
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

LOX_METHOD(Behavior, clone) {
    ASSERT_ARG_COUNT("Behavior::clone()", 0);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Behavior, getMethod) {
    ASSERT_ARG_COUNT("Behavior::getMethod(method)", 1);
    ASSERT_ARG_TYPE("Behavior::getMethod(method)", 0, String);
    ObjClass* self = AS_CLASS(receiver);
    Value value;
    if (tableGet(&self->methods, AS_STRING(args[0]), &value)) {
        if (IS_NATIVE_METHOD(value)) RETURN_OBJ(AS_NATIVE_METHOD(value));
        else if(IS_CLOSURE(value)) RETURN_OBJ(newMethod(vm, self, AS_CLOSURE(value)));
        else {
            raiseError(vm, "Invalid method object found.");
            RETURN_NIL;
        }
    }
    else RETURN_NIL;
}

LOX_METHOD(Behavior, hasMethod) {
    ASSERT_ARG_COUNT("Behavior::hasMethod(method)", 1);
    ASSERT_ARG_TYPE("Behavior::hasMethod(method)", 0, String);
    ObjClass* self = AS_CLASS(receiver);
    Value value;
    RETURN_BOOL(tableGet(&self->methods, AS_STRING(args[0]), &value));
}

LOX_METHOD(Behavior, init) {
    raiseError(vm, "Cannot instantiate from class Behavior.");
    RETURN_NIL;
}

LOX_METHOD(Behavior, isBehavior) {
    ASSERT_ARG_COUNT("Behavior::isBehavior()", 0);
    RETURN_TRUE;
}

LOX_METHOD(Behavior, isClass) {
    ASSERT_ARG_COUNT("Behavior::isClass()", 0);
    RETURN_BOOL(AS_CLASS(receiver)->behavior == BEHAVIOR_CLASS || AS_CLASS(receiver)->behavior == BEHAVIOR_METACLASS);
}

LOX_METHOD(Behavior, isMetaclass) {
    ASSERT_ARG_COUNT("Behavior::isMetaclass()", 0);
    RETURN_BOOL(AS_CLASS(receiver)->behavior == BEHAVIOR_METACLASS);
}

LOX_METHOD(Behavior, isNative) {
    ASSERT_ARG_COUNT("Behavior::isNative()", 0);
    RETURN_BOOL(AS_CLASS(receiver)->isNative);
}

LOX_METHOD(Behavior, isTrait) {
    ASSERT_ARG_COUNT("Behavior::isTrait()", 0);
    RETURN_BOOL(AS_CLASS(receiver)->behavior == BEHAVIOR_TRAIT);
}

LOX_METHOD(Behavior, methods) {
    ASSERT_ARG_COUNT("Behavior::methods()", 0);
    ObjClass* self = AS_CLASS(receiver);
    ObjDictionary* dict = newDictionary(vm);
    push(vm, OBJ_VAL(dict));

    for (int i = 0; i < self->methods.capacity; i++) {
        Entry* entry = &self->methods.entries[i];
        if (entry != NULL) {
            if(IS_NATIVE_METHOD(entry->value)) dictSet(vm, dict, OBJ_VAL(entry->key), entry->value);
            else if (IS_CLOSURE(entry->value)) {
                ObjMethod* method = newMethod(vm, self, AS_CLOSURE(entry->value));
                push(vm, OBJ_VAL(method));
                dictSet(vm, dict, OBJ_VAL(entry->key), OBJ_VAL(method));
                pop(vm);
            }
        }
    }

    pop(vm);
    RETURN_OBJ(dict);
}

LOX_METHOD(Behavior, name) {
    ASSERT_ARG_COUNT("Behavior::name()", 0);
    RETURN_OBJ(AS_CLASS(receiver)->name);
}

LOX_METHOD(Behavior, traits) {
    ASSERT_ARG_COUNT("Behavior::traits()", 0);
    ObjClass* self = AS_CLASS(receiver);
    ObjArray* traits = newArray(vm);
    for (int i = 0; i < self->traits.count; i++) {
        valueArrayWrite(vm, &traits->elements, self->traits.values[i]);
    }
    RETURN_OBJ(traits);
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

LOX_METHOD(BoundMethod, arity) {
    ASSERT_ARG_COUNT("BoundMethod::arity()", 0);
    RETURN_INT(AS_BOUND_METHOD(receiver)->method->function->arity);
}

LOX_METHOD(BoundMethod, clone) {
    ASSERT_ARG_COUNT("BoundMethod::clone()", 0);
    RETURN_OBJ(receiver);
}

LOX_METHOD(BoundMethod, init) {
    ASSERT_ARG_COUNT("BoundMethod::init(object, method)", 2);
    if (IS_METHOD(args[1])) {
        ObjMethod* method = AS_METHOD(args[1]);
        if (!isObjInstanceOf(vm, args[0], method->behavior)) {
            raiseError(vm, "Cannot bound method to object.");
            RETURN_NIL;
        }
        RETURN_OBJ(newBoundMethod(vm, args[0], method->closure));
    }
    else if (IS_STRING(args[1])) {
        ObjClass* klass = getObjClass(vm, args[0]);
        Value value;
        if (!tableGet(&klass->methods, AS_STRING(args[1]), &value)) {
            raiseError(vm, "Cannot bound method to object.");
            RETURN_NIL;
        }
        RETURN_OBJ(newBoundMethod(vm, args[0], AS_CLOSURE(value)));
    }
    else {
        raiseError(vm, "method BoundMethod::init(object, method) expects argument 2 to be a method or string.");
        RETURN_NIL;
    }
}

LOX_METHOD(BoundMethod, isVariadic) {
    ASSERT_ARG_COUNT("BoundMethod::isVariadic()", 0);
    RETURN_BOOL(AS_BOUND_METHOD(receiver)->method->function->arity == -1);
}

LOX_METHOD(BoundMethod, name) {
    ASSERT_ARG_COUNT("BoundMethod::name()", 0);
    ObjBoundMethod* boundMethod = AS_BOUND_METHOD(receiver);
    RETURN_STRING_FMT("%s::%s", getObjClass(vm, boundMethod->receiver)->name->chars, boundMethod->method->function->name->chars);
}

LOX_METHOD(BoundMethod, receiver) {
    ASSERT_ARG_COUNT("BoundMethod::receiver()", 0);
    RETURN_VAL(AS_BOUND_METHOD(receiver)->receiver);
}

LOX_METHOD(BoundMethod, toString) {
    ASSERT_ARG_COUNT("BoundMethod::toString()", 0);
    ObjBoundMethod* boundMethod = AS_BOUND_METHOD(receiver);
    RETURN_STRING_FMT("<bound method %s::%s>", getObjClass(vm, boundMethod->receiver)->name->chars, boundMethod->method->function->name->chars);
}

LOX_METHOD(BoundMethod, upvalueCount) {
    ASSERT_ARG_COUNT("BoundMethod::upvalueCount()", 0);
    RETURN_INT(AS_BOUND_METHOD(receiver)->method->upvalueCount);
}

LOX_METHOD(Class, getField) {
    ASSERT_ARG_COUNT("Class::getField(field)", 1);
    ASSERT_ARG_TYPE("Class::getField(field)", 0, String);
    if (IS_CLASS(receiver)) {
        ObjClass* klass = AS_CLASS(receiver);
        Value value;
        if (tableGet(&klass->fields, AS_STRING(args[0]), &value)) RETURN_VAL(value);
    }
    RETURN_NIL;
}

LOX_METHOD(Class, hasField) {
    ASSERT_ARG_COUNT("Class::hasField(field)", 1);
    ASSERT_ARG_TYPE("Class::hasField(field)", 0, String);
    if (IS_CLASS(receiver)) {
        ObjClass* klass = AS_CLASS(receiver);
        Value value;
        RETURN_BOOL(tableGet(&klass->fields, AS_STRING(args[0]), &value));
    }
    RETURN_FALSE;
}

LOX_METHOD(Class, init) {
    ASSERT_ARG_COUNT("Class::init(name, superclass, traits)", 3);
    ASSERT_ARG_TYPE("Class::init(name, superclass, traits)", 0, String);
    ASSERT_ARG_TYPE("Class::init(name, superclass, traits)", 1, Class);
    ASSERT_ARG_TYPE("Class::init(name, superclass, traits)", 2, Array);
    ObjClass* klass = newClass(vm, AS_STRING(args[0]));
    bindSuperclass(vm, klass, AS_CLASS(args[1]));
    implementTraits(vm, klass, &AS_ARRAY(args[2])->elements);
    RETURN_OBJ(klass);
}

LOX_METHOD(Class, instanceOf) {
    ASSERT_ARG_COUNT("Class::instanceOf(class)", 1);
    if (!IS_CLASS(args[0])) RETURN_FALSE;
    RETURN_BOOL(isClassExtendingSuperclass(AS_CLASS(receiver)->obj.klass, AS_CLASS(args[0])));
}

LOX_METHOD(Class, isClass) {
    ASSERT_ARG_COUNT("Class::isClass()", 0);
    RETURN_TRUE;
}

LOX_METHOD(Class, memberOf) {
    ASSERT_ARG_COUNT("Class::memberOf(class)", 1);
    if (!IS_CLASS(args[0])) RETURN_FALSE;
    RETURN_BOOL(AS_CLASS(receiver)->obj.klass == AS_CLASS(args[0]));
}

LOX_METHOD(Class, superclass) {
    ASSERT_ARG_COUNT("Class::superclass()", 0);
    ObjClass* self = AS_CLASS(receiver);
    if (self->superclass == NULL) RETURN_NIL;
    RETURN_OBJ(self->superclass);
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

LOX_METHOD(FloatClass, parse) {
    ASSERT_ARG_COUNT("Float class::parse(intString)", 1);
    ASSERT_ARG_TYPE("Float class::parse(intString)", 0, String);
    ObjString* floatString = AS_STRING(args[0]);
    if (strcmp(floatString->chars, "0") == 0 || strcmp(floatString->chars, "0.0") == 0) RETURN_NUMBER(0.0);

    char* endPoint = NULL;
    double floatValue = strtod(floatString->chars, &endPoint);
    if (floatValue == 0.0) {
        raiseError(vm, "Failed to parse float from input string.");
        RETURN_NIL;
    }
    RETURN_NUMBER(floatValue);
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
    RETURN_INT(AS_INT(receiver));
}

LOX_METHOD(Int, downTo) {
    ASSERT_ARG_COUNT("Int::downTo(to, closure)", 2);
    ASSERT_ARG_TYPE("Int::downTo(to, closure)", 0, Int);
    ASSERT_ARG_TYPE("Int::downTo(to, closure)", 1, Closure);
    int self = AS_INT(receiver);
    int to = AS_INT(args[0]);
    ObjClosure* closure = AS_CLOSURE(args[1]);

    for (int i = self; i >= to; i--) {
        callReentrant(vm, receiver, OBJ_VAL(closure), INT_VAL(i));
    }
    RETURN_NIL;
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

LOX_METHOD(Int, timesRepeat) {
    ASSERT_ARG_COUNT("Int::timesRepeat(closure)", 1);
    ASSERT_ARG_TYPE("Int::timesRepeat(closure)", 0, Closure);
    int self = AS_INT(receiver);
    ObjClosure* closure = AS_CLOSURE(args[0]);

    for (int i = 0; i < self; i++) {
        callReentrant(vm, receiver, OBJ_VAL(closure), INT_VAL(i));
    }
    RETURN_NIL;
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

LOX_METHOD(Int, toOctal) {
    ASSERT_ARG_COUNT("Int::toOctal()", 0);
    char buffer[16];
    int length = 16;
    _itoa_s(AS_INT(receiver), buffer, length, 8);
    RETURN_STRING(buffer, length);
}

LOX_METHOD(Int, toString) {
    ASSERT_ARG_COUNT("Int::toString()", 0);
    RETURN_STRING_FMT("%d", AS_INT(receiver));
}

LOX_METHOD(Int, upTo) {
    ASSERT_ARG_COUNT("Int::upTo(to, closure)", 2);
    ASSERT_ARG_TYPE("Int::upTo(to, closure)", 0, Int);
    ASSERT_ARG_TYPE("Int::upTo(to, closure)", 1, Closure);
    int self = AS_INT(receiver);
    int to = AS_INT(args[0]);
    ObjClosure* closure = AS_CLOSURE(args[1]);

    for (int i = self; i <= to; i++) {
        callReentrant(vm, receiver, OBJ_VAL(closure), INT_VAL(i));
    }
    RETURN_NIL;
}

LOX_METHOD(IntClass, parse) {
    ASSERT_ARG_COUNT("Int class::parse(intString)", 1);
    ASSERT_ARG_TYPE("Int class::parse(intString)", 0, String);
    ObjString* intString = AS_STRING(args[0]);
    if (strcmp(intString->chars, "0") == 0) RETURN_INT(0);

    char* endPoint = NULL;
    int intValue = (int)strtol(intString->chars, &endPoint, 10);
    if (intValue == 0) {
        raiseError(vm, "Failed to parse int from input string.");
        RETURN_NIL;
    }
    RETURN_INT(intValue);
}

LOX_METHOD(Metaclass, getClass) {
    ASSERT_ARG_COUNT("Metaclass::getClass()", 0);
    RETURN_OBJ(vm->metaclassClass);
}

LOX_METHOD(Metaclass, getClassName) {
    ASSERT_ARG_COUNT("Metaclass::getClassName()", 0);
    RETURN_OBJ(vm->metaclassClass->name);
}

LOX_METHOD(Metaclass, instanceOf) {
    ASSERT_ARG_COUNT("Metaclass::instanceOf(class)", 1);
    if (!IS_CLASS(args[0])) RETURN_FALSE;
    RETURN_BOOL(isClassExtendingSuperclass(vm->metaclassClass, AS_CLASS(args[0])));
}

LOX_METHOD(Metaclass, isMetaclass) {
    ASSERT_ARG_COUNT("Metaclass::isMetaclass()", 0);
    RETURN_TRUE;
}

LOX_METHOD(Metaclass, memberOf) {
    ASSERT_ARG_COUNT("Metaclass::memberOf(class)", 1);
    if (!IS_CLASS(args[0])) RETURN_FALSE;
    RETURN_BOOL(AS_CLASS(args[0]) == vm->metaclassClass);
}

LOX_METHOD(Metaclass, namedInstance) {
    ASSERT_ARG_COUNT("Metaclass::namedInstance()", 0);
    ObjClass* metaclass = AS_CLASS(receiver);
    ObjString* className = subString(vm, metaclass->name, 0, metaclass->name->length - 7);
    RETURN_OBJ(getNativeClass(vm, vm->langNamespace->fullName->chars, className->chars));
}

LOX_METHOD(Metaclass, superclass) {
    ASSERT_ARG_COUNT("Metaclass::superclass()", 0);
    RETURN_OBJ(AS_CLASS(receiver)->superclass);
}

LOX_METHOD(Metaclass, toString) {
    ASSERT_ARG_COUNT("Metaclass::toString()", 0);
    RETURN_STRING_FMT("<metaclass %s>", AS_CLASS(receiver)->name->chars);
}

LOX_METHOD(Method, arity) {
    ASSERT_ARG_COUNT("Method::arity()", 0);
    if (IS_NATIVE_METHOD(receiver)) {
        RETURN_INT(AS_NATIVE_METHOD(receiver)->arity);
    }
    RETURN_INT(AS_METHOD(receiver)->closure->function->arity);
}

LOX_METHOD(Method, behavior) {
    ASSERT_ARG_COUNT("Method::behavior()", 0);
    if (IS_NATIVE_METHOD(receiver)) {
        RETURN_OBJ(AS_NATIVE_METHOD(receiver)->klass);
    }
    RETURN_OBJ(AS_METHOD(receiver)->behavior);
}

LOX_METHOD(Method, bind) {
    ASSERT_ARG_COUNT("Method::bind(receiver)", 1);
    if (IS_NATIVE_METHOD(receiver)) {
        raiseError(vm, "Cannot bind receiver to native method.");
        RETURN_NIL;
    }
    RETURN_OBJ(newBoundMethod(vm, args[1], AS_METHOD(receiver)->closure));
}

LOX_METHOD(Method, clone) {
    ASSERT_ARG_COUNT("Method::clone()", 0);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Method, init) {
    raiseError(vm, "Cannot instantiate from class Method.");
    RETURN_NIL;
}

LOX_METHOD(Method, isNative) {
    ASSERT_ARG_COUNT("method::isNative()", 0);
    RETURN_BOOL(IS_NATIVE_METHOD(receiver));
}

LOX_METHOD(Method, isVariadic) {
    ASSERT_ARG_COUNT("Method::isVariadic()", 0);
    RETURN_BOOL(AS_METHOD(receiver)->closure->function->arity == -1);
}

LOX_METHOD(Method, name) {
    ASSERT_ARG_COUNT("Method::name()", 0);
    if (IS_NATIVE_METHOD(receiver)) {
        ObjNativeMethod* nativeMethod = AS_NATIVE_METHOD(receiver);
        RETURN_STRING_FMT("%s::%s", nativeMethod->klass->name->chars, nativeMethod->name->chars);
    }
    ObjMethod* method = AS_METHOD(receiver);
    RETURN_STRING_FMT("%s::%s", method->behavior->name->chars, method->closure->function->name->chars);
}

LOX_METHOD(Method, toString) {
    ASSERT_ARG_COUNT("Method::toString()", 0);
    if (IS_NATIVE_METHOD(receiver)) {
        ObjNativeMethod* nativeMethod = AS_NATIVE_METHOD(receiver);
        RETURN_STRING_FMT("<method: %s::%s>", nativeMethod->klass->name->chars, nativeMethod->name->chars);
    }
    ObjMethod* method = AS_METHOD(receiver);
    RETURN_STRING_FMT("<method %s::%s>", method->behavior->name->chars, method->closure->function->name->chars);
}

LOX_METHOD(Namespace, clone) {
    ASSERT_ARG_COUNT("Namespace::clone()", 0);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Namespace, enclosing) {
    ASSERT_ARG_COUNT("Namespace::enclosing()", 0);
    ObjNamespace* self = AS_NAMESPACE(receiver);
    if (self->enclosing != NULL && self->enclosing->enclosing != NULL) RETURN_OBJ(self->enclosing);
    RETURN_NIL;
}

LOX_METHOD(Namespace, fullName) {
    ASSERT_ARG_COUNT("Namespace::fullName()", 0);
    ObjNamespace* self = AS_NAMESPACE(receiver);
    RETURN_OBJ(self->fullName);
}

LOX_METHOD(Namespace, shortName) {
    ASSERT_ARG_COUNT("Namespace::shortName()", 0);
    ObjNamespace* self = AS_NAMESPACE(receiver);
    RETURN_OBJ(self->shortName);
}

LOX_METHOD(Namespace, init) {
    raiseError(vm, "Cannot instantiate from class Namespace.");
    RETURN_NIL;
}

LOX_METHOD(Namespace, toString) {
    ASSERT_ARG_COUNT("Namespace::toString()", 0);
    ObjNamespace* self = AS_NAMESPACE(receiver);
    RETURN_STRING_FMT("<namespace %s>", self->fullName->chars);
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

LOX_METHOD(Number, compareTo) {
    ASSERT_ARG_COUNT("Number::compareTo(other)", 1);
    ASSERT_ARG_TYPE("Number::compareTo(other)", 0, Number);
    double self = AS_NUMBER(receiver);
    double other = AS_NUMBER(args[0]);
    if (self > other) RETURN_INT(1);
    else if (self < other) RETURN_INT(-1);
    else RETURN_INT(0);
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

LOX_METHOD(Number, isInfinity) {
    ASSERT_ARG_COUNT("Number::isInfinity()", 0);
    double self = AS_NUMBER(receiver);
    RETURN_BOOL(self == INFINITY);
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

LOX_METHOD(Number, step) {
    ASSERT_ARG_COUNT("Number::step(to, by, closure)", 3);
    ASSERT_ARG_TYPE("Number::step(to, by, closure)", 0, Number);
    ASSERT_ARG_TYPE("Number::step(to, by, closure)", 1, Number);
    ASSERT_ARG_TYPE("Number::step(to, by, closure)", 2, Closure);
    double self = AS_NUMBER(receiver);
    double to = AS_NUMBER(args[0]);
    double by = AS_NUMBER(args[1]);
    ObjClosure* closure = AS_CLOSURE(args[2]);

    if (by == 0) raiseError(vm, "Step size cannot be 0");
    else {
        if (by > 0) {
            for (double num = self; num <= to; num += by) {
                callReentrant(vm, receiver, OBJ_VAL(closure), NUMBER_VAL(num));
            }
        }
        else {
            for (double num = self; num >= to; num += by) {
                callReentrant(vm, receiver, OBJ_VAL(closure), NUMBER_VAL(num));
            }
        }
    }
    RETURN_NIL;
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

LOX_METHOD(NumberClass, parse) {
    ASSERT_ARG_COUNT("Number class::parse(numberString)", 1);
    raiseError(vm, "Not implemented, subclass responsibility.");
    RETURN_NIL;
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

LOX_METHOD(Object, getField) {
    ASSERT_ARG_COUNT("Object::getField(field)", 1);
    ASSERT_ARG_TYPE("Object::getField(field)", 0, String);
    if (IS_INSTANCE(receiver)) {
        ObjInstance* instance = AS_INSTANCE(receiver);
        Value value;
        if (tableGet(&instance->fields, AS_STRING(args[0]), &value)) RETURN_VAL(value);
    }
    RETURN_NIL;
}

LOX_METHOD(Object, hasField) {
    ASSERT_ARG_COUNT("Object::hasField(field)", 1);
    ASSERT_ARG_TYPE("Object::hasField(field)", 0, String);
    if (IS_INSTANCE(receiver)) {
        ObjInstance* instance = AS_INSTANCE(receiver);
        Value value;
        RETURN_BOOL(tableGet(&instance->fields, AS_STRING(args[0]), &value));
    }
    RETURN_FALSE;
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

    ObjArray* array = newArray(vm);
    push(vm, OBJ_VAL(array));
    char* string = _strdup(self->chars);
    char* next = NULL;
    char* token = strtok_s(string, delimiter->chars, &next);
    while (token != NULL) {
        valueArrayWrite(vm, &array->elements, OBJ_VAL(newString(vm, token)));
        token = strtok_s(NULL, delimiter->chars, &next);
    }
    free(string);
    pop(vm);
    RETURN_OBJ(array);
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

LOX_METHOD(TComparable, compareTo) {
    raiseError(vm, "Not implemented, subclass responsibility.");
    RETURN_NIL;
}

LOX_METHOD(TComparable, equals) {
    ASSERT_ARG_COUNT("TComparable::equals(other)", 1);
    if (valuesEqual(receiver, args[0])) RETURN_TRUE;
    else {
        Value result = callReentrant(vm, receiver, getObjMethod(vm, receiver, "compareTo"), args[0]);
        RETURN_BOOL(result == 0);
    }
}

LOX_METHOD(Trait, getClass) {
    ASSERT_ARG_COUNT("Trait::getClass()", 0);
    RETURN_OBJ(vm->traitClass);
}

LOX_METHOD(Trait, getClassName) {
    ASSERT_ARG_COUNT("Trait::getClassName()", 0);
    RETURN_OBJ(vm->traitClass->name);
}

LOX_METHOD(Trait, init) {
    ASSERT_ARG_COUNT("Trait::init(name, traits)", 2);
    ASSERT_ARG_TYPE("Trait::init(name, traits)", 0, String);
    ASSERT_ARG_TYPE("Trait::init(name, traits)", 1, Array);
    ObjClass* trait = createTrait(vm, AS_STRING(args[0]));
    implementTraits(vm, trait, &AS_ARRAY(args[1])->elements);
    RETURN_OBJ(trait);
}

LOX_METHOD(Trait, instanceOf) {
    ASSERT_ARG_COUNT("Trait::instanceOf(trait)", 1);
    if (!IS_CLASS(args[0])) RETURN_FALSE;
    RETURN_BOOL(isClassExtendingSuperclass(vm->traitClass, AS_CLASS(args[0])));
}

LOX_METHOD(Trait, isTrait) {
    ASSERT_ARG_COUNT("Trait::isTrait()", 0);
    RETURN_TRUE;
}

LOX_METHOD(Trait, memberOf) {
    ASSERT_ARG_COUNT("Trait::memberOf(class)", 1);
    if (!IS_CLASS(args[0])) RETURN_FALSE;
    RETURN_BOOL(AS_CLASS(args[0]) == vm->traitClass);
}

LOX_METHOD(Trait, superclass) {
    ASSERT_ARG_COUNT("Trait::superclass()", 0);
    RETURN_NIL;
}

LOX_METHOD(Trait, toString) {
    ASSERT_ARG_COUNT("Trait::toString()", 0);
    RETURN_STRING_FMT("<trait %s>", AS_CLASS(receiver)->name->chars);
}

static void bindStringClass(VM* vm) {
    for (int i = 0; i < vm->strings.capacity; i++) {
        Entry* entry = &vm->strings.entries[i];
        if (entry->key == NULL) continue;
        entry->key->obj.klass = vm->stringClass;
    }
}

static void bindMethodClass(VM* vm, ObjClass* klass) {
    for (int i = 0; i < klass->methods.capacity; i++) {
        Entry* entry = &klass->methods.entries[i];
        if (entry->key == NULL) continue;
        if (IS_NATIVE_METHOD(entry->value)) {
            ObjNativeMethod* method = AS_NATIVE_METHOD(entry->value);
            method->obj.klass = vm->methodClass;
        }
    }
}

static void bindNamespaceClass(VM* vm) {
    for (int i = 0; i < vm->namespaces.capacity; i++) {
        Entry* entry = &vm->namespaces.entries[i];
        if (entry->key == NULL) continue;
        entry->key->obj.klass = vm->namespaceClass;
    }
}

void registerLangPackage(VM* vm) {
    vm->rootNamespace = defineRootNamespace(vm);
    vm->cloxNamespace = defineNativeNamespace(vm, "clox", vm->rootNamespace);
    vm->stdNamespace = defineNativeNamespace(vm, "std", vm->cloxNamespace);
    vm->langNamespace = defineNativeNamespace(vm, "lang", vm->stdNamespace);
    vm->currentNamespace = vm->langNamespace;

    vm->objectClass = defineSpecialClass(vm, "Object", BEHAVIOR_CLASS);
    DEF_METHOD(vm->objectClass, Object, clone, 0);
    DEF_METHOD(vm->objectClass, Object, equals, 1);
    DEF_METHOD(vm->objectClass, Object, getClass, 0);
    DEF_METHOD(vm->objectClass, Object, getClassName, 0);
    DEF_METHOD(vm->objectClass, Object, getField, 1);
    DEF_METHOD(vm->objectClass, Object, hasField, 1);
    DEF_METHOD(vm->objectClass, Object, hashCode, 0);
    DEF_METHOD(vm->objectClass, Object, instanceOf, 1);
    DEF_METHOD(vm->objectClass, Object, memberOf, 1);
    DEF_METHOD(vm->objectClass, Object, toString, 0);

    ObjClass* behaviorClass = defineSpecialClass(vm, "Behavior", BEHAVIOR_CLASS);
    inheritSuperclass(vm, behaviorClass, vm->objectClass);
    DEF_METHOD(behaviorClass, Behavior, clone, 0);
    DEF_METHOD(behaviorClass, Behavior, getMethod, 1);
    DEF_METHOD(behaviorClass, Behavior, hasMethod, 1);
    DEF_METHOD(behaviorClass, Behavior, init, 2);
    DEF_METHOD(behaviorClass, Behavior, isBehavior, 0);
    DEF_METHOD(behaviorClass, Behavior, isClass, 0);
    DEF_METHOD(behaviorClass, Behavior, isMetaclass, 0);
    DEF_METHOD(behaviorClass, Behavior, isNative, 0);
    DEF_METHOD(behaviorClass, Behavior, methods, 0);
    DEF_METHOD(behaviorClass, Behavior, name, 0);
    DEF_METHOD(behaviorClass, Behavior, traits, 0);

    vm->classClass = defineSpecialClass(vm, "Class", BEHAVIOR_CLASS);
    inheritSuperclass(vm, vm->classClass, behaviorClass);
    DEF_METHOD(vm->classClass, Class, getField, 1);
    DEF_METHOD(vm->classClass, Class, hasField, 1);
    DEF_METHOD(vm->classClass, Class, init, 3);
    DEF_METHOD(vm->classClass, Class, instanceOf, 1);
    DEF_METHOD(vm->classClass, Class, isClass, 0);
    DEF_METHOD(vm->classClass, Class, memberOf, 1);
    DEF_METHOD(vm->classClass, Class, superclass, 0);
    DEF_METHOD(vm->classClass, Class, toString, 0);

    vm->metaclassClass = defineSpecialClass(vm, "Metaclass", BEHAVIOR_METACLASS);
    inheritSuperclass(vm, vm->metaclassClass, behaviorClass);
    DEF_METHOD(vm->metaclassClass, Metaclass, getClass, 0);
    DEF_METHOD(vm->metaclassClass, Metaclass, getClassName, 0);
    DEF_METHOD(vm->metaclassClass, Metaclass, instanceOf, 1);
    DEF_METHOD(vm->metaclassClass, Metaclass, isMetaclass, 0);
    DEF_METHOD(vm->metaclassClass, Metaclass, memberOf, 1);
    DEF_METHOD(vm->metaclassClass, Metaclass, namedInstance, 0);
    DEF_METHOD(vm->metaclassClass, Metaclass, superclass, 0);
    DEF_METHOD(vm->metaclassClass, Metaclass, toString, 0);

    ObjClass* objectMetaclass = defineSpecialClass(vm, "Object class", BEHAVIOR_METACLASS);
    vm->objectClass->obj.klass = objectMetaclass;
    objectMetaclass->obj.klass = vm->classClass;
    inheritSuperclass(vm, objectMetaclass, vm->classClass);

    ObjClass* behaviorMetaclass = defineSpecialClass(vm, "Behavior class", BEHAVIOR_METACLASS);
    behaviorClass->obj.klass = behaviorMetaclass;
    behaviorMetaclass->obj.klass = vm->metaclassClass;
    inheritSuperclass(vm, behaviorMetaclass, objectMetaclass);

    ObjClass* classMetaclass = defineSpecialClass(vm, "Class class", BEHAVIOR_METACLASS);
    vm->classClass->obj.klass = classMetaclass;
    classMetaclass->obj.klass = vm->metaclassClass;
    inheritSuperclass(vm, classMetaclass, behaviorMetaclass);

    ObjClass* metaclassMetaclass = defineSpecialClass(vm, "Metaclass class", BEHAVIOR_METACLASS);
    vm->metaclassClass->obj.klass = metaclassMetaclass;
    metaclassMetaclass->obj.klass = vm->metaclassClass;
    inheritSuperclass(vm, metaclassMetaclass, behaviorMetaclass);

    vm->methodClass = defineNativeClass(vm, "Method");
    bindSuperclass(vm, vm->methodClass, vm->objectClass);
    DEF_METHOD(vm->methodClass, Method, arity, 0);
    DEF_METHOD(vm->methodClass, Method, behavior, 0);
    DEF_METHOD(vm->methodClass, Method, bind, 1);
    DEF_METHOD(vm->methodClass, Method, clone, 0);
    DEF_METHOD(vm->methodClass, Method, init, 0);
    DEF_METHOD(vm->methodClass, Method, isNative, 0);
    DEF_METHOD(vm->methodClass, Method, isVariadic, 0);
    DEF_METHOD(vm->methodClass, Method, name, 0);
    DEF_METHOD(vm->methodClass, Method, toString, 0);

    bindMethodClass(vm, vm->objectClass);
    bindMethodClass(vm, objectMetaclass);
    bindMethodClass(vm, behaviorClass);
    bindMethodClass(vm, behaviorMetaclass);
    bindMethodClass(vm, vm->classClass);
    bindMethodClass(vm, classMetaclass);
    bindMethodClass(vm, vm->metaclassClass);
    bindMethodClass(vm, metaclassMetaclass);

    vm->namespaceClass = defineNativeClass(vm, "Namespace");
    bindSuperclass(vm, vm->namespaceClass, vm->objectClass);
    DEF_METHOD(vm->namespaceClass, Namespace, clone, 0);
    DEF_METHOD(vm->namespaceClass, Namespace, enclosing, 0);
    DEF_METHOD(vm->namespaceClass, Namespace, fullName, 0);
    DEF_METHOD(vm->namespaceClass, Namespace, init, 0);
    DEF_METHOD(vm->namespaceClass, Namespace, shortName, 0);
    DEF_METHOD(vm->namespaceClass, Namespace, toString, 0);
    bindNamespaceClass(vm);

    vm->traitClass = defineNativeClass(vm, "Trait");
    bindSuperclass(vm, vm->traitClass, behaviorClass);
    DEF_METHOD(vm->traitClass, Trait, getClass, 0);
    DEF_METHOD(vm->traitClass, Trait, getClassName, 0);
    DEF_METHOD(vm->traitClass, Trait, init, 2);
    DEF_METHOD(vm->traitClass, Trait, instanceOf, 1);
    DEF_METHOD(vm->traitClass, Trait, isTrait, 0);
    DEF_METHOD(vm->traitClass, Trait, memberOf, 1);
    DEF_METHOD(vm->traitClass, Trait, superclass, 0);
    DEF_METHOD(vm->traitClass, Trait, toString, 0);

    vm->nilClass = defineNativeClass(vm, "Nil");
    bindSuperclass(vm, vm->nilClass, vm->objectClass);
    DEF_METHOD(vm->nilClass, Nil, clone, 0);
    DEF_METHOD(vm->nilClass, Nil, init, 0);
    DEF_METHOD(vm->nilClass, Nil, toString, 0);

    vm->boolClass = defineNativeClass(vm, "Bool");
    bindSuperclass(vm, vm->boolClass, vm->objectClass);
    DEF_METHOD(vm->boolClass, Bool, clone, 0);
    DEF_METHOD(vm->boolClass, Bool, init, 1);
    DEF_METHOD(vm->boolClass, Bool, toString, 0);

    ObjClass* comparableTrait = defineNativeTrait(vm, "TComparable");
    DEF_METHOD(comparableTrait, TComparable, compareTo, 1);
    DEF_METHOD(comparableTrait, TComparable, equals, 1);

    vm->numberClass = defineNativeClass(vm, "Number");
    bindSuperclass(vm, vm->numberClass, vm->objectClass);
    bindTrait(vm, vm->numberClass, comparableTrait);
    DEF_METHOD(vm->numberClass, Number, abs, 0);
    DEF_METHOD(vm->numberClass, Number, acos, 0);
    DEF_METHOD(vm->numberClass, Number, asin, 0);
    DEF_METHOD(vm->numberClass, Number, atan, 0);
    DEF_METHOD(vm->numberClass, Number, cbrt, 0);
    DEF_METHOD(vm->numberClass, Number, ceil, 0);
    DEF_METHOD(vm->numberClass, Number, clone, 0);
    DEF_METHOD(vm->numberClass, Number, compareTo, 1);
    DEF_METHOD(vm->numberClass, Number, cos, 0);
    DEF_METHOD(vm->numberClass, Number, exp, 1);
    DEF_METHOD(vm->numberClass, Number, floor, 0);
    DEF_METHOD(vm->numberClass, Number, hypot, 1);
    DEF_METHOD(vm->numberClass, Number, init, 1);
    DEF_METHOD(vm->numberClass, Number, isInfinity, 0);
    DEF_METHOD(vm->numberClass, Number, log, 0);
    DEF_METHOD(vm->numberClass, Number, log2, 0);
    DEF_METHOD(vm->numberClass, Number, log10, 0);
    DEF_METHOD(vm->numberClass, Number, max, 1);
    DEF_METHOD(vm->numberClass, Number, min, 1);
    DEF_METHOD(vm->numberClass, Number, pow, 1);
    DEF_METHOD(vm->numberClass, Number, round, 0);
    DEF_METHOD(vm->numberClass, Number, sin, 0);
    DEF_METHOD(vm->numberClass, Number, sqrt, 0);
    DEF_METHOD(vm->numberClass, Number, step, 3);
    DEF_METHOD(vm->numberClass, Number, tan, 0);
    DEF_METHOD(vm->numberClass, Number, toInt, 0);
    DEF_METHOD(vm->numberClass, Number, toString, 0);

    ObjClass* numberMetaclass = vm->numberClass->obj.klass;
    setClassProperty(vm, vm->numberClass, "infinity", NUMBER_VAL(INFINITY));
    setClassProperty(vm, vm->numberClass, "pi", NUMBER_VAL(3.14159265358979323846264338327950288));
    DEF_METHOD(numberMetaclass, NumberClass, parse, 1);

    vm->intClass = defineNativeClass(vm, "Int");
    bindSuperclass(vm, vm->intClass, vm->numberClass);
    DEF_METHOD(vm->intClass, Int, abs, 0);
    DEF_METHOD(vm->intClass, Int, clone, 0);
    DEF_METHOD(vm->intClass, Int, downTo, 2);
    DEF_METHOD(vm->intClass, Int, factorial, 0);
    DEF_METHOD(vm->intClass, Int, gcd, 1);
    DEF_METHOD(vm->intClass, Int, init, 1);
    DEF_METHOD(vm->intClass, Int, isEven, 0);
    DEF_METHOD(vm->intClass, Int, isOdd, 0);
    DEF_METHOD(vm->intClass, Int, lcm, 1);
    DEF_METHOD(vm->intClass, Int, timesRepeat, 1);
    DEF_METHOD(vm->intClass, Int, toBinary, 0);
    DEF_METHOD(vm->intClass, Int, toFloat, 0);
    DEF_METHOD(vm->intClass, Int, toHexadecimal, 0);
    DEF_METHOD(vm->intClass, Int, toOctal, 0);
    DEF_METHOD(vm->intClass, Int, toString, 0);
    DEF_METHOD(vm->intClass, Int, upTo, 2);

    ObjClass* intMetaclass = vm->intClass->obj.klass;
    setClassProperty(vm, vm->intClass, "max", INT_VAL(INT32_MAX));
    setClassProperty(vm, vm->intClass, "min", INT_VAL(INT32_MIN));
    DEF_METHOD(intMetaclass, IntClass, parse, 1);

    vm->floatClass = defineNativeClass(vm, "Float");
    bindSuperclass(vm, vm->floatClass, vm->numberClass);
    DEF_METHOD(vm->floatClass, Float, clone, 0);
    DEF_METHOD(vm->floatClass, Float, init, 1);
    DEF_METHOD(vm->floatClass, Float, toString, 0);

    ObjClass* floatMetaclass = vm->floatClass->obj.klass;
    setClassProperty(vm, vm->floatClass, "max", NUMBER_VAL(DBL_MAX));
    setClassProperty(vm, vm->floatClass, "min", NUMBER_VAL(DBL_MIN));
    DEF_METHOD(floatMetaclass, FloatClass, parse, 1);

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
    bindStringClass(vm);

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

    vm->boundMethodClass = defineNativeClass(vm, "BoundMethod");
    bindSuperclass(vm, vm->boundMethodClass, vm->objectClass);
    DEF_METHOD(vm->boundMethodClass, BoundMethod, arity, 0);
    DEF_METHOD(vm->boundMethodClass, BoundMethod, clone, 0);
    DEF_METHOD(vm->boundMethodClass, BoundMethod, init, 2);
    DEF_METHOD(vm->boundMethodClass, BoundMethod, name, 0);
    DEF_METHOD(vm->boundMethodClass, BoundMethod, receiver, 0);
    DEF_METHOD(vm->boundMethodClass, BoundMethod, toString, 0);
    DEF_METHOD(vm->boundMethodClass, BoundMethod, upvalueCount, 0);

    vm->currentNamespace = vm->rootNamespace;
}