#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lang.h"
#include "../common/os.h"
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

static double fetchObjectID(VM* vm, Obj* object) {
    ENSURE_OBJECT_ID(object);
    return (double)object->objectID;
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

LOX_METHOD(Behavior, __init__) {
    THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Cannot instantiate from class Behavior.");
}

LOX_METHOD(Behavior, clone) {
    ASSERT_ARG_COUNT("Behavior::clone()", 0);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Behavior, fullName) {
    ASSERT_ARG_COUNT("Behavior::fullName()", 0);
    ObjClass* self = AS_CLASS(receiver);
    if (self->namespace->isRoot) RETURN_OBJ(self->name);
    else RETURN_OBJ(self->fullName);
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
            THROW_EXCEPTION(clox.std.lang.MethodNotFoundException, "Invalid method object found");
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

LOX_METHOD(Behavior, isBehavior) {
    ASSERT_ARG_COUNT("Behavior::isBehavior()", 0);
    RETURN_TRUE;
}

LOX_METHOD(Behavior, isClass) {
    ASSERT_ARG_COUNT("Behavior::isClass()", 0);
    RETURN_BOOL(AS_CLASS(receiver)->behaviorType == BEHAVIOR_CLASS || AS_CLASS(receiver)->behaviorType == BEHAVIOR_METACLASS);
}

LOX_METHOD(Behavior, isMetaclass) {
    ASSERT_ARG_COUNT("Behavior::isMetaclass()", 0);
    RETURN_BOOL(AS_CLASS(receiver)->behaviorType == BEHAVIOR_METACLASS);
}

LOX_METHOD(Behavior, isNative) {
    ASSERT_ARG_COUNT("Behavior::isNative()", 0);
    RETURN_BOOL(AS_CLASS(receiver)->isNative);
}

LOX_METHOD(Behavior, isTrait) {
    ASSERT_ARG_COUNT("Behavior::isTrait()", 0);
    RETURN_BOOL(AS_CLASS(receiver)->behaviorType == BEHAVIOR_TRAIT);
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

LOX_METHOD(Behavior, __invoke__) {
    THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Cannot call from class Behavior.");
}

LOX_METHOD(Bool, __init__) {
    ASSERT_ARG_COUNT("Bool::__init__(value)", 1);
    ASSERT_ARG_TYPE("Bool::__init__(value)", 0, Bool);
    if (IS_BOOL(receiver)) RETURN_VAL(args[0]);
    else {
        ObjValueInstance* instance = AS_VALUE_INSTANCE(receiver);
        instance->value = args[0];
        RETURN_OBJ(instance);
    }
}

LOX_METHOD(Bool, clone) {
    ASSERT_ARG_COUNT("Bool::clone()", 0);
    if (IS_BOOL(receiver)) RETURN_VAL(receiver);
    else {
        ObjValueInstance* self = AS_VALUE_INSTANCE(receiver);
        RETURN_OBJ(newValueInstance(vm, self->value, self->obj.klass));
    }
}

LOX_METHOD(Bool, objectID) {
    ASSERT_ARG_COUNT("Bool::objectID()", 0);
    if (IS_BOOL(receiver)) RETURN_NUMBER(AS_BOOL(receiver) ? 2 : 3);
    else RETURN_NUMBER(fetchObjectID(vm, AS_OBJ(receiver)));
}

LOX_METHOD(Bool, toString) {
    ASSERT_ARG_COUNT("Bool::toString()", 0);
    if (AS_BOOL_INSTANCE(receiver)) RETURN_STRING("true", 4);
    else RETURN_STRING("false", 5);
}

LOX_METHOD(BoundMethod, __init__) {
    ASSERT_ARG_COUNT("BoundMethod::__init__(object, method)", 2);
    if (IS_METHOD(args[1])) {
        ObjMethod* method = AS_METHOD(args[1]);
        if (!isObjInstanceOf(vm, args[0], method->behavior)) {
            THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Cannot bound method to object.");
        }

        ObjBoundMethod* boundMethod = AS_BOUND_METHOD(receiver);
        boundMethod->receiver = args[0];
        boundMethod->method = OBJ_VAL(method->closure);
        RETURN_OBJ(boundMethod);
    }
    else if (IS_STRING(args[1])) {
        ObjClass* klass = getObjClass(vm, args[0]);
        Value value;
        if (!tableGet(&klass->methods, AS_STRING(args[1]), &value)) {
            THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Cannot bound method to object.");
        }

        ObjBoundMethod* boundMethod = AS_BOUND_METHOD(receiver);
        boundMethod->receiver = args[0];
        boundMethod->method = value;
        RETURN_OBJ(boundMethod);
    }
    else {
        THROW_EXCEPTION(clox.std.lang.IllegalArgumentException, "method BoundMethod::__init__(object, method) expects argument 2 to be a method or string.");
    }
}

LOX_METHOD(BoundMethod, arity) {
    ASSERT_ARG_COUNT("BoundMethod::arity()", 0);
    Value method = AS_BOUND_METHOD(receiver)->method;
    RETURN_INT(IS_NATIVE_METHOD(method) ? AS_NATIVE_METHOD(method)->arity : AS_CLOSURE(method)->function->arity);
}

LOX_METHOD(BoundMethod, clone) {
    ASSERT_ARG_COUNT("BoundMethod::clone()", 0);
    RETURN_OBJ(receiver);
}

LOX_METHOD(BoundMethod, isAsync) {
    ASSERT_ARG_COUNT("BoundMethod::isAsync()", 0);
    Value method = AS_BOUND_METHOD(receiver)->method;
    RETURN_BOOL(IS_NATIVE_METHOD(method) ? AS_NATIVE_METHOD(method)->isAsync : AS_CLOSURE(method)->function->isAsync);
}

LOX_METHOD(BoundMethod, isNative) { 
    ASSERT_ARG_COUNT("BoundMethod::isNative()", 0);
    RETURN_BOOL(AS_BOUND_METHOD(receiver)->isNative);
}

LOX_METHOD(BoundMethod, isVariadic) {
    ASSERT_ARG_COUNT("BoundMethod::isVariadic()", 0);
    Value method = AS_BOUND_METHOD(receiver)->method;
    int arity = IS_NATIVE_METHOD(method) ? AS_NATIVE_METHOD(method)->arity : AS_CLOSURE(method)->function->arity;
    RETURN_BOOL(arity == -1);
}

LOX_METHOD(BoundMethod, name) {
    ASSERT_ARG_COUNT("BoundMethod::name()", 0);
    ObjBoundMethod* boundMethod = AS_BOUND_METHOD(receiver);
    char* methodName = IS_NATIVE_METHOD(boundMethod->method) ? AS_NATIVE_METHOD(boundMethod->method)->name->chars : AS_CLOSURE(boundMethod->method)->function->name->chars;
    RETURN_STRING_FMT("%s::%s", getObjClass(vm, boundMethod->receiver)->name->chars, methodName);
}

LOX_METHOD(BoundMethod, receiver) {
    ASSERT_ARG_COUNT("BoundMethod::receiver()", 0);
    RETURN_VAL(AS_BOUND_METHOD(receiver)->receiver);
}

LOX_METHOD(BoundMethod, toString) {
    ASSERT_ARG_COUNT("BoundMethod::toString()", 0);
    ObjBoundMethod* boundMethod = AS_BOUND_METHOD(receiver);
    char* methodName = IS_NATIVE_METHOD(boundMethod->method) ? AS_NATIVE_METHOD(boundMethod->method)->name->chars : AS_CLOSURE(boundMethod->method)->function->name->chars;
    RETURN_STRING_FMT("<bound method %s::%s>", getObjClass(vm, boundMethod->receiver)->name->chars, methodName);
}

LOX_METHOD(BoundMethod, upvalueCount) {
    ASSERT_ARG_COUNT("BoundMethod::upvalueCount()", 0);
    Value method = AS_BOUND_METHOD(receiver)->method;
    if (!IS_CLOSURE(method)) RETURN_INT(0);
    RETURN_INT(AS_CLOSURE(method)->upvalueCount);
}

LOX_METHOD(BoundMethod, __invoke__) {
    ObjBoundMethod* self = AS_BOUND_METHOD(receiver);
    RETURN_VAL(callMethod(vm, self->method, argCount));
}

LOX_METHOD(Class, __init__) {
    ASSERT_ARG_COUNT("Class::__init__(name, superclass, traits)", 3);
    ASSERT_ARG_TYPE("Class::__init__(name, superclass, traits)", 0, String);
    ASSERT_ARG_TYPE("Class::__init__(name, superclass, traits)", 1, Class);
    ASSERT_ARG_TYPE("Class::__init__(name, superclass, traits)", 2, Array);

    ObjClass* klass = AS_CLASS(receiver);
    ObjString* name = AS_STRING(args[0]);
    ObjString* metaclassName = formattedString(vm, "%s class", name->chars);
    ObjClass* metaclass = createClass(vm, metaclassName, vm->metaclassClass, BEHAVIOR_METACLASS);

    initClass(vm, klass, name, metaclass, BEHAVIOR_CLASS);
    bindSuperclass(vm, klass, AS_CLASS(args[1]));
    implementTraits(vm, klass, &AS_ARRAY(args[2])->elements);
    RETURN_OBJ(klass);
}

LOX_METHOD(Class, getField) {
    ASSERT_ARG_COUNT("Class::getField(field)", 1);
    ASSERT_ARG_TYPE("Class::getField(field)", 0, String);
    if (IS_CLASS(receiver)) {
        ObjClass* klass = AS_CLASS(receiver);
        int index;
        if (idMapGet(&klass->indexes, AS_STRING(args[0]), &index)) {
            RETURN_VAL(klass->fields.values[index]);
        }
    }
    RETURN_NIL;
}

LOX_METHOD(Class, hasField) {
    ASSERT_ARG_COUNT("Class::hasField(field)", 1);
    ASSERT_ARG_TYPE("Class::hasField(field)", 0, String);
    if (IS_CLASS(receiver)) {
        ObjClass* klass = AS_CLASS(receiver);
        int index;
        RETURN_BOOL(idMapGet(&klass->indexes, AS_STRING(args[0]), &index));
    }
    RETURN_FALSE;
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
    ObjClass* self = AS_CLASS(receiver);
    if (self->namespace->isRoot) RETURN_STRING_FMT("<class %s>", self->name->chars);
    else RETURN_STRING_FMT("<class %s.%s>", self->namespace->fullName->chars, self->name->chars);
}

LOX_METHOD(Class, __invoke__) { 
    ObjClass* self = AS_CLASS(receiver);
    ObjInstance* instance = newInstance(vm, self);
    Value initMethod;
    push(vm, OBJ_VAL(instance));

    if (tableGet(&self->methods, vm->initString, &initMethod)) {
        callReentrantMethod(vm, receiver, initMethod, args);
    }
    pop(vm);
    RETURN_OBJ(instance);
}

LOX_METHOD(Exception, __init__) {
    ASSERT_ARG_COUNT("Exception::__init__(message)", 1);
    ASSERT_ARG_TYPE("Exception::__init__(message)", 0, String);
    ObjException* self = AS_EXCEPTION(receiver);
    self->message = AS_STRING(args[0]);
    RETURN_OBJ(self);
}

LOX_METHOD(Exception, message) {
    ASSERT_ARG_COUNT("Exception::message()", 0);
    ObjException* self = AS_EXCEPTION(receiver);
    RETURN_OBJ(self->message);
}

LOX_METHOD(Exception, toString) {
    ASSERT_ARG_COUNT("Exception::toString()", 0);
    ObjException* self = AS_EXCEPTION(receiver);
    RETURN_STRING_FMT("<Exception %s - %s>", self->obj.klass->name->chars, self->message->chars);
}

LOX_METHOD(Float, clone) {
    ASSERT_ARG_COUNT("Float::clone()", 0);
    if (IS_FLOAT(receiver)) RETURN_VAL(receiver);
    else {
        ObjValueInstance* self = AS_VALUE_INSTANCE(receiver);
        RETURN_OBJ(newValueInstance(vm, self->value, self->obj.klass));
    }
}

LOX_METHOD(Float, __init__) {
    ASSERT_ARG_COUNT("Float::__init__(value)", 1);
    ASSERT_ARG_TYPE("Float::__init__(value)", 0, Float);
    if (IS_FLOAT(receiver)) RETURN_VAL(args[0]);
    else {
        ObjValueInstance* instance = AS_VALUE_INSTANCE(receiver);
        instance->value = args[0];
        RETURN_OBJ(instance);
    }
}

LOX_METHOD(Float, toString) {
    ASSERT_ARG_COUNT("Float::toString()", 0);
    RETURN_STRING_FMT("%g", AS_FLOAT_INSTANCE(receiver));
}

LOX_METHOD(FloatClass, parse) {
    ASSERT_ARG_COUNT("Float class::parse(intString)", 1);
    ASSERT_ARG_TYPE("Float class::parse(intString)", 0, String);
    ObjString* floatString = AS_STRING(args[0]);
    if (strcmp(floatString->chars, "0") == 0 || strcmp(floatString->chars, "0.0") == 0) RETURN_NUMBER(0.0);

    char* endPoint = NULL;
    double floatValue = strtod(floatString->chars, &endPoint);
    if (floatValue == 0.0) THROW_EXCEPTION(clox.std.lang.FormatException, "Failed to parse float from input string.");
    RETURN_NUMBER(floatValue);
}

LOX_METHOD(Function, __init__) {
    ASSERT_ARG_COUNT("Function::__init__(name, closure)", 2);
    ASSERT_ARG_TYPE("Function::__init__(name, closure)", 0, String);
    ASSERT_ARG_TYPE("Function::__init__(name, closure)", 1, Closure);

    ObjClosure* self = AS_CLOSURE(receiver);
    ObjString* name = AS_STRING(args[0]);
    ObjClosure* closure = AS_CLOSURE(args[1]);

    int index;
    if (idMapGet(&vm->currentModule->valIndexes, name, &index)) {
        THROW_EXCEPTION_FMT(clox.std.lang.UnsupportedOperationException, "Function %s already exists.", name->chars);
    }
    idMapSet(vm, &vm->currentModule->valIndexes, name, vm->currentModule->valFields.count);
    valueArrayWrite(vm, &vm->currentModule->valFields, OBJ_VAL(self));

    initClosure(vm, self, closure->function);
    closure->function->name = name;
    RETURN_OBJ(self);
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

LOX_METHOD(Function, isAnonymous) {
    ASSERT_ARG_COUNT("Function::isAnonymous()", 0);
    if (IS_NATIVE_FUNCTION(receiver)) RETURN_FALSE;
    RETURN_BOOL(AS_CLOSURE(receiver)->function->name->length == 0);
}

LOX_METHOD(Function, isAsync) {
    ASSERT_ARG_COUNT("Function::isAsync()", 0);
    if (IS_NATIVE_FUNCTION(receiver)) RETURN_FALSE;
    RETURN_BOOL(AS_CLOSURE(receiver)->function->isAsync);
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
        RETURN_STRING_FMT("<function %s>", AS_NATIVE_FUNCTION(receiver)->name->chars);
    }
    RETURN_STRING_FMT("<function %s>", AS_CLOSURE(receiver)->function->name->chars);
}

LOX_METHOD(Function, upvalueCount) {
    ASSERT_ARG_COUNT("Function::upvalueCount()", 0);
    RETURN_INT(AS_CLOSURE(receiver)->upvalueCount);
}

LOX_METHOD(Function, __invoke__) {
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

LOX_METHOD(Generator, __init__) {
    ASSERT_ARG_COUNT("Generator::__init__(callee, args)", 2);
    ASSERT_ARG_TCALLABLE("Generator::__init__(callee, args)", 0);
    ASSERT_ARG_TYPE("Generator::__init__(callee, args)", 1, Array);

    ObjGenerator* self = AS_GENERATOR(receiver);
    initGenerator(vm, self, args[0], AS_ARRAY(args[1]));
    RETURN_OBJ(self);
}

LOX_METHOD(Generator, getReceiver) {
    ASSERT_ARG_COUNT("Generator::getReceiver()", 0);
    RETURN_VAL(AS_GENERATOR(receiver)->frame->slots[0]);
}

LOX_METHOD(Generator, isFinished) { 
    ASSERT_ARG_COUNT("Generator::isFinished()", 0);
    RETURN_BOOL(AS_GENERATOR(receiver)->state == GENERATOR_RETURN);
}

LOX_METHOD(Generator, isReady) {
    ASSERT_ARG_COUNT("Generator::isReady()", 0);
    RETURN_BOOL(AS_GENERATOR(receiver)->state == GENERATOR_START);
}

LOX_METHOD(Generator, isSuspended) {
    ASSERT_ARG_COUNT("Generator::isSuspended()", 0);
    RETURN_BOOL(AS_GENERATOR(receiver)->state == GENERATOR_YIELD);
}

LOX_METHOD(Generator, next) {
    ASSERT_ARG_COUNT("Generator::next()", 0);
    ObjGenerator* self = AS_GENERATOR(receiver);
    if (self->state == GENERATOR_RETURN) RETURN_OBJ(self);
    else if (self->state == GENERATOR_RESUME) THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Generator is already running.");
    else if (self->state == GENERATOR_THROW) THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Generator has already thrown an exception.");
    else {
        resumeGenerator(vm, self);
        RETURN_OBJ(self);
    }
}

LOX_METHOD(Generator, nextFinished) {
    ASSERT_ARG_COUNT("Generator::nextFinished()", 0);
    ObjGenerator* self = AS_GENERATOR(receiver);
    if (self->state == GENERATOR_RETURN) RETURN_TRUE;
    else if (self->state == GENERATOR_RESUME) THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Generator is already running.");
    else if (self->state == GENERATOR_THROW) THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Generator has already thrown an exception.");
    else {
        resumeGenerator(vm, self);
        RETURN_BOOL(self->state == GENERATOR_RETURN);
    }
}

LOX_METHOD(Generator, returns) {
    ASSERT_ARG_COUNT("Generator::returns(value)", 1);
    ObjGenerator* self = AS_GENERATOR(receiver);
    if (self->state == GENERATOR_RETURN) THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Generator has already returned.");
    else {
        self->state = GENERATOR_RETURN;
        self->value = args[0];
        RETURN_VAL(args[0]);
    }
}

LOX_METHOD(Generator, send) {
    ASSERT_ARG_COUNT("Generator::send(value)", 1);
    ObjGenerator* self = AS_GENERATOR(receiver);
    if (self->state == GENERATOR_RETURN) THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Generator has already returned.");
    else if (self->state == GENERATOR_RESUME) THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Generator is already running.");
    else if (self->state == GENERATOR_THROW) THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Generator has already thrown an exception.");
    else {
        self->value = args[0];
        resumeGenerator(vm, self);
        RETURN_OBJ(self);
    }
}

LOX_METHOD(Generator, setReceiver) {
    ASSERT_ARG_COUNT("Generator::setReceiver(receiver)", 1);
    ObjGenerator* self = AS_GENERATOR(receiver);
    self->frame->slots[0] = args[0];
    RETURN_NIL;
}

LOX_METHOD(Generator, step) {
    ASSERT_ARG_COUNT("Generator::step(argument)", 1);
    ObjGenerator* self = AS_GENERATOR(receiver);
    RETURN_VAL(stepGenerator(vm, AS_GENERATOR(receiver), args[0]));
}

LOX_METHOD(Generator, throws) {
    ASSERT_ARG_COUNT("Generator::throws(exception)", 1);
    ASSERT_ARG_TYPE("Generator::throws(exception)", 0, Exception);
    ObjGenerator* self = AS_GENERATOR(receiver);
    if (self->state == GENERATOR_RETURN) THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Generator has already returned.");
    else {
        ObjException* exception = AS_EXCEPTION(args[0]);
        self->state = GENERATOR_THROW;
        THROW_EXCEPTION(exception->obj.klass, exception->message->chars);
    }
}

LOX_METHOD(Generator, toString) {
    ASSERT_ARG_COUNT("Generator::toString()", 0);
    ObjGenerator* self = AS_GENERATOR(receiver);
    RETURN_STRING_FMT("<generator %s>", self->frame->closure->function->name->chars);
}

LOX_METHOD(Generator, __invoke__) {
    if(argCount > 1) THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Generator::() accepts 0 or 1 argument.");
    ObjGenerator* self = AS_GENERATOR(receiver);
    if (self->state == GENERATOR_RETURN) RETURN_OBJ(self);
    else if (self->state == GENERATOR_RESUME) THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Generator is already running.");
    else if (self->state == GENERATOR_THROW) THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Generator has already thrown an exception.");
    else {
        if(argCount == 1) self->value = args[0];
        resumeGenerator(vm, self);
        RETURN_OBJ(self);
    }
}

LOX_METHOD(GeneratorClass, run) { 
    ASSERT_ARG_COUNT("Generator class::run(callee, arguments)", 2);
    ASSERT_ARG_TCALLABLE("Generator class::run(callee, arguments)", 0);
    ASSERT_ARG_TYPE("Generator class::run(callee, arguments)", 1, Array);
    RETURN_VAL(runGeneratorAsync(vm, args[0], AS_ARRAY(args[1])));
}

LOX_METHOD(Int, __init__) {
    ASSERT_ARG_COUNT("Int::__init__(value)", 1);
    ASSERT_ARG_TYPE("Int::__init__(value)", 0, Int);
    if (IS_INT(receiver)) RETURN_VAL(args[0]);
    else {
        ObjValueInstance* instance = AS_VALUE_INSTANCE(receiver);
        instance->value = args[0];
        RETURN_OBJ(instance);
    }
}

LOX_METHOD(Int, abs) {
    ASSERT_ARG_COUNT("Int::abs()", 0);
    int self = AS_INT_INSTANCE(receiver);
    RETURN_INT(abs(self));
}

LOX_METHOD(Int, clone) {
    ASSERT_ARG_COUNT("Int::clone()", 0);
    if (IS_INT(receiver)) RETURN_VAL(receiver);
    else {
        ObjValueInstance* self = AS_VALUE_INSTANCE(receiver);
        RETURN_OBJ(newValueInstance(vm, self->value, self->obj.klass));
    }
}

LOX_METHOD(Int, downTo) {
    ASSERT_ARG_COUNT("Int::downTo(to, closure)", 2);
    ASSERT_ARG_TYPE("Int::downTo(to, closure)", 0, Int);
    ASSERT_ARG_TCALLABLE("Int::downTo(to, closure)", 1);
    int self = AS_INT_INSTANCE(receiver);
    int to = AS_INT_INSTANCE(args[0]);
    Value closure = args[1];

    for (int i = self; i >= to; i--) {
        callReentrantMethod(vm, receiver, closure, INT_VAL(i));
    }
    RETURN_NIL;
}

LOX_METHOD(Int, factorial) {
    ASSERT_ARG_COUNT("Int::factorial()", 0);
    int self = AS_INT_INSTANCE(receiver);;
    if (self < 0) THROW_EXCEPTION_FMT(clox.std.lang.ArithmeticException, "method Int::factorial() expects receiver to be a non negative integer but got %d.", self);
    RETURN_INT(factorial(self));
}

LOX_METHOD(Int, gcd) {
    ASSERT_ARG_COUNT("Int::gcd(other)", 1);
    ASSERT_ARG_TYPE("Int::gcd(other)", 0, Int);
    RETURN_INT(gcd(abs(AS_INT_INSTANCE(receiver)), abs(AS_INT_INSTANCE(args[0]))));
}

LOX_METHOD(Int, isEven) {
    ASSERT_ARG_COUNT("Int::isEven()", 0);
    RETURN_BOOL(AS_INT_INSTANCE(receiver) % 2 == 0);
}

LOX_METHOD(Int, isOdd) {
    ASSERT_ARG_COUNT("Int::isOdd()", 0);
    RETURN_BOOL(AS_INT_INSTANCE(receiver) % 2 != 0);
}

LOX_METHOD(Int, lcm) {
    ASSERT_ARG_COUNT("Int::lcm(other)", 1);
    ASSERT_ARG_TYPE("Int::lcm(other)", 0, Int);
    RETURN_INT(lcm(abs(AS_INT_INSTANCE(receiver)), abs(AS_INT_INSTANCE(args[0]))));
}

LOX_METHOD(Int, objectID) {
    ASSERT_ARG_COUNT("Int::objectID()", 0);
    if (IS_INT(receiver)) RETURN_NUMBER((double)(8 * (uint64_t)AS_INT_INSTANCE(receiver) + 4));
    else RETURN_NUMBER(fetchObjectID(vm, AS_OBJ(receiver)));
}

LOX_METHOD(Int, timesRepeat) {
    ASSERT_ARG_COUNT("Int::timesRepeat(closure)", 1);
    ASSERT_ARG_TCALLABLE("Int::timesRepeat(closure)", 0);
    int self = AS_INT_INSTANCE(receiver);
    Value closure = args[0];

    for (int i = 0; i < self; i++) {
        callReentrantMethod(vm, receiver, closure, INT_VAL(i));
    }
    RETURN_NIL;
}

LOX_METHOD(Int, toBinary) {
    ASSERT_ARG_COUNT("Int::toBinary()", 0);
    char buffer[32];
    int length = 32;
    _itoa_s(AS_INT_INSTANCE(receiver), buffer, length, 2);
    RETURN_STRING(buffer, length);
}

LOX_METHOD(Int, toFloat) {
    ASSERT_ARG_COUNT("Int::toFloat()", 0);
    RETURN_NUMBER((double)AS_INT_INSTANCE(receiver));
}

LOX_METHOD(Int, toHexadecimal) {
    ASSERT_ARG_COUNT("Int::toHexadecimal()", 0);
    char buffer[8];
    int length = 8;
    _itoa_s(AS_INT_INSTANCE(receiver), buffer, length, 16);
    RETURN_STRING(buffer, length);
}

LOX_METHOD(Int, toOctal) {
    ASSERT_ARG_COUNT("Int::toOctal()", 0);
    char buffer[16];
    int length = 16;
    _itoa_s(AS_INT_INSTANCE(receiver), buffer, length, 8);
    RETURN_STRING(buffer, length);
}

LOX_METHOD(Int, toString) {
    ASSERT_ARG_COUNT("Int::toString()", 0);
    RETURN_STRING_FMT("%d", AS_INT_INSTANCE(receiver));
}

LOX_METHOD(Int, upTo) {
    ASSERT_ARG_COUNT("Int::upTo(to, closure)", 2);
    ASSERT_ARG_TYPE("Int::upTo(to, closure)", 0, Int);
    ASSERT_ARG_TCALLABLE("Int::upTo(to, closure)", 1);
    int self = AS_INT_INSTANCE(receiver);
    int to = AS_INT_INSTANCE(args[0]);
    Value closure = args[1];

    for (int i = self; i <= to; i++) {
        callReentrantMethod(vm, receiver, closure, INT_VAL(i));
    }
    RETURN_NIL;
}

LOX_METHOD(Int, __add__) {
    ASSERT_ARG_COUNT("Int::+(other)", 1);
    ASSERT_ARG_TYPE("Int::+(other)", 0, Number);
    
    if (IS_INT_INSTANCE(args[0])) RETURN_INT(AS_INT_INSTANCE(receiver) + AS_INT_INSTANCE(args[0]));
    else RETURN_NUMBER((AS_NUMBER(receiver) + AS_NUMBER(args[0])));
}

LOX_METHOD(Int, __subtract__) {
    ASSERT_ARG_COUNT("Int::-(other)", 1);
    ASSERT_ARG_TYPE("Int::-(other)", 0, Number);
    if (IS_INT_INSTANCE(args[0])) RETURN_INT((AS_INT_INSTANCE(receiver) - AS_INT_INSTANCE(args[0])));
    else RETURN_NUMBER((AS_NUMBER(receiver) - AS_NUMBER(args[0])));
}

LOX_METHOD(Int, __multiply__) {
    ASSERT_ARG_COUNT("Int::*(other)", 1);
    ASSERT_ARG_TYPE("Int::*(other)", 0, Number);
    if (IS_INT_INSTANCE(args[0])) RETURN_INT((AS_INT_INSTANCE(receiver) * AS_INT_INSTANCE(args[0])));
    else RETURN_NUMBER((AS_NUMBER(receiver) * AS_NUMBER(args[0])));
}

LOX_METHOD(Int, __modulo__) {
    ASSERT_ARG_COUNT("Int::%(other)", 1);
    ASSERT_ARG_TYPE("Int::%(other)", 0, Number);
    if (IS_INT_INSTANCE(args[0])) RETURN_INT((AS_INT_INSTANCE(receiver) % AS_INT_INSTANCE(args[0])));
    else RETURN_NUMBER(fmod(AS_NUMBER(receiver), AS_NUMBER(args[0])));
}

LOX_METHOD(Int, __range__) { 
    ASSERT_ARG_COUNT("Int::..(other)", 1);
    ASSERT_ARG_TYPE("Int::..(other)", 0, Int);
    RETURN_OBJ(newRange(vm, AS_INT_INSTANCE(receiver), AS_INT_INSTANCE(args[0])));
}

LOX_METHOD(IntClass, parse) {
    ASSERT_ARG_COUNT("Int class::parse(intString)", 1);
    ASSERT_ARG_TYPE("Int class::parse(intString)", 0, String);
    ObjString* intString = AS_STRING(args[0]);
    if (strcmp(intString->chars, "0") == 0) RETURN_INT(0);

    char* endPoint = NULL;
    int intValue = (int)strtol(intString->chars, &endPoint, 10);
    if (intValue == 0) THROW_EXCEPTION(clox.std.lang.FormatException, "Failed to parse int from input string.");
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
    ObjClass* self = AS_CLASS(receiver);
    ObjString* className = subString(vm, self->fullName, 0, self->name->length - 7);
    RETURN_OBJ(getNativeClass(vm, className->chars));
}

LOX_METHOD(Metaclass, superclass) {
    ASSERT_ARG_COUNT("Metaclass::superclass()", 0);
    RETURN_OBJ(AS_CLASS(receiver)->superclass);
}

LOX_METHOD(Metaclass, toString) {
    ASSERT_ARG_COUNT("Metaclass::toString()", 0);
    ObjClass* self = AS_CLASS(receiver);
    if (self->namespace->isRoot) RETURN_STRING_FMT("<class %s>", self->name->chars);
    else RETURN_STRING_FMT("<metaclass %s.%s>", self->namespace->fullName->chars, self->name->chars);
}

LOX_METHOD(Method, __init__) {
    ASSERT_ARG_COUNT("Method::__init__(behavior, name, closure)", 3);
    ASSERT_ARG_TYPE("Method::__init__(behavior, name, closure)", 0, Class);
    ASSERT_ARG_TYPE("Method::__init__(behavior, name, closure)", 1, String);
    ASSERT_ARG_TYPE("Method::__init__(behavior, name, closure)", 2, Closure);

    ObjMethod* self = AS_METHOD(receiver);
    ObjClass* behavior = AS_CLASS(args[0]);
    ObjString* name = AS_STRING(args[1]);
    ObjClosure* closure = AS_CLOSURE(args[2]);

    Value value;
    if (tableGet(&behavior->methods, name, &value)) {
        THROW_EXCEPTION_FMT(clox.std.lang.UnsupportedOperationException, "Method %s already exists in behavior %s.", name->chars, behavior->fullName->chars);
    }
    tableSet(vm, &behavior->methods, name, OBJ_VAL(closure));

    self->behavior = behavior;
    self->closure = closure;
    self->closure->function->name = name;
    RETURN_OBJ(self);
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
    Value method = IS_NATIVE_METHOD(receiver) ? receiver : OBJ_VAL(AS_METHOD(receiver)->closure);
    RETURN_OBJ(newBoundMethod(vm, args[1], method));
}

LOX_METHOD(Method, clone) {
    ASSERT_ARG_COUNT("Method::clone()", 0);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Method, isAsync) {
    ASSERT_ARG_COUNT("Method::isAsync()", 0);
    RETURN_BOOL(IS_NATIVE_METHOD(receiver) ? AS_NATIVE_METHOD(receiver)->isAsync : AS_METHOD(receiver)->closure->function->isAsync);
}

LOX_METHOD(Method, isNative) {
    ASSERT_ARG_COUNT("Method::isNative()", 0);
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

LOX_METHOD(Namespace, __init__) {
    ASSERT_ARG_COUNT("Namespace::__init__(shortName, enclosing)", 2);
    ASSERT_ARG_TYPE("Namespace::__init__(shortName, enclosing)", 0, String);
    ASSERT_ARG_TYPE("Namespace::__init__(shortName, enclosing)", 1, Namespace);

    ObjNamespace* self = AS_NAMESPACE(receiver);
    ObjString* shortName = AS_STRING(args[0]);
    ObjNamespace* enclosing = AS_NAMESPACE(args[1]);

    Value value;
    if (tableGet(&enclosing->values, shortName, &value)) {
        THROW_EXCEPTION_FMT(clox.std.lang.UnsupportedOperationException, "Namespace %s already exists in enclosing namespace %s.", shortName->chars, enclosing->fullName->chars);
    }
    initNamespace(vm, self, shortName, enclosing);
    RETURN_OBJ(self);
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

LOX_METHOD(Namespace, toString) {
    ASSERT_ARG_COUNT("Namespace::toString()", 0);
    ObjNamespace* self = AS_NAMESPACE(receiver);
    RETURN_STRING_FMT("<namespace %s>", self->fullName->chars);
}

LOX_METHOD(Nil, __init__) {
    ASSERT_ARG_COUNT("Nil::__init__(value)", 1);
    ASSERT_ARG_TYPE("Nil::__init__(value)", 0, Nil);
    if (IS_NIL(receiver)) RETURN_NIL;
    else {
        ObjValueInstance* instance = AS_VALUE_INSTANCE(receiver);
        instance->value = args[0];
        RETURN_OBJ(instance);
    }
}

LOX_METHOD(Nil, clone) {
    ASSERT_ARG_COUNT("Nil::clone()", 0);
    if (IS_NIL(receiver)) RETURN_NIL;
    else {
        ObjValueInstance* self = AS_VALUE_INSTANCE(receiver);
        RETURN_OBJ(newValueInstance(vm, self->value, self->obj.klass));
    }
}

LOX_METHOD(Nil, objectID) {
    ASSERT_ARG_COUNT("Nil::objectID()", 0);
    if (IS_NIL(receiver)) RETURN_NUMBER(1);
    else RETURN_NUMBER(fetchObjectID(vm, AS_OBJ(receiver)));    
}

LOX_METHOD(Nil, toString) {
    ASSERT_ARG_COUNT("Nil::toString()", 0);
    RETURN_STRING("nil", 3);
}

LOX_METHOD(Number, __init__) {
    ASSERT_ARG_COUNT("Number::__init__(value)", 1);
    ASSERT_ARG_TYPE("Number::__init__(value)", 0, Number);
    if (IS_NUMBER(receiver)) RETURN_VAL(args[0]);
    else {
        ObjValueInstance* instance = AS_VALUE_INSTANCE(receiver);
        instance->value = args[0];
        RETURN_OBJ(instance);
    }
}

LOX_METHOD(Number, abs) {
    ASSERT_ARG_COUNT("Number::abs()", 0);
    RETURN_NUMBER(fabs(AS_NUMBER_INSTANCE(receiver)));
}

LOX_METHOD(Number, acos) {
    ASSERT_ARG_COUNT("Number::acos()", 0);
    RETURN_NUMBER(acos(AS_NUMBER_INSTANCE(receiver)));
}

LOX_METHOD(Number, asin) {
    ASSERT_ARG_COUNT("Number::asin()", 0);
    RETURN_NUMBER(asin(AS_NUMBER_INSTANCE(receiver)));
}

LOX_METHOD(Number, atan) {
    ASSERT_ARG_COUNT("Number::atan()", 0);
    RETURN_NUMBER(atan(AS_NUMBER_INSTANCE(receiver)));
}

LOX_METHOD(Number, cbrt) {
    ASSERT_ARG_COUNT("Number::cbrt()", 0);
    RETURN_NUMBER(cbrt(AS_NUMBER_INSTANCE(receiver)));
}

LOX_METHOD(Number, ceil) {
    ASSERT_ARG_COUNT("Number::ceil()", 0);
    RETURN_NUMBER(ceil(AS_NUMBER_INSTANCE(receiver)));
}

LOX_METHOD(Number, clone) {
    ASSERT_ARG_COUNT("Number::clone()", 0);
    if (IS_NUMBER(receiver)) RETURN_VAL(receiver);
    else {
        ObjValueInstance* self = AS_VALUE_INSTANCE(receiver);
        RETURN_OBJ(newValueInstance(vm, self->value, self->obj.klass));
    }
}

LOX_METHOD(Number, compareTo) {
    ASSERT_ARG_COUNT("Number::compareTo(other)", 1);
    ASSERT_ARG_TYPE("Number::compareTo(other)", 0, Number);
    double self = AS_NUMBER_INSTANCE(receiver);
    double other = AS_NUMBER_INSTANCE(args[0]);
    if (self > other) RETURN_INT(1);
    else if (self < other) RETURN_INT(-1);
    else RETURN_INT(0);
}

LOX_METHOD(Number, cos) {
    ASSERT_ARG_COUNT("Number::cos()", 0);
    RETURN_NUMBER(cos(AS_NUMBER_INSTANCE(receiver)));
}

LOX_METHOD(Number, exp) {
    ASSERT_ARG_COUNT("Number::exp()", 0);
    RETURN_NUMBER(exp(AS_NUMBER_INSTANCE(receiver)));
}

LOX_METHOD(Number, floor) {
    ASSERT_ARG_COUNT("Number::floor()", 0);
    RETURN_NUMBER(floor(AS_NUMBER_INSTANCE(receiver)));
}

LOX_METHOD(Number, hypot) {
    ASSERT_ARG_COUNT("Number::hypot(other)", 1);
    ASSERT_ARG_TYPE("Number::hypot(other)", 0, Number);
    RETURN_NUMBER(hypot(AS_NUMBER_INSTANCE(receiver), AS_NUMBER_INSTANCE(args[0])));
}

LOX_METHOD(Number, isInfinity) {
    ASSERT_ARG_COUNT("Number::isInfinity()", 0);
    double self = AS_NUMBER_INSTANCE(receiver);
    RETURN_BOOL(self == INFINITY);
}

LOX_METHOD(Number, log) {
    ASSERT_ARG_COUNT("Number::log()", 0);
    double self = AS_NUMBER_INSTANCE(receiver);
    if (self <= 0) THROW_EXCEPTION_FMT(clox.std.lang.ArithmeticException, "method Number::log() expects receiver to be a positive number but got %g.", self);
    RETURN_NUMBER(log(self));
}

LOX_METHOD(Number, log10) {
    ASSERT_ARG_COUNT("Number::log10()", 0);
    double self = AS_NUMBER_INSTANCE(receiver);
    if (self < 0) THROW_EXCEPTION_FMT(clox.std.lang.ArithmeticException, "method Number::log10() expects receiver to be a positive number but got %g.", self);
    RETURN_NUMBER(log10(self));
}

LOX_METHOD(Number, log2) {
    ASSERT_ARG_COUNT("Number::log2()", 0);
    double self = AS_NUMBER_INSTANCE(receiver);
    if (self < 0) THROW_EXCEPTION_FMT(clox.std.lang.ArithmeticException, "method Number::log2() expects receiver to be a positive number but got %g.", self);
    RETURN_NUMBER(log2(self));
}

LOX_METHOD(Number, max) {
    ASSERT_ARG_COUNT("Number::max(other)", 1);
    ASSERT_ARG_TYPE("Number::max(other)", 0, Number);
    RETURN_NUMBER(fmax(AS_NUMBER_INSTANCE(receiver), AS_NUMBER_INSTANCE(args[0])));
}

LOX_METHOD(Number, min) {
    ASSERT_ARG_COUNT("Number::min(other)", 1);
    ASSERT_ARG_TYPE("Number::min(other)", 0, Number);
    RETURN_NUMBER(fmin(AS_NUMBER_INSTANCE(receiver), AS_NUMBER_INSTANCE(args[0])));
}

LOX_METHOD(Number, objectID) {
    ASSERT_ARG_COUNT("Number::objectID()", 0);
    if (IS_NUMBER(receiver)) RETURN_NUMBER((double)receiver);
    else RETURN_NUMBER(fetchObjectID(vm, AS_OBJ(receiver)));
}

LOX_METHOD(Number, pow) {
    ASSERT_ARG_COUNT("Number::pow(exponent)", 1);
    ASSERT_ARG_TYPE("Number::pow(exponent)", 0, Number);
    RETURN_NUMBER(pow(AS_NUMBER_INSTANCE(receiver), AS_NUMBER_INSTANCE(args[0])));
}

LOX_METHOD(Number, round) {
    ASSERT_ARG_COUNT("Number::round()", 0);
    RETURN_NUMBER(round(AS_NUMBER_INSTANCE(receiver)));
}

LOX_METHOD(Number, sin) {
    ASSERT_ARG_COUNT("Number::sin()", 0);
    RETURN_NUMBER(sin(AS_NUMBER_INSTANCE(receiver)));
}

LOX_METHOD(Number, sqrt) {
    ASSERT_ARG_COUNT("Number::sqrt()", 0);
    double self = AS_NUMBER_INSTANCE(receiver);
    if (self < 0) THROW_EXCEPTION_FMT(clox.std.lang.ArithmeticException, "method Number::sqrt() expects receiver to be a non-negative number but got %g.", self);
    RETURN_NUMBER(sqrt(self));
}

LOX_METHOD(Number, step) {
    ASSERT_ARG_COUNT("Number::step(to, by, closure)", 3);
    ASSERT_ARG_TYPE("Number::step(to, by, closure)", 0, Number);
    ASSERT_ARG_TYPE("Number::step(to, by, closure)", 1, Number);
    ASSERT_ARG_TCALLABLE("Number::step(to, by, closure)", 2);

    double self = AS_NUMBER_INSTANCE(receiver);
    double to = AS_NUMBER_INSTANCE(args[0]);
    double by = AS_NUMBER_INSTANCE(args[1]);
    Value closure = args[2];

    if (by == 0) THROW_EXCEPTION(clox.std.lang.IllegalArgumentException, "Step size cannot be 0.");
    else {
        if (by > 0) {
            for (double num = self; num <= to; num += by) {
                callReentrantMethod(vm, receiver, closure, NUMBER_VAL(num));
            }
        }
        else {
            for (double num = self; num >= to; num += by) {
                callReentrantMethod(vm, receiver, closure, NUMBER_VAL(num));
            }
        }
    }
    RETURN_NIL;
}

LOX_METHOD(Number, tan) {
    ASSERT_ARG_COUNT("Number::tan()", 0);
    RETURN_NUMBER(tan(AS_NUMBER_INSTANCE(receiver)));
}

LOX_METHOD(Number, toInt) {
    ASSERT_ARG_COUNT("Number::toInt()", 0);
    RETURN_INT((int)AS_NUMBER_INSTANCE(receiver));
}

LOX_METHOD(Number, toString) {
    ASSERT_ARG_COUNT("Number::toString()", 0);
    char chars[24];
    int length = sprintf_s(chars, 24, "%.14g", AS_NUMBER_INSTANCE(receiver));
    RETURN_STRING(chars, length);
}

LOX_METHOD(Number, __equal__) {
    ASSERT_ARG_COUNT("Number::==(other)", 1);
    if (!IS_NUMBER_INSTANCE(args[0])) RETURN_FALSE;
    RETURN_BOOL((AS_NUMBER_INSTANCE(receiver) == AS_NUMBER_INSTANCE(args[0])));
}

LOX_METHOD(Number, __greater__) {
    ASSERT_ARG_COUNT("Number::>(other)", 1);
    ASSERT_ARG_TYPE("Number::>(other)", 0, Number);
    RETURN_BOOL((AS_NUMBER_INSTANCE(receiver) > AS_NUMBER_INSTANCE(args[0])));
}

LOX_METHOD(Number, __less__) {
    ASSERT_ARG_COUNT("Number::<(other)", 1);
    ASSERT_ARG_TYPE("Number::<(other)", 0, Number);
    RETURN_BOOL((AS_NUMBER_INSTANCE(receiver) < AS_NUMBER_INSTANCE(args[0])));
}

LOX_METHOD(Number, __add__) { 
    ASSERT_ARG_COUNT("Number::+(other)", 1);
    ASSERT_ARG_TYPE("Number::+(other)", 0, Number);
    RETURN_NUMBER((AS_NUMBER_INSTANCE(receiver) + AS_NUMBER_INSTANCE(args[0])));
}

LOX_METHOD(Number, __subtract__) {
    ASSERT_ARG_COUNT("Number::-(other)", 1);
    ASSERT_ARG_TYPE("Number::-(other)", 0, Number);
    RETURN_NUMBER((AS_NUMBER_INSTANCE(receiver) - AS_NUMBER_INSTANCE(args[0])));
}

LOX_METHOD(Number, __multiply__) {
    ASSERT_ARG_COUNT("Number::*(other)", 1);
    ASSERT_ARG_TYPE("Number::*(other)", 0, Number);
    RETURN_NUMBER((AS_NUMBER_INSTANCE(receiver) * AS_NUMBER_INSTANCE(args[0])));
}

LOX_METHOD(Number, __divide__) { 
    ASSERT_ARG_COUNT("Number::/(other)", 1);
    ASSERT_ARG_TYPE("Number::/(other)", 0, Number);
    RETURN_NUMBER((AS_NUMBER_INSTANCE(receiver) / AS_NUMBER_INSTANCE(args[0])));
}

LOX_METHOD(Number, __modulo__) {
    ASSERT_ARG_COUNT("Number::%(other)", 1);
    ASSERT_ARG_TYPE("Number::%(other)", 0, Number);
    RETURN_NUMBER(fmod(AS_NUMBER_INSTANCE(receiver), AS_NUMBER_INSTANCE(args[0])));
}

LOX_METHOD(NumberClass, parse) {
    ASSERT_ARG_COUNT("Number class::parse(numberString)", 1);
    THROW_EXCEPTION(clox.std.lang.NotImplementedException, "Not implemented, subclass responsibility.");
}

LOX_METHOD(Object, clone) {
    ASSERT_ARG_COUNT("Object::clone()", 0);
    ObjInstance* thisObject = AS_INSTANCE(receiver);
    ObjInstance* thatObject = newInstance(vm, OBJ_KLASS(receiver));
    push(vm, OBJ_VAL(thatObject));
    copyObjProperties(vm, thisObject, thatObject);
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
        IDMap* idMap = getShapeIndexes(vm, instance->obj.shapeID);
        int index;
        if (idMapGet(idMap, AS_STRING(args[0]), &index)) RETURN_VAL(instance->fields.values[index]);
    }
    RETURN_NIL;
}

LOX_METHOD(Object, hasField) {
    ASSERT_ARG_COUNT("Object::hasField(field)", 1);
    ASSERT_ARG_TYPE("Object::hasField(field)", 0, String);
    if (IS_INSTANCE(receiver)) {
        ObjInstance* instance = AS_INSTANCE(receiver);
        IDMap* indexMap = getShapeIndexes(vm, instance->obj.shapeID);
        int index;
        RETURN_BOOL(idMapGet(indexMap, AS_STRING(args[0]), &index));
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

LOX_METHOD(Object, objectID) {
    ASSERT_ARG_COUNT("Object::objectID()", 0);
    RETURN_NUMBER(fetchObjectID(vm, AS_OBJ(receiver)));
}

LOX_METHOD(Object, toString) {
    ASSERT_ARG_COUNT("Object::toString()", 0);
    RETURN_STRING_FMT("<object %s>", AS_OBJ(receiver)->klass->name->chars);
}

LOX_METHOD(Object, __equal__) {
    ASSERT_ARG_COUNT("Object::==(other)", 1);
    RETURN_BOOL(receiver == args[0]);
}

LOX_METHOD(String, __init__) {
    ASSERT_ARG_COUNT("String::__init__(chars)", 1);
    ASSERT_ARG_TYPE("String::__init__(chars)", 0, String);
    ObjString* string = AS_STRING(args[0]);
    RETURN_OBJ(createString(vm, string->chars, string->length, string->hash, AS_OBJ(receiver)->klass));
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

LOX_METHOD(String, count) {
    ASSERT_ARG_COUNT("String::count()", 0);
    ObjString* self = AS_STRING(receiver);
    RETURN_INT((int)utf8len(self->chars));
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

LOX_METHOD(String, getByte) {
    ASSERT_ARG_COUNT("String::getByte(index)", 1);
    ASSERT_ARG_TYPE("String::getByte(index)", 0, Int);

    ObjString* self = AS_STRING(receiver);
    int index = AS_INT(args[0]);
    ASSERT_INDEX_WITHIN_BOUNDS("String::getByte(index)", index, 0, self->length, 0);
    RETURN_INT((int)self->chars[index]);
}

LOX_METHOD(String, getCodePoint) {
    ASSERT_ARG_COUNT("String::getCodePoint(index)", 1);
    ASSERT_ARG_TYPE("String::getCodePoint(index)", 0, Int);
    
    ObjString* self = AS_STRING(receiver);
    int index = AS_INT(args[0]);
    ASSERT_INDEX_WITHIN_BOUNDS("String::getCodePoint(index)", index, 0, self->length, 0);
    RETURN_OBJ(utf8CodePointAtIndex(vm, self->chars, index));
}

LOX_METHOD(String, indexOf) {
    ASSERT_ARG_COUNT("String::indexOf(chars)", 1);
    ASSERT_ARG_TYPE("String::indexOf(chars)", 0, String);
    ObjString* haystack = AS_STRING(receiver);
    ObjString* needle = AS_STRING(args[0]);
    RETURN_INT(searchString(vm, haystack, needle, 0));
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
    if (index < 0 || index < self->length - 1) {
        RETURN_INT(index + utf8CodePointOffset(vm, self->chars, index));
    }
    RETURN_NIL;
}

LOX_METHOD(String, nextValue) {
    ASSERT_ARG_COUNT("String::nextValue(index)", 1);
    ASSERT_ARG_TYPE("String::nextValue(index)", 0, Int);
    ObjString* self = AS_STRING(receiver);
    int index = AS_INT(args[0]);
    if (index > -1 && index < self->length) {
        RETURN_OBJ(utf8CodePointAtIndex(vm, self->chars, index));
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
    RETURN_OBJ(reverseString(vm, self));
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

LOX_METHOD(String, toBytes) {
    ASSERT_ARG_COUNT("String::toBytes()", 0);
    ObjString* self = AS_STRING(receiver);
    ObjArray* bytes = newArray(vm);
    push(vm, OBJ_VAL(bytes));

    for (int i = 0; i < self->length; i++) {
        valueArrayWrite(vm, &bytes->elements, INT_VAL(self->chars[i]));
    }
    pop(vm);
    RETURN_OBJ(bytes);
}

LOX_METHOD(String, toCodePoints) {
    ASSERT_ARG_COUNT("String::toCodePoints()", 0);
    ObjString* self = AS_STRING(receiver);
    ObjArray* codePoints = newArray(vm);
    push(vm, OBJ_VAL(codePoints));

    int i = 0;
    while (i < self->length) {
        ObjString* codePoint = utf8CodePointAtIndex(vm, self->chars, i);
        valueArrayWrite(vm, &codePoints->elements, OBJ_VAL(codePoint));
        i += codePoint->length;
    }
    pop(vm);
    RETURN_OBJ(codePoints);
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

LOX_METHOD(String, __add__) {
    ASSERT_ARG_COUNT("String::+(other)", 1);
    ASSERT_ARG_TYPE("String::+(other)", 0, String);
    RETURN_STRING_FMT("%s%s", AS_CSTRING(receiver), AS_CSTRING(args[0]));
}

LOX_METHOD(String, __getSubscript__) {
    ASSERT_ARG_COUNT("String::[](index)", 1);
    ASSERT_ARG_TYPE("String::[getChar]](index)", 0, Int);
    ObjString* self = AS_STRING(receiver);
    int index = AS_INT(args[0]);
    ASSERT_INDEX_WITHIN_BOUNDS("String::[](index)", index, 0, self->length, 0);
    char chars[2] = { self->chars[index], '\0' };
    RETURN_STRING(chars, 1);
}

LOX_METHOD(StringClass, fromByte) {
    ASSERT_ARG_COUNT("String class::fromByte(byte)", 1);
    ASSERT_ARG_TYPE("String class::fromByte(byte)", 0, Int);
    int byte = AS_INT(args[0]);
    ASSERT_INDEX_WITHIN_BOUNDS("String class::fromByte(byte)", byte, 0, 255, 0);
    RETURN_OBJ(utf8StringFromByte(vm, (uint8_t)byte));
}

LOX_METHOD(StringClass, fromCodePoint) {
    ASSERT_ARG_COUNT("String class::fromCodePoint(codePoint)", 1);
    ASSERT_ARG_TYPE("String class::fromCodePoint(codePoint)", 0, Int);
    RETURN_OBJ(utf8StringFromCodePoint(vm, AS_INT(args[0])));
}

LOX_METHOD(TCallable, arity) {
    THROW_EXCEPTION(clox.std.lang.NotImplementedException, "Not implemented, subclass responsibility.");
}

LOX_METHOD(TCallable, isAsync) {
    THROW_EXCEPTION(clox.std.lang.NotImplementedException, "Not implemented, subclass responsibility.");
}

LOX_METHOD(TCallable, isNative) {
    THROW_EXCEPTION(clox.std.lang.NotImplementedException, "Not implemented, subclass responsibility.");
}

LOX_METHOD(TCallable, isVariadic) {
    THROW_EXCEPTION(clox.std.lang.NotImplementedException, "Not implemented, subclass responsibility.");
}

LOX_METHOD(TCallable, name) {
    THROW_EXCEPTION(clox.std.lang.NotImplementedException, "Not implemented, subclass responsibility.");
}

LOX_METHOD(TCallable, __invoke__) {
    THROW_EXCEPTION(clox.std.lang.NotImplementedException, "Not implemented, subclass responsibility.");
}

LOX_METHOD(TComparable, compareTo) {
    THROW_EXCEPTION(clox.std.lang.NotImplementedException, "Not implemented, subclass responsibility.");
}

LOX_METHOD(TComparable, equals) {
    ASSERT_ARG_COUNT("TComparable::equals(other)", 1);
    if (valuesEqual(receiver, args[0])) RETURN_TRUE;
    else {
        Value result = callReentrantMethod(vm, receiver, getObjMethod(vm, receiver, "compareTo"), args[0]);
        RETURN_BOOL(result == 0);
    }
}

LOX_METHOD(TComparable, __equal__) {
    ASSERT_ARG_COUNT("TComparable::==(other)", 1);
    if (valuesEqual(receiver, args[0])) RETURN_TRUE;
    else {
        Value result = callReentrantMethod(vm, receiver, getObjMethod(vm, receiver, "compareTo"), args[0]);
        RETURN_BOOL(result > 0);
    }
}

LOX_METHOD(TComparable, __greater__) { 
    ASSERT_ARG_COUNT("TComparable::>(other)", 1);
    if (valuesEqual(receiver, args[0])) RETURN_TRUE;
    else {
        Value result = callReentrantMethod(vm, receiver, getObjMethod(vm, receiver, "compareTo"), args[0]);
        RETURN_BOOL(result == 0);
    }
}

LOX_METHOD(TComparable, __less__) {
    ASSERT_ARG_COUNT("TComparable::<(other)", 1);
    if (valuesEqual(receiver, args[0])) RETURN_TRUE;
    else {
        Value result = callReentrantMethod(vm, receiver, getObjMethod(vm, receiver, "compareTo"), args[0]);
        RETURN_BOOL(result < 0);
    }
}

LOX_METHOD(Trait, __init__) {
    ASSERT_ARG_COUNT("Trait::__init__(name, traits)", 2);
    ASSERT_ARG_TYPE("Trait::__init__(name, traits)", 0, String);
    ASSERT_ARG_TYPE("Trait::__init__(name, traits)", 1, Array);
    ObjClass* trait = createTrait(vm, AS_STRING(args[0]));
    implementTraits(vm, trait, &AS_ARRAY(args[1])->elements);
    RETURN_OBJ(trait);
}

LOX_METHOD(Trait, getClass) {
    ASSERT_ARG_COUNT("Trait::getClass()", 0);
    RETURN_OBJ(vm->traitClass);
}

LOX_METHOD(Trait, getClassName) {
    ASSERT_ARG_COUNT("Trait::getClassName()", 0);
    RETURN_OBJ(vm->traitClass->name);
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
    ObjClass* self = AS_CLASS(receiver);
    if (self->namespace->isRoot) RETURN_STRING_FMT("<trait %s>", self->name->chars);
    else RETURN_STRING_FMT("<trait %s.%s>", self->namespace->fullName->chars, self->name->chars);
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
        ObjNamespace* namespace = AS_NAMESPACE(entry->value);
        namespace->obj.klass = vm->namespaceClass;
    }
}

static void bindGlobalSymbolTable(VM* vm) {
    TypeInfo* type = typeTableGet(vm->typetab, newString(vm, "clox.std.lang.Class"));
    for (int i = 0; i < vm->symtab->capacity; i++) {
        SymbolEntry* entry = &vm->symtab->entries[i];
        if (entry->key == NULL) continue;
        entry->value->type = type;
    }
}

static ObjClass* defineSpecialClass(VM* vm, const char* name, BehaviorType behavior) {
    ObjString* className = newString(vm, name);
    push(vm, OBJ_VAL(className));
    ObjClass* nativeClass = createClass(vm, className, NULL, behavior);
    nativeClass->isNative = true;
    push(vm, OBJ_VAL(nativeClass));

    tableSet(vm, &vm->classes, nativeClass->fullName, OBJ_VAL(nativeClass));
    tableSet(vm, &vm->rootNamespace->values, AS_STRING(vm->stack[0]), vm->stack[1]);
    pop(vm);
    pop(vm);
    if (nativeClass->behaviorType != BEHAVIOR_METACLASS) {
        insertTypeTable(vm, TYPE_CATEGORY_CLASS, className, nativeClass->fullName);
    }
    return nativeClass;
}

static ObjNamespace* defineRootNamespace(VM* vm) {
    ObjString* name = newString(vm, "");
    push(vm, OBJ_VAL(name));
    ObjNamespace* rootNamespace = newNamespace(vm, name, NULL);
    rootNamespace->isRoot = true;
    push(vm, OBJ_VAL(rootNamespace));
    tableSet(vm, &vm->namespaces, name, OBJ_VAL(rootNamespace));
    pop(vm);
    pop(vm);
    return rootNamespace;
}

void registerLangPackage(VM* vm) {
    vm->rootNamespace = defineRootNamespace(vm);
    vm->cloxNamespace = defineNativeNamespace(vm, "clox", vm->rootNamespace);
    vm->stdNamespace = defineNativeNamespace(vm, "std", vm->cloxNamespace);
    vm->langNamespace = defineNativeNamespace(vm, "lang", vm->stdNamespace);
    vm->currentNamespace = vm->langNamespace;
    insertGlobalSymbolTable(vm, "clox");

    vm->objectClass = defineSpecialClass(vm, "Object", BEHAVIOR_CLASS);
    vm->objectClass->classType = OBJ_INSTANCE;
    DEF_METHOD(vm->objectClass, Object, clone, 0);
    DEF_METHOD(vm->objectClass, Object, equals, 1);
    DEF_METHOD(vm->objectClass, Object, getClass, 0);
    DEF_METHOD(vm->objectClass, Object, getClassName, 0);
    DEF_METHOD(vm->objectClass, Object, getField, 1);
    DEF_METHOD(vm->objectClass, Object, hasField, 1);
    DEF_METHOD(vm->objectClass, Object, hashCode, 0);
    DEF_METHOD(vm->objectClass, Object, instanceOf, 1);
    DEF_METHOD(vm->objectClass, Object, memberOf, 1);
    DEF_METHOD(vm->objectClass, Object, objectID, 0);
    DEF_METHOD(vm->objectClass, Object, toString, 0);
    DEF_OPERATOR(vm->objectClass, Object, == , __equal__, 1);
    insertGlobalSymbolTable(vm, "Object");

    ObjClass* behaviorClass = defineSpecialClass(vm, "Behavior", BEHAVIOR_CLASS);
    inheritSuperclass(vm, behaviorClass, vm->objectClass);
    behaviorClass->classType = OBJ_CLASS;
    DEF_INTERCEPTOR(behaviorClass, Behavior, INTERCEPTOR_INIT, __init__, 2);
    DEF_METHOD(behaviorClass, Behavior, clone, 0);
    DEF_METHOD(behaviorClass, Behavior, getMethod, 1);
    DEF_METHOD(behaviorClass, Behavior, hasMethod, 1);
    DEF_METHOD(behaviorClass, Behavior, isBehavior, 0);
    DEF_METHOD(behaviorClass, Behavior, isClass, 0);
    DEF_METHOD(behaviorClass, Behavior, isMetaclass, 0);
    DEF_METHOD(behaviorClass, Behavior, isNative, 0);
    DEF_METHOD(behaviorClass, Behavior, methods, 0);
    DEF_METHOD(behaviorClass, Behavior, name, 0);
    DEF_METHOD(behaviorClass, Behavior, traits, 0);
    DEF_OPERATOR(behaviorClass, Behavior, (), __invoke__, -1);
    insertGlobalSymbolTable(vm, "Behavior");

    vm->classClass = defineSpecialClass(vm, "Class", BEHAVIOR_CLASS);
    inheritSuperclass(vm, vm->classClass, behaviorClass);
    DEF_INTERCEPTOR(vm->classClass, Class, INTERCEPTOR_INIT, __init__, 3);
    DEF_METHOD(vm->classClass, Class, getField, 1);
    DEF_METHOD(vm->classClass, Class, hasField, 1);
    DEF_METHOD(vm->classClass, Class, instanceOf, 1);
    DEF_METHOD(vm->classClass, Class, isClass, 0);
    DEF_METHOD(vm->classClass, Class, memberOf, 1);
    DEF_METHOD(vm->classClass, Class, superclass, 0);
    DEF_METHOD(vm->classClass, Class, toString, 0);
    DEF_OPERATOR(vm->classClass, Class, (), __invoke__, -1);
    insertGlobalSymbolTable(vm, "Class");

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
    insertGlobalSymbolTable(vm, "Metaclass");

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
    vm->methodClass->classType = OBJ_METHOD;
    DEF_INTERCEPTOR(vm->methodClass, Method, INTERCEPTOR_INIT, __init__, 0);
    DEF_METHOD(vm->methodClass, Method, arity, 0);
    DEF_METHOD(vm->methodClass, Method, behavior, 0);
    DEF_METHOD(vm->methodClass, Method, bind, 1);
    DEF_METHOD(vm->methodClass, Method, clone, 0);
    DEF_METHOD(vm->methodClass, Method, isAsync, 0);
    DEF_METHOD(vm->methodClass, Method, isNative, 0);
    DEF_METHOD(vm->methodClass, Method, isVariadic, 0);
    DEF_METHOD(vm->methodClass, Method, name, 0);
    DEF_METHOD(vm->methodClass, Method, toString, 0);
    insertGlobalSymbolTable(vm, "Method");

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
    vm->namespaceClass->classType = OBJ_NAMESPACE;
    DEF_INTERCEPTOR(vm->namespaceClass, Namespace, INTERCEPTOR_INIT, __init__, 0);
    DEF_METHOD(vm->namespaceClass, Namespace, clone, 0);
    DEF_METHOD(vm->namespaceClass, Namespace, enclosing, 0);
    DEF_METHOD(vm->namespaceClass, Namespace, fullName, 0);
    DEF_METHOD(vm->namespaceClass, Namespace, shortName, 0);
    DEF_METHOD(vm->namespaceClass, Namespace, toString, 0);
    bindNamespaceClass(vm);
    insertGlobalSymbolTable(vm, "Namespace");

    vm->traitClass = defineNativeClass(vm, "Trait");
    bindSuperclass(vm, vm->traitClass, behaviorClass);
    DEF_INTERCEPTOR(vm->traitClass, Trait, INTERCEPTOR_INIT, __init__, 2);
    DEF_METHOD(vm->traitClass, Trait, getClass, 0);
    DEF_METHOD(vm->traitClass, Trait, getClassName, 0);
    DEF_METHOD(vm->traitClass, Trait, instanceOf, 1);
    DEF_METHOD(vm->traitClass, Trait, isTrait, 0);
    DEF_METHOD(vm->traitClass, Trait, memberOf, 1);
    DEF_METHOD(vm->traitClass, Trait, superclass, 0);
    DEF_METHOD(vm->traitClass, Trait, toString, 0);
    insertGlobalSymbolTable(vm, "Trait");

    vm->nilClass = defineNativeClass(vm, "Nil");
    bindSuperclass(vm, vm->nilClass, vm->objectClass);
    DEF_INTERCEPTOR(vm->nilClass, Nil, INTERCEPTOR_INIT, __init__, 0);
    DEF_METHOD(vm->nilClass, Nil, clone, 0);
    DEF_METHOD(vm->nilClass, Nil, objectID, 0);
    DEF_METHOD(vm->nilClass, Nil, toString, 0);
    insertGlobalSymbolTable(vm, "Nil");

    vm->boolClass = defineNativeClass(vm, "Bool");
    bindSuperclass(vm, vm->boolClass, vm->objectClass);
    DEF_INTERCEPTOR(vm->boolClass, Bool, INTERCEPTOR_INIT, __init__, 1);
    DEF_METHOD(vm->boolClass, Bool, clone, 0);
    DEF_METHOD(vm->boolClass, Bool, objectID, 0);
    DEF_METHOD(vm->boolClass, Bool, toString, 0);
    insertGlobalSymbolTable(vm, "Bool");

    ObjClass* comparableTrait = defineNativeTrait(vm, "TComparable");
    DEF_METHOD(comparableTrait, TComparable, compareTo, 1);
    DEF_METHOD(comparableTrait, TComparable, equals, 1);
    DEF_OPERATOR(comparableTrait, TComparable, == , __equal__, 1);
    DEF_OPERATOR(comparableTrait, TComparable, > , __greater__, 1);
    DEF_OPERATOR(comparableTrait, TComparable, < , __less__, 1);
    insertGlobalSymbolTable(vm, "TComparable");

    vm->numberClass = defineNativeClass(vm, "Number");
    bindSuperclass(vm, vm->numberClass, vm->objectClass);
    bindTrait(vm, vm->numberClass, comparableTrait);
    vm->numberClass->classType = OBJ_VALUE_INSTANCE;
    DEF_INTERCEPTOR(vm->numberClass, Number, INTERCEPTOR_INIT, __init__, 1);
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
    DEF_METHOD(vm->numberClass, Number, isInfinity, 0);
    DEF_METHOD(vm->numberClass, Number, log, 0);
    DEF_METHOD(vm->numberClass, Number, log2, 0);
    DEF_METHOD(vm->numberClass, Number, log10, 0);
    DEF_METHOD(vm->numberClass, Number, max, 1);
    DEF_METHOD(vm->numberClass, Number, min, 1);
    DEF_METHOD(vm->numberClass, Number, objectID, 0);
    DEF_METHOD(vm->numberClass, Number, pow, 1);
    DEF_METHOD(vm->numberClass, Number, round, 0);
    DEF_METHOD(vm->numberClass, Number, sin, 0);
    DEF_METHOD(vm->numberClass, Number, sqrt, 0);
    DEF_METHOD(vm->numberClass, Number, step, 3);
    DEF_METHOD(vm->numberClass, Number, tan, 0);
    DEF_METHOD(vm->numberClass, Number, toInt, 0);
    DEF_METHOD(vm->numberClass, Number, toString, 0);
    DEF_OPERATOR(vm->numberClass, Number, == , __equal__, 1);
    DEF_OPERATOR(vm->numberClass, Number, > , __greater__, 1);
    DEF_OPERATOR(vm->numberClass, Number, < , __less__, 1);
    DEF_OPERATOR(vm->numberClass, Number, +, __add__, 1);
    DEF_OPERATOR(vm->numberClass, Number, -, __subtract__, 1);
    DEF_OPERATOR(vm->numberClass, Number, *, __multiply__, 1);
    DEF_OPERATOR(vm->numberClass, Number, / , __divide__, 1);
    DEF_OPERATOR(vm->numberClass, Number, %, __modulo__, 1);
    insertGlobalSymbolTable(vm, "Number");

    ObjClass* numberMetaclass = vm->numberClass->obj.klass;
    setClassProperty(vm, vm->numberClass, "infinity", NUMBER_VAL(INFINITY));
    setClassProperty(vm, vm->numberClass, "pi", NUMBER_VAL(3.14159265358979323846264338327950288));
    DEF_METHOD(numberMetaclass, NumberClass, parse, 1);

    vm->intClass = defineNativeClass(vm, "Int");
    bindSuperclass(vm, vm->intClass, vm->numberClass);
    DEF_INTERCEPTOR(vm->intClass, Int, INTERCEPTOR_INIT, __init__, 1);
    DEF_METHOD(vm->intClass, Int, abs, 0);
    DEF_METHOD(vm->intClass, Int, clone, 0);
    DEF_METHOD(vm->intClass, Int, downTo, 2);
    DEF_METHOD(vm->intClass, Int, factorial, 0);
    DEF_METHOD(vm->intClass, Int, gcd, 1);
    DEF_METHOD(vm->intClass, Int, isEven, 0);
    DEF_METHOD(vm->intClass, Int, isOdd, 0);
    DEF_METHOD(vm->intClass, Int, lcm, 1);
    DEF_METHOD(vm->intClass, Int, objectID, 0);
    DEF_METHOD(vm->intClass, Int, timesRepeat, 1);
    DEF_METHOD(vm->intClass, Int, toBinary, 0);
    DEF_METHOD(vm->intClass, Int, toFloat, 0);
    DEF_METHOD(vm->intClass, Int, toHexadecimal, 0);
    DEF_METHOD(vm->intClass, Int, toOctal, 0);
    DEF_METHOD(vm->intClass, Int, toString, 0);
    DEF_METHOD(vm->intClass, Int, upTo, 2);
    DEF_OPERATOR(vm->intClass, Int, +, __add__, 1);
    DEF_OPERATOR(vm->intClass, Int, -, __subtract__, 1);
    DEF_OPERATOR(vm->intClass, Int, *, __multiply__, 1);
    DEF_OPERATOR(vm->intClass, Int, %, __modulo__, 1);
    DEF_OPERATOR(vm->intClass, Int, .., __range__, 1);
    insertGlobalSymbolTable(vm, "Int");

    ObjClass* intMetaclass = vm->intClass->obj.klass;
    setClassProperty(vm, vm->intClass, "max", INT_VAL(INT32_MAX));
    setClassProperty(vm, vm->intClass, "min", INT_VAL(INT32_MIN));
    DEF_METHOD(intMetaclass, IntClass, parse, 1);

    vm->floatClass = defineNativeClass(vm, "Float");
    bindSuperclass(vm, vm->floatClass, vm->numberClass);
    DEF_INTERCEPTOR(vm->floatClass, Float, INTERCEPTOR_INIT, __init__, 1);
    DEF_METHOD(vm->floatClass, Float, clone, 0);
    DEF_METHOD(vm->floatClass, Float, toString, 0);
    insertGlobalSymbolTable(vm, "Float");

    ObjClass* floatMetaclass = vm->floatClass->obj.klass;
    setClassProperty(vm, vm->floatClass, "max", NUMBER_VAL(DBL_MAX));
    setClassProperty(vm, vm->floatClass, "min", NUMBER_VAL(DBL_MIN));
    DEF_METHOD(floatMetaclass, FloatClass, parse, 1);

    vm->stringClass = defineNativeClass(vm, "String");
    bindSuperclass(vm, vm->stringClass, vm->objectClass);
    vm->stringClass->classType = OBJ_STRING;
    DEF_INTERCEPTOR(vm->stringClass, String, INTERCEPTOR_INIT, __init__, 1);
    DEF_METHOD(vm->stringClass, String, capitalize, 0);
    DEF_METHOD(vm->stringClass, String, clone, 0);
    DEF_METHOD(vm->stringClass, String, contains, 1);
    DEF_METHOD(vm->stringClass, String, count, 0);
    DEF_METHOD(vm->stringClass, String, decapitalize, 0);
    DEF_METHOD(vm->stringClass, String, endsWith, 1);
    DEF_METHOD(vm->stringClass, String, getByte, 1);
    DEF_METHOD(vm->stringClass, String, getCodePoint, 1);
    DEF_METHOD(vm->stringClass, String, indexOf, 1);
    DEF_METHOD(vm->stringClass, String, length, 0);
    DEF_METHOD(vm->stringClass, String, next, 1);
    DEF_METHOD(vm->stringClass, String, nextValue, 1);
    DEF_METHOD(vm->stringClass, String, replace, 2);
    DEF_METHOD(vm->stringClass, String, reverse, 0);
    DEF_METHOD(vm->stringClass, String, split, 1);
    DEF_METHOD(vm->stringClass, String, startsWith, 1);
    DEF_METHOD(vm->stringClass, String, subString, 2);
    DEF_METHOD(vm->stringClass, String, toBytes, 0);
    DEF_METHOD(vm->stringClass, String, toCodePoints, 0);
    DEF_METHOD(vm->stringClass, String, toLowercase, 0);
    DEF_METHOD(vm->stringClass, String, toString, 0);
    DEF_METHOD(vm->stringClass, String, toUppercase, 0);
    DEF_METHOD(vm->stringClass, String, trim, 0);
    DEF_OPERATOR(vm->stringClass, String, +, __add__, 1);
    DEF_OPERATOR(vm->stringClass, String, [], __getSubscript__, 1);
    bindStringClass(vm);
    insertGlobalSymbolTable(vm, "String");

    ObjClass* stringMetaclass = vm->stringClass->obj.klass;
    DEF_METHOD(stringMetaclass, StringClass, fromByte, 1);
    DEF_METHOD(stringMetaclass, StringClass, fromCodePoint, 1);

    ObjClass* callableTrait = defineNativeTrait(vm, "TCallable");
    DEF_METHOD(callableTrait, TCallable, arity, 0);
    DEF_METHOD(callableTrait, TCallable, isAsync, 0);
    DEF_METHOD(callableTrait, TCallable, isNative, 0);
    DEF_METHOD(callableTrait, TCallable, isVariadic, 0);
    DEF_METHOD(callableTrait, TCallable, name, 0);
    DEF_OPERATOR(callableTrait, TCallable, (), __invoke__, -1);
    insertGlobalSymbolTable(vm, "TCallable");

    vm->functionClass = defineNativeClass(vm, "Function");
    bindSuperclass(vm, vm->functionClass, vm->objectClass);
    bindTrait(vm, vm->functionClass, callableTrait);
    vm->functionClass->classType = OBJ_CLOSURE;
    DEF_INTERCEPTOR(vm->functionClass, Function, INTERCEPTOR_INIT, __init__, 0);
    DEF_METHOD(vm->functionClass, Function, arity, 0);
    DEF_METHOD(vm->functionClass, Function, call, -1);
    DEF_METHOD(vm->functionClass, Function, call0, 0);
    DEF_METHOD(vm->functionClass, Function, call1, 1);
    DEF_METHOD(vm->functionClass, Function, call2, 2);
    DEF_METHOD(vm->functionClass, Function, clone, 0);
    DEF_METHOD(vm->functionClass, Function, isAnonymous, 0);
    DEF_METHOD(vm->functionClass, Function, isAsync, 0);
    DEF_METHOD(vm->functionClass, Function, isNative, 0);
    DEF_METHOD(vm->functionClass, Function, isVariadic, 0);
    DEF_METHOD(vm->functionClass, Function, name, 0);
    DEF_METHOD(vm->functionClass, Function, toString, 0);
    DEF_METHOD(vm->functionClass, Function, upvalueCount, 0);
    DEF_OPERATOR(vm->functionClass, Function, (), __invoke__, -1);
    insertGlobalSymbolTable(vm, "Function");

    vm->boundMethodClass = defineNativeClass(vm, "BoundMethod");
    bindSuperclass(vm, vm->boundMethodClass, vm->objectClass);
    bindTrait(vm, vm->boundMethodClass, callableTrait);
    vm->boundMethodClass->classType = OBJ_BOUND_METHOD;
    DEF_INTERCEPTOR(vm->boundMethodClass, BoundMethod, INTERCEPTOR_INIT, __init__, 2);
    DEF_METHOD(vm->boundMethodClass, BoundMethod, arity, 0);
    DEF_METHOD(vm->boundMethodClass, BoundMethod, clone, 0);
    DEF_METHOD(vm->boundMethodClass, BoundMethod, isAsync, 0);
    DEF_METHOD(vm->boundMethodClass, BoundMethod, isNative, 0);
    DEF_METHOD(vm->boundMethodClass, BoundMethod, isVariadic, 0);
    DEF_METHOD(vm->boundMethodClass, BoundMethod, name, 0);
    DEF_METHOD(vm->boundMethodClass, BoundMethod, receiver, 0);
    DEF_METHOD(vm->boundMethodClass, BoundMethod, toString, 0);
    DEF_METHOD(vm->boundMethodClass, BoundMethod, upvalueCount, 0);
    DEF_OPERATOR(vm->boundMethodClass, BoundMethod, (), __invoke__, -1);
    insertGlobalSymbolTable(vm, "BoundMethod");

    vm->generatorClass = defineNativeClass(vm, "Generator");
    bindSuperclass(vm, vm->generatorClass, vm->objectClass);
    vm->generatorClass->classType = OBJ_GENERATOR;
    DEF_INTERCEPTOR(vm->generatorClass, Generator, INTERCEPTOR_INIT, __init__, 1);
    DEF_METHOD(vm->generatorClass, Generator, getReceiver, 0);
    DEF_METHOD(vm->generatorClass, Generator, isFinished, 0);
    DEF_METHOD(vm->generatorClass, Generator, isReady, 0);
    DEF_METHOD(vm->generatorClass, Generator, isSuspended, 0);
    DEF_METHOD(vm->generatorClass, Generator, next, 0);
    DEF_METHOD(vm->generatorClass, Generator, nextFinished, 0);
    DEF_METHOD(vm->generatorClass, Generator, returns, 1);
    DEF_METHOD(vm->generatorClass, Generator, send, 1);
    DEF_METHOD(vm->generatorClass, Generator, setReceiver, 1);
    DEF_METHOD(vm->generatorClass, Generator, step, 1);
    DEF_METHOD(vm->generatorClass, Generator, throws, 1);
    DEF_METHOD(vm->generatorClass, Generator, toString, 0);
    DEF_OPERATOR(vm->generatorClass, Generator, (), __invoke__, -1);
    insertGlobalSymbolTable(vm, "Generator");

    ObjClass* generatorMetaclass = vm->generatorClass->obj.klass;
    setClassProperty(vm, vm->generatorClass, "stateStart", INT_VAL(GENERATOR_START));
    setClassProperty(vm, vm->generatorClass, "stateYield", INT_VAL(GENERATOR_YIELD));
    setClassProperty(vm, vm->generatorClass, "stateResume", INT_VAL(GENERATOR_RESUME));
    setClassProperty(vm, vm->generatorClass, "stateReturn", INT_VAL(GENERATOR_RETURN));
    setClassProperty(vm, vm->generatorClass, "stateThrow", INT_VAL(GENERATOR_THROW));
    setClassProperty(vm, vm->generatorClass, "stateError", INT_VAL(GENERATOR_ERROR));
    DEF_METHOD(generatorMetaclass, GeneratorClass, run, 2);

    vm->exceptionClass = defineNativeClass(vm, "Exception");
    bindSuperclass(vm, vm->exceptionClass, vm->objectClass);
    vm->exceptionClass->classType = OBJ_EXCEPTION;
    DEF_INTERCEPTOR(vm->exceptionClass, Exception, INTERCEPTOR_INIT, __init__, 1);
    DEF_METHOD(vm->exceptionClass, Exception, message, 0);
    DEF_METHOD(vm->exceptionClass, Exception, toString, 0);
    insertGlobalSymbolTable(vm, "Exception");

    ObjClass* runtimeExceptionClass = defineNativeException(vm, "RuntimeException", vm->exceptionClass);
    defineNativeException(vm, "AssertionException", runtimeExceptionClass);
    defineNativeException(vm, "ArithmeticException", runtimeExceptionClass);
    defineNativeException(vm, "FormatException", runtimeExceptionClass);
    defineNativeException(vm, "IllegalArgumentException", runtimeExceptionClass);
    defineNativeException(vm, "IndexOutOfBoundsException", runtimeExceptionClass);
    defineNativeException(vm, "MethodNotFoundException", runtimeExceptionClass);
    defineNativeException(vm, "NotImplementedException", runtimeExceptionClass);
    defineNativeException(vm, "OutOfMemoryException", runtimeExceptionClass);
    defineNativeException(vm, "StackOverflowException", runtimeExceptionClass);
    defineNativeException(vm, "UnsupportedOperationException", runtimeExceptionClass);

    insertGlobalSymbolTable(vm, "AssertionException");
    insertGlobalSymbolTable(vm, "ArithmeticException");
    insertGlobalSymbolTable(vm, "FormatException");
    insertGlobalSymbolTable(vm, "IllegalArgumentException");
    insertGlobalSymbolTable(vm, "IndexOutOfBoundsException");
    insertGlobalSymbolTable(vm, "MethodNotFoundException");
    insertGlobalSymbolTable(vm, "NotImplementedException");
    insertGlobalSymbolTable(vm, "OutOfMemoryException");
    insertGlobalSymbolTable(vm, "StackOverflowException");
    insertGlobalSymbolTable(vm, "UnsupportedOperationException");
    bindGlobalSymbolTable(vm);

    vm->currentNamespace = vm->rootNamespace;
}