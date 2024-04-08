#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "vm.h"

ObjPromise* promiseAll(VM* vm, ObjClass* klass, ObjArray* promises) {
    ObjArray* results = newArray(vm);
    push(vm, OBJ_VAL(results));
    int numCompleted = 0;
    for (int i = 0; i < promises->elements.count; i++) {
        promiseCapture(vm, AS_PROMISE(promises->elements.values[i]), 4, OBJ_VAL(promises), OBJ_VAL(results), INT_VAL(numCompleted), INT_VAL(i));
    }
    pop(vm);

    ObjPromise* allPromise = newPromise(vm, NIL_VAL);
    push(vm, OBJ_VAL(allPromise));
    allPromise->obj.klass = klass;
    Value execute = getObjMethod(vm, OBJ_VAL(allPromise), "execute");
    ObjBoundMethod* executor = newBoundMethod(vm, OBJ_VAL(allPromise), execute);
    allPromise->executor = OBJ_VAL(executor);

    promiseCapture(vm, allPromise, 3, OBJ_VAL(promises), OBJ_VAL(results), INT_VAL(numCompleted));
    promiseExecute(vm, allPromise);
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
    Value reject = getObjMethod(vm, OBJ(promise), "reject");

    ObjBoundMethod* onFulfill = newBoundMethod(vm, OBJ_VAL(promise), fulfill);
    ObjBoundMethod* onReject = newBoundMethod(vm, OBJ_VAL(promise), reject);
    callReentrantMethod(vm, OBJ_VAL(promise), promise->executor, OBJ_VAL(onFulfill), OBJ_VAL(onReject));
}

void promiseFulfill(VM* vm, ObjPromise* promise, Value value) {
    for (int i = 0; i < promise->handlers.count; i++) {
        ObjBoundMethod* handler = newBoundMethod(vm, OBJ_VAL(promise), promise->handlers.values[i]);
        callReentrantMethod(vm, OBJ_VAL(promise), handler->method, value);
    }
    initValueArray(&promise->handlers);
}

ObjPromise* promiseRace(VM* vm, ObjClass* klass, ObjArray* promises) {
    ObjPromise* racePromise = newPromise(vm, NIL_VAL);
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
