#include <stdio.h>
#include <stdlib.h>

#include "assert.h"
#include "vm.h"

void assertArgCount(VM* vm, const char* method, int expectedCount, int actualCount){
	if (expectedCount != actualCount) {
		runtimeError(vm, "method %s expects %d argument(s) but got %d instead.", method, expectedCount, actualCount);
		exit(70);
	}
}

void assertArgIsBool(VM* vm, const char* method, Value* args, int index){
	if (!IS_BOOL(args[index])) {
		runtimeError(vm, "method %s expects argument %d to be a boolean value.", method, index + 1);
		exit(70);
	}
}

void assertArgIsClass(VM* vm, const char* method, Value* args, int index){
	if (!IS_CLASS(args[index])) {
		runtimeError(vm, "method %s expects argument %d to be a class.", method, index + 1);
		exit(70);
	}
}

void assertArgIsNumber(VM* vm, const char* method, Value* args, int index){
	if (!IS_NUMBER(args[index])) {
		runtimeError(vm, "method %s expects argument %d to be a number.", method, index + 1);
		exit(70);
	}
}

void assertArgIsString(VM* vm, const char* method, Value* args, int index){
	if (!IS_STRING(args[index])) {
		runtimeError(vm, "method %s expects argument %d to be a string.", method, index + 1);
		exit(70);
	}
}

void assertNonZero(VM* vm, const char* method, double number, int index) {
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
