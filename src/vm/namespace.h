#pragma once
#ifndef clox_namespace_h
#define clox_namespace_h

#include "value.h"

ObjNamespace* declareNamespace(VM* vm, uint8_t namespaceDepth);
Value usingNamespace(VM* vm, uint8_t namespaceDepth);
bool isNativeNamespace(ObjString* fullName);
bool sourceFileExists(ObjString* filePath);
ObjString* locateSourceFile(VM* vm, ObjString* shortName, ObjNamespace* enclosingNamespace);
ObjString* locateSourceFileFromFullName(VM* vm, ObjString* fullName);
bool sourceDirectoryExists(ObjString* directoryPath);
ObjString* locateSourceDirectory(VM* vm, ObjString* shortName, ObjNamespace* enclosingNamespace);
ObjString* locateSourceDirectoryFromFullName(VM* vm, ObjString* fullName);
InterpretResult runModule(VM* vm, ObjModule* module, bool isRootModule);
bool loadModule(VM* vm, ObjString* path);

#endif // !clox_namespace_h
