#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "native.h"
#include "object.h"
#include "os.h"
#include "vm.h"
#include "file.h"

bool fileClose(VM* vm, ObjFile* file) {
    if (file->isOpen) {
        uv_fs_t fsClose;
        int closed = uv_fs_close(vm->eventLoop, &fsClose, (uv_file)file->fsOpen->result, NULL);
        uv_fs_req_cleanup(&fsClose);
        return (closed == 0);
    }
    return false;
}

ObjPromise* fileCloseAsync(VM* vm, ObjFile* file, uv_fs_cb callback) {
    if (file->isOpen && file->fsOpen != NULL) {
        uv_fs_t* fsClose = ALLOCATE_STRUCT(uv_fs_t);
        ObjPromise* promise = newPromise(vm, PROMISE_PENDING, NIL_VAL, NIL_VAL);
        if (fsClose == NULL) return NULL;
        else { 
            fsClose->data = fileData(vm, file, promise);
            uv_fs_close(vm->eventLoop, fsClose, (uv_file)file->fsOpen->result, callback);
            return promise;
        }
    }
    return newPromise(vm, PROMISE_FULFILLED, NIL_VAL, NIL_VAL);
}

bool fileExists(VM* vm, ObjFile* file) {
    uv_fs_t fsAccess;
    bool exists = (uv_fs_access(vm->eventLoop, &fsAccess, file->name->chars, F_OK, NULL) == 0);
    uv_fs_req_cleanup(&fsAccess);
    return exists;
}

bool fileFlush(VM* vm, ObjFile* file) {
    if (file->isOpen) {
        uv_fs_t fsSync;
        int flushed = uv_fs_fsync(vm->eventLoop, &fsSync, (uv_file)file->fsOpen->result, NULL);
        uv_fs_req_cleanup(&fsSync);
        return (flushed == 0);
    }
    return false;
}

bool fileOpen(VM* vm, ObjFile* file, const char* mode) {
    if (file->fsOpen == NULL) file->fsOpen = ALLOCATE_STRUCT(uv_fs_t);
    if (file->fsOpen != NULL) {
        int openMode = fileMode(mode);
        int openFlags = (strcmp(mode, "w") == 0 || strcmp(mode, "wb") == 0) ? S_IWRITE : 0;
        int descriptor = uv_fs_open(vm->eventLoop, file->fsOpen, file->name->chars, openMode, openFlags, NULL);
        if (descriptor < 0) return false;
        file->isOpen = true;
        file->mode = newString(vm, mode);
        return true;
    }
    return false;
}

ObjPromise* fileOpenAsync(VM* vm, ObjFile* file, const char* mode, uv_fs_cb callback) {
    if (file->fsOpen == NULL) file->fsOpen = ALLOCATE_STRUCT(uv_fs_t);
    if (file->fsOpen != NULL) {
        int openMode = fileMode(mode);
        int openFlags = (strcmp(mode, "w") == 0 || strcmp(mode, "wb") == 0) ? S_IWRITE : 0;
        ObjPromise* promise = newPromise(vm, PROMISE_PENDING, NIL_VAL, NIL_VAL);
        file->fsOpen->data = fileData(vm, file, promise);
        file->mode = newString(vm, mode);
        uv_fs_open(vm->eventLoop, file->fsOpen, file->name->chars, openMode, openFlags, callback);
        return promise;
    }
    return NULL;
}

ObjString* fileRead(VM* vm, ObjFile* file, bool isPeek) {
    char c = 0;
    uv_buf_t uvBuf = uv_buf_init(&c, 1);
    int numRead = uv_fs_read(vm->eventLoop, file->fsRead, (uv_file)file->fsOpen->result, &uvBuf, 1, file->offset, NULL);
    if (numRead == 0) return NULL;
    if (!isPeek) file->offset += 1;

    char ch[2] = { c, '\0' };
    return copyString(vm, ch, 2);
}

void fileWrite(VM* vm, ObjFile* file, char c) {
    uv_buf_t uvBuf = uv_buf_init(&c, 1);
    uv_fs_write(vm->eventLoop, file->fsWrite, (uv_file)file->fsOpen->result, &uvBuf, 1, file->offset, NULL);
    int numWrite = uv_fs_write(vm->eventLoop, file->fsWrite, (uv_file)file->fsOpen->result, &uvBuf, 1, file->offset, NULL);
    if (numWrite > 0) file->offset += 1;
}

ObjFile* getFileArgument(VM* vm, Value arg) {
    ObjFile* file = NULL;
    if (IS_STRING(arg)) file = newFile(vm, AS_STRING(arg));
    else if (IS_FILE(arg)) file = AS_FILE(arg);
    return file;
}

ObjFile* getFileProperty(VM* vm, ObjInstance* object, char* field) {
    return AS_FILE(getObjProperty(vm, object, field));
}

bool loadFileRead(VM* vm, ObjFile* file) {
    if (file->isOpen == false) return false;
    if (file->fsRead == NULL) file->fsRead = ALLOCATE_STRUCT(uv_fs_t);
    return true;
}

bool loadFileStat(VM* vm, ObjFile* file) {
    if (file->fsStat == NULL) file->fsStat = ALLOCATE_STRUCT(uv_fs_t);
    return (uv_fs_stat(vm->eventLoop, file->fsStat, file->name->chars, NULL) == 0);
}

bool loadFileWrite(VM* vm, ObjFile* file) {
    if (file->isOpen == false) return false;
    if (file->fsWrite == NULL) file->fsWrite = ALLOCATE_STRUCT(uv_fs_t);
    return true;
}

bool loadFileOperation(VM* vm, ObjFile* file, const char* streamClass) {
    if (strcmp(streamClass, "clox.std.io.BinaryReadStream") == 0 || strcmp(streamClass, "clox.std.io.FileReadStream") == 0) {
        return loadFileRead(vm, file);
    }
    else if (strcmp(streamClass, "clox.std.io.BinaryWriteStream") == 0 || strcmp(streamClass, "clox.std.io.FileWriteStream") == 0) {
        return loadFileWrite(vm, file);
    }
    else return false;
}

bool setFileProperty(VM* vm, ObjInstance* object, ObjFile* file, const char* mode) {
    if(!fileOpen(vm, file, mode)) return false;
    setObjProperty(vm, object, "file", OBJ_VAL(file));
    return true;
}