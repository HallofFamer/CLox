#include <stdlib.h>

#include "chunk.h"
#include "../vm/memory.h"
#include "../vm/vm.h"

void initChunk(Chunk* chunk, GCGenerationType generation) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    chunk->inlineCaches = NULL;
    initValueArray(&chunk->constants);
    initValueArray(&chunk->identifiers);
}

void freeChunk(VM* vm, Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    FREE_ARRAY(InlineCache, chunk->inlineCaches, chunk->identifiers.capacity);
    freeValueArray(vm, &chunk->constants);
    freeValueArray(vm, &chunk->identifiers);
    initChunk(chunk, chunk->generation);
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

int addIdentifier(VM* vm, Chunk* chunk, Value value) {
    push(vm, value);
    int oldCapacity = chunk->identifiers.capacity;
    int oldCount = chunk->identifiers.count;

    valueArrayWrite(vm, &chunk->identifiers, value);
    if (oldCapacity < chunk->identifiers.capacity) {
        chunk->inlineCaches = GROW_ARRAY(InlineCache, chunk->inlineCaches, oldCapacity, chunk->identifiers.capacity);
    }
    InlineCache inlineCache = { .type = CACHE_NONE, .id = 0, .index = 0 };
    
    chunk->inlineCaches[oldCount] = inlineCache;
    pop(vm);
    return chunk->identifiers.count - 1;
}

int opCodeOffset(Chunk* chunk, int ip) {
    OpCode code = chunk->code[ip];

    switch (code) {
        case OP_CONSTANT: return 2;
        case OP_NAMESPACE: return 2;
        case OP_NIL: return 1;
        case OP_TRUE: return 1;
        case OP_FALSE: return 1;
        case OP_POP: return 1;
        case OP_DUP: return 1;
        case OP_GET_LOCAL: return 2;
        case OP_SET_LOCAL: return 2;
        case OP_DEFINE_GLOBAL_VAL: return 2;
        case OP_DEFINE_GLOBAL_VAR: return 2;
        case OP_GET_GLOBAL: return 2;
        case OP_SET_GLOBAL: return 2;
        case OP_GET_UPVALUE: return 2;
        case OP_SET_UPVALUE: return 2;
        case OP_GET_PROPERTY: return 2;
        case OP_SET_PROPERTY: return 2;
        case OP_GET_PROPERTY_OPTIONAL: return 2;
        case OP_GET_SUBSCRIPT: return 1;
        case OP_SET_SUBSCRIPT: return 1;
        case OP_GET_SUBSCRIPT_OPTIONAL: return 1;
        case OP_GET_SUPER: return 2;
        case OP_EQUAL: return 1;
        case OP_GREATER: return 1;
        case OP_LESS: return 1;
        case OP_ADD: return 1;
        case OP_SUBTRACT: return 1;
        case OP_MULTIPLY: return 1;
        case OP_DIVIDE: return 1;
        case OP_MODULO: return 1;
        case OP_NIL_COALESCING: return 1;
        case OP_ELVIS: return 1;
        case OP_NOT: return 1;
        case OP_NEGATE: return 1;
        case OP_JUMP: return 3;
        case OP_JUMP_IF_FALSE: return 3;
        case OP_JUMP_IF_EMPTY: return 3;
        case OP_LOOP: return 3;
        case OP_CALL: return 2;
        case OP_OPTIONAL_CALL: return 2;
        case OP_INVOKE: return 3;
        case OP_SUPER_INVOKE: return 3;
        case OP_OPTIONAL_INVOKE: return 3;
        case OP_CLOSURE: {
            int constant = (chunk->code[ip + 1] << 8) | chunk->code[ip + 2];
            ObjFunction* function = AS_FUNCTION(chunk->constants.values[constant]);
            return 2 + (function->upvalueCount * 2);
        }
        case OP_CLOSE_UPVALUE: return 1;
        case OP_CLASS: return 2;
        case OP_TRAIT: return 2;
        case OP_ANONYMOUS: return 2;
        case OP_INHERIT: return 1;
        case OP_IMPLEMENT: return 2;
        case OP_INSTANCE_METHOD: return 2;
        case OP_CLASS_METHOD: return 2;
        case OP_ARRAY: return 2;
        case OP_DICTIONARY: return 2;
        case OP_RANGE: return 1;
        case OP_REQUIRE: return 1;
        case OP_DECLARE_NAMESPACE: return 2;
        case OP_GET_NAMESPACE: return 2;
        case OP_USING_NAMESPACE: return 2;
        case OP_THROW: return 1;
        case OP_TRY: return 6;
        case OP_CATCH: return 1;
        case OP_FINALLY: return 1;
        case OP_RETURN: return 1;
        case OP_RETURN_NONLOCAL: return 2;
        case OP_YIELD: return 1;
        case OP_YIELD_FROM: return 1;
        case OP_AWAIT: return 1;
        case OP_END: return 1;
        default: return 0;
    }
}