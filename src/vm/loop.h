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
    uv_buf_t buffer;
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
FileData* fileData(VM* vm, ObjFile* file, ObjPromise* promise);
int fileMode(const char* mode);
void fileOnClose(uv_fs_t* fsClose);
void fileOnOpen(uv_fs_t* fsOpen);
void fileOnRead(uv_fs_t* fsRead);
void fileOnWrite(uv_fs_t* fsWrite);
char* streamClassName(const char* mode);
void timerClose(uv_handle_t* handle);
TimerData* timerData(VM* vm, ObjClosure* closure, int delay, int interval);
void timerRun(uv_timer_t* timer);

#endif // !clox_loop_h