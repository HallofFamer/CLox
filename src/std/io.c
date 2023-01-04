#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#define _chmod(path, mode) chmod(path, mode)
#define fopen_s(fp,filename,mode) ((*(fp))=fopen((filename),(mode)))==NULL
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
    if (file->file == NULL) raiseError(vm, "Cannot create IOStream, file either does not exist or require additional permission to access.");
    file->isOpen = true;
    file->mode = newString(vm, mode);
    setObjProperty(vm, object, "file", OBJ_VAL(file));
}

LOX_METHOD(BinaryReadStream, init) {
    ASSERT_ARG_COUNT("BinaryReadStream::init(file)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjFile* file = getFileArgument(vm, args[0]);
    if (file == NULL) raiseError(vm, "Method BinaryReadStream::init(file) expects argument 1 to be a string or file.");
    setFileProperty(vm, AS_INSTANCE(receiver), file, "rb");
    RETURN_OBJ(self);
}

LOX_METHOD(BinaryReadStream, next) {
    ASSERT_ARG_COUNT("BinaryReadStream::next()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) raiseError(vm, "Cannot read the next byte because file is already closed.");
    if (file->file == NULL) RETURN_NIL;
    else {
        unsigned char byte;
        if (fread(&byte, sizeof(char), 1, file->file) > 0) {
            RETURN_INT((int)byte);
        }
        RETURN_NIL;
    }
    RETURN_NIL;
}

LOX_METHOD(BinaryReadStream, nextBytes) {
    ASSERT_ARG_COUNT("BinaryReadStream::nextBytes(length)", 1);
    ASSERT_ARG_TYPE("BinaryReadStream::nextBytes(length)", 0, Int);
    assertNumberPositive(vm, "BinaryReadStream::nextBytes(length)", AS_NUMBER(args[0]), 0);
    int length = AS_INT(args[0]);

    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) raiseError(vm, "Cannot read the next byte because file is already closed.");
    if (file->file == NULL) RETURN_NIL;
    else {
        ObjArray* bytes = newArray(vm);
        push(vm, OBJ_VAL(bytes));
        for (int i = 0; i < length; i++) {
            unsigned char byte;
            if (fread(&byte, sizeof(char), 1, file->file) == 0) break;
            valueArrayWrite(vm, &bytes->elements, INT_VAL((int)byte));
        }
        pop(vm);
        RETURN_OBJ(bytes);
    }
}

LOX_METHOD(BinaryWriteStream, init) {
    ASSERT_ARG_COUNT("BinaryWriteStream::init(file)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjFile* file = getFileArgument(vm, args[0]);
    if (file == NULL) raiseError(vm, "Method BinaryWriteStream::init(file) expects argument 1 to be a string or file.");
    setFileProperty(vm, AS_INSTANCE(receiver), file, "wb");
    RETURN_OBJ(self);
}

LOX_METHOD(BinaryWriteStream, put) {
    ASSERT_ARG_COUNT("BinaryWriteStream::put(byte)", 1);
    ASSERT_ARG_TYPE("BinaryWriteStream::put(byte)", 0, Int);
    int byte = AS_INT(args[0]);
    assertIntWithinRange(vm, "BinaryWriteStream::put(byte)", byte, 0, 255, 0);

    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) raiseError(vm, "Cannot write byte to stream because file is already closed.");
    if (file->file == NULL) RETURN_NIL;
    else {
        unsigned char bytes[1] = { (unsigned char)byte };
        fwrite(bytes, sizeof(char), 1, file->file);
        RETURN_NIL;
    }
}

LOX_METHOD(BinaryWriteStream, putBytes) {
    ASSERT_ARG_COUNT("BinaryWriteStream::put(bytes)", 1);
    ASSERT_ARG_TYPE("BinaryWriteStream::put(bytes)", 0, Array);
    ObjArray* bytes = AS_ARRAY(args[0]);
    if (bytes->elements.count == 0) raiseError(vm, "Cannot write empty byte array to stream.");

    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) raiseError(vm, "Cannot write bytes to stream because file is already closed.");
    if (file->file == NULL) RETURN_NIL;
    else {
        unsigned char* byteArray = (unsigned char*)malloc(bytes->elements.count);
        if (byteArray != NULL) {
            for (int i = 0; i < bytes->elements.count; i++) {
                if (!IS_INT(bytes->elements.values[i])) raiseError(vm, "Cannot write bytes to stream because data is corrupted.");
                int byte = AS_INT(bytes->elements.values[i]);
                byteArray[i] = (unsigned char)byte;
            }
            fwrite(byteArray, sizeof(char), (size_t)bytes->elements.count, file->file);
            free(byteArray);
        }
        RETURN_NIL;
    }
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
    if (file->file != NULL) {
        ObjString* character = AS_STRING(args[0]);
        if (character->length != 1) raiseError(vm, "Method FileWriteStream::put(char) expects argument 1 to be a character(string of length 1)");
        fputc(character->chars[0], file->file);
    }
    RETURN_NIL;
}

LOX_METHOD(FileWriteStream, putLine) {
    ASSERT_ARG_COUNT("FileWriteStream::putLine()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) raiseError(vm, "Cannot write new line to stream because file is already closed.");
    if (file->file != NULL) fputc('\n', file->file);
    RETURN_NIL;
}

LOX_METHOD(FileWriteStream, putSpace) {
    ASSERT_ARG_COUNT("FileWriteStream::putSpace()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) raiseError(vm, "Cannot write empty space to stream because file is already closed.");
    if (file->file != NULL) fputc(' ', file->file);    
    RETURN_NIL;
}

LOX_METHOD(FileWriteStream, putString) {
    ASSERT_ARG_COUNT("FileWriteStream::putString(string)", 1);
    ASSERT_ARG_TYPE("FileWriteStream::putString(string)", 0, String);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) raiseError(vm, "Cannot write string to stream because file is already closed.");
    if (file->file != NULL) {
        ObjString* string = AS_STRING(args[0]);
        fputs(string->chars, file->file);
    }
    RETURN_NIL;
}

LOX_METHOD(IOStream, close) {
    ASSERT_ARG_COUNT("IOStream::close()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    file->isOpen = false;
    RETURN_BOOL(fclose(file->file) == 0);
}

LOX_METHOD(IOStream, file) {
    ASSERT_ARG_COUNT("IOStream::file()", 0);
    RETURN_OBJ(getFileProperty(vm, AS_INSTANCE(receiver), "file"));
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

LOX_METHOD(ReadStream, init) {
    raiseError(vm, "Cannot instantiate from class ReadStream.");
    RETURN_NIL;
}

LOX_METHOD(ReadStream, isAtEnd) {
    ASSERT_ARG_COUNT("ReadStream::next()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen || file->file == NULL) RETURN_FALSE;
    else {
        int c = getc(file->file);
        ungetc(c, file->file);
        RETURN_BOOL(c == EOF);
    }
}

LOX_METHOD(ReadStream, next) {
    raiseError(vm, "Cannot call method ReadStream::next(), it must be implemented by subclasses.");
    RETURN_NIL;
}

LOX_METHOD(ReadStream, skip) {
    ASSERT_ARG_COUNT("ReadStream::skip(offset)", 1);
    ASSERT_ARG_TYPE("ReadStream::skip(offset)", 0, Int);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) raiseError(vm, "Cannot skip stream by offset because file is already closed.");
    if (file->file == NULL) RETURN_FALSE;
    RETURN_BOOL(fseek(file->file, (long)AS_INT(args[0]), SEEK_CUR));
}

LOX_METHOD(WriteStream, flush) {
    ASSERT_ARG_COUNT("WriteStream::flush()", 0);
    ObjFile* file = getFileProperty(vm, AS_INSTANCE(receiver), "file");
    if (!file->isOpen) raiseError(vm, "Cannot flush stream because file is already closed.");
    if (file->file == NULL) RETURN_FALSE;
    RETURN_BOOL(fflush(file->file) == 0);
}

LOX_METHOD(WriteStream, init) {
    raiseError(vm, "Cannot instantiate from class WriteStream.");
    RETURN_NIL;
}

LOX_METHOD(WriteStream, put) {
    raiseError(vm, "Cannot call method WriteStream::put(param), it must be implemented by subclasses.");
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
    DEF_METHOD(ioStreamClass, IOStream, file, 0);
    DEF_METHOD(ioStreamClass, IOStream, getPosition, 0);
    DEF_METHOD(ioStreamClass, IOStream, init, 1);
    DEF_METHOD(ioStreamClass, IOStream, reset, 0);

    ObjClass* readStreamClass = defineNativeClass(vm, "ReadStream");
    bindSuperclass(vm, readStreamClass, ioStreamClass);
    DEF_METHOD(readStreamClass, ReadStream, init, 1);
    DEF_METHOD(readStreamClass, ReadStream, isAtEnd, 0);
    DEF_METHOD(readStreamClass, ReadStream, next, 0);
    DEF_METHOD(readStreamClass, ReadStream, skip, 1);

    ObjClass* writeStreamClass = defineNativeClass(vm, "WriteStream");
    bindSuperclass(vm, writeStreamClass, ioStreamClass);
    DEF_METHOD(writeStreamClass, WriteStream, flush, 0);
    DEF_METHOD(writeStreamClass, WriteStream, init, 1);
    DEF_METHOD(writeStreamClass, WriteStream, put, 1);

    ObjClass* binaryReadStreamClass = defineNativeClass(vm, "BinaryReadStream");
    bindSuperclass(vm, binaryReadStreamClass, readStreamClass);
    DEF_METHOD(binaryReadStreamClass, BinaryReadStream, init, 1);
    DEF_METHOD(binaryReadStreamClass, BinaryReadStream, next, 0);
    DEF_METHOD(binaryReadStreamClass, BinaryReadStream, nextBytes, 1);

    ObjClass* binaryWriteStreamClass = defineNativeClass(vm, "BinaryWriteStream");
    bindSuperclass(vm, binaryWriteStreamClass, writeStreamClass);
    DEF_METHOD(binaryWriteStreamClass, BinaryWriteStream, init, 1);
    DEF_METHOD(binaryWriteStreamClass, BinaryWriteStream, put, 1);
    DEF_METHOD(binaryWriteStreamClass, BinaryWriteStream, putBytes, 1);

    ObjClass* fileReadStreamClass = defineNativeClass(vm, "FileReadStream");
    bindSuperclass(vm, fileReadStreamClass, readStreamClass);
    DEF_METHOD(fileReadStreamClass, FileReadStream, init, 1);
    DEF_METHOD(fileReadStreamClass, FileReadStream, next, 0);
    DEF_METHOD(fileReadStreamClass, FileReadStream, nextLine, 0);
    DEF_METHOD(fileReadStreamClass, FileReadStream, peek, 0);

    ObjClass* fileWriteStreamClass = defineNativeClass(vm, "FileWriteStream");
    bindSuperclass(vm, fileWriteStreamClass, writeStreamClass);
    DEF_METHOD(fileWriteStreamClass, FileWriteStream, init, 1);
    DEF_METHOD(fileWriteStreamClass, FileWriteStream, put, 1);
    DEF_METHOD(fileWriteStreamClass, FileWriteStream, putLine, 0);
    DEF_METHOD(fileWriteStreamClass, FileWriteStream, putSpace, 0);
    DEF_METHOD(fileWriteStreamClass, FileWriteStream, putString, 1);
}
