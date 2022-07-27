#pragma once
#ifndef clox_assert_h
#define clox_assert_h

#include "common.h"
#include "value.h"

void assertArgCount(VM* vm, const char* method, int expectedCount, int actualCount);
void assertArgIsBool(VM* vm, const char* method, Value* args, int index);
void assertArgIsClass(VM* vm, const char* method, Value* args, int index);
void assertArgIsClosure(VM* vm, const char* method, Value* args, int index);
void assertArgIsDictionary(VM* vm, const char* method, Value* args, int index);
void assertArgIsFloat(VM* vm, const char* method, Value* args, int index);
void assertArgIsInt(VM* vm, const char* method, Value* args, int index);
void assertArgIsList(VM* vm, const char* method, Value* args, int index);
void assertArgIsNumber(VM* vm, const char* method, Value* args, int index);
void assertArgIsString(VM* vm, const char* method, Value* args, int index);
void assertIntWithinRange(VM* vm, const char* method, int value, int min, int max, int index);
void assertNumberNonNegative(VM* vm, const char* method, double number, int index);
void assertNumberNonZero(VM* vm, const char* method, double number, int index);
void assertNumberPositive(VM* vm, const char* method, double number, int index);
void assertNumberWithinRange(VM* vm, const char* method, double value, double min, double max, int index);
void assertObjInstanceOfClass(VM* vm, const char* method, Value arg, char* className, int index);
void raiseError(VM* vm, const char* message);

#endif // !clox_assert_h