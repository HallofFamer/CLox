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
    INTERCEPTOR_NEW,
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
bool interceptBeforeGet(VM* vm, ObjClass* klass, ObjString* name);
bool interceptAfterGet(VM* vm, ObjClass* klass, ObjString* name);
bool interceptUndefinedGet(VM* vm, ObjClass* klass, ObjString* name);
bool interceptUndefinedInvoke(VM* vm, ObjClass* klass, ObjString* name, int argCount);

#endif // !clox_interceptor_h
