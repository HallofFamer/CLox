#pragma once
#ifndef clox_file_h
#define clox_file_h

#include "common.h"
#include "loop.h"

typedef struct {
    VM* vm;
    ObjFile* file;
    ObjPromise* promise;
    uv_buf_t buffer;
} FileData;

bool fileClose(VM* vm, ObjFile* file);
ObjPromise* fileCloseAsync(VM* vm, ObjFile* file, uv_fs_cb callback);
bool fileExists(VM* vm, ObjFile* file);
bool fileFlush(VM* vm, ObjFile* file);
ObjPromise* fileFlushAsync(VM* vm, ObjFile* file, uv_fs_cb callback);
int fileMode(const char* mode);
void fileOnClose(uv_fs_t* fsClose);
void fileOnFlush(uv_fs_t* fsSync);
void fileOnOpen(uv_fs_t* fsOpen);
void fileOnRead(uv_fs_t* fsRead);
void fileOnReadByte(uv_fs_t* fsRead);
void fileOnReadBytes(uv_fs_t* fsRead);
void fileOnReadLine(uv_fs_t* fsRead);
void fileOnReadString(uv_fs_t* fsRead);
void fileOnWrite(uv_fs_t* fsWrite);
bool fileOpen(VM* vm, ObjFile* file, const char* mode);
ObjPromise* fileOpenAsync(VM* vm, ObjFile* file, const char* mode, uv_fs_cb callback);
ObjString* fileRead(VM* vm, ObjFile* file, bool isPeek);
ObjPromise* fileReadAsync(VM* vm, ObjFile* file, uv_fs_cb callback);
ObjString* fileReadLine(VM* vm, ObjFile* file);
ObjString* fileReadString(VM* vm, ObjFile* file, size_t length);
ObjPromise* fileReadStringAsync(VM* vm, ObjFile* file, size_t length, uv_fs_cb callback);
void fileWrite(VM* vm, ObjFile* file, char c);
ObjPromise* fileWriteAsync(VM* vm, ObjFile* file, ObjString* string, uv_fs_cb callback);
ObjPromise* fileWriteByteAsync(VM* vm, ObjFile* file, uint8_t byte, uv_fs_cb callback);
ObjPromise* fileWriteBytesAsync(VM* vm, ObjFile* file, ObjArray* bytes, uv_fs_cb callback);
ObjFile* getFileArgument(VM* vm, Value arg);
ObjFile* getFileProperty(VM* vm, ObjInstance* object, char* field);
bool loadFileOperation(VM* vm, ObjFile* file, const char* streamClass);
bool loadFileRead(VM* vm, ObjFile* file);
bool loadFileStat(VM* vm, ObjFile* file);
bool loadFileWrite(VM* vm, ObjFile* file);
bool setFileProperty(VM* vm, ObjInstance* object, ObjFile* file, const char* mode);
char* streamClassName(const char* mode);

#endif // !clox_file_h