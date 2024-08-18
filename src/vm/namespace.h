#pragma once
#ifndef clox_namespace_h
#define clox_namespace_h

#include "value.h"

ObjNamespace* declareNamespace(VM* vm, uint8_t namespaceDepth);
Value usingNamespace(VM* vm, uint8_t namespaceDepth);
bool sourceFileExists(ObjString* filePath);
ObjString* resolveSourceFile(VM* vm, ObjString* shortName, ObjNamespace* enclosingNamespace);
bool sourceDirectoryExists(ObjString* directoryPath);
ObjString* resolveSourceDirectory(VM* vm, ObjString* shortName, ObjNamespace* enclosingNamespace);
InterpretResult runModule(VM* vm, ObjModule* module, bool isRootModule);
bool loadModule(VM* vm, ObjString* path);

#endif // !clox_namespace_h
