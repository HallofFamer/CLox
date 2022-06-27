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
