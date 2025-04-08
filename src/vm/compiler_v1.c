#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler_v1.h"
#include "debug.h"
#include "id.h"
#include "memory.h"
#include "scanner.h"
#include "string.h"

typedef struct {
    VM* vm;
    Scanner* scanner;
    TokenV1 next;
    TokenV1 current;
    TokenV1 previous;
    TokenV1 rootClass;
    bool hadError;
    bool panicMode;
} ParserV1;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,
    PREC_COND,
    PREC_OR,
    PREC_AND,
    PREC_EQUALITY,
    PREC_COMPARISON,
    PREC_TERM,
    PREC_FACTOR,
    PREC_UNARY,
    PREC_CALL,
    PREC_PRIMARY
} PrecedenceV1;

typedef void (*ParseFn)(CompilerV1* compiler, bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    PrecedenceV1 precedence;
} ParseRuleV1;

typedef struct {
    TokenV1 name;
    int depth;
    bool isCaptured;
    bool isMutable;
} LocalV1;

typedef struct {
    uint8_t index;
    bool isLocal;
    bool isMutable;
} UpvalueV1;

typedef enum {
    TYPE_FUNCTION,
    TYPE_INITIALIZER,
    TYPE_LAMBDA,
    TYPE_METHOD,
    TYPE_SCRIPT
} FunctionType;

struct CompilerV1 {
    struct CompilerV1* enclosing;
    ParserV1* parser;
    ObjFunction* function;
    FunctionType type;

    LocalV1 locals[UINT8_COUNT];
    int localCount;
    UpvalueV1 upvalues[UINT8_COUNT];
    IDMap indexes;

    int scopeDepth;
    int innermostLoopStart;
    int innermostLoopScopeDepth;
    bool isAsync;
};

struct ClassCompilerV1 {
    struct ClassCompilerV1* enclosing;
    TokenV1 name;
    TokenV1 superclass;
    BehaviorType type;
};

static void initParserV1(ParserV1* parser, VM* vm, Scanner* scanner) {
    parser->vm = vm;
    parser->scanner = scanner;
    parser->rootClass = synthesizeToken("Object");
    parser->hadError = false;
    parser->panicMode = false;
}

static void errorAt(ParserV1* parser, TokenV1* token, const char* message) {
    if (parser->panicMode) return;
    parser->panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF_V1) {
        fprintf(stderr, " at end");
    }
    else if (token->type == TOKEN_ERROR_V1) {

    }
    else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser->hadError = true;
}

static void error(ParserV1* parser, const char* message) {
    errorAt(parser, &parser->previous, message);
}

static void errorAtCurrent(ParserV1* parser, const char* message) {
    errorAt(parser, &parser->current, message);
}

static void advance(ParserV1* parser) {
    parser->previous = parser->current;
    parser->current = parser->next;

    for (;;) {
        parser->next = scanNextToken(parser->scanner);
        if (parser->next.type != TOKEN_ERROR_V1) break;

        errorAtCurrent(parser, parser->next.start);
    }
}

static void consume(ParserV1* parser, TokenSymbolV1 type, const char* message) {
    if (parser->current.type == type) {
        advance(parser);
        return;
    }
    errorAtCurrent(parser, message);
}

static bool check(ParserV1* parser, TokenSymbolV1 type) {
    return parser->current.type == type;
}

static bool checkNext(ParserV1* parser, TokenSymbolV1 type) {
    return parser->next.type == type;
}

static bool match(ParserV1* parser, TokenSymbolV1 type) {
    if (!check(parser, type)) return false;
    advance(parser);
    return true;
}

static int hexDigit(ParserV1* parser, char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;

    error(parser, "Invalid hex escape sequence.");
    return -1;
}

static int hexEscape(ParserV1* parser, const char* source, int base, int startIndex) {
    int value = 0;
    for (int i = 0; i < base; i++) {
        int index = startIndex + i + 2;
        if (source[index] == '"' || source[index] == '\0') {
            error(parser, "Incomplete hex escape sequence.");
            break;
        }

        int digit = hexDigit(parser, source[index]);
        if (digit == -1) break;
        value = (value * 16) | digit;
    }
    return value;
}

static int unicodeEscape(ParserV1* parser, const char* source, char* target, int base, int startIndex, int currentLength) {
    int value = hexEscape(parser, source, base, startIndex);
    int numBytes = utf8NumBytes(value);
    if (numBytes < 0) error(parser, "Negative unicode character specified.");
    if (value > 0xffff) numBytes++;

    if (numBytes > 0) {
        char* utfChar = utf8Encode(value);
        if (utfChar == NULL) error(parser, "Invalid unicode character specified.");
        else {
            memcpy(target + currentLength, utfChar, (size_t)numBytes + 1);
            free(utfChar);
        }
    }
    return numBytes;
}

static char* parseString(ParserV1* parser, int* length) {
    int maxLength = parser->previous.length - 2;
    const char* source = parser->previous.start + 1;
    char* target = (char*)malloc((size_t)maxLength + 1);
    ABORT_IFNULL(target, "Not enough memory to allocate string in compiler. \n");

    int i = 0, j = 0;
    while (i < maxLength) {
        if (source[i] == '\\') {
            switch (source[i + 1]) {
            case 'a': {
                target[j] = '\a';
                i++;
                break;
            }
            case 'b': {
                target[j] = '\b';
                i++;
                break;
            }
            case 'f': {
                target[j] = '\f';
                i++;
                break;
            }
            case 'n': {
                target[j] = '\n';
                i++;
                break;
            }
            case 'r': {
                target[j] = '\r';
                i++;
                break;
            }
            case 't': {
                target[j] = '\t';
                i++;
                break;
            }
            case 'u': {
                int numBytes = unicodeEscape(parser, source, target, 4, i, j);
                j += numBytes > 4 ? numBytes - 2 : numBytes - 1;
                i += numBytes > 4 ? 6 : 5;
                break;
            }
            case 'U': {
                int numBytes = unicodeEscape(parser, source, target, 8, i, j);
                j += numBytes > 4 ? numBytes - 2 : numBytes - 1;
                i += 9;
                break;
            }
            case 'v': {
                target[j] = '\v';
                i++;
                break;
            }
            case 'x': {
                target[j] = hexEscape(parser, source, 2, i);
                i += 3;
                break;
            }
            case '"': {
                target[j] = '"';
                i++;
                break;
            }
            case '\\': {
                target[j] = '\\';
                i++;
                break;
            }
            default: target[j] = source[i];
            }
        }
        else target[j] = source[i];
        i++;
        j++;
    }

    target = (char*)reallocate(parser->vm, target, (size_t)maxLength + 1, (size_t)j + 1, GC_GENERATION_TYPE_PERMANENT);
    target[j] = '\0';
    *length = j;
    return target;
}

static void synchronize(ParserV1* parser) {
    parser->panicMode = false;

    while (parser->current.type != TOKEN_EOF_V1) {
        if (parser->previous.type == TOKEN_SEMICOLON_V1) return;

        switch (parser->current.type) {
        case TOKEN_ASYNC_V1:
        case TOKEN_AWAIT_V1:
        case TOKEN_CLASS_V1:
        case TOKEN_FOR_V1:
        case TOKEN_FUN_V1:
        case TOKEN_IF_V1:
        case TOKEN_NAMESPACE_V1:
        case TOKEN_RETURN_V1:
        case TOKEN_SWITCH_V1:
        case TOKEN_TRAIT_V1:
        case TOKEN_THROW_V1:
        case TOKEN_USING_V1:
        case TOKEN_VAL_V1:
        case TOKEN_VAR_V1:
        case TOKEN_WHILE_V1:
        case TOKEN_WITH_V1:
        case TOKEN_YIELD_V1:
            return;

        default:
            ;
        }
        advance(parser);
    }
}

static Chunk* currentChunk(CompilerV1* compiler) {
    return &compiler->function->chunk;
}

static void emitByte(CompilerV1* compiler, uint8_t byte) {
    writeChunk(compiler->parser->vm, currentChunk(compiler), byte, compiler->parser->previous.line);
}

static void emitBytes(CompilerV1* compiler, uint8_t byte1, uint8_t byte2) {
    emitByte(compiler, byte1);
    emitByte(compiler, byte2);
}

static int emitJump(CompilerV1* compiler, uint8_t instruction) {
    emitByte(compiler, instruction);
    emitByte(compiler, 0xff);
    emitByte(compiler, 0xff);
    return currentChunk(compiler)->count - 2;
}

static void emitLoop(CompilerV1* compiler, int loopStart) {
    emitByte(compiler, OP_LOOP);

    int offset = currentChunk(compiler)->count - loopStart + 2;
    if (offset > UINT16_MAX) error(compiler->parser, "Loop body too large.");

    emitByte(compiler, (offset >> 8) & 0xff);
    emitByte(compiler, offset & 0xff);
}

static void emitReturn(CompilerV1* compiler, uint8_t depth) {
    if (compiler->type == TYPE_INITIALIZER) {
        emitBytes(compiler, OP_GET_LOCAL, 0);
    }
    else {
        emitByte(compiler, OP_NIL);
    }

    if (depth == 0) {
        emitByte(compiler, OP_RETURN);
    }
    else {
        emitBytes(compiler, OP_RETURN_NONLOCAL, depth);
    }
}

static uint8_t makeConstant(CompilerV1* compiler, Value value) {
    int constant = addConstant(compiler->parser->vm, currentChunk(compiler), value);
    if (constant > UINT8_MAX) {
        error(compiler->parser, "Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static void emitConstant(CompilerV1* compiler, Value value) {
    emitBytes(compiler, OP_CONSTANT, makeConstant(compiler, value));
}

static void patchJump(CompilerV1* compiler, int offset) {
    int jump = currentChunk(compiler)->count - offset - 2;
    if (jump > UINT16_MAX) {
        error(compiler->parser, "Too much code to jump over.");
    }

    currentChunk(compiler)->code[offset] = (jump >> 8) & 0xff;
    currentChunk(compiler)->code[offset + 1] = jump & 0xff;
}

static void patchAddress(CompilerV1* compiler, int offset) {
    currentChunk(compiler)->code[offset] = (currentChunk(compiler)->count >> 8) & 0xff;
    currentChunk(compiler)->code[offset + 1] = currentChunk(compiler)->count & 0xff;
}

static void endLoop(CompilerV1* compiler) {
    int offset = compiler->innermostLoopStart;
    Chunk* chunk = currentChunk(compiler);
    while (offset < chunk->count) {
        if (chunk->code[offset] == OP_END) {
            chunk->code[offset] = OP_JUMP;
            patchJump(compiler, offset + 1);
        }
        else {
            offset += opCodeOffset(chunk, offset);
        }
    }
}

static void initCompiler(CompilerV1* compiler, ParserV1* parser, CompilerV1* enclosing, FunctionType type, bool isAsync) {
    compiler->parser = parser;
    compiler->enclosing = enclosing;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->innermostLoopStart = -1;
    compiler->innermostLoopScopeDepth = 0;

    compiler->isAsync = isAsync;
    compiler->function = newFunction(parser->vm);
    compiler->function->isAsync = isAsync;

    initIDMap(&compiler->indexes);
    parser->vm->currentCompiler = compiler;
    if (type != TYPE_SCRIPT) {
        if (parser->previous.length == 3 && memcmp(parser->previous.start, "fun", 3) == 0) {
            compiler->function->name = copyString(parser->vm, "", 0);
        }
        else compiler->function->name = copyString(parser->vm, parser->previous.start, parser->previous.length);
    }

    LocalV1* local = &compiler->locals[compiler->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    local->isMutable = false;

    if (type != TYPE_FUNCTION && type != TYPE_LAMBDA) {
        local->name.start = "this";
        local->name.length = 4;
    }
    else {
        local->name.start = "";
        local->name.length = 0;
    }
}

static ObjFunction* endCompiler(CompilerV1* compiler) {
    emitReturn(compiler, 0);
    ObjFunction* function = compiler->function;

#ifdef DEBUG_PRINT_CODE
    if (!compiler->parser->hadError) {
        disassembleChunk(currentChunk(compiler), function->name != NULL ? function->name->chars : "<script>");
    }
#endif

    freeIDMap(compiler->parser->vm, &compiler->indexes);
    compiler->parser->vm->currentCompiler = compiler->enclosing;
    return function;
}

static void beginScope(CompilerV1* compiler) {
    compiler->scopeDepth++;
}

static void endScope(CompilerV1* compiler) {
    compiler->scopeDepth--;

    while (compiler->localCount > 0 && compiler->locals[compiler->localCount - 1].depth > compiler->scopeDepth) {
        if (compiler->locals[compiler->localCount - 1].isCaptured) {
            emitByte(compiler, OP_CLOSE_UPVALUE);
        }
        else {
            emitByte(compiler, OP_POP);
        }
        compiler->localCount--;
    }
}

static void expression(CompilerV1* compiler);
static void statement(CompilerV1* compiler);
static void block(CompilerV1* compiler);
static void behavior(CompilerV1* compiler, BehaviorType type, TokenV1 name);
static void function(CompilerV1* enclosing, FunctionType type, bool isAsync);
static void declaration(CompilerV1* compiler);
static ParseRuleV1* getRule(TokenSymbolV1 type);
static void parsePrecedence(CompilerV1* compiler, PrecedenceV1 precedence);

static uint8_t makeIdentifier(CompilerV1* compiler, Value value) {
    ObjString* name = AS_STRING(value);
    int identifier;
    if (!idMapGet(&compiler->indexes, name, &identifier)) {
        identifier = addIdentifier(compiler->parser->vm, currentChunk(compiler), value);
        if (identifier > UINT8_MAX) {
            error(compiler->parser, "Too many identifiers in one chunk.");
            return 0;
        }
        idMapSet(compiler->parser->vm, &compiler->indexes, name, identifier);
    }

    return (uint8_t)identifier;
}

static uint8_t identifierConstant(CompilerV1* compiler, TokenV1* name) {
    const char* start = name->start[0] != '`' ? name->start : name->start + 1;
    int length = name->start[0] != '`' ? name->length : name->length - 2;
    return makeIdentifier(compiler, OBJ_VAL(copyString(compiler->parser->vm, start, length)));
}

static ObjString* identifierName(CompilerV1* compiler, uint8_t arg) {
    return AS_STRING(currentChunk(compiler)->identifiers.values[arg]);
}

static bool identifiersEqual(TokenV1* a, TokenV1* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static uint8_t propertyConstant(CompilerV1* compiler, const char* message) {
    switch (compiler->parser->current.type) {
        case TOKEN_IDENTIFIER_V1:
        case TOKEN_EQUAL_EQUAL_V1:
        case TOKEN_GREATER_V1:
        case TOKEN_LESS_V1:
        case TOKEN_PLUS_V1:
        case TOKEN_MINUS_V1:
        case TOKEN_STAR_V1:
        case TOKEN_SLASH_V1:
        case TOKEN_MODULO_V1:
        case TOKEN_DOT_DOT_V1:
            advance(compiler->parser);
            return identifierConstant(compiler, &compiler->parser->previous);
        case TOKEN_LEFT_BRACKET_V1:
            advance(compiler->parser);
            if (match(compiler->parser, TOKEN_RIGHT_BRACKET_V1)) {
                TokenV1 token = synthesizeToken(match(compiler->parser, TOKEN_EQUAL_V1) ? "[]=" : "[]");
                return identifierConstant(compiler, &token);
            }
            else {
                errorAtCurrent(compiler->parser, message);
                return -1;
            }
        case TOKEN_LEFT_PAREN_V1:
            advance(compiler->parser);
            if (match(compiler->parser, TOKEN_RIGHT_PAREN_V1)) {
                TokenV1 token = synthesizeToken("()");
                return identifierConstant(compiler, &token);
            }
            else {
                errorAtCurrent(compiler->parser, message);
                return -1;
            }
        default:
            errorAtCurrent(compiler->parser, message);
            return -1;
    }
}

static int resolveLocal(CompilerV1* compiler, TokenV1* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        LocalV1* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error(compiler->parser, "Can't read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

static int addUpvalue(CompilerV1* compiler, uint8_t index, bool isLocal, bool isMutable) {
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++) {
        UpvalueV1* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT) {
        error(compiler->parser, "Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    compiler->upvalues[upvalueCount].isMutable = isMutable;
    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(CompilerV1* compiler, TokenV1* name) {
    if (compiler->enclosing == NULL) return -1;

    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true, compiler->enclosing->locals[local].isMutable);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return addUpvalue(compiler, (uint8_t)upvalue, false, compiler->enclosing->upvalues[upvalue].isMutable);
    }
    return -1;
}

static int addLocal(CompilerV1* compiler, TokenV1 name) {
    if (compiler->localCount == UINT8_COUNT) {
        error(compiler->parser, "Too many local variables in function.");
        return -1;
    }

    LocalV1* local = &compiler->locals[compiler->localCount++];
    local->name = name;
    local->depth = -1;
    local->isCaptured = false;
    local->isMutable = true;
    return compiler->localCount - 1;
}

static void getLocal(CompilerV1* compiler, int slot) {
    emitByte(compiler, OP_GET_LOCAL);
    emitByte(compiler, slot);
}

static void setLocal(CompilerV1* compiler, int slot) {
    emitByte(compiler, OP_SET_LOCAL);
    emitByte(compiler, slot);
}

static int discardLocals(CompilerV1* compiler) {
    int i = compiler->localCount - 1;
    for (; i >= 0 && compiler->locals[i].depth > compiler->innermostLoopScopeDepth; i--) {
        if (compiler->locals[i].isCaptured) {
            emitByte(compiler, OP_CLOSE_UPVALUE);
        }
        else {
            emitByte(compiler, OP_POP);
        }
    }
    return compiler->localCount - i - 1;
}

static void invokeMethod(CompilerV1* compiler, int args, const char* name, int length) {
    int slot = makeIdentifier(compiler, OBJ_VAL(copyString(compiler->parser->vm, name, length)));
    emitByte(compiler, OP_INVOKE);
    emitByte(compiler, slot);
    emitByte(compiler, args);
}

static void declareVariable(CompilerV1* compiler) {
    if (compiler->scopeDepth == 0) return;

    TokenV1* name = &compiler->parser->previous;
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        LocalV1* local = &compiler->locals[i];
        if (local->depth != -1 && local->depth < compiler->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            error(compiler->parser, "Already a variable with this name in this scope.");
        }
    }
    addLocal(compiler, *name);
}

static uint8_t parseVariable(CompilerV1* compiler, const char* errorMessage) {
    consume(compiler->parser, TOKEN_IDENTIFIER_V1, errorMessage);
    declareVariable(compiler);
    if (compiler->scopeDepth > 0) return 0;
    return identifierConstant(compiler, &compiler->parser->previous);
}

static void markInitialized(CompilerV1* compiler, bool isMutable) {
    if (compiler->scopeDepth == 0) return;
    compiler->locals[compiler->localCount - 1].depth = compiler->scopeDepth;
    compiler->locals[compiler->localCount - 1].isMutable = isMutable;
}

static void defineVariable(CompilerV1* compiler, uint8_t global, bool isMutable) {
    if (compiler->scopeDepth > 0) {
        markInitialized(compiler, isMutable);
        return;
    }
    else {
        ObjString* name = identifierName(compiler, global);
        int index;
        if (idMapGet(&compiler->parser->vm->currentModule->varIndexes, name, &index)) {
            error(compiler->parser, "Cannot redeclare global variable.");
        }

        if (isMutable) {
            idMapSet(compiler->parser->vm, &compiler->parser->vm->currentModule->varIndexes, name, compiler->parser->vm->currentModule->varFields.count);
            valueArrayWrite(compiler->parser->vm, &compiler->parser->vm->currentModule->varFields, NIL_VAL);
            emitBytes(compiler, OP_DEFINE_GLOBAL_VAR, global);
        }
        else {
            idMapSet(compiler->parser->vm, &compiler->parser->vm->currentModule->valIndexes, name, compiler->parser->vm->currentModule->valFields.count);
            valueArrayWrite(compiler->parser->vm, &compiler->parser->vm->currentModule->valFields, NIL_VAL);
            emitBytes(compiler, OP_DEFINE_GLOBAL_VAL, global);
        }
    }
}

static uint8_t argumentList(CompilerV1* compiler) {
    uint8_t argCount = 0;
    if (!check(compiler->parser, TOKEN_RIGHT_PAREN_V1)) {
        do {
            expression(compiler);
            if (argCount == UINT8_MAX) {
                error(compiler->parser, "Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(compiler->parser, TOKEN_COMMA_V1));
    }
    consume(compiler->parser, TOKEN_RIGHT_PAREN_V1, "Expect ')' after arguments.");
    return argCount;
}

static void parameterList(CompilerV1* compiler) {
    if (match(compiler->parser, TOKEN_DOT_DOT_V1)) {
        compiler->function->arity = -1;
        uint8_t constant = parseVariable(compiler, "Expect variadic parameter name.");
        defineVariable(compiler, constant, false);
        return;
    }

    do {
        compiler->function->arity++;
        if (compiler->function->arity > UINT8_MAX) {
            errorAtCurrent(compiler->parser, "Can't have more than 255 parameters.");
        }
        bool isMutable = match(compiler->parser, TOKEN_VAR_V1);
        uint8_t constant = parseVariable(compiler, "Expect parameter name.");
        defineVariable(compiler, constant, isMutable);
    } while (match(compiler->parser, TOKEN_COMMA_V1));
}

static void and_(CompilerV1* compiler, bool canAssign) {
    int endJump = emitJump(compiler, OP_JUMP_IF_FALSE);

    emitByte(compiler, OP_POP);
    parsePrecedence(compiler, PREC_AND);

    patchJump(compiler, endJump);
}

static void binary(CompilerV1* compiler, bool canAssign) {
    TokenSymbolV1 operatorType = compiler->parser->previous.type;
    ParseRuleV1* rule = getRule(operatorType);
    parsePrecedence(compiler, (PrecedenceV1)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL_V1:        emitBytes(compiler, OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL_V1:       emitByte(compiler, OP_EQUAL); break;
        case TOKEN_GREATER_V1:           emitByte(compiler, OP_GREATER); break;
        case TOKEN_GREATER_EQUAL_V1:     emitBytes(compiler, OP_LESS, OP_NOT); break;
        case TOKEN_LESS_V1:              emitByte(compiler, OP_LESS); break;
        case TOKEN_LESS_EQUAL_V1:        emitBytes(compiler, OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS_V1:              emitByte(compiler, OP_ADD); break;
        case TOKEN_MINUS_V1:             emitByte(compiler, OP_SUBTRACT); break;
        case TOKEN_STAR_V1:              emitByte(compiler, OP_MULTIPLY); break;
        case TOKEN_SLASH_V1:             emitByte(compiler, OP_DIVIDE); break;
        case TOKEN_MODULO_V1:            emitByte(compiler, OP_MODULO); break;
        case TOKEN_DOT_DOT_V1:           emitByte(compiler, OP_RANGE); break;
        default: return;
    }
}

static void call(CompilerV1* compiler, bool canAssign) {
    uint8_t argCount = argumentList(compiler);
    emitBytes(compiler, OP_CALL, argCount);
}

static void dot(CompilerV1* compiler, bool canAssign) {
    uint8_t name = propertyConstant(compiler, "Expect property name after '.'.");

    if (canAssign && match(compiler->parser, TOKEN_EQUAL_V1)) {
        expression(compiler);
        emitBytes(compiler, OP_SET_PROPERTY, name);
    }
    else if (match(compiler->parser, TOKEN_LEFT_PAREN_V1)) {
        uint8_t argCount = argumentList(compiler);
        emitBytes(compiler, OP_INVOKE, name);
        emitByte(compiler, argCount);
    }
    else {
        emitBytes(compiler, OP_GET_PROPERTY, name);
    }
}

static void question(CompilerV1* compiler, bool canAssign) {
#define PARSE_PRECEDENCE() \
    do { \
        TokenSymbolV1 operatorType = compiler->parser->previous.type; \
        ParseRuleV1* rule = getRule(operatorType); \
        parsePrecedence(compiler, (PrecedenceV1)(rule->precedence + 1)); \
    } while (false)

    if (match(compiler->parser, TOKEN_DOT_V1)) {
        uint8_t name = propertyConstant(compiler, "Expect property name after '?.'.");

        if (match(compiler->parser, TOKEN_LEFT_PAREN_V1)) {
            uint8_t argCount = argumentList(compiler);
            emitBytes(compiler, OP_OPTIONAL_INVOKE, name);
            emitByte(compiler, argCount);
        }
        else {
            emitBytes(compiler, OP_GET_PROPERTY_OPTIONAL, name);
        }
    }
    else if (match(compiler->parser, TOKEN_LEFT_BRACKET_V1)) {
        expression(compiler);
        consume(compiler->parser, TOKEN_RIGHT_BRACKET_V1, "Expect ']' after subscript.");
        emitByte(compiler, OP_GET_SUBSCRIPT_OPTIONAL);
    }
    else if (match(compiler->parser, TOKEN_LEFT_PAREN_V1)) {
        uint8_t argCount = argumentList(compiler);
        emitBytes(compiler, OP_OPTIONAL_CALL, argCount);
    }
    else if (match(compiler->parser, TOKEN_QUESTION_V1)) {
        PARSE_PRECEDENCE();
        emitByte(compiler, OP_NIL_COALESCING);
    }
    else if (match(compiler->parser, TOKEN_COLON_V1)) {
        PARSE_PRECEDENCE();
        emitByte(compiler, OP_ELVIS);
    }

#undef PARSE_PRECEDENCE
}

static void subscript(CompilerV1* compiler, bool canAssign) {
    expression(compiler);
    consume(compiler->parser, TOKEN_RIGHT_BRACKET_V1, "Expect ']' after subscript.");

    if (canAssign && match(compiler->parser, TOKEN_EQUAL_V1)) {
        expression(compiler);
        emitByte(compiler, OP_SET_SUBSCRIPT);
    }
    else {
        emitByte(compiler, OP_GET_SUBSCRIPT);
    }
}

static void literal(CompilerV1* compiler, bool canAssign) {
    switch (compiler->parser->previous.type) {
        case TOKEN_FALSE_V1: emitByte(compiler, OP_FALSE); break;
        case TOKEN_NIL_V1: emitByte(compiler, OP_NIL); break;
        case TOKEN_TRUE_V1: emitByte(compiler, OP_TRUE); break;
        default: return;
    }
}

static void grouping(CompilerV1* compiler, bool canAssign) {
    expression(compiler);
    consume(compiler->parser, TOKEN_RIGHT_PAREN_V1, "Expect ')' after expression.");
}

static void integer(CompilerV1* compiler, bool canAssign) {
    int value = strtol(compiler->parser->previous.start, NULL, 10);
    emitConstant(compiler, INT_VAL(value));
}

static void number(CompilerV1* compiler, bool canAssign) {
    double value = strtod(compiler->parser->previous.start, NULL);
    emitConstant(compiler, NUMBER_VAL(value));
}

static void or_(CompilerV1* compiler, bool canAssign) {
    int elseJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    int endJump = emitJump(compiler, OP_JUMP);

    patchJump(compiler, elseJump);
    emitByte(compiler, OP_POP);

    parsePrecedence(compiler, PREC_OR);
    patchJump(compiler, endJump);
}

static void string(CompilerV1* compiler, bool canAssign) {
    int length = 0;
    char* string = parseString(compiler->parser, &length);
    emitConstant(compiler, OBJ_VAL(takeString(compiler->parser->vm, string, length)));
}

static void interpolation(CompilerV1* compiler, bool canAssign) {
    int count = 0;
    do {
        bool concatenate = false;
        bool isString = false;

        if (compiler->parser->previous.length > 2) {
            string(compiler, canAssign);
            concatenate = true;
            isString = true;
            if (count > 0) emitByte(compiler, OP_ADD);
        }

        expression(compiler);
        invokeMethod(compiler, 0, "toString", 8);
        if (concatenate || (count >= 1 && !isString)) {
            emitByte(compiler, OP_ADD);
        }
        count++;
    } while (match(compiler->parser, TOKEN_INTERPOLATION_V1));

    consume(compiler->parser, TOKEN_STRING_V1, "Expect end of string interpolation.");
    if (compiler->parser->previous.length > 2) {
        string(compiler, canAssign);
        emitByte(compiler, OP_ADD);
    }
}

static void array(CompilerV1* compiler) {
    uint8_t elementCount = 1;
    while (match(compiler->parser, TOKEN_COMMA_V1)) {
        expression(compiler);
        if (elementCount == UINT8_MAX) {
            error(compiler->parser, "Cannot have more than 255 elements.");
        }
        elementCount++;
    }

    consume(compiler->parser, TOKEN_RIGHT_BRACKET_V1, "Expect ']' after elements.");
    emitBytes(compiler, OP_ARRAY, elementCount);
}

static void dictionary(CompilerV1* compiler) {
    uint8_t entryCount = 1;
    while (match(compiler->parser, TOKEN_COMMA_V1)) {
        expression(compiler);
        consume(compiler->parser, TOKEN_COLON_V1, "Expect ':' after entry key.");
        expression(compiler);

        if (entryCount == UINT8_MAX) {
            error(compiler->parser, "Cannot have more than 255 entries.");
        }
        entryCount++;
    }

    consume(compiler->parser, TOKEN_RIGHT_BRACKET_V1, "Expect ']' after entries.");
    emitBytes(compiler, OP_DICTIONARY, entryCount);
}

static void collection(CompilerV1* compiler, bool canAssign) {
    if (match(compiler->parser, TOKEN_RIGHT_BRACKET_V1)) {
        emitBytes(compiler, OP_ARRAY, 0);
    }
    else {
        expression(compiler);
        if (match(compiler->parser, TOKEN_COLON_V1)) {
            expression(compiler);
            dictionary(compiler);
        }
        else array(compiler);
    }
}

static void closure(CompilerV1* compiler, bool canAssign) {
    function(compiler, TYPE_FUNCTION, false);
}

static void lambda(CompilerV1* compiler, bool canAssign) {
    function(compiler, TYPE_LAMBDA, false);
}

static void checkMutability(CompilerV1* compiler, int arg, uint8_t opCode) {
    switch (opCode) {
        case OP_SET_LOCAL:
            if (!compiler->locals[arg].isMutable) {
                error(compiler->parser, "Cannot assign to immutable local variable.");
            }
            break;
        case OP_SET_UPVALUE:
            if (!compiler->upvalues[arg].isMutable) {
                error(compiler->parser, "Cannot assign to immutable captured upvalue.");
            }
            break;
        case OP_SET_GLOBAL: {
            ObjString* name = identifierName(compiler, arg);
            int index;
            if (idMapGet(&compiler->parser->vm->currentModule->valIndexes, name, &index)) {
                error(compiler->parser, "Cannot assign to immutable global variables.");
            }
            break;
        }
        default:
            break;
    }
}

static void namedVariable(CompilerV1* compiler, TokenV1 name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(compiler, &name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    }
    else if ((arg = resolveUpvalue(compiler, &name)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    }
    else {
        arg = identifierConstant(compiler, &name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(compiler->parser, TOKEN_EQUAL_V1)) {
        checkMutability(compiler, arg, setOp);
        expression(compiler);
        emitBytes(compiler, setOp, (uint8_t)arg);
    }
    else {
        emitBytes(compiler, getOp, (uint8_t)arg);
    }
}

static void variable(CompilerV1* compiler, bool canAssign) {
    namedVariable(compiler, compiler->parser->previous, canAssign);
}

static void klass(CompilerV1* compiler, bool canAssign) {
    behavior(compiler, BEHAVIOR_CLASS, synthesizeToken("@"));
}

static void trait(CompilerV1* compiler, bool canAssign) {
    behavior(compiler, BEHAVIOR_TRAIT, synthesizeToken("@"));
}

static void namespace_(CompilerV1* compiler, bool canAssign) {
    consume(compiler->parser, TOKEN_IDENTIFIER_V1, "Expect Namespace identifier.");
    ObjString* name = copyString(compiler->parser->vm, compiler->parser->previous.start, compiler->parser->previous.length);
    emitBytes(compiler, OP_NAMESPACE, makeIdentifier(compiler, OBJ_VAL(name)));
}

static void super_(CompilerV1* compiler, bool canAssign) {
    if (compiler->parser->vm->currentClass == NULL) {
        error(compiler->parser, "Cannot use 'super' outside of a class.");
    }
    else {
        consume(compiler->parser, TOKEN_DOT_V1, "Expect '.' after 'super'.");
        consume(compiler->parser, TOKEN_IDENTIFIER_V1, "Expect superclass method name.");
        uint8_t name = identifierConstant(compiler, &compiler->parser->previous);

        namedVariable(compiler, synthesizeToken("this"), false);
        if (match(compiler->parser, TOKEN_LEFT_PAREN_V1)) {
            uint8_t argCount = argumentList(compiler);
            namedVariable(compiler, compiler->parser->vm->currentClass->superclass, false);
            emitBytes(compiler, OP_SUPER_INVOKE, name);
            emitByte(compiler, argCount);
        }
        else {
            namedVariable(compiler, compiler->parser->vm->currentClass->superclass, false);
            emitBytes(compiler, OP_GET_SUPER, name);
        }
    }
}

static void this_(CompilerV1* compiler, bool canAssign) {
    if (compiler->parser->vm->currentClass == NULL) {
        error(compiler->parser, "Cannot use 'this' outside of a class.");
        return;
    }
    variable(compiler, false);
}

static void unary(CompilerV1* compiler, bool canAssign) {
    TokenSymbolV1 operatorType = compiler->parser->previous.type;
    parsePrecedence(compiler, PREC_UNARY);

    switch (operatorType) {
        case TOKEN_BANG_V1: emitByte(compiler, OP_NOT); break;
        case TOKEN_MINUS_V1: emitByte(compiler, OP_NEGATE); break;
        default: return;
    }
}

static void yield(CompilerV1* compiler, bool canAssign) {
    if (compiler->type == TYPE_SCRIPT) {
        error(compiler->parser, "Can't yield from top-level code.");
    }
    else if (compiler->type == TYPE_INITIALIZER) {
        error(compiler->parser, "Cannot yield from an initializer.");
    }

    compiler->function->isGenerator = true;
    if (match(compiler->parser, TOKEN_RIGHT_PAREN_V1) || match(compiler->parser, TOKEN_RIGHT_BRACKET_V1) || match(compiler->parser, TOKEN_RIGHT_BRACE_V1)
        || match(compiler->parser, TOKEN_COMMA_V1) || match(compiler->parser, TOKEN_SEMICOLON_V1))
    {
        emitBytes(compiler, OP_NIL, OP_YIELD);
    }
    else if (match(compiler->parser, TOKEN_WITH_V1)) {
        expression(compiler);
        emitByte(compiler, OP_YIELD_FROM);
    }
    else {
        expression(compiler);
        emitByte(compiler, OP_YIELD);
    }
}

static void async(CompilerV1* compiler, bool canAssign) {
    if (match(compiler->parser, TOKEN_FUN_V1)) {
        function(compiler, TYPE_FUNCTION, true);
    }
    else if (match(compiler->parser, TOKEN_LEFT_BRACE_V1)) {
        function(compiler, TYPE_LAMBDA, true);
    }
    else {
        error(compiler->parser, "Can only use async as expression modifier for anonymous functions or lambda.");
    }
}

static void await(CompilerV1* compiler, bool canAssign) {
    if (compiler->type == TYPE_SCRIPT) {
        compiler->isAsync = true;
        compiler->function->isAsync = true;
    }
    else if (!compiler->isAsync) error(compiler->parser, "Cannot use await unless in top level code or inside async functions/methods.");
    expression(compiler);
    emitByte(compiler, OP_AWAIT);
}

ParseRuleV1 rules[] = {
    [TOKEN_LEFT_PAREN_V1]       = {grouping,      call,        PREC_CALL},
    [TOKEN_RIGHT_PAREN_V1]      = {NULL,          NULL,        PREC_NONE},
    [TOKEN_LEFT_BRACKET_V1]     = {collection,    subscript,   PREC_CALL},
    [TOKEN_RIGHT_BRACKET_V1]    = {NULL,          NULL,        PREC_NONE},
    [TOKEN_LEFT_BRACE_V1]       = {lambda,        NULL,        PREC_NONE},
    [TOKEN_RIGHT_BRACE_V1]      = {NULL,          NULL,        PREC_NONE},
    [TOKEN_COLON_V1]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_COMMA_V1]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_MINUS_V1]            = {unary,         binary,      PREC_TERM},
    [TOKEN_MODULO_V1]           = {NULL,          binary,      PREC_FACTOR},
    [TOKEN_PIPE_V1]             = {NULL,          NULL,        PREC_NONE},
    [TOKEN_PLUS_V1]             = {NULL,          binary,      PREC_TERM},
    [TOKEN_QUESTION_V1]         = {NULL,          question,    PREC_CALL},
    [TOKEN_SEMICOLON_V1]        = {NULL,          NULL,        PREC_NONE},
    [TOKEN_SLASH_V1]            = {NULL,          binary,      PREC_FACTOR},
    [TOKEN_STAR_V1]             = {NULL,          binary,      PREC_FACTOR},
    [TOKEN_BANG_V1]             = {unary,         NULL,        PREC_NONE},
    [TOKEN_BANG_EQUAL_V1]       = {NULL,          binary,      PREC_EQUALITY},
    [TOKEN_EQUAL_V1]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_EQUAL_EQUAL_V1]      = {NULL,          binary,      PREC_EQUALITY},
    [TOKEN_GREATER_V1]          = {NULL,          binary,      PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL_V1]    = {NULL,          binary,      PREC_COMPARISON},
    [TOKEN_LESS_V1]             = {NULL,          binary,      PREC_COMPARISON},
    [TOKEN_LESS_EQUAL_V1]       = {NULL,          binary,      PREC_COMPARISON},
    [TOKEN_DOT_V1]              = {NULL,          dot,         PREC_CALL},
    [TOKEN_DOT_DOT_V1]          = {NULL,          binary,      PREC_CALL},
    [TOKEN_IDENTIFIER_V1]       = {variable,      NULL,        PREC_NONE},
    [TOKEN_STRING_V1]           = {string,        NULL,        PREC_NONE},
    [TOKEN_INTERPOLATION_V1]    = {interpolation, NULL,        PREC_NONE},
    [TOKEN_NUMBER_V1]           = {number,        NULL,        PREC_NONE},
    [TOKEN_INT_V1]              = {integer,       NULL,        PREC_NONE},
    [TOKEN_AND_V1]              = {NULL,          and_,        PREC_AND},
    [TOKEN_AS_V1]               = {NULL,          NULL,        PREC_NONE},
    [TOKEN_ASYNC_V1]            = {async,         NULL,        PREC_NONE},
    [TOKEN_AWAIT_V1]            = {await,         NULL,        PREC_NONE},
    [TOKEN_BREAK_V1]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_CASE_V1]             = {NULL,          NULL,        PREC_NONE},
    [TOKEN_CATCH_V1]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_CLASS_V1]            = {klass,         NULL,        PREC_NONE},
    [TOKEN_CONTINUE_V1]         = {NULL,          NULL,        PREC_NONE},
    [TOKEN_DEFAULT_V1]          = {NULL,          NULL,        PREC_NONE},
    [TOKEN_ELSE_V1]             = {NULL,          NULL,        PREC_NONE},
    [TOKEN_FALSE_V1]            = {literal,       NULL,        PREC_NONE},
    [TOKEN_FINALLY_V1]          = {NULL,          NULL,        PREC_NONE},
    [TOKEN_FOR_V1]              = {NULL,          NULL,        PREC_NONE},
    [TOKEN_FUN_V1]              = {closure,       NULL,        PREC_NONE},
    [TOKEN_IF_V1]               = {NULL,          NULL,        PREC_NONE},
    [TOKEN_NAMESPACE_V1]        = {NULL,          NULL,        PREC_NONE},
    [TOKEN_NIL_V1]              = {literal,       NULL,        PREC_NONE},
    [TOKEN_OR_V1]               = {NULL,          or_,         PREC_OR},
    [TOKEN_REQUIRE_V1]          = {NULL,          NULL,        PREC_NONE},
    [TOKEN_RETURN_V1]           = {NULL,          NULL,        PREC_NONE},
    [TOKEN_SUPER_V1]            = {super_,        NULL,        PREC_NONE},
    [TOKEN_SWITCH_V1]           = {NULL,          NULL,        PREC_NONE},
    [TOKEN_THIS_V1]             = {this_,         NULL,        PREC_NONE},
    [TOKEN_THROW_V1]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_TRAIT_V1]            = {trait,         NULL,        PREC_NONE},
    [TOKEN_TRUE_V1]             = {literal,       NULL,        PREC_NONE},
    [TOKEN_TRY_V1]              = {NULL,          NULL,        PREC_NONE},
    [TOKEN_USING_V1]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_VAL_V1]              = {NULL,          NULL,        PREC_NONE},
    [TOKEN_VAR_V1]              = {NULL,          NULL,        PREC_NONE},
    [TOKEN_WHILE_V1]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_WITH_V1]             = {NULL,          NULL,        PREC_NONE},
    [TOKEN_YIELD_V1]            = {yield,         NULL,        PREC_NONE},
    [TOKEN_ERROR_V1]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_EOF_V1]              = {NULL,          NULL,        PREC_NONE},
};

static void parsePrecedence(CompilerV1* compiler, PrecedenceV1 precedence) {
    advance(compiler->parser);
    ParseFn prefixRule = getRule(compiler->parser->previous.type)->prefix;
    if (prefixRule == NULL) {
        error(compiler->parser, "Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(compiler, canAssign);

    while (precedence <= getRule(compiler->parser->current.type)->precedence) {
        advance(compiler->parser);
        ParseFn infixRule = getRule(compiler->parser->previous.type)->infix;
        infixRule(compiler, canAssign);
    }

    if (canAssign && match(compiler->parser, TOKEN_EQUAL_V1)) {
        error(compiler->parser, "Invalid assignment target.");
    }
}

static ParseRuleV1* getRule(TokenSymbolV1 type) {
    return &rules[type];
}

static void expression(CompilerV1* compiler) {
    parsePrecedence(compiler, PREC_ASSIGNMENT);
}

static void block(CompilerV1* compiler) {
    while (!check(compiler->parser, TOKEN_RIGHT_BRACE_V1) && !check(compiler->parser, TOKEN_EOF_V1)) {
        declaration(compiler);
    }
    consume(compiler->parser, TOKEN_RIGHT_BRACE_V1, "Expect '}' after block.");
}

static void functionParameters(CompilerV1* compiler) {
    consume(compiler->parser, TOKEN_LEFT_PAREN_V1, "Expect '(' after function keyword/name.");
    if (!check(compiler->parser, TOKEN_RIGHT_PAREN_V1)) {
        parameterList(compiler);
    }
    consume(compiler->parser, TOKEN_RIGHT_PAREN_V1, "Expect ')' after parameters.");
    consume(compiler->parser, TOKEN_LEFT_BRACE_V1, "Expect '{' before function body.");
}

static void lambdaParameters(CompilerV1* compiler) {
    if (!match(compiler->parser, TOKEN_PIPE_V1)) return;
    if (!check(compiler->parser, TOKEN_PIPE_V1)) {
        parameterList(compiler);
    }
    consume(compiler->parser, TOKEN_PIPE_V1, "Expect '|' after lambda parameters.");
}

static uint8_t lambdaDepth(CompilerV1* compiler) {
    uint8_t depth = 1;
    CompilerV1* current = compiler->enclosing;
    while (current->type == TYPE_LAMBDA) {
        depth++;
        current = current->enclosing;
    }
    return depth;
}

static void function(CompilerV1* enclosing, FunctionType type, bool isAsync) {
    CompilerV1 compiler;
    initCompiler(&compiler, enclosing->parser, enclosing, type, isAsync);
    beginScope(&compiler);

    if (type == TYPE_LAMBDA) lambdaParameters(&compiler);
    else functionParameters(&compiler);

    block(&compiler);
    ObjFunction* function = endCompiler(&compiler);
    emitBytes(enclosing, OP_CLOSURE, makeIdentifier(enclosing, OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(enclosing, compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(enclosing, compiler.upvalues[i].index);
    }
}

static void method(CompilerV1* compiler) {
    bool isAsync = false;
    if (match(compiler->parser, TOKEN_ASYNC_V1)) isAsync = true;
    uint8_t opCode = match(compiler->parser, TOKEN_CLASS_V1) ? OP_CLASS_METHOD : OP_INSTANCE_METHOD;
    uint8_t constant = propertyConstant(compiler, "Expect method name.");

    FunctionType type = TYPE_METHOD;
    if (compiler->parser->previous.length == 8 && memcmp(compiler->parser->previous.start, "__init__", 8) == 0) {
        type = TYPE_INITIALIZER;
    }

    function(compiler, type, isAsync);
    emitBytes(compiler, opCode, constant);
}

static void methods(CompilerV1* compiler) {
    consume(compiler->parser, TOKEN_LEFT_BRACE_V1, "Expect '{' before class/trait body.");
    while (!check(compiler->parser, TOKEN_RIGHT_BRACE_V1) && !check(compiler->parser, TOKEN_EOF_V1)) {
        method(compiler);
    }
    consume(compiler->parser, TOKEN_RIGHT_BRACE_V1, "Expect '}' after class/trait body.");
}

static uint8_t traits(CompilerV1* compiler, TokenV1* name) {
    uint8_t traitCount = 0;

    do {
        traitCount++;
        if (compiler->function->arity > UINT4_MAX) {
            errorAtCurrent(compiler->parser, "Can't have more than 15 parameters.");
        }

        consume(compiler->parser, TOKEN_IDENTIFIER_V1, "Expect class/trait name.");
        variable(compiler, false);
    } while (match(compiler->parser, TOKEN_COMMA_V1));

    return traitCount;
}

static void behavior(CompilerV1* compiler, BehaviorType type, TokenV1 name) {
    bool isAnonymous = (name.type != TOKEN_IDENTIFIER_V1 && name.length == 1);
    if (isAnonymous) {
        emitBytes(compiler, OP_ANONYMOUS, type);
        emitByte(compiler, OP_DUP);
    }

    ClassCompilerV1* enclosingClass = compiler->parser->vm->currentClass;
    ClassCompilerV1 classCompiler = { .name = name, .enclosing = enclosingClass, .type = type, .superclass = compiler->parser->rootClass };
    compiler->parser->vm->currentClass = &classCompiler;

    if (type == BEHAVIOR_CLASS) {
        if (match(compiler->parser, TOKEN_LESS_V1)) {
            consume(compiler->parser, TOKEN_IDENTIFIER_V1, "Expect super class name.");
            classCompiler.superclass = compiler->parser->previous;
            variable(compiler, false);
            if (identifiersEqual(&name, &compiler->parser->previous)) {
                error(compiler->parser, "A class cannot inherit from itself.");
            }
        }
        else {
            namedVariable(compiler, compiler->parser->rootClass, false);
            if (identifiersEqual(&name, &compiler->parser->rootClass)) {
                error(compiler->parser, "Cannot redeclare root class Object.");
            }
        }
    }

    beginScope(compiler);
    addLocal(compiler, synthesizeToken("super"));
    defineVariable(compiler, 0, false);

    if (type == BEHAVIOR_CLASS) emitByte(compiler, OP_INHERIT);
    uint8_t traitCount = match(compiler->parser, TOKEN_WITH_V1) ? traits(compiler, &name) : 0;
    if (traitCount > 0) emitBytes(compiler, OP_IMPLEMENT, traitCount);

    methods(compiler);
    endScope(compiler);
    compiler->parser->vm->currentClass = enclosingClass;
}

static void classDeclaration(CompilerV1* compiler) {
    consume(compiler->parser, TOKEN_IDENTIFIER_V1, "Expect class name.");
    TokenV1 className = compiler->parser->previous;
    uint8_t nameConstant = identifierConstant(compiler, &compiler->parser->previous);

    declareVariable(compiler);
    emitBytes(compiler, OP_CLASS, nameConstant);
    behavior(compiler, BEHAVIOR_CLASS, className);
}

static void funDeclaration(CompilerV1* compiler, bool isAsync) {
    uint8_t global = parseVariable(compiler, "Expect function name.");
    markInitialized(compiler, false);
    function(compiler, TYPE_FUNCTION, isAsync);
    defineVariable(compiler, global, false);
}

static void namespaceDeclaration(CompilerV1* compiler) {
    uint8_t namespaceDepth = 0;
    do {
        if (namespaceDepth > UINT4_MAX) {
            errorAtCurrent(compiler->parser, "Can't have more than 15 levels of namespace depth.");
        }
        namespace_(compiler, false);
        namespaceDepth++;
    } while (match(compiler->parser, TOKEN_DOT_V1));

    consume(compiler->parser, TOKEN_SEMICOLON_V1, "Expect semicolon after namespace declaration.");
    emitBytes(compiler, OP_DECLARE_NAMESPACE, namespaceDepth);
}

static void traitDeclaration(CompilerV1* compiler) {
    consume(compiler->parser, TOKEN_IDENTIFIER_V1, "Expect trait name.");
    TokenV1 traitName = compiler->parser->previous;
    uint8_t nameConstant = identifierConstant(compiler, &compiler->parser->previous);

    declareVariable(compiler);
    emitBytes(compiler, OP_TRAIT, nameConstant);
    behavior(compiler, BEHAVIOR_TRAIT, traitName);
}

static void varDeclaration(CompilerV1* compiler, bool isMutable) {
    uint8_t global = parseVariable(compiler, "Expect variable name.");

    if (!isMutable && !check(compiler->parser, TOKEN_EQUAL_V1)) {
        error(compiler->parser, "Immutable variable must be initialized upon declaration.");
    }
    else if (match(compiler->parser, TOKEN_EQUAL_V1)) {
        expression(compiler);
    }
    else {
        emitByte(compiler, OP_NIL);
    }
    consume(compiler->parser, TOKEN_SEMICOLON_V1, "Expect ';' after variable declaration.");

    defineVariable(compiler, global, isMutable);
}

static void awaitStatement(CompilerV1* compiler) {
    if (compiler->type == TYPE_SCRIPT) {
        compiler->isAsync = true;
        compiler->function->isAsync = true;
    }
    else if (!compiler->isAsync) error(compiler->parser, "Can only use 'await' in async methods or top level code.");
    
    expression(compiler);
    consume(compiler->parser, TOKEN_SEMICOLON_V1, "Expect ';' after await value.");
    emitBytes(compiler, OP_AWAIT, OP_POP);
}

static void breakStatement(CompilerV1* compiler) {
    if (compiler->innermostLoopStart == -1) {
        error(compiler->parser, "Cannot use 'break' outside of a loop.");
    }
    consume(compiler->parser, TOKEN_SEMICOLON_V1, "Expect ';' after 'break'.");

    discardLocals(compiler);
    emitJump(compiler, OP_END);
}

static void continueStatement(CompilerV1* compiler) {
    if (compiler->innermostLoopStart == -1) {
        error(compiler->parser, "Cannot use 'continue' outside of a loop.");
    }
    consume(compiler->parser, TOKEN_SEMICOLON_V1, "Expect ';' after 'continue'.");

    discardLocals(compiler);
    emitLoop(compiler, compiler->innermostLoopStart);
}

static void expressionStatement(CompilerV1* compiler) {
    expression(compiler);
    if (compiler->type == TYPE_LAMBDA && !check(compiler->parser, TOKEN_SEMICOLON_V1)) {
        emitByte(compiler, OP_RETURN);
    }
    else {
        consume(compiler->parser, TOKEN_SEMICOLON_V1, "Expect ';' after expression.");
        emitByte(compiler, OP_POP);
    }
}

static void forStatement(CompilerV1* compiler) {
    beginScope(compiler);
    TokenV1 indexToken, valueToken;
    consume(compiler->parser, TOKEN_LEFT_PAREN_V1, "Expect '(' after 'for'.");
    consume(compiler->parser, TOKEN_VAR_V1, "Expect 'var' keyword after '(' in For loop.");
    if (match(compiler->parser, TOKEN_LEFT_PAREN_V1)) {
        consume(compiler->parser, TOKEN_IDENTIFIER_V1, "Expect first variable name after '('.");
        indexToken = compiler->parser->previous;
        consume(compiler->parser, TOKEN_COMMA_V1, "Expect ',' after first variable declaration.");
        consume(compiler->parser, TOKEN_IDENTIFIER_V1, "Expect second variable name after ','.");
        valueToken = compiler->parser->previous;
        consume(compiler->parser, TOKEN_RIGHT_PAREN_V1, "Expect ')' after second variable declaration.");
    }
    else {
        consume(compiler->parser, TOKEN_IDENTIFIER_V1, "Expect variable name after 'var'.");
        indexToken = synthesizeToken("index ");
        valueToken = compiler->parser->previous;
    }

    consume(compiler->parser, TOKEN_COLON_V1, "Expect ':' after variable name.");
    expression(compiler);
    if (compiler->localCount + 3 > UINT8_MAX) {
        error(compiler->parser, "for loop can only contain up to 252 variables.");
    }

    int collectionSlot = addLocal(compiler, synthesizeToken("collection "));
    emitByte(compiler, OP_NIL);
    int indexSlot = addLocal(compiler, indexToken);
    markInitialized(compiler, true);
    consume(compiler->parser, TOKEN_RIGHT_PAREN_V1, "Expect ')' after loop expression.");

    int loopStart = compiler->innermostLoopStart;
    int scopeDepth = compiler->innermostLoopScopeDepth;
    compiler->innermostLoopStart = currentChunk(compiler)->count;
    compiler->innermostLoopScopeDepth = compiler->scopeDepth;

    getLocal(compiler, collectionSlot);
    getLocal(compiler, indexSlot);
    invokeMethod(compiler, 1, "next", 4);
    setLocal(compiler, indexSlot);
    emitByte(compiler, OP_POP);
    int exitJump = emitJump(compiler, OP_JUMP_IF_EMPTY);

    getLocal(compiler, collectionSlot);
    getLocal(compiler, indexSlot);
    invokeMethod(compiler, 1, "nextValue", 9);

    beginScope(compiler);
    int valueSlot = addLocal(compiler, valueToken);
    markInitialized(compiler, false);
    setLocal(compiler, valueSlot);
    statement(compiler);
    endScope(compiler);

    emitLoop(compiler, compiler->innermostLoopStart);
    patchJump(compiler, exitJump);
    endLoop(compiler);
    emitByte(compiler, OP_POP);
    emitByte(compiler, OP_POP);

    compiler->localCount -= 2;
    compiler->innermostLoopStart = loopStart;
    compiler->innermostLoopScopeDepth = scopeDepth;
    endScope(compiler);
}

static void ifStatement(CompilerV1* compiler) {
    consume(compiler->parser, TOKEN_LEFT_PAREN_V1, "Expect '(' after 'if'.");
    expression(compiler);
    consume(compiler->parser, TOKEN_RIGHT_PAREN_V1, "Expect ')' after condition.");

    int thenJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    emitByte(compiler, OP_POP);
    statement(compiler);

    int elseJump = emitJump(compiler, OP_JUMP);
    patchJump(compiler, thenJump);
    emitByte(compiler, OP_POP);

    if (match(compiler->parser, TOKEN_ELSE_V1)) statement(compiler);
    patchJump(compiler, elseJump);
}

static void requireStatement(CompilerV1* compiler) {
    if (compiler->type != TYPE_SCRIPT) {
        error(compiler->parser, "Can only require source files from top-level code.");
    }

    expression(compiler);
    consume(compiler->parser, TOKEN_SEMICOLON_V1, "Expect ';' after required file path.");
    emitByte(compiler, OP_REQUIRE);
}

static void returnStatement(CompilerV1* compiler) {
    if (compiler->type == TYPE_SCRIPT) {
        error(compiler->parser, "Can't return from top-level code.");
    }

    uint8_t depth = 0;
    if (compiler->type == TYPE_LAMBDA) depth = lambdaDepth(compiler);

    if (match(compiler->parser, TOKEN_SEMICOLON_V1)) {
        emitReturn(compiler, depth);
    }
    else {
        if (compiler->type == TYPE_INITIALIZER) {
            error(compiler->parser, "Cannot return value from an initializer.");
        }

        expression(compiler);
        consume(compiler->parser, TOKEN_SEMICOLON_V1, "Expect ';' after return value.");

        if (compiler->type == TYPE_LAMBDA) {
            emitBytes(compiler, OP_RETURN_NONLOCAL, depth);
        }
        else {
            emitByte(compiler, OP_RETURN);
        }
    }
}

static void switchStatement(CompilerV1* compiler) {
    consume(compiler->parser, TOKEN_LEFT_PAREN_V1, "Expect '(' after 'switch'.");
    expression(compiler);
    consume(compiler->parser, TOKEN_RIGHT_PAREN_V1, "Expect ')' after value.");
    consume(compiler->parser, TOKEN_LEFT_BRACE_V1, "Expect '{' before switch cases.");

    int state = 0;
    int caseEnds[MAX_CASES];
    int caseCount = 0;
    int previousCaseSkip = -1;

    while (!match(compiler->parser, TOKEN_RIGHT_BRACE_V1) && !check(compiler->parser, TOKEN_EOF_V1)) {
        if (match(compiler->parser, TOKEN_CASE_V1) || match(compiler->parser, TOKEN_DEFAULT_V1)) {
            TokenSymbolV1 caseType = compiler->parser->previous.type;
            if (state == 2) {
                error(compiler->parser, "Can't have another case or default after the default case.");
            }

            if (state == 1) {
                caseEnds[caseCount++] = emitJump(compiler, OP_JUMP);
                patchJump(compiler, previousCaseSkip);
                emitByte(compiler, OP_POP);
            }

            if (caseType == TOKEN_CASE_V1) {
                state = 1;
                emitByte(compiler, OP_DUP);
                expression(compiler);

                consume(compiler->parser, TOKEN_COLON_V1, "Expect ':' after case value.");
                emitByte(compiler, OP_EQUAL);
                previousCaseSkip = emitJump(compiler, OP_JUMP_IF_FALSE);
                emitByte(compiler, OP_POP);
            }
            else {
                state = 2;
                consume(compiler->parser, TOKEN_COLON_V1, "Expect ':' after default.");
                previousCaseSkip = -1;
            }
        }
        else {
            if (state == 0) {
                error(compiler->parser, "Can't have statements before any case.");
            }
            statement(compiler);
        }
    }

    if (state == 1) {
        caseEnds[caseCount++] = emitJump(compiler, OP_JUMP);
        patchJump(compiler, previousCaseSkip);
        emitByte(compiler, OP_POP);
    }

    for (int i = 0; i < caseCount; i++) {
        patchJump(compiler, caseEnds[i]);
    }

    emitByte(compiler, OP_POP);
}

static void throwStatement(CompilerV1* compiler) {
    expression(compiler);
    consume(compiler->parser, TOKEN_SEMICOLON_V1, "Expect ';' after thrown exception object.");
    emitByte(compiler, OP_THROW);
}

static void tryStatement(CompilerV1* compiler) {
    emitByte(compiler, OP_TRY);
    int exceptionType = currentChunk(compiler)->count;
    emitByte(compiler, 0xff);
    int handlerAddress = currentChunk(compiler)->count;
    emitBytes(compiler, 0xff, 0xff);
    int finallyAddress = currentChunk(compiler)->count;
    emitBytes(compiler, 0xff, 0xff);
    statement(compiler);
    emitByte(compiler, OP_CATCH);
    int catchJump = emitJump(compiler, OP_JUMP);

    if (match(compiler->parser, TOKEN_CATCH_V1)) {
        beginScope(compiler);
        consume(compiler->parser, TOKEN_LEFT_PAREN_V1, "Expect '(' after catch");
        consume(compiler->parser, TOKEN_IDENTIFIER_V1, "Expect type name to catch");
        uint8_t name = identifierConstant(compiler, &compiler->parser->previous);
        currentChunk(compiler)->code[exceptionType] = name;
        patchAddress(compiler, handlerAddress);

        if (check(compiler->parser, TOKEN_IDENTIFIER_V1)) {
            consume(compiler->parser, TOKEN_IDENTIFIER_V1, "Expect identifier after exception type.");
            addLocal(compiler, compiler->parser->previous);
            markInitialized(compiler, false);
            uint8_t variable = resolveLocal(compiler, &compiler->parser->previous);
            emitBytes(compiler, OP_SET_LOCAL, variable);
        }

        consume(compiler->parser, TOKEN_RIGHT_PAREN_V1, "Expect ')' after catch statement");
        emitByte(compiler, OP_CATCH);
        statement(compiler);
        endScope(compiler);
    }
    else {
        errorAtCurrent(compiler->parser, "Must have a catch statement following a try statement.");
    }
    patchJump(compiler, catchJump);

    if (match(compiler->parser, TOKEN_FINALLY_V1)) {
        emitByte(compiler, OP_FALSE);
        patchAddress(compiler, finallyAddress);
        statement(compiler);

        int finallyJump = emitJump(compiler, OP_JUMP_IF_FALSE);
        emitByte(compiler, OP_POP);
        emitByte(compiler, OP_FINALLY);
        patchJump(compiler, finallyJump);
        emitByte(compiler, OP_POP);
    }
}

static void usingStatement(CompilerV1* compiler) {
    uint8_t namespaceDepth = 0;
    do {
        consume(compiler->parser, TOKEN_IDENTIFIER_V1, "Expect namespace identifier.");
        uint8_t namespace = identifierConstant(compiler, &compiler->parser->previous);
        emitBytes(compiler, OP_NAMESPACE, namespace);
        namespaceDepth++;
    } while (match(compiler->parser, TOKEN_DOT_V1));

    emitBytes(compiler, OP_GET_NAMESPACE, namespaceDepth);
    uint8_t alias = makeIdentifier(compiler, OBJ_VAL(emptyString(compiler->parser->vm)));

    if (match(compiler->parser, TOKEN_AS_V1)) {
        consume(compiler->parser, TOKEN_IDENTIFIER_V1, "Expect alias after 'as'.");
        TokenV1 name = compiler->parser->previous;
        alias = identifierConstant(compiler, &name);
    }
    consume(compiler->parser, TOKEN_SEMICOLON_V1, "Expect ';' after using statement.");
    emitBytes(compiler, OP_USING_NAMESPACE, alias);
}

static void whileStatement(CompilerV1* compiler) {
    int loopStart = compiler->innermostLoopStart;
    int scopeDepth = compiler->innermostLoopScopeDepth;
    compiler->innermostLoopStart = currentChunk(compiler)->count;
    compiler->innermostLoopScopeDepth = compiler->scopeDepth;

    consume(compiler->parser, TOKEN_LEFT_PAREN_V1, "Expect '(' after 'while'.");
    expression(compiler);
    consume(compiler->parser, TOKEN_RIGHT_PAREN_V1, "Expect ')' after condition.");

    int exitJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    emitByte(compiler, OP_POP);
    statement(compiler);
    emitLoop(compiler, compiler->innermostLoopStart);

    patchJump(compiler, exitJump);
    emitByte(compiler, OP_POP);

    endLoop(compiler);
    compiler->innermostLoopStart = loopStart;
    compiler->innermostLoopScopeDepth = scopeDepth;
}

static void yieldStatement(CompilerV1* compiler) {
    if (compiler->type == TYPE_SCRIPT) {
        error(compiler->parser, "Can't yield from top-level code.");
    }
    else if (compiler->type == TYPE_INITIALIZER) {
        error(compiler->parser, "Cannot yield from an initializer.");
    }

    compiler->function->isGenerator = true;
    if (match(compiler->parser, TOKEN_SEMICOLON_V1)) {
        emitBytes(compiler, OP_YIELD, OP_POP);
    }
    else if (match(compiler->parser, TOKEN_WITH_V1)) {
        expression(compiler);
        consume(compiler->parser, TOKEN_SEMICOLON_V1, "Expect ';' after yield value.");
        emitByte(compiler, OP_YIELD_FROM);
    }
    else {
        expression(compiler);
        consume(compiler->parser, TOKEN_SEMICOLON_V1, "Expect ';' after yield value.");
        emitBytes(compiler, OP_YIELD, OP_POP);
    }
}

static void declaration(CompilerV1* compiler) {
    if (check(compiler->parser, TOKEN_ASYNC_V1) && checkNext(compiler->parser, TOKEN_FUN_V1)) {
        advance(compiler->parser);
        advance(compiler->parser);
        funDeclaration(compiler, true);
    }
    else if (check(compiler->parser, TOKEN_CLASS_V1) && checkNext(compiler->parser, TOKEN_IDENTIFIER_V1)) {
        advance(compiler->parser);
        classDeclaration(compiler);
    }
    else if (check(compiler->parser, TOKEN_FUN_V1) && checkNext(compiler->parser, TOKEN_IDENTIFIER_V1)) {
        advance(compiler->parser);
        funDeclaration(compiler, false);
    }
    else if (match(compiler->parser, TOKEN_NAMESPACE_V1)) {
        namespaceDeclaration(compiler);
    }
    else if (check(compiler->parser, TOKEN_TRAIT_V1) && checkNext(compiler->parser, TOKEN_IDENTIFIER_V1)) {
        advance(compiler->parser);
        traitDeclaration(compiler);
    }
    else if (match(compiler->parser, TOKEN_VAL_V1)) {
        varDeclaration(compiler, false);
    }
    else if (match(compiler->parser, TOKEN_VAR_V1)) {
        varDeclaration(compiler, true);
    }
    else {
        statement(compiler);
    }

    if (compiler->parser->panicMode) {
        synchronize(compiler->parser);
    }
}

static void statement(CompilerV1* compiler) {
    if (match(compiler->parser, TOKEN_AWAIT_V1)) {
        awaitStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_BREAK_V1)) {
        breakStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_CONTINUE_V1)) {
        continueStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_FOR_V1)) {
        forStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_IF_V1)) {
        ifStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_REQUIRE_V1)) {
        requireStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_RETURN_V1)) {
        returnStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_SWITCH_V1)) {
        switchStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_THROW_V1)) {
        throwStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_TRY_V1)) {
        tryStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_USING_V1)) {
        usingStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_WHILE_V1)) {
        whileStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_YIELD_V1)) {
        yieldStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_LEFT_BRACE_V1)) {
        beginScope(compiler);
        block(compiler);
        endScope(compiler);
    }
    else {
        expressionStatement(compiler);
    }
}

ObjFunction* compileV1(VM* vm, const char* source) {
    Scanner scanner;
    initScanner(&scanner, source);

    ParserV1 parser;
    initParserV1(&parser, vm, &scanner);

    CompilerV1 compiler;
    initCompiler(&compiler, &parser, NULL, TYPE_SCRIPT, false);

    advance(&parser);
    advance(&parser);
    while (!match(&parser, TOKEN_EOF_V1)) {
        declaration(&compiler);
    }

    ObjFunction* function = endCompiler(&compiler);
    return parser.hadError ? NULL : function;
}

void markCompilerRoots(VM* vm) {
    CompilerV1* compiler = vm->currentCompiler;
    while (compiler != NULL) {
        markObject(vm, (Obj*)compiler->function);
        markIDMap(vm, &compiler->indexes);
        compiler = compiler->enclosing;
    }
}