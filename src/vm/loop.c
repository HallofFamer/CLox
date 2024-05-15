#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "native.h"
#include "object.h"
#include "vm.h"

void initLoop(VM* vm) {
    vm->eventLoop = ALLOCATE_STRUCT(uv_loop_t);
    ABORT_IFNULL(vm->eventLoop, "Not enough memory to create event loop.");
    uv_loop_init(vm->eventLoop);
}

void freeLoop(VM* vm) {
    if (vm->eventLoop != NULL) {
        uv_loop_close(vm->eventLoop);
        free(vm->eventLoop);
    }
}

FileData* fileData(VM* vm, ObjFile* file, ObjPromise* promise) {
    FileData* data = ALLOCATE_STRUCT(FileData);
    if (data != NULL) {
        data->vm = vm;
        data->file = file;
        data->promise = promise;
    }
    return data;
}

int fileMode(const char* mode) {
    size_t length = strlen(mode);
    switch (length) {
        case 1:
            if (mode[0] == 'r') return O_RDONLY;
            else if (mode[0] == 'w') return O_WRONLY | O_CREAT | O_TRUNC;
            else if (mode[0] == 'a') return O_WRONLY | O_CREAT | O_APPEND;
            else return -1;
        case 2:
            if (mode[0] == 'r' && mode[1] == 'b') return O_RDONLY | O_BINARY;
            else if (mode[0] == 'w' && mode[1] == 'b') return O_WRONLY | O_TRUNC | O_CREAT | O_BINARY;
            else if (mode[0] == 'a' && mode[1] == 'b') return O_WRONLY | O_APPEND | O_CREAT | O_BINARY;
            else if (mode[0] == 'r' && mode[1] == '+') return O_RDWR;
            else if (mode[0] == 'w' && mode[1] == '+') return O_RDWR | O_CREAT | O_TRUNC;
            else if (mode[0] == 'w' && mode[1] == '+') return O_RDWR | O_CREAT | O_APPEND;
            else return -1;
        case 3:
            if (mode[0] == 'r' && mode[1] == 'b' && mode[2] == '+') return O_RDWR | O_BINARY;
            else if (mode[0] == 'w' && mode[1] == 'b' && mode[2] == '+') return O_RDWR | O_TRUNC | O_BINARY;
            else if (mode[0] == 'a' && mode[1] == 'b' && mode[2] == '+') return O_RDWR | O_APPEND | O_BINARY;
            else return -1;
        default: return -1;
    }
}

void fileOnClose(uv_fs_t* fsClose) {
    FileData* data = (FileData*)fsClose->data;
    push(data->vm, OBJ_VAL(data->vm->currentModule->closure));
    data->vm->frameCount++;
    data->file->isOpen = false;
    promiseFulfill(data->vm, data->promise, NIL_VAL);

    pop(data->vm);
    uv_fs_req_cleanup(fsClose);
    free(data);
    free(fsClose);
}

void fileOnOpen(uv_fs_t* fsOpen) {
    FileData* data = (FileData*)fsOpen->data;
    push(data->vm, OBJ_VAL(data->vm->currentModule->closure));
    data->vm->frameCount++;
    data->file->isOpen = true;

    ObjClass* streamClass = getNativeClass(data->vm, streamClassName(data->file->mode->chars));
    ObjInstance* stream = newInstance(data->vm, streamClass);
    setObjProperty(data->vm, stream, "file", OBJ_VAL(data->file));
    promiseFulfill(data->vm, data->promise, OBJ_VAL(stream));

    pop(data->vm);
    data->vm->frameCount--;
    free(data);
}

void fileOnRead(uv_fs_t* fsRead) {
    FileData* data = (FileData*)fsRead->data;
    push(data->vm, OBJ_VAL(data->vm->currentModule->closure));
    data->vm->frameCount++;

    int numRead = (int)fsRead->result;
    if (numRead > 0) data->file->offset++;
    char ch[2] = { data->buffer.base[0], '\0' };
    ObjString* character = copyString(data->vm, ch, 1);
    promiseFulfill(data->vm, data->promise, OBJ_VAL(character));

    pop(data->vm);
    data->vm->frameCount--;
    free(data);
}

void fileOnWrite(uv_fs_t* fsWrite) {
    FileData* data = (FileData*)fsWrite->data;
    push(data->vm, OBJ_VAL(data->vm->currentModule->closure));
    data->vm->frameCount++;

    int numWrite = (int)fsWrite->result;
    if (numWrite > 0) data->file->offset++;
    promiseFulfill(data->vm, data->promise, NIL_VAL);

    pop(data->vm);
    data->vm->frameCount--;
    free(data->buffer.base);
    free(data);
}

char* streamClassName(const char* mode) {
    size_t length = strlen(mode);
    switch (length) { 
        case 1:
            if (mode[0] == 'r') return "clox.std.io.FileReadStream";
            else if (mode[0] == 'w' || mode[0] == 'a') return "clox.std.io.FileWriteStream";
            else return NULL;
        case 2:
            if (mode[0] == 'r' && mode[1] == 'b') return "clox.std.io.BinaryReadStream";
            else if ((mode[0] == 'w' || mode[1] == 'a') && mode[1] == 'b') return "clox.std.io.BinaryWriteStream";
            else if (mode[0] == 'r' && mode[1] == '+') return "clox.std.io.FileReadStream";
            else if ((mode[0] == 'w' || mode[1] == 'a') && mode[1] == '+') return "clox.std.io.FileWriteStream";
            else return NULL;
        case 3:
            if (mode[0] == 'r' && mode[1] == 'b' && mode[2] == '+') return "clox.std.io.BinaryReadStream";
            else if ((mode[0] == 'w' || mode[0] == 'a') && mode[1] == 'b' && mode[2] == '+') return "clox.std.io.BinaryWriteStream";
            else return NULL;
        default: return NULL;
    }
}

void timerClose(uv_handle_t* handle) {
    free(handle->data);
    free(handle);
}

TimerData* timerData(VM* vm, ObjClosure* closure, int delay, int interval) {
    TimerData* data = ALLOCATE_STRUCT(TimerData);
    if (data != NULL) {
        data->receiver = NIL_VAL;
        data->vm = vm;
        data->closure = closure;
        data->delay = delay;
        data->interval = interval;
    }
    return data;
}

void timerRun(uv_timer_t* timer) {
    TimerData* data = (TimerData*)timer->data;
    if (data->interval == 0) uv_close((uv_handle_t*)timer, timerClose);
    push(data->vm, OBJ_VAL(data->vm->currentModule->closure));
    data->vm->frameCount++;

    switch (data->closure->function->arity) {
        case 0:
            callReentrantMethod(data->vm, data->receiver, OBJ_VAL(data->closure));
            break;
        case 1:
            callReentrantMethod(data->vm, data->receiver, OBJ_VAL(data->closure), data->receiver);
            break;
        default:
            throwNativeException(data->vm, "clox.std.lang.IllegalArgumentException", "timer callback closure may accept only 0 or 1 argument");
    }
    pop(data->vm);
    data->vm->frameCount--;
}