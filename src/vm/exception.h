#pragma once
#ifndef clox_exception_h
#define clox_exception_h

#include "value.h"

typedef struct {
    uint16_t handlerAddress;
    uint16_t finallyAddress;
    ObjClass* exceptionClass;
} ExceptionHandler;

bool propagateException(VM* vm, bool isPromise);
void pushExceptionHandler(VM* vm, ObjClass* exceptionClass, uint16_t handlerAddress, uint16_t finallyAddress);
ObjArray* getStackTrace(VM* vm);
ObjException* createException(VM* vm, ObjClass* exceptionClass, const char* format, ...);
ObjException* createNativeException(VM* vm, const char* exceptionClassName, const char* format, ...);
ObjException* throwException(VM* vm, ObjClass* exceptionClass, const char* format, ...);
ObjException* throwNativeException(VM* vm, const char* exceptionClassName, const char* format, ...);
ObjException* throwPromiseException(VM* vm, ObjPromise* promise);

#endif // !clox_exception_h