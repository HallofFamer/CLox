#pragma once
#ifndef clox_variable_h
#define clox_variable_h

#include "chunk.h"
#include "common.h"

bool loadGlobal(VM* vm, Chunk* chunk, uint8_t byte, Value* value);
bool getInstanceVariable(VM* vm, Value receiver, Chunk* chunk, uint8_t byte);
bool setInstanceField(VM* vm, Value receiver, Chunk* chunk, uint8_t byte, Value value);

#endif // !clox_variable_h