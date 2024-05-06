#pragma once
#ifndef clox_loop_h
#define clox_loop_h

#include <stdio.h>
#include <uv.h>

#include "common.h"
#include "value.h"

typedef struct {
    VM* vm;
    ObjFile* file;
    ObjPromise* promise;
} FileData;

typedef struct {
    VM* vm;
    Value receiver;
    ObjClosure* closure;
    int delay;
    int interval;
} TimerData;

void initLoop(VM* vm);
void freeLoop(VM* vm);
void fileOpen(uv_fs_t* fsRequest);
void timerClose(uv_handle_t* handle);
void timerRun(uv_timer_t* timer);

#endif // !clox_loop_h