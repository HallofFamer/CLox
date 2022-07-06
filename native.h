#pragma once
#ifndef clox_native_h
#define clox_native_h

#include "common.h"
#include "object.h"
#include "value.h"

#define LOX_FUNCTION(name) static Value name##NativeFunction(VM* vm, int argCount, Value* args)
#define LOX_METHOD(className, name) static Value name##NativeMethodFor##className(VM* vm, Value receiver, int argCount, Value* args)
#define DEF_FUNCTION(name) defineNativeFunction(vm, #name, name##NativeFunction)
#define DEF_METHOD(klass, className, name) defineNativeMethod(vm, klass, #name, name##NativeMethodFor##className)

#define RETURN_NIL return NIL_VAL
#define RETURN_FALSE return BOOL_VAL(false)
#define RETURN_TRUE return BOOL_VAL(true)
#define RETURN_BOOL(value) return BOOL_VAL(value)
#define RETURN_NUMBER(value) return NUMBER_VAL(value)
#define RETURN_OBJ(value) return OBJ_VAL(value)
#define RETURN_STRING(chars) return OBJ_VAL(copyString(vm, chars, (int)strlen(chars)))

ObjClass* defineNativeClass(VM* vm, const char* name);
void defineNativeFunction(VM* vm, const char* name, NativeFn function);
void defineNativeMethod(VM* vm, ObjClass* klass, const char* name, NativeMethod method);
void registerNativeFunctions(VM* vm);

#endif
