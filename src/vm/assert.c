#include <stdio.h>
#include <stdlib.h>

#include "assert.h"
#include "native.h"
#include "vm.h"

void assertArgCount(VM* vm, const char* method, int expectedCount, int actualCount) {
    if (expectedCount != actualCount) {
        runtimeError(vm, "method %s expects %d argument(s) but got %d instead.", method, expectedCount, actualCount);
        exit(70);
    }
}

void assertArgInstanceOf(VM* vm, const char* method, Value* args, int index, const char* namespaceName, const char* className) {
    if (!isObjInstanceOf(vm, args[index], getNativeClass(vm, namespaceName, className))) {
        runtimeError(vm, "method %s expects argument %d to be an instance of class %s but got %s.", 
            method, index + 1, className, getObjClass(vm, args[index])->name->chars);
        exit(70);
    }
}

void assertArgInstanceOfEither(VM* vm, const char* method, Value* args, int index, const char* namespaceName, const char* className, const char* namespaceName2, const char* className2) {
    if (!isObjInstanceOf(vm, args[index], getNativeClass(vm, namespaceName, className)) && !isObjInstanceOf(vm, args[index], getNativeClass(vm, namespaceName2, className2))) {
        runtimeError(vm, "method %s expects argument %d to be an instance of class %s or %s but got %s.",
            method, index + 1, className, className2, getObjClass(vm, args[index])->name->chars);
        exit(70);
    }   
}

void assertArgIsArray(VM* vm, const char* method, Value* args, int index) {
    if (!IS_ARRAY(args[index]) && !isObjInstanceOf(vm, args[index], vm->arrayClass)) {
        runtimeError(vm, "method %s expects argument %d to be an array.", method, index + 1);
        exit(70);
    }
}

void assertArgIsBool(VM* vm, const char* method, Value* args, int index) {
    if (!IS_BOOL(args[index]) && !isObjInstanceOf(vm, args[index], vm->boolClass)) {
        runtimeError(vm, "method %s expects argument %d to be a boolean value.", method, index + 1);
        exit(70);
    }
}

void assertArgIsClass(VM* vm, const char* method, Value* args, int index) {
    if (!IS_CLASS(args[index]) && !isObjInstanceOf(vm, args[index], vm->classClass)) {
        runtimeError(vm, "method %s expects argument %d to be a class.", method, index + 1);
        exit(70);
    }
}

void assertArgIsClosure(VM* vm, const char* method, Value* args, int index) {
    if (!IS_CLOSURE(args[index]) && !isObjInstanceOf(vm, args[index], vm->functionClass)) {
        runtimeError(vm, "method %s expects argument %d to be a closure.", method, index + 1);
        exit(70);
    }
}

void assertArgIsDictionary(VM* vm, const char* method, Value* args, int index) {
    if (!IS_DICTIONARY(args[index]) && !isObjInstanceOf(vm, args[index], vm->dictionaryClass)) {
        runtimeError(vm, "method %s expects argument %d to be a dictionary.", method, index + 1);
        exit(70);
    }
}

void assertArgIsEntry(VM* vm, const char* method, Value* args, int index) {
    if (!IS_ENTRY(args[index]) && !isObjInstanceOf(vm, args[index], vm->entryClass)) {
        runtimeError(vm, "method %s expects argument %d to be a map entry.", method, index + 1);
        exit(70);
    }
}

void assertArgIsFile(VM* vm, const char* method, Value* args, int index) {
    if (!IS_FILE(args[index]) && !isObjInstanceOf(vm, args[index], vm->fileClass)) {
        runtimeError(vm, "method %s expects argument %d to be a file.", method, index + 1);
        exit(70);
    }
}

void assertArgIsFloat(VM* vm, const char* method, Value* args, int index) {
    if (!IS_FLOAT(args[index]) && !isObjInstanceOf(vm, args[index], vm->floatClass)) {
        runtimeError(vm, "method %s expects argument %d to be a floating point number.", method, index + 1);
        exit(70);
    }
}

void assertArgIsInt(VM* vm, const char* method, Value* args, int index) {
    if (!IS_INT(args[index]) && !isObjInstanceOf(vm, args[index], vm->intClass)) {
        runtimeError(vm, "method %s expects argument %d to be an integer number.", method, index + 1);
        exit(70);
    }
}

void assertArgIsMethod(VM* vm, const char* method, Value* args, int index) { 
    if (!IS_NAMESPACE(args[index]) && !isObjInstanceOf(vm, args[index], vm->methodClass)) {
        runtimeError(vm, "method %s expects argument %d to be a method.", method, index + 1);
        exit(70);
    }
}

void assertArgIsNamespace(VM* vm, const char* method, Value* args, int index) {
    if (!IS_NAMESPACE(args[index]) && !isObjInstanceOf(vm, args[index], vm->namespaceClass)) {
        runtimeError(vm, "method %s expects argument %d to be a namespace.", method, index + 1);
        exit(70);
    }
}

void assertArgIsNode(VM* vm, const char* method, Value* args, int index) {
    if (!IS_NODE(args[index]) && !isObjInstanceOf(vm, args[index], vm->nodeClass)) {
        runtimeError(vm, "method %s expects argument %d to be a link node.", method, index + 1);
        exit(70);
    }
}

void assertArgIsNumber(VM* vm, const char* method, Value* args, int index) {
    if (!IS_NUMBER(args[index]) && !isObjInstanceOf(vm, args[index], vm->numberClass)) {
        runtimeError(vm, "method %s expects argument %d to be a number.", method, index + 1);
        exit(70);
    }
}

void assertArgIsRange(VM* vm, const char* method, Value* args, int index) {
    if (!IS_RANGE(args[index]) && !isObjInstanceOf(vm, args[index], vm->rangeClass)) {
        runtimeError(vm, "method %s expects argument %d to be a range.", method, index + 1);
        exit(70);
    }
}

void assertArgIsString(VM* vm, const char* method, Value* args, int index) {
    if (!IS_STRING(args[index]) && !isObjInstanceOf(vm, args[index], vm->stringClass)) {
        runtimeError(vm, "method %s expects argument %d to be a string.", method, index + 1);
        exit(70);
    }
}

void assertIntWithinRange(VM* vm, const char* method, int value, int min, int max, int index){
    if (value < min || value > max) {
        runtimeError(vm, "method %s expects argument %d to be an integer within range %d to %d but got %d.", 
            method, index + 1, min, max, value);
        exit(70);
    }
}

void assertNumberNonNegative(VM* vm, const char* method, double number, int index) {
    if (number < 0) {
        if (index < 0) runtimeError(vm, "method %s expects receiver to be a non negative number but got %g.", method, number);
        else runtimeError(vm, "method %s expects argument %d to be a non negative number but got %g.", method, index + 1, number);
        exit(70);
    }
}

void assertNumberNonZero(VM* vm, const char* method, double number, int index) {
    if (number == 0) {
        if (index < 0) runtimeError(vm, "method %s expects receiver to be a non-zero number but got %g.", method, number);
        else runtimeError(vm, "method %s expects argument %d to be a non-zero number but got %g.", method, index + 1, number);
        exit(70);
    }
}

void assertNumberPositive(VM* vm, const char* method, double number, int index) {
    if (number <= 0) {
        if (index < 0) runtimeError(vm, "method %s expects receiver to be a positive number but got %g.", method, number);
        else runtimeError(vm, "method %s expects argument %d to be a positive number but got %g.", method, index + 1, number);
        exit(70);
    }
}

void assertNumberWithinRange(VM* vm, const char* method, double value, double min, double max, int index) {
    if (value < min || value > max) {
        runtimeError(vm, "method %s expects argument %d to be a number within range %g to %g but got %g.", 
            method, index + 1, min, max, value);
        exit(70);
    }
}

void raiseError(VM* vm, const char* message) {
    runtimeError(vm, message);
    exit(70);
}
