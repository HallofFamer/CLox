#pragma once
#ifndef clox_assert_h
#define clox_assert_h

#include "common.h"
#include "value.h"

#define ASSERT_ARG_COUNT(method, expectedCount) assertArgCount(vm, method, expectedCount, argCount)
#define ASSERT_ARG_INSTANCE_OF(method, index, className) assertArgInstanceOf(vm, method, args, index, #className)
#define ASSERT_ARG_TYPE(method, index, type) assertArgIs##type(vm, method, args, index)

void assertArgCount(VM* vm, const char* method, int expectedCount, int actualCount);
void assertArgInstanceOf(VM* vm, const char* method, Value* args, int index, char* className);
void assertArgIsBool(VM* vm, const char* method, Value* args, int index);
void assertArgIsClass(VM* vm, const char* method, Value* args, int index);
void assertArgIsClosure(VM* vm, const char* method, Value* args, int index);
void assertArgIsDictionary(VM* vm, const char* method, Value* args, int index);
void assertArgIsEntry(VM* vm, const char* method, Value* args, int index);
void assertArgIsFile(VM* vm, const char* method, Value* args, int index);
void assertArgIsFloat(VM* vm, const char* method, Value* args, int index);
void assertArgIsInt(VM* vm, const char* method, Value* args, int index);
void assertArgIsList(VM* vm, const char* method, Value* args, int index);
void assertArgIsNode(VM* vm, const char* method, Value* args, int index);
void assertArgIsNumber(VM* vm, const char* method, Value* args, int index);
void assertArgIsString(VM* vm, const char* method, Value* args, int index);
void assertIntWithinRange(VM* vm, const char* method, int value, int min, int max, int index);
void assertNumberNonNegative(VM* vm, const char* method, double number, int index);
void assertNumberNonZero(VM* vm, const char* method, double number, int index);
void assertNumberPositive(VM* vm, const char* method, double number, int index);
void assertNumberWithinRange(VM* vm, const char* method, double value, double min, double max, int index);
void raiseError(VM* vm, const char* message);

#endif // !clox_assert_h