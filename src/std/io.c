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

LOX_METHOD(File, toString) {
    ASSERT_ARG_COUNT("File::toString()", 0);
    RETURN_OBJ(AS_FILE(receiver)->name);
}

void registerIOPackage(VM* vm) {
    vm->fileClass = defineNativeClass(vm, "File");
    bindSuperclass(vm, vm->fileClass, vm->objectClass);
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
    DEF_METHOD(vm->fileClass, File, toString, 0);
}
