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
void promisePushHandler(VM* vm, ObjPromise* promise, Value handler, ObjPromise* thenPromise);
ObjPromise* promiseRace(VM* vm, ObjClass* klass, ObjArray* promises);
void promiseReject(VM* vm, ObjPromise* promise, Value exception);
void promiseThen(VM* vm, ObjPromise* promise, Value value);
ObjPromise* promiseWithFulfilled(VM* vm, Value value);
ObjPromise* promiseWithRejected(VM* vm, ObjException* exception);

#endif // !clox_promise_h