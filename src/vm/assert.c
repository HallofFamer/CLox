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

void assertArgIsBool(VM* vm, const char* method, Value* args, int index) {
    if (!IS_BOOL(args[index])) {
        runtimeError(vm, "method %s expects argument %d to be a boolean value.", method, index + 1);
        exit(70);
    }
}

void assertArgIsClass(VM* vm, const char* method, Value* args, int index) {
    if (!IS_CLASS(args[index])) {
        runtimeError(vm, "method %s expects argument %d to be a class.", method, index + 1);
        exit(70);
    }
}

void assertArgIsDictionary(VM* vm, const char* method, Value* args, int index) {
    if (!IS_DICTIONARY(args[index])) {
        runtimeError(vm, "method %s expects argument %d to be a dictionary.", method, index + 1);
        exit(70);
    }
}

void assertArgIsFloat(VM* vm, const char* method, Value* args, int index) {
    if (!IS_FLOAT(args[index])) {
        runtimeError(vm, "method %s expects argument %d to be a floating point number.", method, index + 1);
        exit(70);
    }
}

void assertArgIsInt(VM* vm, const char* method, Value* args, int index) {
    if (!IS_INT(args[index])) {
        runtimeError(vm, "method %s expects argument %d to be an integer number.", method, index + 1);
        exit(70);
    }
}

void assertArgIsList(VM* vm, const char* method, Value* args, int index) {
    if (!IS_LIST(args[index])) {
        runtimeError(vm, "method %s expects argument %d to be a list.", method, index + 1);
        exit(70);
    }
}

void assertArgIsNumber(VM* vm, const char* method, Value* args, int index) {
    if (!IS_NUMBER(args[index])) {
        runtimeError(vm, "method %s expects argument %d to be a number.", method, index + 1);
        exit(70);
    }
}

void assertArgIsString(VM* vm, const char* method, Value* args, int index) {
    if (!IS_STRING(args[index])) {
        runtimeError(vm, "method %s expects argument %d to be a string.", method, index + 1);
        exit(70);
    }
}

void assertIndexWithinRange(VM* vm, const char* method, int value, int min, int max, int index){
    if (value < min || value > max) {
        runtimeError(vm, "method %s expects argument %d to be an index within range %d to %d but got %d.", method, index, min, max, value);
        exit(70);
    }
}

void assertInstanceOf(VM* vm, const char* method, Value arg, char* className, int index) {
    if (!isObjInstanceOf(vm, arg, getNativeClass(vm, className))) {
        if (index < 0) {
            runtimeError(vm, "method %s expects receiver to be an instance of class %s but got %s.", 
                className, getObjClass(vm, arg)->name->chars);
        }
        else {
            runtimeError(vm, "method %s expects argument %d to be an instance of class %s but got %s.", 
                method, index, className, getObjClass(vm, arg)->name->chars);
        }
        exit(70);
    }
}

void assertNonNegativeNumber(VM* vm, const char* method, double number, int index) {
    if (number < 0) {
        if (index < 0) runtimeError(vm, "method %s expects receiver to be a non negative number but got %g.", method, number);
        else runtimeError(vm, "method %s expects argument %d to be a non negative number but got %g.", method, index, number);
        exit(70);
    }
}

void assertNonZeroNumber(VM* vm, const char* method, double number, int index) {
    if (number == 0) {
        if (index < 0) runtimeError(vm, "method %s expects receiver to be a non-zero number but got %g.", method, number);
        else runtimeError(vm, "method %s expects argument %d to be a non-zero number but got %g.", method, index, number);
        exit(70);
    }
}

void assertPositiveNumber(VM* vm, const char* method, double number, int index) {
    if (number <= 0) {
        if (index < 0) runtimeError(vm, "method %s expects receiver to be a positive number but got %g.", method, number);
        else runtimeError(vm, "method %s expects argument %d to be a positive number but got %g.", method, index, number);
        exit(70);
    }
}

void raiseError(VM* vm, const char* message) {
    runtimeError(vm, message);
    exit(70);
}
