#pragma once
#ifndef clox_interceptor_h
#define clox_interceptor_h

#include "value.h"

#define HAS_CLASS_INTERCEPTOR(klass, interceptor) ((klass->interceptors) & (1 << interceptor))
#define SET_CLASS_INTERCEPTOR(klass, interceptor) (klass->interceptors = (klass->interceptors) | (1 << interceptor))
#define HAS_OBJ_INTERCEPTOR(object, interceptor) (IS_OBJ(object) && HAS_CLASS_INTERCEPTOR(AS_OBJ(object)->klass, interceptor))

typedef enum {
    INTERCEPTOR_INIT,
    INTERCEPTOR_BEFORE_GET,
    INTERCEPTOR_AFTER_GET,
    INTERCEPTOR_BEFORE_SET,
    INTERCEPTOR_AFTER_SET,
    INTERCEPTOR_ON_INVOKE,
    INTERCEPTOR_ON_RETURN,
    INTERCEPTOR_ON_THROW,
    INTERCEPTOR_ON_YIELD,
    INTERCEPTOR_ON_AWAIT,
    INTERCEPTOR_UNDEFINED_GET,
    INTERCEPTOR_UNDEFINED_INVOKE
} InterceptorType;

void handleInterceptorMethod(VM* vm, ObjClass* klass, ObjString* name);
bool hasInterceptableMethod(VM* vm, Value receiver, ObjString* name);
bool interceptBeforeGet(VM* vm, Value receiver, ObjString* name);
bool interceptAfterGet(VM* vm, Value receiver, ObjString* name, Value value);
bool interceptBeforeSet(VM* vm, Value receiver, ObjString* name, Value value);
bool interceptAfterSet(VM* vm, Value receiver, ObjString* name);
bool interceptOnInvoke(VM* vm, Value receiver, ObjString* name, int argCount);
bool interceptOnReturn(VM* vm, Value receiver, ObjString* name, Value result);
bool interceptOnThrow(VM* vm, Value receiver, ObjString* name, Value exception);
bool interceptOnYield(VM* vm, Value receiver, ObjString* name, Value result);
bool interceptOnAwait(VM* vm, Value receiver, ObjString* name, Value result);
bool interceptUndefinedGet(VM* vm, Value receiver, ObjString* name);
bool interceptUndefinedInvoke(VM* vm, ObjClass* klass, ObjString* name, int argCount);

#endif // !clox_interceptor_h
