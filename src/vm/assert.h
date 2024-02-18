#pragma once
#ifndef clox_assert_h
#define clox_assert_h

#include "common.h"
#include "native.h"
#include "value.h"

#define ASSERT_ARG_COUNT(method, expectedCount) \
    do { \
       Value message = assertArgCount(vm, method, expectedCount, argCount); \
       if (IS_STRING(message)) THROW_EXCEPTION(clox.std.lang.IllegalArgumentException, AS_CSTRING(message)); \
    } while (false)

#define ASSERT_ARG_INSTANCE_OF(method, index, className) \
    do { \
        Value message = assertArgInstanceOf(vm, method, args, index, #className); \
        if (IS_STRING(message)) THROW_EXCEPTION(clox.std.lang.IllegalArgumentException, AS_CSTRING(message)); \
    } while (false)

#define ASSERT_ARG_INSTANCE_OF_ANY(method, index, className, className2) \
    do { \
        Value message = assertArgInstanceOfAny(vm, method, args, index, #className, #className2); \
        if (IS_STRING(message)) THROW_EXCEPTION(clox.std.lang.IllegalArgumentException, AS_CSTRING(message)); \
    } while (false)

#define ASSERT_ARG_TYPE(method, index, type) \
    do { \
        Value message = assertArgIs##type(vm, method, args, index); \
        if (IS_STRING(message)) THROW_EXCEPTION(clox.std.lang.IllegalArgumentException, AS_CSTRING(message)); \
    } while (false)

#define ASSERT_INDEX_WITHIN_BOUNDS(method, value, min, max, index) \
    do { \
        Value message = assertIndexWithinBounds(vm, method, value, min, max, index); \
        if (IS_STRING(message)) THROW_EXCEPTION(clox.std.lang.IndexOutOfBoundsException, AS_CSTRING(message)); \
    } while (false)

Value assertArgCount(VM* vm, const char* method, int expectedCount, int actualCount);
Value assertArgInstanceOf(VM* vm, const char* method, Value* args, int index, const char* className);
Value assertArgInstanceOfAny(VM* vm, const char* method, Value* args, int index, const char* className, const char* className2);
Value assertArgIsArray(VM* vm, const char* method, Value* args, int index);
Value assertArgIsBool(VM* vm, const char* method, Value* args, int index);
Value assertArgIsClass(VM* vm, const char* method, Value* args, int index);
Value assertArgIsClosure(VM* vm, const char* method, Value* args, int index);
Value assertArgIsDictionary(VM* vm, const char* method, Value* args, int index);
Value assertArgIsEntry(VM* vm, const char* method, Value* args, int index);
Value assertArgIsException(VM* vm, const char* method, Value* args, int index);
Value assertArgIsFile(VM* vm, const char* method, Value* args, int index);
Value assertArgIsFloat(VM* vm, const char* method, Value* args, int index);
Value assertArgIsGenerator(VM * vm, const char* method, Value * args, int index);
Value assertArgIsInt(VM* vm, const char* method, Value* args, int index);
Value assertArgIsMethod(VM* vm, const char* method, Value* args, int index);
Value assertArgIsNamespace(VM* vm, const char* method, Value* args, int index);
Value assertArgIsNil(VM * vm, const char* method, Value * args, int index);
Value assertArgIsNode(VM* vm, const char* method, Value* args, int index);
Value assertArgIsNumber(VM* vm, const char* method, Value* args, int index);
Value assertArgIsRange(VM* vm, const char* method, Value* args, int index);
Value assertArgIsString(VM* vm, const char* method, Value* args, int index);
Value assertIndexWithinBounds(VM* vm, const char* method, int value, int min, int max, int index);
void raiseError(VM* vm, const char* message);

#endif // !clox_assert_h