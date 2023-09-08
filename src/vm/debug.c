#include <stdio.h>

#include "debug.h"
#include "object.h"
#include "value.h"

void disassembleChunk(Chunk* chunk, const char* name) {
    printf("== %s ==\n", name);
    for (int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

static int invokeInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    uint8_t argCount = chunk->code[offset + 2];
    printf("%-16s (%d args) %4d '", name, argCount, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 3;
}

static int simpleInstruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int byteInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

static int exceptionHandlerInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t type = chunk->code[offset + 1];
    uint16_t handlerAddress = (uint16_t)(chunk->code[offset + 2] << 8);
    handlerAddress |= chunk->code[offset + 3];
    printf("%-16s %4d -> %d\n", name, type, handlerAddress);
    return offset + 4;
}

static int closureInstruction(const char* name, Chunk* chunk, int offset) {
    offset++;
    uint8_t constant = chunk->code[offset++];
    printf("%-16s %4d ", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("\n");

    ObjFunction* function = AS_FUNCTION(chunk->constants.values[constant]);
    for (int j = 0; j < function->upvalueCount; j++) {
        int isLocal = chunk->code[offset++];
        int index = chunk->code[offset++];
        printf("%04d      |                     %s %d\n", offset - 2, isLocal ? "local" : "upvalue", index);
    }
    return offset;
}

int disassembleInstruction(Chunk* chunk, int offset) {
    printf("%04d ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } 
    else {
        printf("%4d ", chunk->lines[offset]);
    }
  
    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
          case OP_CONSTANT:
              return constantInstruction("OP_CONSTANT", chunk, offset);
          case OP_NIL:
              return simpleInstruction("OP_NIL", offset);
          case OP_TRUE:
              return simpleInstruction("OP_TRUE", offset);
          case OP_FALSE:
              return simpleInstruction("OP_FALSE", offset);
          case OP_POP:
              return simpleInstruction("OP_POP", offset);
          case OP_DUP:
              return simpleInstruction("OP_DUP", offset);
          case OP_GET_LOCAL:
              return byteInstruction("OP_GET_LOCAL", chunk, offset);
          case OP_SET_LOCAL:
              return byteInstruction("OP_SET_LOCAL", chunk, offset);
          case OP_DEFINE_GLOBAL_VAL: 
              return constantInstruction("OP_DEFINE_GLOBAL_VAL", chunk, offset);
          case OP_DEFINE_GLOBAL_VAR:
              return constantInstruction("OP_DEFINE_GLOBAL_VAR", chunk, offset);
          case OP_GET_GLOBAL:
              return constantInstruction("OP_GET_GLOBAL", chunk, offset);
          case OP_SET_GLOBAL:
              return constantInstruction("OP_SET_GLOBAL", chunk, offset);
          case OP_GET_UPVALUE:
              return byteInstruction("OP_GET_UPVALUE", chunk, offset);
          case OP_SET_UPVALUE:
              return byteInstruction("OP_SET_UPVALUE", chunk, offset);
          case OP_GET_PROPERTY:
              return constantInstruction("OP_GET_PROPERTY", chunk, offset);
          case OP_SET_PROPERTY:
              return constantInstruction("OP_SET_PROPERTY", chunk, offset);
          case OP_GET_SUBSCRIPT:
              return simpleInstruction("OP_GET_SUBSCRIPT", offset);
          case OP_SET_SUBSCRIPT:
              return simpleInstruction("OP_SET_SUBSCRIPT", offset);
          case OP_GET_SUPER:
              return constantInstruction("OP_GET_SUPER", chunk, offset);
          case OP_EQUAL:
              return simpleInstruction("OP_EQUAL", offset);
          case OP_GREATER:
              return simpleInstruction("OP_GREATER", offset);
          case OP_LESS:
              return simpleInstruction("OP_LESS", offset);
          case OP_ADD:
              return simpleInstruction("OP_ADD", offset);
          case OP_SUBTRACT:
              return simpleInstruction("OP_SUBTRACT", offset);
          case OP_MULTIPLY:
              return simpleInstruction("OP_MULTIPLY", offset);
          case OP_DIVIDE:
              return simpleInstruction("OP_DIVIDE", offset);
          case OP_NOT:
              return simpleInstruction("OP_NOT", offset);
          case OP_NEGATE:
              return simpleInstruction("OP_NEGATE", offset);
          case OP_JUMP:
              return jumpInstruction("OP_JUMP", 1, chunk, offset);
          case OP_JUMP_IF_FALSE:
              return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
          case OP_JUMP_IF_EMPTY:
              return jumpInstruction("OP_JUMP_IF_EMPTY", 1, chunk, offset);
          case OP_LOOP:
              return jumpInstruction("OP_LOOP", -1, chunk, offset);
          case OP_CALL:
              return byteInstruction("OP_CALL", chunk, offset);
          case OP_INVOKE:
              return invokeInstruction("OP_INVOKE", chunk, offset);
          case OP_SUPER_INVOKE:
              return invokeInstruction("OP_SUPER_INVOKE", chunk, offset);
          case OP_CLOSURE:
              return closureInstruction("OP_CLOSURE", chunk, offset);
          case OP_CLOSE_UPVALUE:
              return simpleInstruction("OP_CLOSE_UPVALUE", offset);
          case OP_CLASS:
              return constantInstruction("OP_CLASS", chunk, offset);
          case OP_TRAIT:
              return constantInstruction("OP_TRAIT", chunk, offset);
          case OP_ANONYMOUS:
              return constantInstruction("OP_ANONYMOUS", chunk, offset);
          case OP_INHERIT:
              return simpleInstruction("OP_INHERIT", offset);
          case OP_IMPLEMENT:
              return byteInstruction("OP_IMPLEMENT", chunk, offset);
          case OP_INSTANCE_METHOD:
              return constantInstruction("OP_INSTANCE_METHOD", chunk, offset);
          case OP_CLASS_METHOD:
              return constantInstruction("OP_CLASS_METHOD", chunk, offset);
          case OP_ARRAY: 
              return byteInstruction("OP_ARRAY", chunk, offset);
          case OP_DICTIONARY:
              return byteInstruction("OP_DICTIONARY", chunk, offset);
          case OP_RANGE:
              return simpleInstruction("OP_RANGE", offset);
          case OP_REQUIRE:
              return simpleInstruction("OP_REQUIRE", offset);
          case OP_DECLARE_NAMESPACE:
              return byteInstruction("OP_DECLARE_NAMESPACE", chunk, offset);
          case OP_GET_NAMESPACE:
              return byteInstruction("OP_GET_NAMESPACE", chunk, offset);
          case OP_USING_NAMESPACE: 
              return byteInstruction("OP_USING_NAMESPACE", chunk, offset);
          case OP_THROW:
              return simpleInstruction("OP_THROW", offset);
          case OP_TRY:
              return exceptionHandlerInstruction("OP_TRY", chunk, offset);
          case OP_END_TRY:
              return simpleInstruction("OP_END_TRY", offset);
          case OP_RETURN:
              return simpleInstruction("OP_RETURN", offset);
          case OP_RETURN_NONLOCAL:
              return byteInstruction("OP_RETURN_NONLOCAL", chunk, offset);
          default:
              printf("Unknown opcode %d\n", instruction);
              return offset + 1;
    }
}