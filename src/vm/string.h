#pragma once
#ifndef clox_string_h
#define clox_string_h

#include "object.h"
#include "../inc/utf8.h"

#define ALLOCATE_STRING(length, stringClass) (ObjString*)allocateObject(vm, sizeof(ObjString) + length + 1, OBJ_STRING, stringClass, GC_GENERATION_TYPE_EDEN)
#define ALLOCATE_STRING_GEN(length, stringClass, generation) (ObjString*)allocateObject(vm, sizeof(ObjString) + length + 1, OBJ_STRING, stringClass, generation)

ObjString* createString(VM* vm, char* chars, int length, uint32_t hash, ObjClass* klass);
ObjString* takeString(VM* vm, char* chars, int length);
ObjString* takeStringPerma(VM* vm, char* chars, int length);
ObjString* copyString(VM* vm, const char* chars, int length);
ObjString* copyStringPerma(VM* vm, const char* chars, int length);
ObjString* newString(VM* vm, const char* chars);
ObjString* newStringPerma(VM* vm, const char* chars);
ObjString* emptyString(VM* vm);
ObjString* formattedString(VM* vm, const char* format, ...);
ObjString* formattedStringPerma(VM* vm, const char* format, ...);

ObjString* capitalizeString(VM* vm, ObjString* string);
ObjString* concatenateString(VM* vm, ObjString* string, ObjString* string2, const char* separator);
ObjString* decapitalizeString(VM* vm, ObjString* string);
ObjString* replaceString(VM* vm, ObjString* original, ObjString* target, ObjString* replace);
ObjString* reverseString(VM* vm, ObjString* original);
int searchString(VM* vm, ObjString* haystack, ObjString* needle, uint32_t start);
ObjString* subString(VM* vm, ObjString* original, int fromIndex, int toIndex);
ObjString* toLowerString(VM* vm, ObjString* string);
ObjString* toUpperString(VM* vm, ObjString* string);
ObjString* trimString(VM* vm, ObjString* string);

int utf8NumBytes(int value);
char* utf8Encode(int value);
int utf8Decode(const uint8_t* bytes, uint32_t length);
ObjString* utf8StringFromByte(VM* vm, uint8_t byte);
ObjString* utf8StringFromCodePoint(VM* vm, int codePoint);
int utf8CodePointOffset(VM* vm, const char* string, int index);
ObjString* utf8CodePointAtIndex(VM* vm, const char* string, int index);

#endif // !clox_string_h