#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "io.h"
#include "../vm/assert.h"
#include "../vm/date.h"
#include "../vm/file.h"
#include "../vm/memory.h"
#include "../vm/native.h"
#include "../vm/object.h"
#include "../vm/os.h"
#include "../vm/string.h"
#include "../vm/vm.h"

LOX_METHOD(BinaryReadStream, __init__) {
    ASSERT_ARG_COUNT("BinaryReadStream::__init__(file)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjFile* file = getFileArgument(vm, args[0]);
    if (file == NULL) raiseError(vm, "Method BinaryReadStream::__init__(file) expects argument 1 to be a string or file.");
    if (!setFileProperty(vm, AS_INSTANCE(receiver), file, "rb")) {
        THROW_EXCEPTION(clox.std.io.IOException, "Cannot create BinaryReadStream, file either does not exist or require additional permission to access.");
    }
    if(!loadFileRead(vm, file)) THROW_EXCEPTION(clox.std.io.IOException, "Unable to read from binary stream.");
    RETURN_OBJ(self);
}

LOX_METHOD(BinaryReadStream, next) {
    ASSERT_ARG_COUNT("BinaryReadStream::next()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot read the next byte because file is already closed.");
    if (file->fsOpen == NULL || file->fsRead == NULL) RETURN_NIL;
    else {
        unsigned char ub;
        uv_buf_t uvBuf = uv_buf_init(&ub, 1);
        int numRead = uv_fs_read(vm->eventLoop, file->fsRead, (uv_file)file->fsOpen->result, &uvBuf, 1, file->offset, NULL);
        if (numRead == 0) RETURN_NIL;
        file->offset += 1;
        RETURN_INT((int)ub);
    }
    RETURN_NIL;
}

LOX_METHOD(BinaryReadStream, nextBytes) {
    ASSERT_ARG_COUNT("BinaryReadStream::nextBytes(length)", 1);
    ASSERT_ARG_TYPE("BinaryReadStream::nextBytes(length)", 0, Int);
    int length = AS_INT(args[0]);
    if (length < 0) THROW_EXCEPTION_FMT(clox.std.lang.IllegalArgumentException, "method BinaryReadStream::nextBytes(length) expects argument 1 to be a positive integer but got %g.", length);

    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot read the next byte because file is already closed.");
    if (file->fsOpen == NULL || file->fsRead == NULL) RETURN_NIL;
    else {
        ObjArray* bytes = newArray(vm);
        push(vm, OBJ_VAL(bytes));
        unsigned char* buffer = (unsigned char*)malloc(length);
        uv_buf_t uvBuf = uv_buf_init(buffer, length);
        int numRead = uv_fs_read(vm->eventLoop, file->fsRead, (uv_file)file->fsOpen->result, &uvBuf, 1, file->offset, NULL);

        for (int i = 0; i < numRead; i++, file->offset++) {
            unsigned char byte = (unsigned char)uvBuf.base[i];
            valueArrayWrite(vm, &bytes->elements, INT_VAL((int)byte));
        }

        free(buffer);
        pop(vm);
        RETURN_OBJ(bytes);
    }
}

LOX_METHOD(BinaryWriteStream, __init__) {
    ASSERT_ARG_COUNT("BinaryWriteStream::__init__(file)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjFile* file = getFileArgument(vm, args[0]);
    if (file == NULL) raiseError(vm, "Method BinaryWriteStream::__init__(file) expects argument 1 to be a string or file.");
    if (!setFileProperty(vm, AS_INSTANCE(receiver), file, "wb")) {
        THROW_EXCEPTION(clox.std.io.IOException, "Cannot create BinaryWriteStream, file either does not exist or require additional permission to access.");
    }
    if (!loadFileWrite(vm, file)) THROW_EXCEPTION(clox.std.io.IOException, "Unable to write to binary stream.");
    RETURN_OBJ(self);
}

LOX_METHOD(BinaryWriteStream, put) {
    ASSERT_ARG_COUNT("BinaryWriteStream::put(byte)", 1);
    ASSERT_ARG_TYPE("BinaryWriteStream::put(byte)", 0, Int);
    int arg = AS_INT(args[0]);
    ASSERT_INDEX_WITHIN_BOUNDS("BinaryWriteStream::put(byte)", arg, 0, 255, 0);

    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot write byte to stream because file is already closed.");
    if (file->fsOpen != NULL && file->fsWrite != NULL) {
        unsigned char byte = (unsigned char)arg;
        uv_buf_t uvBuf = uv_buf_init(&byte, 1);
        uv_fs_write(vm->eventLoop, file->fsWrite, (uv_file)file->fsOpen->result, &uvBuf, 1, file->offset, NULL);
        int numWrite = uv_fs_write(vm->eventLoop, file->fsWrite, (uv_file)file->fsOpen->result, &uvBuf, 1, file->offset, NULL);
        if (numWrite > 0) file->offset += 1;
    }
    RETURN_NIL;
}

LOX_METHOD(BinaryWriteStream, putBytes) {
    ASSERT_ARG_COUNT("BinaryWriteStream::put(bytes)", 1);
    ASSERT_ARG_TYPE("BinaryWriteStream::put(bytes)", 0, Array);
    ObjArray* bytes = AS_ARRAY(args[0]);
    if (bytes->elements.count == 0) THROW_EXCEPTION(clox.std.io.IOException, "Cannot write empty byte array to stream.");

    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot write bytes to stream because file is already closed.");
    if (file->fsOpen != NULL && file->fsWrite != NULL) {
        unsigned char* byteArray = (unsigned char*)malloc(bytes->elements.count);
        if (byteArray != NULL) {
            for (int i = 0; i < bytes->elements.count; i++) {
                if (!IS_INT(bytes->elements.values[i])) THROW_EXCEPTION(clox.std.io.IOException, "Cannot write bytes to stream because data is corrupted.");
                int byte = AS_INT(bytes->elements.values[i]);
                byteArray[i] = (unsigned char)byte;
            }

            uv_buf_t uvBuf = uv_buf_init(byteArray, bytes->elements.count);
            int numWrite = uv_fs_write(vm->eventLoop, file->fsWrite, (uv_file)file->fsOpen->result, &uvBuf, 1, file->offset, NULL);
            if (numWrite > 0) file->offset += numWrite;
            free(byteArray);
        }
    }
    RETURN_NIL;
}

LOX_METHOD(File, __init__) {
    ASSERT_ARG_COUNT("File::__init__(pathname)", 1);
    ASSERT_ARG_TYPE("File::__init__(pathname)", 0, String);
    ObjFile* self = AS_FILE(receiver);
    self->name = AS_STRING(args[0]);
    self->mode = emptyString(vm);
    self->isOpen = false;
    RETURN_OBJ(self);
}

LOX_METHOD(File, create) {
    ASSERT_ARG_COUNT("File::create()", 0);
    ObjFile* self = AS_FILE(receiver);
    if (fileExists(vm, self)) THROW_EXCEPTION(clox.std.io.IOException, "Cannot create new file because it already exists");

    uv_fs_t fsOpen;
    int created = uv_fs_open(vm->eventLoop, &fsOpen, self->name->chars, O_CREAT, 0, NULL);
    if (created == 0) {
        uv_fs_t fsClose;
        uv_fs_close(vm->eventLoop, &fsClose, (uv_file)fsOpen.result, NULL);
        uv_fs_req_cleanup(&fsClose);
    }

    uv_fs_req_cleanup(&fsOpen);
    RETURN_BOOL(created == 0);
}

LOX_METHOD(File, delete) {
    ASSERT_ARG_COUNT("File::delete()", 0);
    ObjFile* self = AS_FILE(receiver);
    if (!fileExists(vm, self)) THROW_EXCEPTION(clox.std.io.IOException, "Cannot delete file because it does not exist.");
    
    uv_fs_t fsUnlink;
    int unlinked = uv_fs_unlink(vm->eventLoop, &fsUnlink, self->name->chars, NULL);
    uv_fs_req_cleanup(&fsUnlink);
    RETURN_BOOL(unlinked == 0);
}

LOX_METHOD(File, exists) {
    ASSERT_ARG_COUNT("File::exists()", 0);
    RETURN_BOOL(fileExists(vm, AS_FILE(receiver)));
}

LOX_METHOD(File, getAbsolutePath) {
    ASSERT_ARG_COUNT("File::getAbsolutePath()", 0);
    ObjFile* self = AS_FILE(receiver);
    uv_fs_t fRealPath;
    if (uv_fs_realpath(vm->eventLoop, &fRealPath, self->name->chars, NULL) != 0) {
        THROW_EXCEPTION(clox.std.io.FileNotFoundException, "Cannot get file absolute path because it does not exist.");
    }

    ObjString* realPath = newString(vm, (const char*)fRealPath.ptr);
    uv_fs_req_cleanup(&fRealPath);
    RETURN_OBJ(realPath);
}

LOX_METHOD(File, isDirectory) {
    ASSERT_ARG_COUNT("File::isDirectory()", 0);
    ObjFile* self = AS_FILE(receiver);
    if (!loadFileStat(vm, self)) RETURN_FALSE;
    RETURN_BOOL(self->fsStat->statbuf.st_mode & S_IFDIR);
}

LOX_METHOD(File, isExecutable) {
    ASSERT_ARG_COUNT("File::isExecutable()", 0);
    ObjFile* self = AS_FILE(receiver);
    if (!loadFileStat(vm, self)) RETURN_FALSE;
    RETURN_BOOL(self->fsStat->statbuf.st_mode & S_IEXEC);
}

LOX_METHOD(File, isFile) {
    ASSERT_ARG_COUNT("File::isFile()", 0);
    ObjFile* self = AS_FILE(receiver);
    if (!loadFileStat(vm, self)) RETURN_FALSE;
    RETURN_BOOL(self->fsStat->statbuf.st_mode & S_IFREG);
}

LOX_METHOD(File, isReadable) {
    ASSERT_ARG_COUNT("File::isReadable()", 0);
    ObjFile* self = AS_FILE(receiver);
    if (!loadFileStat(vm, self)) RETURN_FALSE;
    RETURN_BOOL(self->fsStat->statbuf.st_mode & S_IREAD);
}

LOX_METHOD(File, isWritable) {
    ASSERT_ARG_COUNT("File::isWritable()", 0);
    ObjFile* self = AS_FILE(receiver);
    if (!loadFileStat(vm, self)) RETURN_FALSE;
    RETURN_BOOL(self->fsStat->statbuf.st_mode & S_IWRITE);
}

LOX_METHOD(File, lastAccessed) {
    ASSERT_ARG_COUNT("File::lastAccessed()", 0);
    ObjFile* self = AS_FILE(receiver);
    if (!loadFileStat(vm, self)) THROW_EXCEPTION(clox.std.io.FileNotFoundException, "Cannot get file last accessed date because it does not exist.");
    ObjInstance* lastAccessed = dateTimeObjFromTimestamp(vm, getNativeClass(vm, "clox.std.util.DateTime"), (double)self->fsStat->statbuf.st_atim.tv_sec);
    RETURN_OBJ(lastAccessed);
}

LOX_METHOD(File, lastModified) {
    ASSERT_ARG_COUNT("File::lastModified()", 0);
    ObjFile* self = AS_FILE(receiver);
    if (!loadFileStat(vm, self)) THROW_EXCEPTION(clox.std.io.FileNotFoundException, "Cannot get file last modified date because it does not exist.");
    ObjInstance* lastModified = dateTimeObjFromTimestamp(vm, getNativeClass(vm, "clox.std.util.DateTime"), (double)self->fsStat->statbuf.st_mtim.tv_sec);
    RETURN_OBJ(lastModified);
}

LOX_METHOD(File, mkdir) {
    ASSERT_ARG_COUNT("File::mkdir()", 0);
    ObjFile* self = AS_FILE(receiver);
    if (fileExists(vm, self)) THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Cannot create directory as it already exists in the file system.");
    
    uv_fs_t fsMkdir;
    int created = uv_fs_mkdir(vm->eventLoop, &fsMkdir, self->name->chars, S_IREAD | S_IWRITE | S_IEXEC, NULL);
    uv_fs_req_cleanup(&fsMkdir);
    RETURN_BOOL(created == 0);
}

LOX_METHOD(File, name) {
    ASSERT_ARG_COUNT("File::name()", 0);
    RETURN_OBJ(AS_FILE(receiver)->name);
}

LOX_METHOD(File, rename) {
    ASSERT_ARG_COUNT("File::rename(name)", 1);
    ASSERT_ARG_TYPE("File::rename(name)", 0, String);
    ObjFile* self = AS_FILE(receiver);
    if (!fileExists(vm, self)) THROW_EXCEPTION(clox.std.io.FileNotFoundException, "Cannot rename file as it does not exist.");
    
    uv_fs_t fsRename;
    int renamed = uv_fs_rename(vm->eventLoop, &fsRename, self->name->chars, AS_CSTRING(args[0]), NULL);
    uv_fs_req_cleanup(&fsRename);
    RETURN_BOOL(renamed == 0);
}

LOX_METHOD(File, rmdir) {
    ASSERT_ARG_COUNT("File::rmdir()", 0);
    ObjFile* self = AS_FILE(receiver);
    if (!loadFileStat(vm, self)) THROW_EXCEPTION(clox.std.io.FileNotFoundException, "Cannot remove directory as it does not exist.");
    
    uv_fs_t fsRmdir;
    bool removed = uv_fs_rmdir(vm->eventLoop, &fsRmdir, self->name->chars, NULL);
    uv_fs_req_cleanup(&fsRmdir);
    RETURN_BOOL(removed == 0);
}

LOX_METHOD(File, setExecutable) {
    ASSERT_ARG_COUNT("File::setExecutable(canExecute)", 1);
    ASSERT_ARG_TYPE("File::setExecutable(canExecute)", 0, Bool);
    ObjFile* self = AS_FILE(receiver);
    if (!fileExists(vm, self)) THROW_EXCEPTION(clox.std.io.FileNotFoundException, "Cannot change file permission as it does not exist.");

    uv_fs_t fsChmod;
    int changed = uv_fs_chmod(vm->eventLoop, &fsChmod, self->name->chars, AS_BOOL(args[0]) ? S_IEXEC : ~S_IEXEC, NULL);
    uv_fs_req_cleanup(&fsChmod);
    RETURN_BOOL(changed == 0);
}

LOX_METHOD(File, setReadable) {
    ASSERT_ARG_COUNT("File::setReadable(canRead)", 1);
    ASSERT_ARG_TYPE("File::setReadable(canRead)", 0, Bool);
    ObjFile* self = AS_FILE(receiver);
    if (!fileExists(vm, self)) THROW_EXCEPTION(clox.std.io.FileNotFoundException, "Cannot change file permission as it does not exist.");

    uv_fs_t fsChmod;
    int changed = uv_fs_chmod(vm->eventLoop, &fsChmod, self->name->chars, AS_BOOL(args[0]) ? S_IREAD : ~S_IREAD, NULL);
    uv_fs_req_cleanup(&fsChmod);
    RETURN_BOOL(changed == 0);
}

LOX_METHOD(File, setWritable) {
    ASSERT_ARG_COUNT("File::setWritable(canWrite)", 1);
    ASSERT_ARG_TYPE("File::setWritable(canWrite)", 0, Bool);
    ObjFile* self = AS_FILE(receiver);
    if (!fileExists(vm, self)) THROW_EXCEPTION(clox.std.io.FileNotFoundException, "Cannot change file permission as it does not exist.");

    uv_fs_t fsChmod;
    int changed = uv_fs_chmod(vm->eventLoop, &fsChmod, self->name->chars, AS_BOOL(args[0]) ? S_IWRITE : ~S_IWRITE, NULL);
    uv_fs_req_cleanup(&fsChmod);
    RETURN_BOOL(changed == 0);
}

LOX_METHOD(File, size) {
    ASSERT_ARG_COUNT("File::size()", 0);
    ObjFile* self = AS_FILE(receiver);
    if (!loadFileStat(vm, self)) THROW_EXCEPTION(clox.std.io.FileNotFoundException, "Cannot get file size because it does not exist.");
    RETURN_NUMBER((double)self->fsStat->statbuf.st_size);
}

LOX_METHOD(File, toString) {
    ASSERT_ARG_COUNT("File::toString()", 0);
    RETURN_OBJ(AS_FILE(receiver)->name);
}

LOX_METHOD(FileClass, open) {
    ASSERT_ARG_COUNT("File class::open(pathname, mode)", 2);
    ASSERT_ARG_TYPE("File class::open(pathname, mode)", 0, String);
    ASSERT_ARG_TYPE("File class::open(pathname, mode)", 1, String);
    char* mode = AS_CSTRING(args[1]);
    ObjFile* file = newFile(vm, AS_STRING(args[0]));

    push(vm, OBJ_VAL(file));
    char* streamClass = streamClassName(mode);
    if (streamClass == NULL) THROW_EXCEPTION(clox.std.io.IOException, "Invalid file open mode specified.");
    ObjInstance* stream = newInstance(vm, getNativeClass(vm, streamClass));
    
    if (!setFileProperty(vm, stream, file, mode)) THROW_EXCEPTION(clox.std.io.IOException, "Cannot open IO stream, file either does not exist or require additional permission to access."); 
    loadFileOperation(vm, file, streamClass);
    pop(vm);
    RETURN_OBJ(stream);
}

LOX_METHOD(FileClass, openAsync) {
    ASSERT_ARG_COUNT("File class::openAsync(pathname, mode)", 2);
    ASSERT_ARG_TYPE("File class::openAsync(pathname, mode)", 0, String);
    ASSERT_ARG_TYPE("File class::openAsync(pathname, mode)", 1, String);
    char* mode = AS_CSTRING(args[1]);
    ObjFile* file = newFile(vm, AS_STRING(args[0]));
    ObjPromise* promise = fileOpenAsync(vm, NULL, file, mode, fileOnOpen);
    RETURN_OBJ(promise);
}

LOX_METHOD(FileReadStream, __init__) {
    ASSERT_ARG_COUNT("FileReadStream::__init__(file)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjFile* file = getFileArgument(vm, args[0]);
    if (file == NULL) raiseError(vm, "Method FileReadStream::__init__(file) expects argument 1 to be a string or file.");
    if (!setFileProperty(vm, AS_INSTANCE(receiver), file, "r")) {
        THROW_EXCEPTION(clox.std.io.IOException, "Cannot create FileReadStream, file either does not exist or require additional permission to access.");
    }
    if (!loadFileRead(vm, file)) THROW_EXCEPTION(clox.std.io.IOException, "Unable to read from file stream.");
    RETURN_OBJ(self);
}

LOX_METHOD(FileReadStream, next) {
    ASSERT_ARG_COUNT("FileReadStream::next()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot read the next char because file is already closed.");
    if (file->fsOpen == NULL) RETURN_NIL;
    else {
        ObjString* ch = fileRead(vm, file, false);
        if (ch == NULL) RETURN_NIL;
        RETURN_OBJ(ch);
    }
}

LOX_METHOD(FileReadStream, nextLine) {
    ASSERT_ARG_COUNT("FileReadStream::nextLine()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot read the next line because file is already closed.");
    if (file->fsOpen == NULL) RETURN_NIL;
    else {
        char chars[UINT8_MAX];
        uv_buf_t uvBuf = uv_buf_init(chars, UINT8_MAX);
        int numRead = uv_fs_read(vm->eventLoop, file->fsRead, (uv_file)file->fsOpen->result, &uvBuf, 1, file->offset, NULL);
        if (numRead == 0) RETURN_NIL;

        size_t lineOffset = 0;
        while (lineOffset < numRead) {
            if (uvBuf.base[lineOffset++] == '\n') break;
        }

        char* line = (char*)malloc(lineOffset + 1);
        if (line != NULL) {
            memcpy(line, uvBuf.base, lineOffset);
            line[lineOffset] = '\0';
            file->offset += lineOffset;
            RETURN_STRING(line, (int)lineOffset);
        }
        else THROW_EXCEPTION(clox.std.lang.OutOfMemoryException, "Not enough memory to allocate for next line read.");
    }
}

LOX_METHOD(FileReadStream, peek) {
    ASSERT_ARG_COUNT("FileReadStream::peek()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot peek the next char because file is already closed.");
    if (file->fsOpen == NULL) RETURN_NIL;
    else {
        ObjString* ch = fileRead(vm, file, true);
        if (ch == NULL) RETURN_NIL;
        RETURN_OBJ(ch);
    }
}

LOX_METHOD(FileWriteStream, __init__) {
    ASSERT_ARG_COUNT("FileWriteStream::__init__(file)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjFile* file = getFileArgument(vm, args[0]);
    if (file == NULL) raiseError(vm, "Method FileWriteStream::__init__(file) expects argument 1 to be a string or file.");
    if (!setFileProperty(vm, AS_INSTANCE(receiver), file, "w")) {
        THROW_EXCEPTION(clox.std.io.IOException, "Cannot create FileWriteStream, file either does not exist or require additional permission to access.");
    }
    if (!loadFileWrite(vm, file)) THROW_EXCEPTION(clox.std.io.IOException, "Unable to write to file stream.");
    RETURN_OBJ(self);
}

LOX_METHOD(FileWriteStream, put) {
    ASSERT_ARG_COUNT("FileWriteStream::put(char)", 1);
    ASSERT_ARG_TYPE("FileWriteStream::put(char)", 0, String);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot write character to stream because file is already closed.");
    if (file->fsOpen != NULL && file->fsWrite != NULL) {
        ObjString* character = AS_STRING(args[0]);
        if (character->length != 1) THROW_EXCEPTION(clox.std.lang.IllegalArgumentException, "Method FileWriteStream::put(char) expects argument 1 to be a character(string of length 1)");
        fileWrite(vm, file, character->chars[0]);
    }
    RETURN_NIL;
}

LOX_METHOD(FileWriteStream, putLine) {
    ASSERT_ARG_COUNT("FileWriteStream::putLine()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot write new line to stream because file is already closed.");
    if (file->fsOpen != NULL && file->fsWrite != NULL) fileWrite(vm, file, '\n');
    RETURN_NIL;
}

LOX_METHOD(FileWriteStream, putSpace) {
    ASSERT_ARG_COUNT("FileWriteStream::putSpace()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot write empty space to stream because file is already closed.");
    if (file->fsOpen != NULL && file->fsWrite != NULL) fileWrite(vm, file, ' ');
    RETURN_NIL;
}

LOX_METHOD(FileWriteStream, putString) {
    ASSERT_ARG_COUNT("FileWriteStream::putString(string)", 1);
    ASSERT_ARG_TYPE("FileWriteStream::putString(string)", 0, String);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot write string to stream because file is already closed.");
    if (file->fsOpen != NULL && file->fsWrite != NULL) {
        ObjString* string = AS_STRING(args[0]);
        uv_buf_t uvBuf = uv_buf_init(string->chars, string->length);
        uv_fs_write(vm->eventLoop, file->fsWrite, (uv_file)file->fsOpen->result, &uvBuf, 1, file->offset, NULL);
        int numWrite = uv_fs_write(vm->eventLoop, file->fsWrite, (uv_file)file->fsOpen->result, &uvBuf, 1, file->offset, NULL);
        if (numWrite > 0) file->offset += string->length;
    }
    RETURN_NIL;
}

LOX_METHOD(IOStream, __init__) {
    raiseError(vm, "Cannot instantiate from class IOStream.");
    RETURN_NIL;
}

LOX_METHOD(IOStream, close) {
    ASSERT_ARG_COUNT("IOStream::close()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    file->isOpen = false;
    RETURN_BOOL(fileClose(vm, file));
}

LOX_METHOD(IOStream, file) {
    ASSERT_ARG_COUNT("IOStream::file()", 0);
    RETURN_OBJ(getFileProperty(vm, AS_INSTANCE(receiver), "file"));
}

LOX_METHOD(IOStream, getPosition) {
    ASSERT_ARG_COUNT("IOStream::getPosition()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot get stream position because file is already closed.");
    if (file->fsOpen == NULL) RETURN_INT(0);
    RETURN_INT(file->offset);
}

LOX_METHOD(IOStream, reset) {
    ASSERT_ARG_COUNT("IOStream::reset()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot reset stream because file is already closed.");
    file->offset = 0;
    RETURN_NIL;
}

LOX_METHOD(ReadStream, __init__) {
    raiseError(vm, "Cannot instantiate from class ReadStream.");
    RETURN_NIL;
}

LOX_METHOD(ReadStream, isAtEnd) {
    ASSERT_ARG_COUNT("ReadStream::next()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) RETURN_FALSE;
    else {
        unsigned char c;
        uv_buf_t uvBuf = uv_buf_init(&c, 1);
        int numRead = uv_fs_read(vm->eventLoop, file->fsRead, (uv_file)file->fsOpen->result, &uvBuf, 1, file->offset, NULL);       
        RETURN_BOOL(numRead == 0);
    }
}

LOX_METHOD(ReadStream, next) {
    THROW_EXCEPTION(clox.std.lang.NotImplementedException, "Not implemented, subclass responsibility.");
}

LOX_METHOD(ReadStream, skip) {
    ASSERT_ARG_COUNT("ReadStream::skip(offset)", 1);
    ASSERT_ARG_TYPE("ReadStream::skip(offset)", 0, Int);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot skip stream by offset because file is already closed.");
    if (file->fsOpen == NULL) RETURN_FALSE;
    file->offset += AS_INT(args[0]);
    RETURN_TRUE;
}

LOX_METHOD(TClosable, close) {
    ASSERT_ARG_COUNT("IOStream::close()", 0);
    THROW_EXCEPTION(clox.std.lang.NotImplementedException, "Not implemented, subclass responsibility.");
}

LOX_METHOD(WriteStream, __init__) {
    raiseError(vm, "Cannot instantiate from class WriteStream.");
    RETURN_NIL;
}

LOX_METHOD(WriteStream, flush) {
    ASSERT_ARG_COUNT("WriteStream::flush()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot flush stream because file is already closed.");
    if (file->fsOpen == NULL) RETURN_FALSE;
    RETURN_BOOL(fileFlush(vm, file));
}

LOX_METHOD(WriteStream, put) {
    THROW_EXCEPTION(clox.std.lang.NotImplementedException, "Not implemented, subclass responsibility.");
}

void registerIOPackage(VM* vm) {
    ObjNamespace* ioNamespace = defineNativeNamespace(vm, "io", vm->stdNamespace);
    vm->currentNamespace = ioNamespace;

    vm->fileClass = defineNativeClass(vm, "File");
    bindSuperclass(vm, vm->fileClass, vm->objectClass);
    vm->fileClass->classType = OBJ_FILE;
    DEF_INTERCEPTOR(vm->fileClass, File, INTERCEPTOR_INIT, __init__, 1);
    DEF_METHOD(vm->fileClass, File, create, 0);
    DEF_METHOD(vm->fileClass, File, delete, 0);
    DEF_METHOD(vm->fileClass, File, exists, 0);
    DEF_METHOD(vm->fileClass, File, getAbsolutePath, 0);
    DEF_METHOD(vm->fileClass, File, isDirectory, 0);
    DEF_METHOD(vm->fileClass, File, isExecutable, 0);
    DEF_METHOD(vm->fileClass, File, isFile, 0);
    DEF_METHOD(vm->fileClass, File, isReadable, 0);
    DEF_METHOD(vm->fileClass, File, isWritable, 0);
    DEF_METHOD(vm->fileClass, File, lastAccessed, 0);
    DEF_METHOD(vm->fileClass, File, lastModified, 0);
    DEF_METHOD(vm->fileClass, File, mkdir, 0);
    DEF_METHOD(vm->fileClass, File, name, 0);
    DEF_METHOD(vm->fileClass, File, rename, 1);
    DEF_METHOD(vm->fileClass, File, rmdir, 0);
    DEF_METHOD(vm->fileClass, File, setExecutable, 1);
    DEF_METHOD(vm->fileClass, File, setReadable, 1);
    DEF_METHOD(vm->fileClass, File, setWritable, 1);
    DEF_METHOD(vm->fileClass, File, size, 0);
    DEF_METHOD(vm->fileClass, File, toString, 0);

    ObjClass* fileMetaclass = vm->fileClass->obj.klass;
    DEF_METHOD(fileMetaclass, FileClass, open, 2);
    DEF_METHOD(fileMetaclass, FileClass, openAsync, 2);

    ObjClass* closableTrait = defineNativeTrait(vm, "TClosable");
    DEF_METHOD(closableTrait, TClosable, close, 0);

    ObjClass* ioStreamClass = defineNativeClass(vm, "IOStream");
    bindSuperclass(vm, ioStreamClass, vm->objectClass);
    bindTrait(vm, ioStreamClass, closableTrait);
    DEF_INTERCEPTOR(ioStreamClass, IOStream, INTERCEPTOR_INIT, __init__, 1);
    DEF_METHOD(ioStreamClass, IOStream, close, 0);
    DEF_METHOD(ioStreamClass, IOStream, file, 0);
    DEF_METHOD(ioStreamClass, IOStream, getPosition, 0);
    DEF_METHOD(ioStreamClass, IOStream, reset, 0);

    ObjClass* readStreamClass = defineNativeClass(vm, "ReadStream");
    bindSuperclass(vm, readStreamClass, ioStreamClass);
    DEF_INTERCEPTOR(readStreamClass, ReadStream, INTERCEPTOR_INIT, __init__, 1);
    DEF_METHOD(readStreamClass, ReadStream, isAtEnd, 0);
    DEF_METHOD(readStreamClass, ReadStream, next, 0);
    DEF_METHOD(readStreamClass, ReadStream, skip, 1);

    ObjClass* writeStreamClass = defineNativeClass(vm, "WriteStream");
    bindSuperclass(vm, writeStreamClass, ioStreamClass);
    DEF_INTERCEPTOR(writeStreamClass, WriteStream, INTERCEPTOR_INIT, __init__, 1);
    DEF_METHOD(writeStreamClass, WriteStream, flush, 0);
    DEF_METHOD(writeStreamClass, WriteStream, put, 1);

    ObjClass* binaryReadStreamClass = defineNativeClass(vm, "BinaryReadStream");
    bindSuperclass(vm, binaryReadStreamClass, readStreamClass);
    DEF_INTERCEPTOR(binaryReadStreamClass, BinaryReadStream, INTERCEPTOR_INIT, __init__, 1);
    DEF_METHOD(binaryReadStreamClass, BinaryReadStream, next, 0);
    DEF_METHOD(binaryReadStreamClass, BinaryReadStream, nextBytes, 1);

    ObjClass* binaryWriteStreamClass = defineNativeClass(vm, "BinaryWriteStream");
    bindSuperclass(vm, binaryWriteStreamClass, writeStreamClass);
    DEF_INTERCEPTOR(binaryWriteStreamClass, BinaryWriteStream, INTERCEPTOR_INIT, __init__, 1);
    DEF_METHOD(binaryWriteStreamClass, BinaryWriteStream, put, 1);
    DEF_METHOD(binaryWriteStreamClass, BinaryWriteStream, putBytes, 1);

    ObjClass* fileReadStreamClass = defineNativeClass(vm, "FileReadStream");
    bindSuperclass(vm, fileReadStreamClass, readStreamClass);
    DEF_INTERCEPTOR(fileReadStreamClass, FileReadStream, INTERCEPTOR_INIT, __init__, 1);
    DEF_METHOD(fileReadStreamClass, FileReadStream, next, 0);
    DEF_METHOD(fileReadStreamClass, FileReadStream, nextLine, 0);
    DEF_METHOD(fileReadStreamClass, FileReadStream, peek, 0);

    ObjClass* fileWriteStreamClass = defineNativeClass(vm, "FileWriteStream");
    bindSuperclass(vm, fileWriteStreamClass, writeStreamClass);
    DEF_INTERCEPTOR(fileWriteStreamClass, FileWriteStream, INTERCEPTOR_INIT, __init__, 1);
    DEF_METHOD(fileWriteStreamClass, FileWriteStream, put, 1);
    DEF_METHOD(fileWriteStreamClass, FileWriteStream, putLine, 0);
    DEF_METHOD(fileWriteStreamClass, FileWriteStream, putSpace, 0);
    DEF_METHOD(fileWriteStreamClass, FileWriteStream, putString, 1);

    ObjClass* ioExceptionClass = defineNativeException(vm, "IOException", vm->exceptionClass);
    defineNativeException(vm, "EOFException", ioExceptionClass);
    defineNativeException(vm, "FileNotFoundException", ioExceptionClass);

    vm->currentNamespace = vm->rootNamespace;
}