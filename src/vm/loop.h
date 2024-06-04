#pragma once
#ifndef clox_loop_h
#define clox_loop_h

#include <stdio.h>
#include <uv.h>

#include "common.h"
#include "value.h"

#define LOOP_PUSH_DATA(data) \
    do { \
        push(data->vm, OBJ_VAL(data->vm->currentModule->closure)); \
        data->vm->frameCount++; \
    } while (false)

#define LOOP_POP_DATA(data) \
    do { \
        pop(data->vm); \
        data->vm->frameCount--; \
        free(data); \
    } while (false)

typedef struct {
    VM* vm;
    Value receiver;
    ObjClosure* closure;
    int delay;
    int interval;
} TimerData;

void initLoop(VM* vm);
void freeLoop(VM* vm);
void timerClose(uv_handle_t* handle);
TimerData* timerData(VM* vm, ObjClosure* closure, int delay, int interval);
void timerRun(uv_timer_t* timer);

#endif // !clox_loop_h