#pragma once
#ifndef clox_native_h
#define clox_native_h

#include "common.h"
#include "object.h"
#include "value.h"

ObjClass* defineNativeClass(VM* vm, const char* name);
void defineNativeFunction(VM* vm, const char* name, NativeFn function);
void defineNativeMethod(VM* vm, ObjClass* klass, const char* name, NativeMethod method);
void registerNativeFunctions(VM* vm);

#endif
