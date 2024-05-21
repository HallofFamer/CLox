#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file.h"
#include "memory.h"
#include "native.h"
#include "object.h"
#include "os.h"
#include "vm.h"

static FileData* fileLoadData(VM* vm, ObjFile* file, ObjPromise* promise) {
    FileData* data = ALLOCATE_STRUCT(FileData);
    if (data != NULL) {
        data->vm = vm;
        data->file = file;
        data->promise = promise;
    }
    return data;
}

static void filePopData(FileData* data) {
    pop(data->vm);
    data->vm->frameCount--;
    free(data);
}

static FileData* filePushData(uv_fs_t* fsRequest) {
    FileData* data = (FileData*)fsRequest->data;
    push(data->vm, OBJ_VAL(data->vm->currentModule->closure));
    data->vm->frameCount++;
    return data;
}

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
            fsClose->data = fileLoadData(vm, file, promise);
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

ObjPromise* fileFlushAsync(VM* vm, ObjFile* file, uv_fs_cb callback) {
    if (file->isOpen && file->fsOpen != NULL && file->fsWrite == NULL) {
        uv_fs_t* fsSync = ALLOCATE_STRUCT(uv_fs_t);
        ObjPromise* promise = newPromise(vm, PROMISE_PENDING, NIL_VAL, NIL_VAL);
        if (fsSync == NULL) return NULL;
        else {
            fsSync->data = fileLoadData(vm, file, promise);
            uv_fs_fsync(vm->eventLoop, fsSync, (uv_file)file->fsOpen->result, callback);
            uv_fs_req_cleanup(fsSync);
            return promise;
        }
    }
    return newPromise(vm, PROMISE_FULFILLED, NIL_VAL, NIL_VAL);
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
    FileData* data = filePushData(fsClose);
    data->file->isOpen = false;
    promiseFulfill(data->vm, data->promise, NIL_VAL);
    uv_fs_req_cleanup(fsClose);
    free(fsClose);
    filePopData(data);
}

void fileOnFlush(uv_fs_t* fsSync) {
    FileData* data = filePushData(fsSync);
    data->file->isOpen = false;
    promiseFulfill(data->vm, data->promise, BOOL_VAL(fsSync->result == 0));
    uv_fs_req_cleanup(fsSync);
    free(fsSync);
    filePopData(data);
}

void fileOnOpen(uv_fs_t* fsOpen) {
    FileData* data = filePushData(fsOpen);
    data->file->isOpen = true;
    ObjClass* streamClass = getNativeClass(data->vm, streamClassName(data->file->mode->chars));
    ObjInstance* stream = newInstance(data->vm, streamClass);
    setObjProperty(data->vm, stream, "file", OBJ_VAL(data->file));
    promiseFulfill(data->vm, data->promise, OBJ_VAL(stream));
    filePopData(data);
}

void fileOnRead(uv_fs_t* fsRead) {
    FileData* data = filePushData(fsRead);
    int numRead = (int)fsRead->result;
    if (numRead > 0) data->file->offset++;
    char ch[2] = { data->buffer.base[0], '\0' };
    ObjString* character = copyString(data->vm, ch, 1);
    promiseFulfill(data->vm, data->promise, OBJ_VAL(character));
    filePopData(data);
}

void fileOnReadLine(uv_fs_t* fsRead) {
    FileData* data = filePushData(fsRead);
    int numRead = (int)fsRead->result;
    int numReadLine = 0;

    if (numRead > 0) { 
        while (numReadLine < numRead) {
            if (data->buffer.base[numReadLine++] == '\n') break;
        }
        data->file->offset += numReadLine;
    }

    ObjString* string = takeString(data->vm, data->buffer.base, numReadLine);
    promiseFulfill(data->vm, data->promise, OBJ_VAL(string));
    filePopData(data);
}

void fileOnReadString(uv_fs_t* fsRead) {
    FileData* data = filePushData(fsRead);
    int numRead = (int)fsRead->result;
    if (numRead > 0) data->file->offset += numRead;
    
    ObjString* string = takeString(data->vm, data->buffer.base, numRead);
    promiseFulfill(data->vm, data->promise, OBJ_VAL(string));
    filePopData(data);
}

void fileOnWrite(uv_fs_t* fsWrite) {
    FileData* data = filePushData(fsWrite);
    int numWrite = (int)fsWrite->result;
    if (numWrite > 0) data->file->offset += numWrite;
    promiseFulfill(data->vm, data->promise, NIL_VAL);
    filePopData(data);
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
        file->fsOpen->data = fileLoadData(vm, file, promise);
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
    return copyString(vm, ch, 1);
}

ObjPromise* fileReadAsync(VM* vm, ObjFile* file, uv_fs_cb callback) {
    if (file->isOpen && file->fsOpen != NULL && file->fsRead != NULL) {
        ObjPromise* promise = newPromise(vm, PROMISE_PENDING, NIL_VAL, NIL_VAL);
        char c = 0;
        FileData* data = fileLoadData(vm, file, promise);
        data->buffer = uv_buf_init(&c, 1);
        file->fsRead->data = data;
        uv_fs_read(vm->eventLoop, file->fsRead, (uv_file)file->fsOpen->result, &data->buffer, 1, file->offset, callback);
        return promise;
    }
    return NULL;
}

ObjString* fileReadLine(VM* vm, ObjFile* file) {
    char chars[UINT8_MAX];
    uv_buf_t uvBuf = uv_buf_init(chars, UINT8_MAX);
    int numRead = uv_fs_read(vm->eventLoop, file->fsRead, (uv_file)file->fsOpen->result, &uvBuf, 1, file->offset, NULL);
    if (numRead == 0) return NULL;

    size_t lineOffset = 0;
    while (lineOffset < numRead) {
        if (uvBuf.base[lineOffset++] == '\n') break;
    }

    char* line = (char*)malloc(lineOffset + 1);
    if (line != NULL) {
        memcpy(line, uvBuf.base, lineOffset);
        line[lineOffset] = '\0';
        file->offset += lineOffset;
        return takeString(vm, line, (int)lineOffset);
    }
    else return NULL;
}

ObjString* fileReadString(VM* vm, ObjFile* file, size_t length) {
    char* chars = (char*)malloc(length * sizeof(char));
    uv_buf_t uvBuf = uv_buf_init(chars, UINT8_MAX);
    int numRead = uv_fs_read(vm->eventLoop, file->fsRead, (uv_file)file->fsOpen->result, &uvBuf, 1, file->offset, NULL);
    if (numRead == 0) return NULL;
    return takeString(vm, chars, (int)numRead);
}

ObjPromise* fileReadStringAsync(VM* vm, ObjFile* file, size_t length, uv_fs_cb callback) {
    if (file->isOpen && file->fsOpen != NULL && file->fsRead != NULL) {
        ObjPromise* promise = newPromise(vm, PROMISE_PENDING, NIL_VAL, NIL_VAL);
        FileData* data = fileLoadData(vm, file, promise);
        char* string = (char*)malloc(length * sizeof(char));
        if (string != NULL) {
            data->buffer = uv_buf_init(string, (int)length);
            file->fsRead->data = data;
            uv_fs_read(vm->eventLoop, file->fsRead, (uv_file)file->fsOpen->result, &data->buffer, 1, file->offset, callback);
            return promise;
        }
    }
    return NULL;
}

void fileWrite(VM* vm, ObjFile* file, char c) {
    uv_buf_t uvBuf = uv_buf_init(&c, 1);
    uv_fs_write(vm->eventLoop, file->fsWrite, (uv_file)file->fsOpen->result, &uvBuf, 1, file->offset, NULL);
    int numWrite = uv_fs_write(vm->eventLoop, file->fsWrite, (uv_file)file->fsOpen->result, &uvBuf, 1, file->offset, NULL);
    if (numWrite > 0) file->offset += 1;
}

ObjPromise* fileWriteAsync(VM* vm, ObjFile* file, ObjString* string, uv_fs_cb callback) {
    if (file->isOpen && file->fsOpen != NULL && file->fsWrite != NULL) {
        ObjPromise* promise = newPromise(vm, PROMISE_PENDING, NIL_VAL, NIL_VAL);
        FileData* data = fileLoadData(vm, file, promise);
        data->buffer = uv_buf_init(string->chars, string->length);
        file->fsWrite->data = data;
        uv_fs_write(vm->eventLoop, file->fsWrite, (uv_file)file->fsOpen->result, &data->buffer, 1, file->offset, callback);
        return promise;
    }
    return newPromise(vm, PROMISE_FULFILLED, NIL_VAL, NIL_VAL);
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