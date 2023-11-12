#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"

bool propagateException(VM* vm) {
    ObjInstance* exception = AS_INSTANCE(peek(vm, 0));
    while (vm->frameCount > 0) {
        CallFrame* frame = &vm->frames[vm->frameCount - 1];
        for (int i = frame->handlerCount; i > 0; i--) {
            ExceptionHandler handler = frame->handlerStack[i - 1];
            if (isObjInstanceOf(vm, OBJ_VAL(exception), handler.exceptionClass)) {
                frame->ip = &frame->closure->function->chunk.code[handler.handlerAddress];
                return true;
            }
            else if (handler.finallyAddress != UINT16_MAX) {
                push(vm, TRUE_VAL);
                frame->ip = &frame->closure->function->chunk.code[handler.finallyAddress];
                return true;
            }
        }
        vm->frameCount--;
    }

    ObjString* message = AS_STRING(getObjProperty(vm, exception, "message"));
    fprintf(stderr, "Unhandled %s.%s: %s\n", exception->obj.klass->namespace->fullName->chars, exception->obj.klass->name->chars, message->chars);
    ObjArray* stackTrace = AS_ARRAY(getObjProperty(vm, exception, "stacktrace"));
    for (int i = 0; i < stackTrace->elements.count; i++) {
        Value item = stackTrace->elements.values[i];
        fprintf(stderr, "    %s.\n", AS_CSTRING(item));
    }
    fflush(stderr);
    return false;
}

void pushExceptionHandler(VM* vm, ObjClass* exceptionClass, uint16_t handlerAddress, uint16_t finallyAddress) {
    CallFrame* frame = &vm->frames[vm->frameCount - 1];
    if (frame->handlerCount >= UINT4_MAX) {
        runtimeError(vm, "Too many nested exception handlers.");
        exit(70);
    }
    frame->handlerStack[frame->handlerCount].handlerAddress = handlerAddress;
    frame->handlerStack[frame->handlerCount].exceptionClass = exceptionClass;
    frame->handlerStack[frame->handlerCount].finallyAddress = finallyAddress;
    frame->handlerCount++;
}

ObjArray* getStackTrace(VM* vm) {
    ObjArray* stackTrace = newArray(vm);
    push(vm, OBJ_VAL(stackTrace));
    for (int i = vm->frameCount - 1; i >= 0; i--) {
        char stackTraceBuffer[UINT8_MAX];
        CallFrame* frame = &vm->frames[i];
        ObjModule* module = frame->closure->module;
        ObjFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        uint32_t line = function->chunk.lines[instruction];

        uint8_t length = snprintf(stackTraceBuffer, UINT8_MAX, "in %s() from %s at line %d",
            function->name == NULL ? "script" : function->name->chars, module->path->chars, line);
        ObjString* stackElement = copyString(vm, stackTraceBuffer, length);
        valueArrayWrite(vm, &stackTrace->elements, OBJ_VAL(stackElement));
    }
    pop(vm);
    return stackTrace;
}

ObjInstance* throwException(VM* vm, ObjClass* exceptionClass, const char* format, ...) {
    char chars[UINT8_MAX];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(chars, UINT8_MAX, format, args);
    va_end(args);
    ObjString* message = copyString(vm, chars, length);

    ObjArray* stacktrace = getStackTrace(vm);
    ObjInstance* exception = newInstance(vm, exceptionClass);
    push(vm, OBJ_VAL(exception));
    setObjProperty(vm, exception, "message", OBJ_VAL(message));
    setObjProperty(vm, exception, "stacktrace", OBJ_VAL(stacktrace));
    if (!propagateException(vm)) exit(70);
    else return exception;
}