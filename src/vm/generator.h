#pragma once
#ifndef clox_generator_h
#define clox_generator_h

#include "common.h"
#include "value.h"

typedef enum {
    GENERATOR_START,
    GENERATOR_YIELD,
    GENERATOR_RESUME,
    GENERATOR_RETURN,
    GENERATOR_THROW,
    GENERATOR_ERROR
} GeneratorState;

void initGenerator(VM* vm, ObjGenerator* generator, ObjClosure* closure, ObjArray* arguments);
void resumeGenerator(VM* vm, ObjGenerator* generator);
void loadGeneratorFrame(VM* vm, ObjGenerator* generator);
void saveGeneratorFrame(VM* vm, ObjGenerator* generator, CallFrame* frame, Value result);
Value loadInnerGenerator(VM* vm);
void yieldFromInnerGenerator(VM* vm, ObjGenerator* generator);

#endif // !clox_generator_h