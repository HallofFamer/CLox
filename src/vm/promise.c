#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "vm.h"

ObjPromise* promiseAll(VM* vm, ObjClass* klass, ObjArray* promises) {
    int remainingCount = promises->elements.count;
    ObjPromise* allPromise = newPromise(vm, PROMISE_PENDING, NIL_VAL, NIL_VAL);
    allPromise->obj.klass = klass;
    push(vm, OBJ_VAL(allPromise));
    if (remainingCount == 0) allPromise->state = PROMISE_FULFILLED;
    else {
        ObjArray* results = newArray(vm);
        push(vm, OBJ_VAL(results));
        for (int i = 0; i < promises->elements.count; i++) {
            promiseCapture(vm, AS_PROMISE(promises->elements.values[i]), 5, OBJ_VAL(promises), OBJ_VAL(allPromise), OBJ_VAL(results), INT_VAL(remainingCount), INT_VAL(i));
        }
        pop(vm);

        for (int i = 0; i < promises->elements.count; i++) {
            ObjPromise* promise = AS_PROMISE(promises->elements.values[i]);
            Value then = getObjMethod(vm, OBJ_VAL(promise), "then");
            Value thenAll = getObjMethod(vm, OBJ_VAL(promise), "thenAll");
            ObjBoundMethod* thenAllMethod = newBoundMethod(vm, OBJ_VAL(promise), thenAll);
            callReentrantMethod(vm, OBJ_VAL(promise), then, OBJ_VAL(thenAllMethod));

            Value catch = getObjMethod(vm, OBJ_VAL(promise), "catch");
            Value catchAll = getObjMethod(vm, OBJ_VAL(promise), "catchAll");
            ObjBoundMethod* catchAllMethod = newBoundMethod(vm, OBJ_VAL(promise), catchAll);
            callReentrantMethod(vm, OBJ_VAL(promise), catch, OBJ_VAL(catchAllMethod));
        }
    }
    pop(vm);
    return allPromise;
}

void promiseCapture(VM* vm, ObjPromise* promise, int count, ...) {
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++) {
        Value value = va_arg(args, Value);
        valueArrayWrite(vm, &promise->capturedValues->elements, value);
    }
    va_end(args);
}

void promiseExecute(VM* vm, ObjPromise* promise) {
    Value fulfill = getObjMethod(vm, OBJ_VAL(promise), "fulfill");
    Value reject = getObjMethod(vm, OBJ_VAL(promise), "reject");
    ObjBoundMethod* onFulfill = newBoundMethod(vm, OBJ_VAL(promise), fulfill);
    ObjBoundMethod* onReject = newBoundMethod(vm, OBJ_VAL(promise), reject);
    callReentrantMethod(vm, OBJ_VAL(promise), promise->executor, OBJ_VAL(onFulfill), OBJ_VAL(onReject));
}

void promiseFulfill(VM* vm, ObjPromise* promise, Value value) {
    promise->state = PROMISE_FULFILLED;
    promise->value = value;
    for (int i = 0; i < promise->handlers.count; i++) {
        callReentrantMethod(vm, OBJ_VAL(promise), promise->handlers.values[i], promise->value);
    }
    if (IS_CLOSURE(promise->onFinally)) callReentrantMethod(vm, OBJ_VAL(promise), promise->onFinally, promise->value);
}

ObjPromise* promiseRace(VM* vm, ObjClass* klass, ObjArray* promises) {
    ObjPromise* racePromise = newPromise(vm, PROMISE_PENDING, NIL_VAL, NIL_VAL);
    racePromise->obj.klass = klass;
    push(vm, OBJ_VAL(racePromise));

    for (int i = 0; i < promises->elements.count; i++) {
        ObjPromise* promise = AS_PROMISE(promises->elements.values[i]);
        promiseCapture(vm, promise, 1, OBJ_VAL(racePromise));
        Value then = getObjMethod(vm, OBJ_VAL(promise), "then");
        Value raceAll = getObjMethod(vm, OBJ_VAL(promise), "raceAll");
        ObjBoundMethod* raceAllMethod = newBoundMethod(vm, OBJ_VAL(promise), raceAll);
        callReentrantMethod(vm, OBJ_VAL(promise), then, OBJ_VAL(raceAllMethod));
    }
    pop(vm);
    return racePromise;
}

void promiseReject(VM* vm, ObjPromise* promise, Value exception) {
    promise->state = PROMISE_REJECTED;
    promise->exception = AS_EXCEPTION(exception);
    if (IS_CLOSURE(promise->onCatch)) callReentrantMethod(vm, OBJ_VAL(promise), promise->onCatch, exception);
    if (IS_CLOSURE(promise->onFinally)) callReentrantMethod(vm, OBJ_VAL(promise), promise->onFinally, promise->value);
}

void promiseThen(VM* vm, ObjPromise* promise, Value value) {
    for (int i = 0; i < promise->handlers.count; i++) {
        ObjBoundMethod* handler = newBoundMethod(vm, OBJ_VAL(promise), promise->handlers.values[i]);
        callReentrantMethod(vm, OBJ_VAL(promise), handler->method, value);
    }
    initValueArray(&promise->handlers);
}

ObjPromise* promiseWithFulfilled(VM* vm, Value value) {
    return newPromise(vm, PROMISE_FULFILLED, value, NIL_VAL);
}

ObjPromise* promiseWithRejected(VM* vm, ObjException* exception) {
    ObjPromise* promise = newPromise(vm, PROMISE_REJECTED, NIL_VAL, NIL_VAL);
    promise->exception = exception;
    return promise;
}