#pragma once
#ifndef clox_assert_h
#define clox_assert_h

#include "common.h"
#include "value.h"

void assertArgCount(VM* vm, const char* method, int expectedCount, int actualCount);
void assertArgIsBool(VM* vm, const char* method, Value* args, int index);
void assertArgIsClass(VM* vm, const char* method, Value* args, int index);
void assertArgIsNumber(VM* vm, const char* method, Value* args, int index);
void assertArgIsString(VM* vm, const char* method, Value* args, int index);
void assertPositiveNumber(VM* vm, const char* method, double number, int index);
void raiseError(VM* vm, const char* message);

#endif // !clox_assert_h