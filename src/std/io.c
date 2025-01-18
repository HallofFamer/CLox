#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "io.h"
#include "../common/os.h"
#include "../vm/assert.h"
#include "../vm/date.h"
#include "../vm/file.h"
#include "../vm/memory.h"
#include "../vm/native.h"
#include "../vm/object.h"
#include "../vm/string.h"
#include "../vm/vm.h"

LOX_METHOD(BinaryReadStream, __init__) {
    ASSERT_ARG_COUNT("BinaryReadStream::__init__(file)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjFile* file = getFileArgument(vm, args[0]);
    if (file == NULL) THROW_EXCEPTION(clox.std.lang.IllegalArgumentException, "Method BinaryReadStream::__init__(file) expects argument 1 to be a string or file.");
    if (!setFileProperty(vm, AS_INSTANCE(receiver), file, "rb")) {
        THROW_EXCEPTION(clox.std.io.IOException, "Cannot create BinaryReadStream, file either does not exist or require additional permission to access.");
    }
    if(!loadFileRead(vm, file)) THROW_EXCEPTION(clox.std.io.IOException, "Unable to read from binary stream.");
    RETURN_OBJ(self);
}

LOX_METHOD(BinaryReadStream, read) {
    ASSERT_ARG_COUNT("BinaryReadStream::read()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot read the next byte because file is already closed.");
    if (file->fsOpen != NULL && file->fsRead != NULL) RETURN_INT(fileReadByte(vm, file));
    RETURN_NIL;
}

LOX_METHOD(BinaryReadStream, readAsync) {
    ASSERT_ARG_COUNT("BinaryReadStream::readAsync()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) RETURN_PROMISE_EX(clox.std.io.IOException, "Cannot read the next byte because file is already closed.");
    loadFileRead(vm, file);
    ObjPromise* promise = fileReadAsync(vm, file, fileOnReadByte);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.io.IOException, "Failed to read byte from IO stream.");
    RETURN_OBJ(promise);
}

LOX_METHOD(BinaryReadStream, readBytes) {
    ASSERT_ARG_COUNT("BinaryReadStream::readBytes(length)", 1);
    ASSERT_ARG_TYPE("BinaryReadStream::readBytes(length)", 0, Int);
    int length = AS_INT(args[0]);
    if (length < 0) THROW_EXCEPTION_FMT(clox.std.lang.IllegalArgumentException, "Method BinaryReadStream::readBytes(length) expects argument 1 to be a positive integer but got %g.", length);

    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot read the next bytes because file is already closed.");
    if (file->fsOpen != NULL && file->fsRead != NULL) {
        ObjArray* bytes = fileReadBytes(vm, file, length);
        if (bytes == NULL) THROW_EXCEPTION_FMT(clox.std.lang.OutOfMemoryException, "Not enough memory to read the next %d bytes.", length);
        RETURN_OBJ(bytes);
    }
    RETURN_NIL;
}

LOX_METHOD(BinaryReadStream, readBytesAsync) {
    ASSERT_ARG_COUNT("BinaryReadStream::readBytesAsync(length)", 1);
    ASSERT_ARG_TYPE("BinaryReadStream::readBytesAsync(length)", 0, Int);
    int length = AS_INT(args[0]);
    if (length < 0) RETURN_PROMISE_EX_FMT(clox.std.lang.IllegalArgumentException, "Method BinaryReadStream::readBytesAsync(length) expects argument 1 to be a positive integer but got %g.", length);

    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) RETURN_PROMISE_EX(clox.std.io.IOException, "Cannot read the next bytes because file is already closed.");
    loadFileRead(vm, file);

    ObjPromise* promise = fileReadStringAsync(vm, file, (size_t)length, fileOnReadBytes);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.io.IOException, "Failed to read byte from IO stream.");
    RETURN_OBJ(promise);
}

LOX_METHOD(BinaryWriteStream, __init__) {
    ASSERT_ARG_COUNT("BinaryWriteStream::__init__(file)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjFile* file = getFileArgument(vm, args[0]);
    if (file == NULL) THROW_EXCEPTION(clox.std.lang.IllegalArgumentException, "Method BinaryWriteStream::__init__(file) expects argument 1 to be a string or file.");
    if (!setFileProperty(vm, AS_INSTANCE(receiver), file, "wb")) {
        THROW_EXCEPTION(clox.std.io.IOException, "Cannot create BinaryWriteStream, file either does not exist or require additional permission to access.");
    }
    if (!loadFileWrite(vm, file)) THROW_EXCEPTION(clox.std.io.IOException, "Unable to write to binary stream.");
    RETURN_OBJ(self);
}

LOX_METHOD(BinaryWriteStream, write) {
    ASSERT_ARG_COUNT("BinaryWriteStream::write(byte)", 1);
    ASSERT_ARG_TYPE("BinaryWriteStream::write(byte)", 0, Int);
    int arg = AS_INT(args[0]);
    ASSERT_INDEX_WITHIN_BOUNDS("BinaryWriteStream::write(byte)", arg, 0, 255, 0);

    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot write byte to stream because file is already closed.");
    if (file->fsOpen != NULL && file->fsWrite != NULL) fileWriteByte(vm, file, (uint8_t)arg);
    RETURN_NIL;
}

LOX_METHOD(BinaryWriteStream, writeAsync) {
    ASSERT_ARG_COUNT("BinaryWriteStream::writeAsync(byte)", 1);
    ASSERT_ARG_TYPE("BinaryWriteStream::writeAsync(byte)", 0, Int);
    int arg = AS_INT(args[0]);
    ASSERT_INDEX_WITHIN_BOUNDS("BinaryWriteStream::writeAsync(byte)", arg, 0, 255, 0);

    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) RETURN_PROMISE_EX(clox.std.io.IOException, "Cannot write byte to stream because file is already closed.");
    loadFileWrite(vm, file);

    uint8_t byte = (uint8_t)arg;
    ObjPromise* promise = fileWriteByteAsync(vm, file, byte, fileOnWrite);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.io.IOException, "Failed to write byte to IO stream.");
    RETURN_OBJ(promise);
}

LOX_METHOD(BinaryWriteStream, writeBytes) {
    ASSERT_ARG_COUNT("BinaryWriteStream::writeBytes(bytes)", 1);
    ASSERT_ARG_TYPE("BinaryWriteStream::writeBytes(bytes)", 0, Array);
    ObjArray* bytes = AS_ARRAY(args[0]);
    if (bytes->elements.count == 0) THROW_EXCEPTION(clox.std.io.IOException, "Cannot write empty byte array to stream.");

    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot write bytes to stream because file is already closed.");
    if (file->fsOpen != NULL && file->fsWrite != NULL) fileWriteBytes(vm, file, bytes);
    RETURN_NIL;
}

LOX_METHOD(BinaryWriteStream, writeBytesAsync) {
    ASSERT_ARG_COUNT("BinaryWriteStream::writeBytesAsync(bytes)", 1);
    ASSERT_ARG_TYPE("BinaryWriteStream::writeBytesAsync(bytes)", 0, Array);
    ObjArray* bytes = AS_ARRAY(args[0]);
    if (bytes->elements.count == 0) RETURN_PROMISE_EX(clox.std.io.IOException, "Cannot write empty byte array to stream.");

    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) RETURN_PROMISE_EX(clox.std.io.IOException, "Cannot write bytes to stream because file is already closed.");
    loadFileWrite(vm, file);

    ObjPromise* promise = fileWriteBytesAsync(vm, file, bytes, fileOnWrite);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.io.IOException, "Failed to write to IO stream.");
    RETURN_OBJ(promise);
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
    RETURN_BOOL(fileCreate(vm, self));
}

LOX_METHOD(File, createAsync) {
    ASSERT_ARG_COUNT("File::createAsync()", 0);
    ObjFile* self = AS_FILE(receiver);
    if (fileExists(vm, self)) RETURN_PROMISE_EX(clox.std.io.IOException, "Cannot create new file because it already exists");
    ObjPromise* promise = fileCreateAsync(vm, self, fileOnCreate);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.io.IOException, "Failed to create file because of system runs out of memory.");
    RETURN_OBJ(promise);
}

LOX_METHOD(File, delete) {
    ASSERT_ARG_COUNT("File::delete()", 0);
    ObjFile* self = AS_FILE(receiver);
    if (!fileExists(vm, self)) THROW_EXCEPTION(clox.std.io.IOException, "Cannot delete file because it does not exist.");
    RETURN_BOOL(fileDelete(vm, self));
}

LOX_METHOD(File, deleteAsync) {
    ASSERT_ARG_COUNT("File::deleteAsync()", 0);
    ObjFile* self = AS_FILE(receiver);
    if (!fileExists(vm, self)) RETURN_PROMISE_EX(clox.std.io.IOException, "Cannot delete file because it does not exist.");
    ObjPromise* promise = fileDeleteAsync(vm, self, fileOnHandle);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.io.IOException, "Failed to delete file because of system runs out of memory.");
    RETURN_OBJ(promise);
}

LOX_METHOD(File, exists) {
    ASSERT_ARG_COUNT("File::exists()", 0);
    RETURN_BOOL(fileExists(vm, AS_FILE(receiver)));
}

LOX_METHOD(File, getAbsolutePath) {
    ASSERT_ARG_COUNT("File::getAbsolutePath()", 0);
    ObjFile* self = AS_FILE(receiver);
    ObjString* realPath = fileGetAbsolutePath(vm, self);
    if (realPath == NULL) THROW_EXCEPTION(clox.std.io.FileNotFoundException, "Cannot get file absolute path because it does not exist.");
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
    ObjClass* dateTimeClass = getNativeClass(vm, "clox.std.util.DateTime");
    ObjInstance* lastAccessed = dateTimeObjFromTimestamp(vm, dateTimeClass, (double)self->fsStat->statbuf.st_atim.tv_sec);
    RETURN_OBJ(lastAccessed);
}

LOX_METHOD(File, lastModified) {
    ASSERT_ARG_COUNT("File::lastModified()", 0);
    ObjFile* self = AS_FILE(receiver);
    if (!loadFileStat(vm, self)) THROW_EXCEPTION(clox.std.io.FileNotFoundException, "Cannot get file last modified date because it does not exist.");
    ObjClass* dateTimeClass = getNativeClass(vm, "clox.std.util.DateTime");
    ObjInstance* lastModified = dateTimeObjFromTimestamp(vm, dateTimeClass, (double)self->fsStat->statbuf.st_mtim.tv_sec);
    RETURN_OBJ(lastModified);
}

LOX_METHOD(File, mkdir) {
    ASSERT_ARG_COUNT("File::mkdir()", 0);
    ObjFile* self = AS_FILE(receiver);
    if (fileExists(vm, self)) THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Cannot create directory as it already exists in the file system.");
    RETURN_BOOL(fileMkdir(vm, self));
}

LOX_METHOD(File, mkdirAsync) {
    ASSERT_ARG_COUNT("File::mkdirAsync()", 0);
    ObjFile* self = AS_FILE(receiver);
    if (fileExists(vm, self)) RETURN_PROMISE_EX(clox.std.lang.UnsupportedOperationException, "Cannot create directory as it already exists in the file system.");
    ObjPromise* promise = FileMkdirAsync(vm, self, fileOnHandle);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.io.IOException, "Failed to create directory because of system runs out of memory.");
    RETURN_OBJ(promise);
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
    RETURN_BOOL(fileRename(vm, self, AS_STRING(args[0])));
}

LOX_METHOD(File, renameAsync) {
    ASSERT_ARG_COUNT("File::renameAsync(name)", 1);
    ASSERT_ARG_TYPE("File::renameAsync(name)", 0, String);
    ObjFile* self = AS_FILE(receiver);
    if (!fileExists(vm, self)) RETURN_PROMISE_EX(clox.std.io.FileNotFoundException, "Cannot rename file as it does not exist.");
    ObjPromise* promise = fileRenameAsync(vm, self, AS_STRING(args[0]), fileOnHandle);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.io.IOException, "Failed to rename file because of system runs out of memory.");
    RETURN_OBJ(promise);
}

LOX_METHOD(File, rmdir) {
    ASSERT_ARG_COUNT("File::rmdir()", 0);
    ObjFile* self = AS_FILE(receiver);
    if (!loadFileStat(vm, self)) THROW_EXCEPTION(clox.std.io.FileNotFoundException, "Cannot remove directory as it does not exist.");
    RETURN_BOOL(fileRmdir(vm, self));
}

LOX_METHOD(File, rmdirAsync) {
    ASSERT_ARG_COUNT("File::rmdirAsync()", 0);
    ObjFile* self = AS_FILE(receiver);
    if (!loadFileStat(vm, self)) RETURN_PROMISE_EX(clox.std.io.FileNotFoundException, "Cannot remove directory as it does not exist.");
    ObjPromise* promise = fileRmdirAsync(vm, self, fileOnHandle);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.io.IOException, "Failed to delete directory because of system runs out of memory.");
    RETURN_OBJ(promise);
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
    ObjPromise* promise = fileOpenAsync(vm, file, mode, fileOnOpen);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.io.IOException, "Failed to open IO stream.");
    RETURN_OBJ(promise);
}

LOX_METHOD(FileReadStream, __init__) {
    ASSERT_ARG_COUNT("FileReadStream::__init__(file)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjFile* file = getFileArgument(vm, args[0]);
    if (file == NULL) THROW_EXCEPTION(clox.std.lang.IllegalArgumentException, "Method FileReadStream::__init__(file) expects argument 1 to be a string or file.");
    if (!setFileProperty(vm, AS_INSTANCE(receiver), file, "r")) {
        THROW_EXCEPTION(clox.std.io.IOException, "Cannot create FileReadStream, file either does not exist or require additional permission to access.");
    }
    if (!loadFileRead(vm, file)) THROW_EXCEPTION(clox.std.io.IOException, "Unable to read from file stream.");
    RETURN_OBJ(self);
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

LOX_METHOD(FileReadStream, read) {
    ASSERT_ARG_COUNT("FileReadStream::read()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot read the next char because file is already closed.");
    if (file->fsOpen == NULL) RETURN_NIL;
    else {
        ObjString* ch = fileRead(vm, file, false);
        if (ch == NULL) RETURN_NIL;
        RETURN_OBJ(ch);
    }
}

LOX_METHOD(FileReadStream, readAsync) {
    ASSERT_ARG_COUNT("FileReadStream::readAsync()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) RETURN_PROMISE_EX(clox.std.io.IOException, "Cannot read the next char because file is already closed.");
    loadFileRead(vm, file);

    ObjPromise* promise = fileReadAsync(vm, file, fileOnRead);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.io.IOException, "Failed to read char from IO stream.");
    RETURN_OBJ(promise);
}

LOX_METHOD(FileReadStream, readLine) {
    ASSERT_ARG_COUNT("FileReadStream::readLine()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot read the next line because file is already closed.");
    if (file->fsOpen == NULL) RETURN_NIL;
    else {
        ObjString* line = fileReadLine(vm, file);
        if (line == NULL) THROW_EXCEPTION(clox.std.lang.OutOfMemoryException, "Not enough memory to allocate for next line read.");
        RETURN_OBJ(line);
    }
}

LOX_METHOD(FileReadStream, readLineAsync) {
    ASSERT_ARG_COUNT("FileReadStream::readLineAsync()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) RETURN_PROMISE_EX(clox.std.io.IOException, "Cannot read the next line because file is already closed.");
    loadFileRead(vm, file);

    ObjPromise* promise = fileReadStringAsync(vm, file, UINT8_MAX, fileOnReadLine);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.io.IOException, "Failed to read line from IO stream.");
    RETURN_OBJ(promise);
}

LOX_METHOD(FileReadStream, readString) {
    ASSERT_ARG_COUNT("FileReadStream::readString(length)", 1);
    ASSERT_ARG_TYPE("FileReadStream::readString(length)", 0, Int);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    int length = AS_INT(args[0]);
    if (length < 0) THROW_EXCEPTION_FMT(clox.std.lang.IllegalArgumentException, "Method FileReadStream::readString(length) expects argument 1 to be a positive integer but got %g.", length);

    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot read the next line because file is already closed.");
    if (file->fsOpen == NULL) RETURN_NIL;
    else {
        ObjString* string = fileReadString(vm, file, length);
        if (string == NULL) THROW_EXCEPTION(clox.std.lang.OutOfMemoryException, "Not enough memory to allocate for next line read.");
        RETURN_OBJ(string);
    }
}

LOX_METHOD(FileReadStream, readStringAsync) {
    ASSERT_ARG_COUNT("FileReadStream::readStringAsync(length)", 1);
    ASSERT_ARG_TYPE("FileReadStream::readString(length)", 0, Int);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    int length = AS_INT(args[0]);
    if (length < 0) RETURN_PROMISE_EX_FMT(clox.std.lang.IllegalArgumentException, "Method FileReadStream::readStringAsync(length) expects argument 1 to be a positive integer but got %g.", length);
    loadFileRead(vm, file);

    ObjPromise* promise = fileReadStringAsync(vm, file, (size_t)length, fileOnReadString);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.io.IOException, "Failed to read line from IO stream.");
    RETURN_OBJ(promise);
}

LOX_METHOD(FileReadStream, readToEnd) {
    ASSERT_ARG_COUNT("FileReadStream::readToEnd()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot read to the end of file because file is already closed.");
    loadFileStat(vm, file);
    
    if (file->fsOpen == NULL) RETURN_NIL;
    else {
        ObjString* string = fileReadString(vm, file, (int)file->fsStat->statbuf.st_size);
        if (string == NULL) THROW_EXCEPTION(clox.std.lang.OutOfMemoryException, "Not enough memory to allocate to read to end end of file.");
        RETURN_OBJ(string);
    }
}

LOX_METHOD(FileReadStream, readToEndAsync) {
    ASSERT_ARG_COUNT("FileReadStream::readToEndAsync()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) RETURN_PROMISE_EX(clox.std.io.IOException, "Cannot read to the end of file because file is already closed.");
    loadFileStat(vm, file);
    loadFileRead(vm, file);

    ObjPromise* promise = fileReadStringAsync(vm, file, file->fsStat->statbuf.st_size, fileOnReadString);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.io.IOException, "Failed to read to the end of file from IO stream.");
    RETURN_OBJ(promise);
}

LOX_METHOD(FileWriteStream, __init__) {
    ASSERT_ARG_COUNT("FileWriteStream::__init__(file)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjFile* file = getFileArgument(vm, args[0]);
    if (file == NULL) THROW_EXCEPTION(clox.std.lang.IllegalArgumentException, "Method FileWriteStream::__init__(file) expects argument 1 to be a string or file.");
    if (!setFileProperty(vm, AS_INSTANCE(receiver), file, "w")) {
        THROW_EXCEPTION(clox.std.io.IOException, "Cannot create FileWriteStream, file either does not exist or require additional permission to access.");
    }
    if (!loadFileWrite(vm, file)) THROW_EXCEPTION(clox.std.io.IOException, "Unable to write to file stream.");
    RETURN_OBJ(self);
}

LOX_METHOD(FileWriteStream, write) {
    ASSERT_ARG_COUNT("FileWriteStream::write(char)", 1);
    ASSERT_ARG_TYPE("FileWriteStream::write(char)", 0, String);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot write character to stream because file is already closed.");
    if (file->fsOpen != NULL && file->fsWrite != NULL) {
        ObjString* character = AS_STRING(args[0]);
        if (character->length != 1) THROW_EXCEPTION(clox.std.lang.IllegalArgumentException, "Method FileWriteStream::put(char) expects argument 1 to be a character(string of length 1)");
        fileWrite(vm, file, character->chars[0]);
    }
    RETURN_NIL;
}

LOX_METHOD(FileWriteStream, writeAsync) {
    ASSERT_ARG_COUNT("FileWriteStream::writeAsync(char)", 1);
    ASSERT_ARG_TYPE("FileWriteStream::writeAsync(char)", 0, String);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) RETURN_PROMISE_EX(clox.std.io.IOException, "Cannot write character to stream because file is already closed.");
    loadFileWrite(vm, file);

    ObjString* character = AS_STRING(args[0]);
    if (character->length != 1) RETURN_PROMISE_EX(clox.std.lang.IllegalArgumentException, "Method FileWriteStream::putAsync(char) expects argument 1 to be a character(string of length 1)");
    ObjPromise* promise = fileWriteAsync(vm, file, character, fileOnWrite);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.io.IOException, "Failed to write to IO stream.");
    RETURN_OBJ(promise);
}

LOX_METHOD(FileWriteStream, writeLine) {
    ASSERT_ARG_COUNT("FileWriteStream::writeLine()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot write new line to stream because file is already closed.");
    if (file->fsOpen != NULL && file->fsWrite != NULL) fileWrite(vm, file, '\n');
    RETURN_NIL;
}

LOX_METHOD(FileWriteStream, writeLineAsync) {
    ASSERT_ARG_COUNT("FileWriteStream::writeLineAsync(char)", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) RETURN_PROMISE_EX(clox.std.io.IOException, "Cannot write line to stream because file is already closed.");
    loadFileWrite(vm, file);
    ObjPromise* promise = fileWriteAsync(vm, file, copyString(vm, "\n", 1), fileOnWrite);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.io.IOException, "Failed to write to IO stream.");
    RETURN_OBJ(promise);
}

LOX_METHOD(FileWriteStream, writeSpace) {
    ASSERT_ARG_COUNT("FileWriteStream::writeSpace()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot write empty space to stream because file is already closed.");
    if (file->fsOpen != NULL && file->fsWrite != NULL) fileWrite(vm, file, ' ');
    RETURN_NIL;
}

LOX_METHOD(FileWriteStream, writeSpaceAsync) {
    ASSERT_ARG_COUNT("FileWriteStream::writeSpaceAsync(char)", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) RETURN_PROMISE_EX(clox.std.io.IOException, "Cannot write space to stream because file is already closed.");
    loadFileWrite(vm, file);

    ObjPromise* promise = fileWriteAsync(vm, file, copyString(vm, " ", 1), fileOnWrite);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.io.IOException, "Failed to write to IO stream.");
    RETURN_OBJ(promise);
}

LOX_METHOD(FileWriteStream, writeString) {
    ASSERT_ARG_COUNT("FileWriteStream::writeString(string)", 1);
    ASSERT_ARG_TYPE("FileWriteStream::writeString(string)", 0, String);
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

LOX_METHOD(FileWriteStream, writeStringAsync) {
    ASSERT_ARG_COUNT("FileWriteStream::writeStringAsync(char)", 1);
    ASSERT_ARG_TYPE("FileWriteStream::writeStringAsync(char)", 0, String);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) RETURN_PROMISE_EX(clox.std.io.IOException, "Cannot write string to stream because file is already closed.");
    loadFileWrite(vm, file);

    ObjString* string = AS_STRING(args[0]);
    ObjPromise* promise = fileWriteAsync(vm, file, string, fileOnWrite);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.io.IOException, "Failed to write to IO stream.");
    RETURN_OBJ(promise);
}

LOX_METHOD(IOStream, __init__) {
    THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Cannot instantiate from class IOStream.");
}

LOX_METHOD(IOStream, close) {
    ASSERT_ARG_COUNT("IOStream::close()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    file->isOpen = false;
    RETURN_BOOL(fileClose(vm, file));
}

LOX_METHOD(IOStream, closeAsync) {
    ASSERT_ARG_COUNT("IOStream::close()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    ObjPromise* promise = fileCloseAsync(vm, file, fileOnClose);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.io.IOException, "Failed to close IO stream.");
    RETURN_OBJ(promise);
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
    THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Cannot instantiate from class ReadStream.");
}

LOX_METHOD(ReadStream, isAtEnd) {
    ASSERT_ARG_COUNT("ReadStream::isAtEnd()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) RETURN_FALSE;
    else {
        unsigned char c;
        uv_buf_t uvBuf = uv_buf_init(&c, 1);
        int numRead = uv_fs_read(vm->eventLoop, file->fsRead, (uv_file)file->fsOpen->result, &uvBuf, 1, file->offset, NULL);       
        RETURN_BOOL(numRead == 0);
    }
}

LOX_METHOD(ReadStream, read) {
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
    THROW_EXCEPTION(clox.std.lang.UnsupportedOperationException, "Cannot instantiate from class WriteStream.");
}

LOX_METHOD(WriteStream, flush) {
    ASSERT_ARG_COUNT("WriteStream::flush()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) RETURN_PROMISE_EX(clox.std.io.IOException, "Cannot flush write stream because file is already closed.");
    RETURN_BOOL(fileFlush(vm, file));
}

LOX_METHOD(WriteStream, flushAsync) {
    ASSERT_ARG_COUNT("WriteStream::flushAsync()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) THROW_EXCEPTION(clox.std.io.IOException, "Cannot flush write stream because file is already closed.");
    loadFileWrite(vm, file);
    ObjPromise* promise = fileFlushAsync(vm, file, fileOnFlush);
    if (promise == NULL) THROW_EXCEPTION(clox.std.io.IOException, "Failed to flush write stream.");
    RETURN_OBJ(promise);
}

LOX_METHOD(WriteStream, write) {
    THROW_EXCEPTION(clox.std.lang.NotImplementedException, "Not implemented, subclass responsibility.");
}

void registerIOPackage(VM* vm) {
    ObjNamespace* ioNamespace = defineNativeNamespace(vm, "io", vm->stdNamespace);
    vm->currentNamespace = ioNamespace;

    vm->fileClass = defineNativeClass(vm, "File");
    bindSuperclass(vm, vm->fileClass, vm->objectClass);
    vm->fileClass->classType = OBJ_FILE;
    DEF_INTERCEPTOR(vm->fileClass, File, INTERCEPTOR_INIT, __init__, 1, RETURN_TYPE(clox.std.io.File), PARAM_TYPE(String));
    DEF_METHOD(vm->fileClass, File, create, 0, RETURN_TYPE(Bool));
    DEF_METHOD_ASYNC(vm->fileClass, File, createAsync, 0, RETURN_TYPE(clox.std.util.Promise));
    DEF_METHOD(vm->fileClass, File, delete, 0, RETURN_TYPE(Bool));
    DEF_METHOD_ASYNC(vm->fileClass, File, deleteAsync, 0, RETURN_TYPE(clox.std.util.Promise));
    DEF_METHOD(vm->fileClass, File, exists, 0, RETURN_TYPE(Bool));
    DEF_METHOD(vm->fileClass, File, getAbsolutePath, 0, RETURN_TYPE(String));
    DEF_METHOD(vm->fileClass, File, isDirectory, 0, RETURN_TYPE(Bool));
    DEF_METHOD(vm->fileClass, File, isExecutable, 0, RETURN_TYPE(Bool));
    DEF_METHOD(vm->fileClass, File, isFile, 0, RETURN_TYPE(Bool));
    DEF_METHOD(vm->fileClass, File, isReadable, 0, RETURN_TYPE(Bool));
    DEF_METHOD(vm->fileClass, File, isWritable, 0, RETURN_TYPE(Bool));
    DEF_METHOD(vm->fileClass, File, lastAccessed, 0, RETURN_TYPE(clox.std.util.DateTime));
    DEF_METHOD(vm->fileClass, File, lastModified, 0, RETURN_TYPE(clox.std.util.DateTime));
    DEF_METHOD(vm->fileClass, File, mkdir, 0, RETURN_TYPE(Bool));
    DEF_METHOD_ASYNC(vm->fileClass, File, mkdirAsync, 0, RETURN_TYPE(clox.std.util.Promise));
    DEF_METHOD(vm->fileClass, File, name, 0, RETURN_TYPE(String));
    DEF_METHOD(vm->fileClass, File, rename, 1, RETURN_TYPE(Bool), PARAM_TYPE(String));
    DEF_METHOD_ASYNC(vm->fileClass, File, renameAsync, 1, RETURN_TYPE(clox.std.util.Promise), PARAM_TYPE(String));
    DEF_METHOD(vm->fileClass, File, rmdir, 0, RETURN_TYPE(Bool));
    DEF_METHOD(vm->fileClass, File, rmdirAsync, 0, RETURN_TYPE(clox.std.util.Promise));
    DEF_METHOD(vm->fileClass, File, setExecutable, 1, RETURN_TYPE(Bool), PARAM_TYPE(Bool));
    DEF_METHOD(vm->fileClass, File, setReadable, 1, RETURN_TYPE(Bool), PARAM_TYPE(Bool));
    DEF_METHOD(vm->fileClass, File, setWritable, 1, RETURN_TYPE(Bool), PARAM_TYPE(Bool));
    DEF_METHOD(vm->fileClass, File, size, 0, RETURN_TYPE(Number));
    DEF_METHOD(vm->fileClass, File, toString, 0, RETURN_TYPE(String));

    ObjClass* fileMetaclass = vm->fileClass->obj.klass;
    DEF_METHOD(fileMetaclass, FileClass, open, 2, RETURN_TYPE(Object), PARAM_TYPE(String), PARAM_TYPE(String));
    DEF_METHOD(fileMetaclass, FileClass, openAsync, 2, RETURN_TYPE(clox.std.util.Promise), PARAM_TYPE(String), PARAM_TYPE(String));

    ObjClass* closableTrait = defineNativeTrait(vm, "TClosable");
    DEF_METHOD(closableTrait, TClosable, close, 0, RETURN_TYPE(Bool));

    ObjClass* ioStreamClass = defineNativeClass(vm, "IOStream");
    bindSuperclass(vm, ioStreamClass, vm->objectClass);
    bindTrait(vm, ioStreamClass, closableTrait);
    DEF_INTERCEPTOR(ioStreamClass, IOStream, INTERCEPTOR_INIT, __init__, 1, RETURN_TYPE(clox.std.io.IOStream), PARAM_TYPE(Object));
    DEF_METHOD(ioStreamClass, IOStream, close, 0, RETURN_TYPE(Bool));
    DEF_METHOD_ASYNC(ioStreamClass, IOStream, closeAsync, 0, RETURN_TYPE(clox.std.util.Promise));
    DEF_METHOD(ioStreamClass, IOStream, file, 0, RETURN_TYPE(clox.std.io.File));
    DEF_METHOD(ioStreamClass, IOStream, getPosition, 0, RETURN_TYPE(Int));
    DEF_METHOD(ioStreamClass, IOStream, reset, 0, RETURN_TYPE(Nil));

    ObjClass* readStreamClass = defineNativeClass(vm, "ReadStream");
    bindSuperclass(vm, readStreamClass, ioStreamClass);
    DEF_INTERCEPTOR(readStreamClass, ReadStream, INTERCEPTOR_INIT, __init__, 1, RETURN_TYPE(clox.std.io.ReadStream), PARAM_TYPE(Object));
    DEF_METHOD(readStreamClass, ReadStream, isAtEnd, 0, RETURN_TYPE(Bool));
    DEF_METHOD(readStreamClass, ReadStream, read, 0, RETURN_TYPE(Object));
    DEF_METHOD(readStreamClass, ReadStream, skip, 1, RETURN_TYPE(Bool), PARAM_TYPE(Int));

    ObjClass* writeStreamClass = defineNativeClass(vm, "WriteStream");
    bindSuperclass(vm, writeStreamClass, ioStreamClass);
    DEF_INTERCEPTOR(writeStreamClass, WriteStream, INTERCEPTOR_INIT, __init__, 1, RETURN_TYPE(clox.std.io.WriteStream), PARAM_TYPE(Object));
    DEF_METHOD(writeStreamClass, WriteStream, flush, 0, RETURN_TYPE(Nil));
    DEF_METHOD_ASYNC(writeStreamClass, WriteStream, flushAsync, 0, RETURN_TYPE(clox.std.util.Promise));
    DEF_METHOD(writeStreamClass, WriteStream, write, 1, RETURN_TYPE(Nil), PARAM_TYPE(Object));

    ObjClass* binaryReadStreamClass = defineNativeClass(vm, "BinaryReadStream");
    bindSuperclass(vm, binaryReadStreamClass, readStreamClass);
    DEF_INTERCEPTOR(binaryReadStreamClass, BinaryReadStream, INTERCEPTOR_INIT, __init__, 1, RETURN_TYPE(clox.std.io.BinaryReadStream), PARAM_TYPE(Object));
    DEF_METHOD(binaryReadStreamClass, BinaryReadStream, read, 0, RETURN_TYPE(Int));
    DEF_METHOD_ASYNC(binaryReadStreamClass, BinaryReadStream, readAsync, 0, RETURN_TYPE(clox.std.util.Promise));
    DEF_METHOD(binaryReadStreamClass, BinaryReadStream, readBytes, 1, RETURN_TYPE(clox.std.collection.Array), PARAM_TYPE(Int));
    DEF_METHOD_ASYNC(binaryReadStreamClass, BinaryReadStream, readBytesAsync, 1, RETURN_TYPE(clox.std.util.Promise), PARAM_TYPE(Int));

    ObjClass* binaryWriteStreamClass = defineNativeClass(vm, "BinaryWriteStream");
    bindSuperclass(vm, binaryWriteStreamClass, writeStreamClass);
    DEF_INTERCEPTOR(binaryWriteStreamClass, BinaryWriteStream, INTERCEPTOR_INIT, __init__, 1, RETURN_TYPE(clox.std.io.BinaryWriteStream), PARAM_TYPE(Object));
    DEF_METHOD(binaryWriteStreamClass, BinaryWriteStream, write, 1, RETURN_TYPE(Nil), PARAM_TYPE(Int));
    DEF_METHOD_ASYNC(binaryWriteStreamClass, BinaryWriteStream, writeAsync, 1, RETURN_TYPE(clox.std.util.Promise), PARAM_TYPE(Int));
    DEF_METHOD(binaryWriteStreamClass, BinaryWriteStream, writeBytes, 1, RETURN_TYPE(Nil), PARAM_TYPE(clox.std.collection.Array));
    DEF_METHOD_ASYNC(binaryWriteStreamClass, BinaryWriteStream, writeBytesAsync, 1, RETURN_TYPE(clox.std.util.Promise), PARAM_TYPE(clox.std.collection.Array));

    ObjClass* fileReadStreamClass = defineNativeClass(vm, "FileReadStream");
    bindSuperclass(vm, fileReadStreamClass, readStreamClass);
    DEF_INTERCEPTOR(fileReadStreamClass, FileReadStream, INTERCEPTOR_INIT, __init__, 1, RETURN_TYPE(clox.std.io.FileReadStream), PARAM_TYPE(Object));
    DEF_METHOD(fileReadStreamClass, FileReadStream, peek, 0, RETURN_TYPE(String));
    DEF_METHOD(fileReadStreamClass, FileReadStream, read, 0, RETURN_TYPE(String));
    DEF_METHOD_ASYNC(fileReadStreamClass, FileReadStream, readAsync, 0, RETURN_TYPE(clox.std.util.Promise));
    DEF_METHOD(fileReadStreamClass, FileReadStream, readLine, 0, RETURN_TYPE(String));
    DEF_METHOD_ASYNC(fileReadStreamClass, FileReadStream, readLineAsync, 0, RETURN_TYPE(clox.std.util.Promise));
    DEF_METHOD(fileReadStreamClass, FileReadStream, readString, 0, RETURN_TYPE(String));
    DEF_METHOD_ASYNC(fileReadStreamClass, FileReadStream, readStringAsync, 0, RETURN_TYPE(clox.std.util.Promise));
    DEF_METHOD(fileReadStreamClass, FileReadStream, readToEnd, 0, RETURN_TYPE(String));
    DEF_METHOD_ASYNC(fileReadStreamClass, FileReadStream, readToEndAsync, 0, RETURN_TYPE(clox.std.util.Promise));

    ObjClass* fileWriteStreamClass = defineNativeClass(vm, "FileWriteStream");
    bindSuperclass(vm, fileWriteStreamClass, writeStreamClass);
    DEF_INTERCEPTOR(fileWriteStreamClass, FileWriteStream, INTERCEPTOR_INIT, __init__, 1, RETURN_TYPE(clox.std.io.FileWriteStream), PARAM_TYPE(Object));
    DEF_METHOD(fileWriteStreamClass, FileWriteStream, write, 1, RETURN_TYPE(Nil), PARAM_TYPE(String));
    DEF_METHOD_ASYNC(fileWriteStreamClass, FileWriteStream, writeAsync, 1, RETURN_TYPE(clox.std.util.Promise), PARAM_TYPE(String));
    DEF_METHOD(fileWriteStreamClass, FileWriteStream, writeLine, 0, RETURN_TYPE(Nil));
    DEF_METHOD_ASYNC(fileWriteStreamClass, FileWriteStream, writeLineAsync, 0, RETURN_TYPE(clox.std.util.Promise));
    DEF_METHOD(fileWriteStreamClass, FileWriteStream, writeSpace, 0, RETURN_TYPE(Nil));
    DEF_METHOD_ASYNC(fileWriteStreamClass, FileWriteStream, writeSpaceAsync, 0, RETURN_TYPE(clox.std.util.Promise));
    DEF_METHOD(fileWriteStreamClass, FileWriteStream, writeString, 1, RETURN_TYPE(Nil), PARAM_TYPE(String));
    DEF_METHOD_ASYNC(fileWriteStreamClass, FileWriteStream, writeStringAsync, 1, RETURN_TYPE(clox.std.util.Promise), PARAM_TYPE(String));

    ObjClass* ioExceptionClass = defineNativeException(vm, "IOException", vm->exceptionClass);
    defineNativeException(vm, "EOFException", ioExceptionClass);
    defineNativeException(vm, "FileNotFoundException", ioExceptionClass);

    vm->currentNamespace = vm->rootNamespace;
}