#pragma once
#ifndef clox_native_h
#define clox_native_h

#include "common.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define LOX_FUNCTION(name) static Value name##NativeFunction(VM* vm, int argCount, Value* args)
#define LOX_METHOD(className, name) static Value name##NativeMethodFor##className(VM* vm, Value receiver, int argCount, Value* args)
#define DEF_FUNCTION(name, arity) defineNativeFunction(vm, #name, arity, name##NativeFunction)
#define DEF_METHOD(klass, className, name, arity) defineNativeMethod(vm, klass, #name, arity, name##NativeMethodFor##className)

#define RETURN_VAL(value) return (value)
#define RETURN_NIL return NIL_VAL
#define RETURN_FALSE return BOOL_VAL(false)
#define RETURN_TRUE return BOOL_VAL(true)
#define RETURN_BOOL(value) return BOOL_VAL(value)
#define RETURN_INT(value) return INT_VAL(value)
#define RETURN_NUMBER(value) return NUMBER_VAL(value)
#define RETURN_OBJ(value) return OBJ_VAL(value)
#define RETURN_STRING(chars, length) return OBJ_VAL(copyString(vm, chars, length))
#define RETURN_STRING_FMT(...) return OBJ_VAL(formattedString(vm, __VA_ARGS__))
#define THROW_EXCEPTION(klass, message) return OBJ_VAL(throwException(vm, getNativeClass(vm, #klass), message))
#define THROW_EXCEPTION_FMT(klass, message, ...) return OBJ_VAL(throwException(vm, getNativeClass(vm, #klass), message, __VA_ARGS__))

ObjClass* defineNativeClass(VM* vm, const char* name);
void defineNativeFunction(VM* vm, const char* name, int arity, NativeFunction function);
void defineNativeMethod(VM* vm, ObjClass* klass, const char* name, int arity, NativeMethod method);
ObjClass* defineNativeTrait(VM* vm, const char* name);
ObjNamespace* defineNativeNamespace(VM* vm, const char* name, ObjNamespace* enclosing);
ObjClass* defineNativeException(VM* vm, const char* name, ObjClass* superClass);
ObjClass* getNativeClass(VM* vm, const char* fullName);
ObjNativeFunction* getNativeFunction(VM* vm, const char* name);
ObjNativeMethod* getNativeMethod(VM* vm, ObjClass* klass, const char* name);
ObjNamespace* getNativeNamespace(VM* vm, const char* name);
void loadSourceFile(VM* vm, const char* filePath);
void registerNativeFunctions(VM* vm);

#endif // !clox_native_h
