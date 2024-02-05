#pragma once
#ifndef clox_interceptor_h
#define clox_interceptor_h

#include "common.h"
#include "value.h"

#define HAS_CLASS_INTERCEPTOR(klass, interceptor) ((klass->interceptors) & (1 << interceptor))
#define SET_CLASS_INTERCEPTOR(klass, interceptor) (klass->interceptors = (klass->interceptors) | (1 << interceptor))
#define HAS_OBJ_INTERCEPTOR(object, interceptor) (IS_OBJ(object) && HAS_CLASS_INTERCEPTOR(AS_OBJ(object)->klass, interceptor))

typedef enum {
    INTERCEPTOR_INIT,
    INTERCEPTOR_BEFORE_GET,
    INTERCEPTOR_AFTER_GET,
    INTERCEPTOR_UNDEFINED_GET,
    INTERCEPTOR_BEFORE_SET,
    INTERCEPTOR_AFTER_SET,
    INTERCEPTOR_BEFORE_INVOKE,
    INTERCEPTOR_AFTER_INVOKE,
    INTERCEPTOR_UNDEFINED_INVOKE,
    INTERCEPTOR_BEFORE_THROW,
    INTERCEPTOR_AFTER_THROW
} InterceptorType;

void handleInterceptorMethod(VM* vm, ObjClass* klass, ObjString* name);
bool hasInterceptableMethod(VM* vm, Value receiver, ObjString* name);
bool interceptBeforeGet(VM* vm, Value receiver, ObjString* name);
bool interceptAfterGet(VM* vm, Value receiver, ObjString* name, Value value);
bool interceptUndefinedGet(VM* vm, Value receiver, ObjString* name);
bool interceptBeforeSet(VM* vm, Value receiver, ObjString* name, Value value);
bool interceptAfterSet(VM* vm, Value receiver, ObjString* name);
bool interceptBeforeInvoke(VM* vm, Value receiver, ObjString* name, int argCount);
bool interceptAfterInvoke(VM* vm, Value receiver, ObjString* name, Value result);
bool interceptUndefinedInvoke(VM* vm, ObjClass* klass, ObjString* name, int argCount);
bool interceptBeforeThrow(VM* vm, Value receiver, ObjString* name, Value exception);
bool interceptAfterThrow(VM* vm, Value receiver, ObjString* name);

#endif // !clox_interceptor_h
