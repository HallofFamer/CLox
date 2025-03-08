#pragma once
#ifndef clox_class_h
#define clox_class_h

#include "value.h"

typedef enum {
    BEHAVIOR_CLASS,
    BEHAVIOR_METACLASS,
    BEHAVIOR_TRAIT
} BehaviorType;

void initClass(VM* vm, ObjClass* klass, ObjString* name, ObjClass* metaclass, BehaviorType behaviorType);
ObjClass* createClass(VM* vm, ObjString* name, ObjClass* metaclass, BehaviorType behaviorType);
void initTrait(VM* vm, ObjClass* trait, ObjString* name);
ObjClass* createTrait(VM* vm, ObjString* name);
ObjClass* getObjClass(VM* vm, Value value);
ObjString* getClassNameFromMetaclass(VM* vm, ObjString* metaclassName);
ObjString* getMetaclassNameFromClass(VM* vm, ObjString* className);
ObjString* getClassFullName(VM* vm, ObjString* shortName, ObjString* currentNamespace);
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

#endif // !clox_class_h
