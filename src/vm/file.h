#pragma once
#ifndef clox_file_h
#define clox_file_h

#include "common.h"
#include "loop.h"

bool fileClose(VM* vm, ObjFile* file);
bool fileExists(VM* vm, ObjFile* file);
bool fileFlush(VM* vm, ObjFile* file);
bool fileOpen(VM* vm, ObjFile* file, const char* mode);
bool fileOpenAsync(VM* vm, ObjFile* file, const char* mode, uv_fs_cb callback);
ObjString* fileRead(VM* vm, ObjFile* file, bool isPeek);
void fileWrite(VM* vm, ObjFile* file, char c);
ObjFile* getFileArgument(VM* vm, Value arg);
ObjFile* getFileProperty(VM* vm, ObjInstance* object, char* field);
bool loadFileOperation(VM* vm, ObjFile* file, const char* streamClass);
bool loadFileRead(VM* vm, ObjFile* file);
bool loadFileStat(VM* vm, ObjFile* file);
bool loadFileWrite(VM* vm, ObjFile* file);
bool setFileProperty(VM* vm, ObjInstance* object, ObjFile* file, const char* mode);

#endif // !clox_file_h