#pragma once
#ifndef clox_klass_h
#define clox_klass_h

#include "common.h"
#include "value.h"

#define HAS_CLASS_INTERCEPTOR(klass, interceptor) ((klass->interceptors) & (1 << interceptor))

typedef enum {
    BEHAVIOR_CLASS,
    BEHAVIOR_METACLASS,
    BEHAVIOR_TRAIT
} BehaviorType;

typedef enum {
    INTERCEPTOR_INIT,
    INTERCEPTOR_NEW,
    INTERCEPTOR_GET_PROPERTY,
    INTERCEPTOR_INVOKE_METHOD,
    INTERCEPTOR_BEFORE_GET_PROPERTY,
    INTERCEPTOR_AFTER_GET_PROPERTY,
    INTERCEPTOR_BEFORE_SET_PROPERTY,
    INTERCEPTOR_AFTER_SET_PROPERTY,
    INTERCEPTOR_BEFORE_INVOKE_METHOD,
    INTERCEPTOR_AFTER_INVOKE_METHOD
} InterceptorType;

void initClass(VM* vm, ObjClass* klass, ObjString* name, ObjClass* metaclass, BehaviorType behaviorType);
ObjClass* createClass(VM* vm, ObjString* name, ObjClass* metaclass, BehaviorType behaviorType);
void initTrait(VM* vm, ObjClass* trait, ObjString* name);
ObjClass* createTrait(VM* vm, ObjString* name);
ObjClass* getObjClass(VM* vm, Value value);
bool isObjInstanceOf(VM* vm, Value value, ObjClass* klass);
bool isClassExtendingSuperclass(ObjClass* klass, ObjClass* superclass);
bool isClassImplementingTrait(ObjClass* trait, ObjClass* klass);
void inheritSuperclass(VM* vm, ObjClass* subclass, ObjClass* superclass);
void bindSuperclass(VM* vm, ObjClass* subclass, ObjClass* superclass);
void implementTraits(VM* vm, ObjClass* klass, ValueArray* traits);
void bindTrait(VM* vm, ObjClass* klass, ObjClass* trait);
void bindTraits(VM* vm, int numTraits, ObjClass* klass, ...);
Value getClassProperty(VM* vm, ObjClass* klass, char* name);
void setClassProperty(VM* vm, ObjClass* klass, char* name, Value value);

#endif // !clox_klass_h
