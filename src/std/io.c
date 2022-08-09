#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#define _chmod(path, mode) chmod(path, mode)
#define _getcwd(buffer, size) getcwd(buffer, size)
#define _mkdir(path) mkdir(path, 777)
#define _rmdir(path) rmdir(path)
#endif // _WIN32

#include "io.h"
#include "../vm/assert.h"
#include "../vm/native.h"
#include "../vm/object.h"
#include "../vm/string.h"
#include "../vm/vm.h"

static bool fileExists(ObjFile* file, struct stat* fileStat) {
    return (stat(file->name->chars, fileStat) == 0);
}

static ObjFile* getFileArgument(VM* vm, Value arg) {
    ObjFile* file = NULL;
    if (IS_STRING(arg)) file = newFile(vm, AS_STRING(arg));
    else if (IS_FILE(arg)) file = AS_FILE(arg);
    return file;
}

static ObjFile* getFileProperty(VM* vm, ObjInstance* object, char* field) {
    return AS_FILE(getObjProperty(vm, object, field));
}

static void setFileProperty(VM* vm, ObjInstance* object, ObjFile* file, char* mode) {
    fopen_s(&file->file, file->name->chars, mode);
    if (file->file == NULL) raiseError(vm, "Cannot create stream object because file does not exist.");
    file->isOpen = true;
    file->mode = newString(vm, mode);
    setObjProperty(vm, object, "file", OBJ_VAL(file));
}

LOX_METHOD(File, create) {
    ASSERT_ARG_COUNT("File::create()", 0);
    ObjFile* self = AS_FILE(receiver);
    struct stat fileStat;
    if (fileExists(self, &fileStat)) raiseError(vm, "Cannot create new file because it already exists.");
    FILE* file;
    fopen_s(&file, self->name->chars, "w");
    if (file != NULL) {
        fclose(file);
        RETURN_TRUE;
    }
    RETURN_FALSE;
}

LOX_METHOD(File, delete) {
    ASSERT_ARG_COUNT("File::delete()", 0);
    ObjFile* self = AS_FILE(receiver);
    struct stat fileStat;
    if (!fileExists(self, &fileStat)) RETURN_FALSE;
    RETURN_BOOL(remove(self->name->chars) == 0);
}

LOX_METHOD(File, exists) {
    ASSERT_ARG_COUNT("File::exists()", 0);
    ObjFile* self = AS_FILE(receiver);
    struct stat fileStat;
    RETURN_BOOL(fileExists(self, &fileStat));
}

LOX_METHOD(File, getAbsolutePath) {
    ASSERT_ARG_COUNT("File::getAbsolutePath()", 0);
    ObjFile* self = AS_FILE(receiver);
    struct stat fileStat;
    if (!fileExists(self, &fileStat)) raiseError(vm, "Cannot get file absolute path because it does not exist.");
}

LOX_METHOD(File, init) {
    ASSERT_ARG_COUNT("File::init(pathname)", 1);
    ASSERT_ARG_TYPE("File::init(pathname)", 0, String);
    ObjFile* self = newFile(vm, AS_STRING(args[0]));
    RETURN_OBJ(self);
}

LOX_METHOD(File, isDirectory) {
    ASSERT_ARG_COUNT("File::isDirectory()", 0);
    ObjFile* self = AS_FILE(receiver);
    struct stat fileStat;
    if (!fileExists(self, &fileStat)) RETURN_FALSE;
    RETURN_BOOL(fileStat.st_mode & S_IFDIR);
}

LOX_METHOD(File, isExecutable) {
    ASSERT_ARG_COUNT("File::isExecutable()", 0);
    ObjFile* self = AS_FILE(receiver);
    struct stat fileStat;
    if (!fileExists(self, &fileStat)) RETURN_FALSE;
    RETURN_BOOL(fileStat.st_mode & S_IEXEC);
}

LOX_METHOD(File, isFile) {
    ASSERT_ARG_COUNT("File::isFile()", 0);
    ObjFile* self = AS_FILE(receiver);
    struct stat fileStat;
    if (!fileExists(self, &fileStat)) RETURN_FALSE;
    RETURN_BOOL(fileStat.st_mode & S_IFREG);
}

LOX_METHOD(File, isReadable) {
    ASSERT_ARG_COUNT("File::isReadable()", 0);
    ObjFile* self = AS_FILE(receiver);
    struct stat fileStat;
    if (!fileExists(self, &fileStat)) RETURN_FALSE;
    RETURN_BOOL(fileStat.st_mode & S_IREAD);
}

LOX_METHOD(File, isWritable) {
    ASSERT_ARG_COUNT("File::isWritable()", 0);
    ObjFile* self = AS_FILE(receiver);
    struct stat fileStat;
    if (!fileExists(self, &fileStat)) RETURN_FALSE;
    RETURN_BOOL(fileStat.st_mode & S_IWRITE);
}

LOX_METHOD(File, lastAccessed) {
    ASSERT_ARG_COUNT("File::lastAccessed()", 0);
    ObjFile* self = AS_FILE(receiver);
    struct stat fileStat;
    if (!fileExists(self, &fileStat)) raiseError(vm, "Cannot get file last accessed date because it does not exist.");
    RETURN_INT(fileStat.st_atime);
}

LOX_METHOD(File, lastModified) {
    ASSERT_ARG_COUNT("File::lastModified()", 0);
    ObjFile* self = AS_FILE(receiver);
    struct stat fileStat;
    if (!fileExists(self, &fileStat)) raiseError(vm, "Cannot get file last modified date because it does not exist.");
    RETURN_INT(fileStat.st_mtime);
}

LOX_METHOD(File, mkdir) {
    ASSERT_ARG_COUNT("File::mkdir()", 0);
    ObjFile* self = AS_FILE(receiver);
    struct stat fileStat;
    if (fileExists(self, &fileStat)) RETURN_FALSE;
    RETURN_BOOL(_mkdir(self->name->chars) == 0);
}

LOX_METHOD(File, name) {
    ASSERT_ARG_COUNT("File::name()", 0);
    RETURN_OBJ(AS_FILE(receiver)->name);
}

LOX_METHOD(File, rename) {
    ASSERT_ARG_COUNT("File::rename(name)", 1);
    ASSERT_ARG_TYPE("File::rename(name)", 0, String);
    ObjFile* self = AS_FILE(receiver);
    struct stat fileStat;
    if (!fileExists(self, &fileStat)) RETURN_FALSE;
    RETURN_BOOL(rename(self->name->chars, AS_STRING(args[0])->chars) == 0);
}

LOX_METHOD(File, rmdir) {
    ASSERT_ARG_COUNT("File::rmdir()", 0);
    ObjFile* self = AS_FILE(receiver);
    struct stat fileStat;
    if (!fileExists(self, &fileStat)) RETURN_FALSE;
    RETURN_BOOL(_rmdir(self->name->chars) == 0);
}

LOX_METHOD(File, setExecutable) {
    ASSERT_ARG_COUNT("File::setExecutable(canExecute)", 1);
    ASSERT_ARG_TYPE("File::setExecutable(canExecute)", 0, Bool);
    ObjFile* self = AS_FILE(receiver);
    struct stat fileStat;
    if (!fileExists(self, &fileStat)) RETURN_FALSE;
    if (AS_BOOL(args[0])) RETURN_BOOL(_chmod(self->name->chars, S_IEXEC));
    else RETURN_BOOL(_chmod(self->name->chars, ~S_IEXEC));
}

LOX_METHOD(File, setReadable) {
    ASSERT_ARG_COUNT("File::setReadable(canRead)", 1);
    ASSERT_ARG_TYPE("File::setReadable(canWrite)", 0, Bool);
    ObjFile* self = AS_FILE(receiver);
    struct stat fileStat;
    if (!fileExists(self, &fileStat)) RETURN_FALSE;
    if (AS_BOOL(args[0])) RETURN_BOOL(_chmod(self->name->chars, S_IREAD));
    else RETURN_BOOL(_chmod(self->name->chars, ~S_IREAD));
}

LOX_METHOD(File, setWritable) {
    ASSERT_ARG_COUNT("File::setWritable(canWrite)", 1);
    ASSERT_ARG_TYPE("File::setWritable(canWrite)", 0, Bool);
    ObjFile* self = AS_FILE(receiver);
    struct stat fileStat;
    if (!fileExists(self, &fileStat)) RETURN_FALSE;
    if (AS_BOOL(args[0])) RETURN_BOOL(_chmod(self->name->chars, S_IWRITE));
    else RETURN_BOOL(_chmod(self->name->chars, ~S_IWRITE));
}

LOX_METHOD(File, size) {
    ASSERT_ARG_COUNT("File::size()", 0);
    ObjFile* self = AS_FILE(receiver);
    struct stat fileStat;
    if (!fileExists(self, &fileStat)) raiseError(vm, "Cannot get file size because it does not exist.");
    RETURN_NUMBER(fileStat.st_size);
}

LOX_METHOD(File, toString) {
    ASSERT_ARG_COUNT("File::toString()", 0);
    RETURN_OBJ(AS_FILE(receiver)->name);
}


LOX_METHOD(FileReadStream, init) {
    ASSERT_ARG_COUNT("FileReadStream::init(file)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjFile* file = getFileArgument(vm, args[0]);
    if(file == NULL) raiseError(vm, "Method FileReadStream::init(file) expects argument 1 to be a string or file.");
    setFileProperty(vm, AS_INSTANCE(receiver), file, "r");
    RETURN_OBJ(self);
}

LOX_METHOD(FileReadStream, isAtEnd) {
    ASSERT_ARG_COUNT("FileReadStream::next()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen || file->file == NULL) RETURN_FALSE;
    else RETURN_BOOL(feof(file->file) != 0);
}

LOX_METHOD(FileReadStream, next) {
    ASSERT_ARG_COUNT("FileReadStream::next()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) raiseError(vm, "Cannot read the next char because file is already closed.");
    if (file->file == NULL) RETURN_NIL;
    else {
        int c = fgetc(file->file);
        if (c == EOF) RETURN_NIL;
        char ch[2] = { c, '\0' };
        RETURN_STRING(ch, 1);
    }
}

LOX_METHOD(FileReadStream, nextLine) {
    ASSERT_ARG_COUNT("FileReadStream::nextLine()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) raiseError(vm, "Cannot read the next line because file is already closed.");
    if (file->file == NULL) RETURN_NIL;
    else {
        char line[UINT8_MAX];
        if (fgets(line, sizeof line, file->file) == NULL) RETURN_NIL;
        RETURN_STRING(line, (int)strlen(line));
    }
}

LOX_METHOD(FileReadStream, peek) {
    ASSERT_ARG_COUNT("FileReadStream::peek()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) raiseError(vm, "Cannot peek the next char because file is already closed.");
    if (file->file == NULL) RETURN_NIL;
    else {
        int c = fgetc(file->file);
        ungetc(c, file->file);
        if (c == EOF) RETURN_NIL;
        char ch[2] = { c, '\0' };
        RETURN_STRING(ch, 1);
    }
}

LOX_METHOD(FileReadStream, skip) {
    ASSERT_ARG_COUNT("FileReadStream::skip(offset)", 1);
    ASSERT_ARG_TYPE("FileReadStream::offset(offset)", 0, Int);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) raiseError(vm, "Cannot skip by offset because file is already closed.");
    if (file->file == NULL) RETURN_FALSE;
    RETURN_BOOL(fseek(file->file, (long)AS_INT(args[0]), SEEK_CUR));
}

LOX_METHOD(FileWriteStream, flush) {
    ASSERT_ARG_COUNT("FileWriteStream::flush()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) raiseError(vm, "Cannot flush file stream because file is already closed.");
    if (file->file == NULL) RETURN_FALSE;
    RETURN_BOOL(fflush(file->file) == 0);
}

LOX_METHOD(FileWriteStream, init) {
    ASSERT_ARG_COUNT("FileWriteStream::init(file)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjFile* file = getFileArgument(vm, args[0]);
    if(file == NULL) raiseError(vm, "Method FileWriteStream::init(file) expects argument 1 to be a string or file.");
    setFileProperty(vm, AS_INSTANCE(receiver), file, "w");
    RETURN_OBJ(self);
}

LOX_METHOD(FileWriteStream, put) {
    ASSERT_ARG_COUNT("FileWriteStream::put(char)", 1);
    ASSERT_ARG_TYPE("FileWriteStream::put(char)", 0, String);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) raiseError(vm, "Cannot write character to stream because file is already closed.");
    if (file->file == NULL) RETURN_NIL;

    ObjString* character = AS_STRING(args[0]);
    if (character->length != 1) raiseError(vm, "Method FileWriteStream::put(char) expects argument 1 to be a character(string of length 1)");
    fputc(character->chars[0], file->file);
    RETURN_NIL;
}

LOX_METHOD(FileWriteStream, putLine) {
    ASSERT_ARG_COUNT("FileWriteStream::putLine()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) raiseError(vm, "Cannot write new line to stream because file is already closed.");
    if (file->file == NULL) RETURN_NIL;
    fputc('\n', file->file);
    RETURN_NIL;
}

LOX_METHOD(FileWriteStream, putSpace) {
    ASSERT_ARG_COUNT("FileWriteStream::putSpace()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) raiseError(vm, "Cannot write empty space to stream because file is already closed.");
    if (file->file == NULL) RETURN_NIL;
    fputc(' ', file->file);
    RETURN_NIL;
}

LOX_METHOD(FileWriteStream, putString) {
    ASSERT_ARG_COUNT("FileWriteStream::putString(string)", 1);
    ASSERT_ARG_TYPE("FileWriteStream::putString(string)", 0, String);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) raiseError(vm, "Cannot write string to stream because file is already closed.");
    if (file->file == NULL) RETURN_NIL;

    ObjString* string = AS_STRING(args[0]);
    fputs(string->chars, file->file);
    RETURN_NIL;
}

LOX_METHOD(IOStream, close) {
    ASSERT_ARG_COUNT("IOStream::close()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    file->isOpen = false;
    RETURN_BOOL(fclose(file->file) == 0);
}

LOX_METHOD(IOStream, getPosition) {
    ASSERT_ARG_COUNT("IOStream::getPosition()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) raiseError(vm, "Cannot get stream position because file is already closed.");
    if (file->file == NULL) RETURN_INT(0);
    else RETURN_INT(ftell(file->file));
}

LOX_METHOD(IOStream, init) {
    raiseError(vm, "Cannot instantiate from class IOStream.");
    RETURN_NIL;
}

LOX_METHOD(IOStream, reset) {
    ASSERT_ARG_COUNT("IOStream::reset()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) raiseError(vm, "Cannot reset stream because file is already closed.");
    if (file->file != NULL) rewind(file->file);
    RETURN_NIL;
}

void registerIOPackage(VM* vm) {
    vm->fileClass = defineNativeClass(vm, "File");
    bindSuperclass(vm, vm->fileClass, vm->objectClass);
    DEF_METHOD(vm->fileClass, File, create, 0);
    DEF_METHOD(vm->fileClass, File, delete, 0);
    DEF_METHOD(vm->fileClass, File, exists, 0);
    DEF_METHOD(vm->fileClass, File, init, 1);
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

    ObjClass* ioStreamClass = defineNativeClass(vm, "IOStream");
    bindSuperclass(vm, ioStreamClass, vm->objectClass);
    DEF_METHOD(ioStreamClass, IOStream, close, 0);
    DEF_METHOD(ioStreamClass, IOStream, getPosition, 0);
    DEF_METHOD(ioStreamClass, IOStream, init, 1);
    DEF_METHOD(ioStreamClass, IOStream, reset, 0);

    ObjClass* fileReadStreamClass = defineNativeClass(vm, "FileReadStream");
    bindSuperclass(vm, fileReadStreamClass, ioStreamClass);
    DEF_METHOD(fileReadStreamClass, FileReadStream, init, 1);
    DEF_METHOD(fileReadStreamClass, FileReadStream, isAtEnd, 0);
    DEF_METHOD(fileReadStreamClass, FileReadStream, next, 0);
    DEF_METHOD(fileReadStreamClass, FileReadStream, nextLine, 0);
    DEF_METHOD(fileReadStreamClass, FileReadStream, peek, 0);
    DEF_METHOD(fileReadStreamClass, FileReadStream, skip, 1);

    ObjClass* fileWriteStreamClass = defineNativeClass(vm, "FileWriteStream");
    bindSuperclass(vm, fileWriteStreamClass, ioStreamClass);
    DEF_METHOD(fileWriteStreamClass, FileWriteStream, flush, 0);
    DEF_METHOD(fileWriteStreamClass, FileWriteStream, init, 1);
    DEF_METHOD(fileWriteStreamClass, FileWriteStream, put, 1);
    DEF_METHOD(fileWriteStreamClass, FileWriteStream, putLine, 0);
    DEF_METHOD(fileWriteStreamClass, FileWriteStream, putSpace, 0);
    DEF_METHOD(fileWriteStreamClass, FileWriteStream, putString, 1);
}
