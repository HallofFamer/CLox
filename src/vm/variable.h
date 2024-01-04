#pragma once
#ifndef clox_variable_h
#define clox_variable_h

#include "chunk.h"
#include "common.h"

bool matchVariableName(ObjString* sourceString, const char* targetChars, int targetLength);
bool loadGlobal(VM* vm, Chunk* chunk, uint8_t byte, Value* value);
bool getInstanceVariable(VM* vm, Value receiver, Chunk* chunk, uint8_t byte);
bool setInstanceVariable(VM* vm, Value receiver, Chunk* chunk, uint8_t byte, Value value);

#endif // !clox_variable_h