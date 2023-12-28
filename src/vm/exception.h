#pragma once
#ifndef clox_exception_h
#define clox_exception_h

#include "common.h"
#include "object.h"

typedef struct {
    uint16_t handlerAddress;
    uint16_t finallyAddress;
    ObjClass* exceptionClass;
} ExceptionHandler;

bool propagateException(VM* vm);
void pushExceptionHandler(VM* vm, ObjClass* exceptionClass, uint16_t handlerAddress, uint16_t finallyAddress);
ObjArray* getStackTrace(VM* vm);
ObjException* throwException(VM* vm, ObjClass* exceptionClass, const char* format, ...);


#endif