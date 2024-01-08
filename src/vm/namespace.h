#pragma once
#ifndef clox_namespace_h
#define clox_namespace_h

#include "common.h"
#include "value.h"

ObjNamespace* declareNamespace(VM* vm, uint8_t namespaceDepth);
Value usingNamespace(VM* vm, uint8_t namespaceDepth);
bool sourceFileExists(ObjString* filePath);
ObjString* resolveSourceFile(VM* vm, ObjString* shortName, ObjNamespace* enclosingNamespace);
bool sourceDirectoryExists(ObjString* directoryPath);
ObjString* resolveSourceDirectory(VM* vm, ObjString* shortName, ObjNamespace* enclosingNamespace);
bool loadModule(VM* vm, ObjString* path);

#endif // !clox_namespace_h
