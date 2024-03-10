#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "vm.h"

void resumeGenerator(VM* vm, ObjGenerator* generator) {
    vm->apiStackDepth++;
    Value result = callGenerator(vm, generator);
    vm->stackTop -= generator->frame->slotCount + 1;
    push(vm, OBJ_VAL(generator));
    vm->apiStackDepth--;
    generator->value = result;
}

void loadGeneratorFrame(VM* vm, ObjGenerator* generator) {
    CallFrame* frame = &vm->frames[vm->frameCount++];
    frame->closure = generator->frame->closure;
    frame->ip = generator->frame->ip;
    frame->slots = vm->stackTop - 1;

    for (int i = 1; i < generator->frame->slotCount; i++) {
        push(vm, generator->frame->slots[i]);
    }
    frame->slots[0] = generator->frame->slots[0];
    if (generator->state != GENERATOR_START) push(vm, generator->value);
    generator->state = GENERATOR_RESUME;
}

void saveGeneratorFrame(VM* vm, ObjGenerator* generator, CallFrame* frame, Value result) {
    generator->frame->closure = frame->closure;
    generator->frame->ip = frame->ip;
    generator->state = GENERATOR_YIELD;
    generator->value = result;

    generator->frame->slotCount = 0;
    for (Value* slot = frame->slots; slot < vm->stackTop - 1; slot++) {
        generator->frame->slots[generator->frame->slotCount++] = *slot;
    }
}

Value loadInnerGenerator(VM* vm) {
    if (vm->runningGenerator->inner == NULL) {
        throwNativeException(vm, "clox.std.lang.IllegalArgumentException", "Cannot only yield from a generator.");
    }
    Value result = vm->runningGenerator->inner != NULL ? OBJ_VAL(vm->runningGenerator->inner) : NIL_VAL;
    pop(vm);
    push(vm, result);
    return result;
}

void yieldFromInnerGenerator(VM* vm, ObjGenerator* generator) {
    vm->runningGenerator->frame->ip--;
    vm->runningGenerator->inner = generator;
    Value result = callGenerator(vm, generator);
    pop(vm);
    push(vm, result);
}