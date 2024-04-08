#pragma once
#ifndef clox_promise_h
#define clox_promise_h

#include "common.h"
#include "value.h"

typedef enum {
    PROMISE_PENDING,
    PROMISE_FULFILLED,
    PROMISE_REJECTED
} PromiseState;

ObjPromise* promiseAll(VM* vm, ObjClass* klass, ObjArray* promises);
void promiseCapture(VM* vm, ObjPromise* promise, int count, ...);
void promiseExecute(VM* vm, ObjPromise* promise);
void promiseFulfill(VM* vm, ObjPromise* promise, Value value);
ObjPromise* promiseRace(VM* vm, ObjClass* klass, ObjArray* promises);

#endif // !clox_promise_h