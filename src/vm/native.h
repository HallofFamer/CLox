#pragma once
#ifndef clox_native_h
#define clox_native_h

#include <time.h>

#include "object.h"
#include "value.h"
#include "vm.h"

#define LOX_FUNCTION(name) static Value name##NativeFunction(VM* vm, int argCount, Value* args)
#define LOX_METHOD(className, name) static Value name##NativeMethodFor##className(VM* vm, Value receiver, int argCount, Value* args)
#define DEF_FUNCTION(name, arity, ...) defineNativeFunction(vm, #name, arity, false, name##NativeFunction, __VA_ARGS__)
#define DEF_FUNCTION_ASYNC(name, arity, ...) defineNativeFunction(vm, #name, arity, true, name##NativeFunction, __VA_ARGS__)
#define DEF_METHOD(klass, className, name, arity) defineNativeMethod(vm, klass, #name, arity, false, name##NativeMethodFor##className)
#define DEF_METHOD_ASYNC(klass, className, name, arity) defineNativeMethod(vm, klass, #name, arity, true, name##NativeMethodFor##className)
#define DEF_OPERATOR(klass, className, symbol, name, arity) defineNativeMethod(vm, klass, #symbol, arity, false, name##NativeMethodFor##className)
#define DEF_INTERCEPTOR(klass, className, type, name, arity) defineNativeInterceptor(vm, klass, type, arity, name##NativeMethodFor##className)

#define RETURN_VAL(value) return (value)
#define RETURN_NIL return NIL_VAL
#define RETURN_FALSE return BOOL_VAL(false)
#define RETURN_TRUE return BOOL_VAL(true)
#define RETURN_BOOL(value) return BOOL_VAL(value)
#define RETURN_INT(value) return INT_VAL(value)
#define RETURN_NUMBER(value) return NUMBER_VAL(value)
#define RETURN_OBJ(value) return OBJ_VAL(value)
#define RETURN_PROMISE(state, value) return OBJ_VAL(newPromise(vm, state, value, NIL_VAL));
#define RETURN_PROMISE_EX(klass, message) return OBJ_VAL(newPromise(vm, PROMISE_REJECTED, OBJ_VAL(createException(vm, getNativeClass(vm, #klass), message)), NIL_VAL))
#define RETURN_PROMISE_EX_FMT(klass, message, ...) return OBJ_VAL(newPromise(vm, PROMISE_REJECTED, OBJ_VAL(createException(vm, getNativeClass(vm, #klass), message, __VA_ARGS__)), NIL_VAL))
#define RETURN_STRING(chars, length) return OBJ_VAL(copyString(vm, chars, length))
#define RETURN_STRING_FMT(...) return OBJ_VAL(formattedString(vm, __VA_ARGS__))
#define THROW_EXCEPTION(klass, message) return OBJ_VAL(throwException(vm, getNativeClass(vm, #klass), message))
#define THROW_EXCEPTION_FMT(klass, message, ...) return OBJ_VAL(throwException(vm, getNativeClass(vm, #klass), message, __VA_ARGS__))
#define RETURN_TYPE(type) getNativeClassByName(vm, #type)
#define PARAM_TYPE(type) getNativeClassByName(vm, #type)

ObjClass* defineNativeClass(VM* vm, const char* name);
void defineNativeFunction(VM* vm, const char* name, int arity, bool isAsync, NativeFunction function, ...);
void defineNativeMethod(VM* vm, ObjClass* klass, const char* name, int arity, bool isAsync, NativeMethod method);
void defineNativeInterceptor(VM* vm, ObjClass* klass, InterceptorType type, int arity, NativeMethod method);
ObjClass* defineNativeTrait(VM* vm, const char* name);
ObjNamespace* defineNativeNamespace(VM* vm, const char* name, ObjNamespace* enclosing);
ObjClass* defineNativeException(VM* vm, const char* name, ObjClass* superClass);
ObjClass* getNativeClass(VM* vm, const char* fullName);
ObjClass* getNativeClassByName(VM* vm, const char* name);
ObjNativeFunction* getNativeFunction(VM* vm, const char* name);
ObjNativeMethod* getNativeMethod(VM* vm, ObjClass* klass, const char* name);
ObjNamespace* getNativeNamespace(VM* vm, const char* name);
SymbolItem* insertGlobalSymbolTable(VM * vm, const char* symbolName);
TypeInfo* insertTypeTable(VM* vm, TypeCategory category, ObjString* shortName, ObjString* fullName);
FunctionTypeInfo* insertTypeSignature(VM * vm, int arity, const char* returnTypeName, ...);
void loadSourceFile(VM* vm, const char* filePath);
void registerNativeFunctions(VM* vm);

static inline double currentTimeInSec() {
    return (double)clock() / CLOCKS_PER_SEC;
}

#endif // !clox_native_h
