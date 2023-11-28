#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_DUP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_DEFINE_GLOBAL_VAL,
    OP_DEFINE_GLOBAL_VAR,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,
    OP_GET_PROPERTY_OPTIONAL,
    OP_GET_SUBSCRIPT,
    OP_SET_SUBSCRIPT,
    OP_GET_SUBSCRIPT_OPTIONAL,
    OP_GET_SUPER,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_MODULO,
    OP_NIL_COALESCING,
    OP_ELVIS,
    OP_NOT,
    OP_NEGATE,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_JUMP_IF_EMPTY,
    OP_LOOP,
    OP_CALL,
    OP_INVOKE,
    OP_SUPER_INVOKE,
    OP_OPTIONAL_INVOKE,
    OP_CLOSURE,
    OP_CLOSE_UPVALUE,
    OP_CLASS,
    OP_TRAIT,
    OP_ANONYMOUS,
    OP_INHERIT,
    OP_IMPLEMENT,
    OP_INSTANCE_METHOD,
    OP_CLASS_METHOD,
    OP_ARRAY,
    OP_DICTIONARY,
    OP_RANGE,
    OP_REQUIRE,
    OP_NAMESPACE,
    OP_DECLARE_NAMESPACE,
    OP_GET_NAMESPACE,
    OP_USING_NAMESPACE,
    OP_THROW,
    OP_TRY,
    OP_CATCH,
    OP_FINALLY,
    OP_RETURN,
    OP_RETURN_NONLOCAL,
    OP_END
} OpCode;

typedef enum {
    CACHE_NONE,
    CACHE_IVAR,
    CACHE_CVAR,
    CACHE_GVAL,
    CACHE_GVAR,
    CACHE_METHOD
} InlineCacheType;

typedef struct {
    InlineCacheType type;
    int id;
    int index;
} InlineCache;

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    int* lines;
    ValueArray constants;
    ValueArray identifiers;
    InlineCache* inlineCaches;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(VM* vm, Chunk* chunk);
void writeChunk(VM* vm, Chunk* chunk, uint8_t byte, int line);
int addConstant(VM* vm, Chunk* chunk, Value value);
int addIdentifier(VM* vm, Chunk* chunk, Value value);
int opCodeOffset(Chunk* chunk, int ip);

static inline void writeInlineCache(InlineCache* inlineCache, InlineCacheType type, int id, int index) {
    inlineCache->type = type;
    inlineCache->id = id;
    inlineCache->index = index;
}


#endif // !clox_chunk_h