#pragma once
#ifndef clox_string_h
#define clox_string_h

#include "common.h"
#include "object.h"
#include "value.h"

ObjString* takeString(VM* vm, char* chars, int length);
ObjString* copyString(VM* vm, const char* chars, int length);
ObjString* newString(VM* vm, const char* chars);
ObjString* formattedString(VM* vm, const char* format, ...);
ObjString* formattedLongString(VM* vm, const char* format, ...);

int searchString(VM* vm, ObjString* haystack, ObjString* needle, uint32_t start);
ObjString* capitalizeString(VM* vm, ObjString* string);
ObjString* decapitalizeString(VM* vm, ObjString* string);
ObjString* replaceString(VM* vm, ObjString* original, ObjString* target, ObjString* replace);
ObjString* subString(VM* vm, ObjString* original, int fromIndex, int toIndex);
ObjString* toLowerString(VM* vm, ObjString* string);
ObjString* toUpperString(VM* vm, ObjString* string);
ObjString* trimString(VM* vm, ObjString* string);

#endif // !clox_string_h