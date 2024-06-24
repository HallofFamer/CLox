#include <stdio.h>
#include <stdlib.h>

#include "assert.h"
#include "native.h"
#include "vm.h"

Value assertArgCount(VM* vm, const char* method, int expectedCount, int actualCount) {
    if (expectedCount != actualCount) {
        RETURN_STRING_FMT("method %s expects %d argument(s) but got %d instead.", method, expectedCount, actualCount);
    }
    RETURN_NIL;
}

Value assertArgInstanceOf(VM* vm, const char* method, Value* args, int index, const char* className) {
    if (!isObjInstanceOf(vm, args[index], getNativeClass(vm, className))) {
        RETURN_STRING_FMT("method %s expects argument %d to be an instance of class/trait %s but got %s.",
            method, index + 1, className, getObjClass(vm, args[index])->name->chars);
    }
    RETURN_NIL;
}

Value assertArgInstanceOfAny(VM* vm, const char* method, Value* args, int index, const char* className, const char* className2) {
    if (!isObjInstanceOf(vm, args[index], getNativeClass(vm, className)) && !isObjInstanceOf(vm, args[index], getNativeClass(vm, className2))) {
        RETURN_STRING_FMT("method %s expects argument %d to be an instance of class/trait %s or %s but got %s.",
            method, index + 1, className, className2, getObjClass(vm, args[index])->name->chars);
    }   
    RETURN_NIL;
}

Value assertArgIsArray(VM* vm, const char* method, Value* args, int index) {
    if (!IS_ARRAY(args[index]) && !isObjInstanceOf(vm, args[index], vm->arrayClass)) {
        RETURN_STRING_FMT("method %s expects argument %d to be an array.", method, index + 1);
    }
    RETURN_NIL;
}

Value assertArgIsBool(VM* vm, const char* method, Value* args, int index) {
    if (!IS_BOOL(args[index]) && !isObjInstanceOf(vm, args[index], vm->boolClass)) {
        RETURN_STRING_FMT("method %s expects argument %d to be a boolean value.", method, index + 1);
    }
    RETURN_NIL;
}

Value assertArgIsCallable(VM* vm, const char* method, Value* args, int index) {
    if (!isObjInstanceOf(vm, args[index], getNativeClass(vm, "clox.std.lang.TCallable"))) {
        RETURN_STRING_FMT("method %s expects argument %d to an instance of trait TCallable(ie. Closure) but got %s.",
            method, index + 1, getObjClass(vm, args[index])->name->chars);
    }
    RETURN_NIL;
}

Value assertArgIsClass(VM* vm, const char* method, Value* args, int index) {
    if (!IS_CLASS(args[index]) && !isObjInstanceOf(vm, args[index], vm->classClass)) {
        RETURN_STRING_FMT("method %s expects argument %d to be a class.", method, index + 1);
    }
    RETURN_NIL;
}

Value assertArgIsClosure(VM* vm, const char* method, Value* args, int index) {
    if (!IS_CLOSURE(args[index]) && !isObjInstanceOf(vm, args[index], vm->functionClass)) {
        RETURN_STRING_FMT("method %s expects argument %d to be a closure.", method, index + 1);
    }
    RETURN_NIL;
}

Value assertArgIsDictionary(VM* vm, const char* method, Value* args, int index) {
    if (!IS_DICTIONARY(args[index]) && !isObjInstanceOf(vm, args[index], vm->dictionaryClass)) {
        RETURN_STRING_FMT("method %s expects argument %d to be a dictionary.", method, index + 1);
    }
    RETURN_NIL;
}

Value assertArgIsEntry(VM* vm, const char* method, Value* args, int index) {
    if (!IS_ENTRY(args[index]) && !isObjInstanceOf(vm, args[index], vm->entryClass)) {
        RETURN_STRING_FMT("method %s expects argument %d to be a map entry.", method, index + 1);
    }
    RETURN_NIL;
}

Value assertArgIsException(VM* vm, const char* method, Value* args, int index) {
    if (!IS_EXCEPTION(args[index]) && !isObjInstanceOf(vm, args[index], vm->exceptionClass)) {
        RETURN_STRING_FMT("method %s expects argument %d to be an exception.", method, index + 1);
    }
    RETURN_NIL;
}

Value assertArgIsFile(VM* vm, const char* method, Value* args, int index) {
    if (!IS_FILE(args[index]) && !isObjInstanceOf(vm, args[index], vm->fileClass)) {
        RETURN_STRING_FMT("method %s expects argument %d to be a file.", method, index + 1);
    }
    RETURN_NIL;
}

Value assertArgIsFloat(VM* vm, const char* method, Value* args, int index) {
    if (!IS_FLOAT(args[index]) && !isObjInstanceOf(vm, args[index], vm->floatClass)) {
        RETURN_STRING_FMT("method %s expects argument %d to be a floating point number.", method, index + 1);
    }
    RETURN_NIL;
}

Value assertArgIsGenerator(VM* vm, const char* method, Value* args, int index) {
    if (!IS_GENERATOR(args[index]) && !isObjInstanceOf(vm, args[index], vm->generatorClass)) {
        RETURN_STRING_FMT("method %s expects argument %d to be a generator.", method, index + 1);
    }
    RETURN_NIL;
}

Value assertArgIsInt(VM* vm, const char* method, Value* args, int index) {
    if (!IS_INT(args[index]) && !isObjInstanceOf(vm, args[index], vm->intClass)) {
        RETURN_STRING_FMT("method %s expects argument %d to be an integer number.", method, index + 1);
    }
    RETURN_NIL;
}

Value assertArgIsMethod(VM* vm, const char* method, Value* args, int index) { 
    if (!IS_NAMESPACE(args[index]) && !isObjInstanceOf(vm, args[index], vm->methodClass)) {
        RETURN_STRING_FMT("method %s expects argument %d to be a method.", method, index + 1);
    }
    RETURN_NIL;
}

Value assertArgIsNamespace(VM* vm, const char* method, Value* args, int index) {
    if (!IS_NAMESPACE(args[index]) && !isObjInstanceOf(vm, args[index], vm->namespaceClass)) {
        RETURN_STRING_FMT("method %s expects argument %d to be a namespace.", method, index + 1);
    }
    RETURN_NIL;
}

Value assertArgIsNil(VM* vm, const char* method, Value* args, int index) {
    if (!IS_NIL(args[index]) && !isObjInstanceOf(vm, args[index], vm->nilClass)) {
        RETURN_STRING_FMT("method %s expects argument %d to be nil.", method, index + 1);
    }
    RETURN_NIL;
}

Value assertArgIsNode(VM* vm, const char* method, Value* args, int index) {
    if (!IS_NODE(args[index]) && !isObjInstanceOf(vm, args[index], vm->nodeClass)) {
        RETURN_STRING_FMT("method %s expects argument %d to be a link node.", method, index + 1);
    }
    RETURN_NIL;
}

Value assertArgIsNumber(VM* vm, const char* method, Value* args, int index) {
    if (!IS_NUMBER(args[index]) && !isObjInstanceOf(vm, args[index], vm->numberClass)) {
        RETURN_STRING_FMT("method %s expects argument %d to be a number.", method, index + 1);
    }
    RETURN_NIL;
}

Value assertArgIsPromise(VM* vm, const char* method, Value* args, int index) {
    if (!IS_PROMISE(args[index]) && !isObjInstanceOf(vm, args[index], vm->promiseClass)) {
        RETURN_STRING_FMT("method %s expects argument %d to be a promise.", method, index + 1);
    }
    RETURN_NIL;
}

Value assertArgIsRange(VM* vm, const char* method, Value* args, int index) {
    if (!IS_RANGE(args[index]) && !isObjInstanceOf(vm, args[index], vm->rangeClass)) {
        RETURN_STRING_FMT("method %s expects argument %d to be a range.", method, index + 1);
    }
    RETURN_NIL;
}

Value assertArgIsString(VM* vm, const char* method, Value* args, int index) {
    if (!IS_STRING(args[index]) && !isObjInstanceOf(vm, args[index], vm->stringClass)) {
        RETURN_STRING_FMT("method %s expects argument %d to be a string.", method, index + 1);
    }
    RETURN_NIL;
}

Value assertArgIsTimer(VM* vm, const char* method, Value* args, int index) {
    if (!IS_TIMER(args[index]) && !isObjInstanceOf(vm, args[index], vm->timerClass)) {
        RETURN_STRING_FMT("method %s expects argument %d to be a timer.", method, index + 1);
    }
    RETURN_NIL;
}

Value assertIndexWithinBounds(VM* vm, const char* method, int value, int min, int max, int index){
    if (value < min || value > max) {
        RETURN_STRING_FMT("method %s expects argument %d to be an integer within range %d to %d but got %d.",
            method, index + 1, min, max, value);
    }
    RETURN_NIL;
}

void raiseError(VM* vm, const char* message) {
    runtimeError(vm, message);
    exit(70);
}