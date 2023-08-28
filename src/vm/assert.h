#pragma once
#ifndef clox_assert_h
#define clox_assert_h

#include "common.h"
#include "value.h"

#define ASSERT_ARG_COUNT(method, expectedCount) assertArgCount(vm, method, expectedCount, argCount)
#define ASSERT_ARG_INSTANCE_OF(method, index, namespaceName, className) assertArgInstanceOf(vm, method, args, index, #namespaceName, #className)
#define ASSERT_ARG_INSTANCE_OF_EITHER(method, index, namespaceName, className, namespaceName2, className2) assertArgInstanceOfEither(vm, method, args, index, #namespaceName, #className, #namespaceName2, #className2)
#define ASSERT_ARG_TYPE(method, index, type) assertArgIs##type(vm, method, args, index)

void assertArgCount(VM* vm, const char* method, int expectedCount, int actualCount);
void assertArgInstanceOf(VM* vm, const char* method, Value* args, int index, const char* namespaceName, const char* className);
void assertArgIsArray(VM* vm, const char* method, Value* args, int index);
void assertArgIsBool(VM* vm, const char* method, Value* args, int index);
void assertArgIsClass(VM* vm, const char* method, Value* args, int index);
void assertArgIsClosure(VM* vm, const char* method, Value* args, int index);
void assertArgIsDictionary(VM* vm, const char* method, Value* args, int index);
void assertArgIsEntry(VM* vm, const char* method, Value* args, int index);
void assertArgIsFile(VM* vm, const char* method, Value* args, int index);
void assertArgIsFloat(VM* vm, const char* method, Value* args, int index);
void assertArgIsInt(VM* vm, const char* method, Value* args, int index);
void assertArgIsMethod(VM* vm, const char* method, Value* args, int index);
void assertArgIsNamespace(VM* vm, const char* method, Value* args, int index);
void assertArgIsNode(VM* vm, const char* method, Value* args, int index);
void assertArgIsNumber(VM* vm, const char* method, Value* args, int index);
void assertArgIsRange(VM* vm, const char* method, Value* args, int index);
void assertArgIsString(VM* vm, const char* method, Value* args, int index);
void assertIntWithinRange(VM* vm, const char* method, int value, int min, int max, int index);
void assertNumberNonNegative(VM* vm, const char* method, double number, int index);
void assertNumberNonZero(VM* vm, const char* method, double number, int index);
void assertNumberPositive(VM* vm, const char* method, double number, int index);
void assertNumberWithinRange(VM* vm, const char* method, double value, double min, double max, int index);
void raiseError(VM* vm, const char* message);

#endif // !clox_assert_h