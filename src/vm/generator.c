#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "vm.h"

void initGenerator(VM* vm, ObjGenerator* generator, Value callee, ObjArray* arguments) {
    ObjClosure* closure = AS_CLOSURE(IS_BOUND_METHOD(callee) ? AS_BOUND_METHOD(callee)->method : callee);
    for (int i = 0; i < arguments->elements.count; i++) {
        push(vm, arguments->elements.values[i]);
    } 

    CallFrame callFrame = {
        .closure = closure,
        .ip = closure->function->chunk.code,
        .slots = vm->stackTop - arguments->elements.count - 1
    };
    ObjFrame* frame = newFrame(vm, &callFrame);

    generator->frame = frame;
    generator->outer = vm->runningGenerator;
    generator->inner = NULL;
    generator->state = GENERATOR_START;
    generator->value = NIL_VAL;
}

void resumeGenerator(VM* vm, ObjGenerator* generator) {
    vm->apiStackDepth++;
    Value result = callGenerator(vm, generator);
    vm->stackTop -= generator->frame->slotCount;
    push(vm, OBJ_VAL(generator));
    vm->apiStackDepth--;
    generator->value = result;
}

void loadGeneratorFrame(VM* vm, ObjGenerator* generator) {
    CallFrame* frame = &vm->frames[vm->frameCount++];
    frame->closure = generator->frame->closure;
    frame->ip = generator->frame->ip;
    frame->slots = vm->stackTop - 1;
    frame->slots[0] = generator->frame->slots[0];

    for (int i = 1; i < generator->frame->slotCount; i++) {
        push(vm, generator->frame->slots[i]);
    }

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

    for (int i = 0; i < generator->frame->closure->function->arity + 1; i++) {
        pop(vm);
    }
    if (generator->state != GENERATOR_RETURN) push(vm, result);
}

Value runGeneratorAsync(VM* vm, Value callee, ObjArray* arguments) {
    ObjGenerator* generator = newGenerator(vm, NULL, NULL);
    push(vm, OBJ_VAL(generator));
    initGenerator(vm, generator, callee, arguments);

    for (int i = 0; i < arguments->elements.count; i++) {
        pop(vm);
    }
    pop(vm);

    Value step = getObjMethod(vm, OBJ_VAL(generator), "step");
    Value result = callReentrantMethod(vm, OBJ_VAL(generator), step, NIL_VAL);
    return result;
}
