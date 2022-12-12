#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "vm.h"

void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    initValueArray(&chunk->constants);
}

void freeChunk(VM* vm, Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    freeValueArray(vm, &chunk->constants);
    initChunk(chunk);
}

void writeChunk(VM* vm, Chunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity);
    }
    
    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int addConstant(VM* vm, Chunk* chunk, Value value) {
    push(vm, value);
    valueArrayWrite(vm, &chunk->constants, value);
    pop(vm);
    return chunk->constants.count - 1;
}

int opCodeOffset(Chunk* chunk, int ip) {
    OpCode code = chunk->code[ip];
    switch (code) {
        case OP_CALL:
        case OP_GET_SUBSCRIPT:
            return 1;

        case OP_DEFINE_GLOBAL:
        case OP_GET_GLOBAL:
        case OP_SET_GLOBAL:
        case OP_GET_LOCAL:
        case OP_SET_LOCAL:
        case OP_GET_UPVALUE:
        case OP_SET_UPVALUE:
        case OP_JUMP_IF_FALSE:
        case OP_JUMP_IF_EMPTY:
        case OP_JUMP:
        case OP_END:
        case OP_LOOP:
        case OP_CONSTANT:
        case OP_CLASS:
        case OP_GET_PROPERTY:
        case OP_SET_PROPERTY:
        case OP_ARRAY:
        case OP_DICTIONARY:
        case OP_METHOD:
            return 2;

        case OP_INVOKE:
        case OP_SUPER_INVOKE:
            return 3;

        case OP_CLOSURE: {
            int constant = (chunk->code[ip + 1] << 8) | chunk->code[ip + 2];
            ObjFunction* function = AS_FUNCTION(chunk->constants.values[constant]);
            return 2 + (function->upvalueCount * 3);
        }

        default:
            return 0;
    }
}