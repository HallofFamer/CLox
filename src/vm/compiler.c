#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "debug.h"
#include "id.h"
#include "memory.h"
#include "parser.h"
#include "scanner.h"
#include "string.h"

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
} Precedence;

typedef void (*ParseFn)(Compiler* compiler, bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name;
    int depth;
    bool isCaptured;
    bool isMutable;
} Local;

typedef struct {
    uint8_t index;
    bool isLocal;
    bool isMutable;
} Upvalue;

typedef enum {
    TYPE_FUNCTION,
    TYPE_INITIALIZER,
    TYPE_LAMBDA,
    TYPE_METHOD,
    TYPE_SCRIPT
} FunctionType;

struct Compiler {
    struct Compiler* enclosing;
    Parser* parser;
    ObjFunction* function;
    FunctionType type;

    Local locals[UINT8_COUNT];
    int localCount;
    Upvalue upvalues[UINT8_COUNT];
    IDMap indexes;

    int scopeDepth;
    int innermostLoopStart;
    int innermostLoopScopeDepth;
    bool isAsync;
};

struct ClassCompiler {
    struct ClassCompiler* enclosing;
    Token name;
    Token superclass;
    BehaviorType type;
};

static Chunk* currentChunk(Compiler* compiler) {
    return &compiler->function->chunk;
}

static void emitByte(Compiler* compiler, uint8_t byte) {
    writeChunk(compiler->parser->vm, currentChunk(compiler), byte, compiler->parser->previous.line);
}

static void emitBytes(Compiler* compiler, uint8_t byte1, uint8_t byte2) {
    emitByte(compiler, byte1);
    emitByte(compiler, byte2);
}

static int emitJump(Compiler* compiler, uint8_t instruction) {
    emitByte(compiler, instruction);
    emitByte(compiler, 0xff);
    emitByte(compiler, 0xff);
    return currentChunk(compiler)->count - 2;
}

static void emitLoop(Compiler* compiler, int loopStart) {
    emitByte(compiler, OP_LOOP);

    int offset = currentChunk(compiler)->count - loopStart + 2;
    if (offset > UINT16_MAX) error(compiler->parser, "Loop body too large.");

    emitByte(compiler, (offset >> 8) & 0xff);
    emitByte(compiler, offset & 0xff);
}

static void emitReturn(Compiler* compiler, uint8_t depth) {
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

static uint8_t makeConstant(Compiler* compiler, Value value) {
    int constant = addConstant(compiler->parser->vm, currentChunk(compiler), value);
    if (constant > UINT8_MAX) {
        error(compiler->parser, "Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static void emitConstant(Compiler* compiler, Value value) {
    emitBytes(compiler, OP_CONSTANT, makeConstant(compiler, value));
}

static void patchJump(Compiler* compiler, int offset) {
    int jump = currentChunk(compiler)->count - offset - 2;
    if (jump > UINT16_MAX) {
        error(compiler->parser, "Too much code to jump over.");
    }

    currentChunk(compiler)->code[offset] = (jump >> 8) & 0xff;
    currentChunk(compiler)->code[offset + 1] = jump & 0xff;
}

static void patchAddress(Compiler* compiler, int offset) {
    currentChunk(compiler)->code[offset] = (currentChunk(compiler)->count >> 8) & 0xff;
    currentChunk(compiler)->code[offset + 1] = currentChunk(compiler)->count & 0xff;
}

static void endLoop(Compiler* compiler) {
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

static void initCompiler(Compiler* compiler, Parser* parser, Compiler* enclosing, FunctionType type, bool isAsync) {
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

    Local* local = &compiler->locals[compiler->localCount++];
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

static ObjFunction* endCompiler(Compiler* compiler) {
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

static void beginScope(Compiler* compiler) {
    compiler->scopeDepth++;
}

static void endScope(Compiler* compiler) {
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

static void expression(Compiler* compiler);
static void statement(Compiler* compiler);
static void block(Compiler* compiler);
static void behavior(Compiler* compiler, BehaviorType type, Token name);
static void function(Compiler* enclosing, FunctionType type, bool isAsync);
static void declaration(Compiler* compiler);
static ParseRule* getRule(TokenSymbol type);
static void parsePrecedence(Compiler* compiler, Precedence precedence);

static uint8_t makeIdentifier(Compiler* compiler, Value value) {
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

static uint8_t identifierConstant(Compiler* compiler, Token* name) {
    const char* start = name->start[0] != '`' ? name->start : name->start + 1;
    int length = name->start[0] != '`' ? name->length : name->length - 2;
    return makeIdentifier(compiler, OBJ_VAL(copyString(compiler->parser->vm, start, length)));
}

static ObjString* identifierName(Compiler* compiler, uint8_t arg) {
    return AS_STRING(currentChunk(compiler)->identifiers.values[arg]);
}

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static uint8_t propertyConstant(Compiler* compiler, const char* message) {
    switch (compiler->parser->current.type) {
        case TOKEN_IDENTIFIER:
        case TOKEN_EQUAL_EQUAL:
        case TOKEN_GREATER:
        case TOKEN_LESS:
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_STAR:
        case TOKEN_SLASH:
        case TOKEN_MODULO:
        case TOKEN_DOT_DOT:
            advance(compiler->parser);
            return identifierConstant(compiler, &compiler->parser->previous);
        case TOKEN_LEFT_BRACKET:
            advance(compiler->parser);
            if (match(compiler->parser, TOKEN_RIGHT_BRACKET)) {
                Token token = syntheticToken(match(compiler->parser, TOKEN_EQUAL) ? "[]=" : "[]");
                return identifierConstant(compiler, &token);
            }
            else {
                errorAtCurrent(compiler->parser, message);
                return -1;
            }
        case TOKEN_LEFT_PAREN:
            advance(compiler->parser);
            if (match(compiler->parser, TOKEN_RIGHT_PAREN)) {
                Token token = syntheticToken("()");
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

static int resolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error(compiler->parser, "Can't read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal, bool isMutable) {
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
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

static int resolveUpvalue(Compiler* compiler, Token* name) {
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

static int addLocal(Compiler* compiler, Token name) {
    if (compiler->localCount == UINT8_COUNT) {
        error(compiler->parser, "Too many local variables in function.");
        return -1;
    }

    Local* local = &compiler->locals[compiler->localCount++];
    local->name = name;
    local->depth = -1;
    local->isCaptured = false;
    local->isMutable = true;
    return compiler->localCount - 1;
}

static void getLocal(Compiler* compiler, int slot) {
    emitByte(compiler, OP_GET_LOCAL);
    emitByte(compiler, slot);
}

static void setLocal(Compiler* compiler, int slot) {
    emitByte(compiler, OP_SET_LOCAL);
    emitByte(compiler, slot);
}

static int discardLocals(Compiler* compiler) {
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

static void invokeMethod(Compiler* compiler, int args, const char* name, int length) {
    int slot = makeIdentifier(compiler, OBJ_VAL(copyString(compiler->parser->vm, name, length)));
    emitByte(compiler, OP_INVOKE);
    emitByte(compiler, slot);
    emitByte(compiler, args);
}

static void declareVariable(Compiler* compiler) {
    if (compiler->scopeDepth == 0) return;

    Token* name = &compiler->parser->previous;
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (local->depth != -1 && local->depth < compiler->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            error(compiler->parser, "Already a variable with this name in this scope.");
        }
    }
    addLocal(compiler, *name);
}

static uint8_t parseVariable(Compiler* compiler, const char* errorMessage) {
    consume(compiler->parser, TOKEN_IDENTIFIER, errorMessage);
    declareVariable(compiler);
    if (compiler->scopeDepth > 0) return 0;
    return identifierConstant(compiler, &compiler->parser->previous);
}

static void markInitialized(Compiler* compiler, bool isMutable) {
    if (compiler->scopeDepth == 0) return;
    compiler->locals[compiler->localCount - 1].depth = compiler->scopeDepth;
    compiler->locals[compiler->localCount - 1].isMutable = isMutable;
}

static void defineVariable(Compiler* compiler, uint8_t global, bool isMutable) {
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

static uint8_t argumentList(Compiler* compiler) {
    uint8_t argCount = 0;
    if (!check(compiler->parser, TOKEN_RIGHT_PAREN)) {
        do {
            expression(compiler);
            if (argCount == UINT8_MAX) {
                error(compiler->parser, "Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(compiler->parser, TOKEN_COMMA));
    }
    consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

static void parameterList(Compiler* compiler) {
    if (match(compiler->parser, TOKEN_DOT_DOT)) {
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
        bool isMutable = match(compiler->parser, TOKEN_VAR);
        uint8_t constant = parseVariable(compiler, "Expect parameter name.");
        defineVariable(compiler, constant, isMutable);
    } while (match(compiler->parser, TOKEN_COMMA));
}

static void and_(Compiler* compiler, bool canAssign) {
    int endJump = emitJump(compiler, OP_JUMP_IF_FALSE);

    emitByte(compiler, OP_POP);
    parsePrecedence(compiler, PREC_AND);

    patchJump(compiler, endJump);
}

static void binary(Compiler* compiler, bool canAssign) {
    TokenSymbol operatorType = compiler->parser->previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence(compiler, (Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL:        emitBytes(compiler, OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:       emitByte(compiler, OP_EQUAL); break;
        case TOKEN_GREATER:           emitByte(compiler, OP_GREATER); break;
        case TOKEN_GREATER_EQUAL:     emitBytes(compiler, OP_LESS, OP_NOT); break;
        case TOKEN_LESS:              emitByte(compiler, OP_LESS); break;
        case TOKEN_LESS_EQUAL:        emitBytes(compiler, OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS:              emitByte(compiler, OP_ADD); break;
        case TOKEN_MINUS:             emitByte(compiler, OP_SUBTRACT); break;
        case TOKEN_STAR:              emitByte(compiler, OP_MULTIPLY); break;
        case TOKEN_SLASH:             emitByte(compiler, OP_DIVIDE); break;
        case TOKEN_MODULO:            emitByte(compiler, OP_MODULO); break;
        case TOKEN_DOT_DOT:           emitByte(compiler, OP_RANGE); break;
        default: return;
    }
}

static void call(Compiler* compiler, bool canAssign) {
    uint8_t argCount = argumentList(compiler);
    emitBytes(compiler, OP_CALL, argCount);
}

static void dot(Compiler* compiler, bool canAssign) {
    uint8_t name = propertyConstant(compiler, "Expect property name after '.'.");

    if (canAssign && match(compiler->parser, TOKEN_EQUAL)) {
        expression(compiler);
        emitBytes(compiler, OP_SET_PROPERTY, name);
    }
    else if (match(compiler->parser, TOKEN_LEFT_PAREN)) {
        uint8_t argCount = argumentList(compiler);
        emitBytes(compiler, OP_INVOKE, name);
        emitByte(compiler, argCount);
    }
    else {
        emitBytes(compiler, OP_GET_PROPERTY, name);
    }
}

static void question(Compiler* compiler, bool canAssign) {
#define PARSE_PRECEDENCE() \
    do { \
        TokenSymbol operatorType = compiler->parser->previous.type; \
        ParseRule* rule = getRule(operatorType); \
        parsePrecedence(compiler, (Precedence)(rule->precedence + 1)); \
    } while (false)

    if (match(compiler->parser, TOKEN_DOT)) {
        uint8_t name = propertyConstant(compiler, "Expect property name after '?.'.");

        if (match(compiler->parser, TOKEN_LEFT_PAREN)) {
            uint8_t argCount = argumentList(compiler);
            emitBytes(compiler, OP_OPTIONAL_INVOKE, name);
            emitByte(compiler, argCount);
        }
        else {
            emitBytes(compiler, OP_GET_PROPERTY_OPTIONAL, name);
        }
    }
    else if (match(compiler->parser, TOKEN_LEFT_BRACKET)) {
        expression(compiler);
        consume(compiler->parser, TOKEN_RIGHT_BRACKET, "Expect ']' after subscript.");
        emitByte(compiler, OP_GET_SUBSCRIPT_OPTIONAL);
    }
    else if (match(compiler->parser, TOKEN_LEFT_PAREN)) {
        uint8_t argCount = argumentList(compiler);
        emitBytes(compiler, OP_OPTIONAL_CALL, argCount);
    }
    else if (match(compiler->parser, TOKEN_QUESTION)) {
        PARSE_PRECEDENCE();
        emitByte(compiler, OP_NIL_COALESCING);
    }
    else if (match(compiler->parser, TOKEN_COLON)) {
        PARSE_PRECEDENCE();
        emitByte(compiler, OP_ELVIS);
    }

#undef PARSE_PRECEDENCE
}

static void subscript(Compiler* compiler, bool canAssign) {
    expression(compiler);
    consume(compiler->parser, TOKEN_RIGHT_BRACKET, "Expect ']' after subscript.");

    if (canAssign && match(compiler->parser, TOKEN_EQUAL)) {
        expression(compiler);
        emitByte(compiler, OP_SET_SUBSCRIPT);
    }
    else {
        emitByte(compiler, OP_GET_SUBSCRIPT);
    }
}

static void literal(Compiler* compiler, bool canAssign) {
    switch (compiler->parser->previous.type) {
        case TOKEN_FALSE: emitByte(compiler, OP_FALSE); break;
        case TOKEN_NIL: emitByte(compiler, OP_NIL); break;
        case TOKEN_TRUE: emitByte(compiler, OP_TRUE); break;
        default: return;
    }
}

static void grouping(Compiler* compiler, bool canAssign) {
    expression(compiler);
    consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void integer(Compiler* compiler, bool canAssign) {
    int value = strtol(compiler->parser->previous.start, NULL, 10);
    emitConstant(compiler, INT_VAL(value));
}

static void number(Compiler* compiler, bool canAssign) {
    double value = strtod(compiler->parser->previous.start, NULL);
    emitConstant(compiler, NUMBER_VAL(value));
}

static void or_(Compiler* compiler, bool canAssign) {
    int elseJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    int endJump = emitJump(compiler, OP_JUMP);

    patchJump(compiler, elseJump);
    emitByte(compiler, OP_POP);

    parsePrecedence(compiler, PREC_OR);
    patchJump(compiler, endJump);
}

static void string(Compiler* compiler, bool canAssign) {
    int length = 0;
    char* string = parseString(compiler->parser, &length);
    emitConstant(compiler, OBJ_VAL(takeString(compiler->parser->vm, string, length)));
}

static void interpolation(Compiler* compiler, bool canAssign) {
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
    } while (match(compiler->parser, TOKEN_INTERPOLATION));

    consume(compiler->parser, TOKEN_STRING, "Expect end of string interpolation.");
    if (compiler->parser->previous.length > 2) {
        string(compiler, canAssign);
        emitByte(compiler, OP_ADD);
    }
}

static void array(Compiler* compiler) {
    uint8_t elementCount = 1;
    while (match(compiler->parser, TOKEN_COMMA)) {
        expression(compiler);
        if (elementCount == UINT8_MAX) {
            error(compiler->parser, "Cannot have more than 255 elements.");
        }
        elementCount++;
    }

    consume(compiler->parser, TOKEN_RIGHT_BRACKET, "Expect ']' after elements.");
    emitBytes(compiler, OP_ARRAY, elementCount);
}

static void dictionary(Compiler* compiler) {
    uint8_t entryCount = 1;
    while (match(compiler->parser, TOKEN_COMMA)) {
        expression(compiler);
        consume(compiler->parser, TOKEN_COLON, "Expect ':' after entry key.");
        expression(compiler);

        if (entryCount == UINT8_MAX) {
            error(compiler->parser, "Cannot have more than 255 entries.");
        }
        entryCount++;
    }

    consume(compiler->parser, TOKEN_RIGHT_BRACKET, "Expect ']' after entries.");
    emitBytes(compiler, OP_DICTIONARY, entryCount);
}

static void collection(Compiler* compiler, bool canAssign) {
    if (match(compiler->parser, TOKEN_RIGHT_BRACKET)) {
        emitBytes(compiler, OP_ARRAY, 0);
    }
    else {
        expression(compiler);
        if (match(compiler->parser, TOKEN_COLON)) {
            expression(compiler);
            dictionary(compiler);
        }
        else array(compiler);
    }
}

static void closure(Compiler* compiler, bool canAssign) {
    function(compiler, TYPE_FUNCTION, false);
}

static void lambda(Compiler* compiler, bool canAssign) {
    function(compiler, TYPE_LAMBDA, false);
}

static void checkMutability(Compiler* compiler, int arg, uint8_t opCode) {
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

static void namedVariable(Compiler* compiler, Token name, bool canAssign) {
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

    if (canAssign && match(compiler->parser, TOKEN_EQUAL)) {
        checkMutability(compiler, arg, setOp);
        expression(compiler);
        emitBytes(compiler, setOp, (uint8_t)arg);
    }
    else {
        emitBytes(compiler, getOp, (uint8_t)arg);
    }
}

static void variable(Compiler* compiler, bool canAssign) {
    namedVariable(compiler, compiler->parser->previous, canAssign);
}

static void klass(Compiler* compiler, bool canAssign) {
    behavior(compiler, BEHAVIOR_CLASS, syntheticToken("@"));
}

static void trait(Compiler* compiler, bool canAssign) {
    behavior(compiler, BEHAVIOR_TRAIT, syntheticToken("@"));
}

static void namespace_(Compiler* compiler, bool canAssign) {
    consume(compiler->parser, TOKEN_IDENTIFIER, "Expect Namespace identifier.");
    ObjString* name = copyString(compiler->parser->vm, compiler->parser->previous.start, compiler->parser->previous.length);
    emitBytes(compiler, OP_NAMESPACE, makeIdentifier(compiler, OBJ_VAL(name)));
}

static void super_(Compiler* compiler, bool canAssign) {
    if (compiler->parser->vm->currentClass == NULL) {
        error(compiler->parser, "Cannot use 'super' outside of a class.");
    }
    else {
        consume(compiler->parser, TOKEN_DOT, "Expect '.' after 'super'.");
        consume(compiler->parser, TOKEN_IDENTIFIER, "Expect superclass method name.");
        uint8_t name = identifierConstant(compiler, &compiler->parser->previous);

        namedVariable(compiler, syntheticToken("this"), false);
        if (match(compiler->parser, TOKEN_LEFT_PAREN)) {
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

static void this_(Compiler* compiler, bool canAssign) {
    if (compiler->parser->vm->currentClass == NULL) {
        error(compiler->parser, "Cannot use 'this' outside of a class.");
        return;
    }
    variable(compiler, false);
}

static void unary(Compiler* compiler, bool canAssign) {
    TokenSymbol operatorType = compiler->parser->previous.type;
    parsePrecedence(compiler, PREC_UNARY);

    switch (operatorType) {
        case TOKEN_BANG: emitByte(compiler, OP_NOT); break;
        case TOKEN_MINUS: emitByte(compiler, OP_NEGATE); break;
        default: return;
    }
}

static void yield(Compiler* compiler, bool canAssign) {
    if (compiler->type == TYPE_SCRIPT) {
        error(compiler->parser, "Can't yield from top-level code.");
    }
    else if (compiler->type == TYPE_INITIALIZER) {
        error(compiler->parser, "Cannot yield from an initializer.");
    }

    compiler->function->isGenerator = true;
    if (match(compiler->parser, TOKEN_RIGHT_PAREN) || match(compiler->parser, TOKEN_RIGHT_BRACKET) || match(compiler->parser, TOKEN_RIGHT_BRACE)
        || match(compiler->parser, TOKEN_COMMA) || match(compiler->parser, TOKEN_SEMICOLON))
    {
        emitBytes(compiler, OP_NIL, OP_YIELD);
    }
    else if (match(compiler->parser, TOKEN_WITH)) {
        expression(compiler);
        emitByte(compiler, OP_YIELD_WITH);
    }
    else {
        expression(compiler);
        emitByte(compiler, OP_YIELD);
    }
}

static void async(Compiler* compiler, bool canAssign) {
    if (match(compiler->parser, TOKEN_FUN)) {
        function(compiler, TYPE_FUNCTION, true);
    }
    else if (match(compiler->parser, TOKEN_LEFT_BRACE)) {
        function(compiler, TYPE_LAMBDA, true);
    }
    else {
        error(compiler->parser, "Can only use async as expression modifier for anonymous functions or lambda.");
    }
}

static void await(Compiler* compiler, bool canAssign) {
    if (compiler->type == TYPE_SCRIPT) compiler->isAsync = true;
    else if (!compiler->isAsync) error(compiler->parser, "Cannot use await unless in top level code or inside async functions/methods.");
    expression(compiler);
    emitByte(compiler, OP_AWAIT);
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]       = {grouping,      call,        PREC_CALL},
    [TOKEN_RIGHT_PAREN]      = {NULL,          NULL,        PREC_NONE},
    [TOKEN_LEFT_BRACKET]     = {collection,    subscript,   PREC_CALL},
    [TOKEN_RIGHT_BRACKET]    = {NULL,          NULL,        PREC_NONE},
    [TOKEN_LEFT_BRACE]       = {lambda,        NULL,        PREC_NONE},
    [TOKEN_RIGHT_BRACE]      = {NULL,          NULL,        PREC_NONE},
    [TOKEN_COLON]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_COMMA]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_MINUS]            = {unary,         binary,      PREC_TERM},
    [TOKEN_MODULO]           = {NULL,          binary,      PREC_FACTOR},
    [TOKEN_PIPE]             = {NULL,          NULL,        PREC_NONE},
    [TOKEN_PLUS]             = {NULL,          binary,      PREC_TERM},
    [TOKEN_QUESTION]         = {NULL,          question,    PREC_CALL},
    [TOKEN_SEMICOLON]        = {NULL,          NULL,        PREC_NONE},
    [TOKEN_SLASH]            = {NULL,          binary,      PREC_FACTOR},
    [TOKEN_STAR]             = {NULL,          binary,      PREC_FACTOR},
    [TOKEN_BANG]             = {unary,         NULL,        PREC_NONE},
    [TOKEN_BANG_EQUAL]       = {NULL,          binary,      PREC_EQUALITY},
    [TOKEN_EQUAL]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_EQUAL_EQUAL]      = {NULL,          binary,      PREC_EQUALITY},
    [TOKEN_GREATER]          = {NULL,          binary,      PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL]    = {NULL,          binary,      PREC_COMPARISON},
    [TOKEN_LESS]             = {NULL,          binary,      PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]       = {NULL,          binary,      PREC_COMPARISON},
    [TOKEN_DOT]              = {NULL,          dot,         PREC_CALL},
    [TOKEN_DOT_DOT]          = {NULL,          binary,      PREC_CALL},
    [TOKEN_IDENTIFIER]       = {variable,      NULL,        PREC_NONE},
    [TOKEN_STRING]           = {string,        NULL,        PREC_NONE},
    [TOKEN_INTERPOLATION]    = {interpolation, NULL,        PREC_NONE},
    [TOKEN_NUMBER]           = {number,        NULL,        PREC_NONE},
    [TOKEN_INT]              = {integer,       NULL,        PREC_NONE},
    [TOKEN_AND]              = {NULL,          and_,        PREC_AND},
    [TOKEN_AS]               = {NULL,          NULL,        PREC_NONE},
    [TOKEN_ASYNC]            = {async,         NULL,        PREC_NONE},
    [TOKEN_AWAIT]            = {await,         NULL,        PREC_NONE},
    [TOKEN_BREAK]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_CASE]             = {NULL,          NULL,        PREC_NONE},
    [TOKEN_CATCH]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_CLASS]            = {klass,         NULL,        PREC_NONE},
    [TOKEN_CONTINUE]         = {NULL,          NULL,        PREC_NONE},
    [TOKEN_DEFAULT]          = {NULL,          NULL,        PREC_NONE},
    [TOKEN_ELSE]             = {NULL,          NULL,        PREC_NONE},
    [TOKEN_FALSE]            = {literal,       NULL,        PREC_NONE},
    [TOKEN_FINALLY]          = {NULL,          NULL,        PREC_NONE},
    [TOKEN_FOR]              = {NULL,          NULL,        PREC_NONE},
    [TOKEN_FUN]              = {closure,       NULL,        PREC_NONE},
    [TOKEN_IF]               = {NULL,          NULL,        PREC_NONE},
    [TOKEN_NAMESPACE]        = {NULL,          NULL,        PREC_NONE},
    [TOKEN_NIL]              = {literal,       NULL,        PREC_NONE},
    [TOKEN_OR]               = {NULL,          or_,         PREC_OR},
    [TOKEN_REQUIRE]          = {NULL,          NULL,        PREC_NONE},
    [TOKEN_RETURN]           = {NULL,          NULL,        PREC_NONE},
    [TOKEN_SUPER]            = {super_,        NULL,        PREC_NONE},
    [TOKEN_SWITCH]           = {NULL,          NULL,        PREC_NONE},
    [TOKEN_THIS]             = {this_,         NULL,        PREC_NONE},
    [TOKEN_THROW]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_TRAIT]            = {trait,         NULL,        PREC_NONE},
    [TOKEN_TRUE]             = {literal,       NULL,        PREC_NONE},
    [TOKEN_TRY]              = {NULL,          NULL,        PREC_NONE},
    [TOKEN_USING]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_VAL]              = {NULL,          NULL,        PREC_NONE},
    [TOKEN_VAR]              = {NULL,          NULL,        PREC_NONE},
    [TOKEN_WHILE]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_WITH]             = {NULL,          NULL,        PREC_NONE},
    [TOKEN_YIELD]            = {yield,         NULL,        PREC_NONE},
    [TOKEN_ERROR]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_EOF]              = {NULL,          NULL,        PREC_NONE},
};

static void parsePrecedence(Compiler* compiler, Precedence precedence) {
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

    if (canAssign && match(compiler->parser, TOKEN_EQUAL)) {
        error(compiler->parser, "Invalid assignment target.");
    }
}

static ParseRule* getRule(TokenSymbol type) {
    return &rules[type];
}

static void expression(Compiler* compiler) {
    parsePrecedence(compiler, PREC_ASSIGNMENT);
}

static void block(Compiler* compiler) {
    while (!check(compiler->parser, TOKEN_RIGHT_BRACE) && !check(compiler->parser, TOKEN_EOF)) {
        declaration(compiler);
    }
    consume(compiler->parser, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void functionParameters(Compiler* compiler) {
    consume(compiler->parser, TOKEN_LEFT_PAREN, "Expect '(' after function keyword/name.");
    if (!check(compiler->parser, TOKEN_RIGHT_PAREN)) {
        parameterList(compiler);
    }
    consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(compiler->parser, TOKEN_LEFT_BRACE, "Expect '{' before function body.");
}

static void lambdaParameters(Compiler* compiler) {
    if (!match(compiler->parser, TOKEN_PIPE)) return;
    if (!check(compiler->parser, TOKEN_PIPE)) {
        parameterList(compiler);
    }
    consume(compiler->parser, TOKEN_PIPE, "Expect '|' after lambda parameters.");
}

static uint8_t lambdaDepth(Compiler* compiler) {
    uint8_t depth = 1;
    Compiler* current = compiler->enclosing;
    while (current->type == TYPE_LAMBDA) {
        depth++;
        current = current->enclosing;
    }
    return depth;
}

static void function(Compiler* enclosing, FunctionType type, bool isAsync) {
    Compiler compiler;
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

static void method(Compiler* compiler) {
    bool isAsync = false;
    if (match(compiler->parser, TOKEN_ASYNC)) isAsync = true;
    uint8_t opCode = match(compiler->parser, TOKEN_CLASS) ? OP_CLASS_METHOD : OP_INSTANCE_METHOD;
    uint8_t constant = propertyConstant(compiler, "Expect method name.");

    FunctionType type = TYPE_METHOD;
    if (compiler->parser->previous.length == 8 && memcmp(compiler->parser->previous.start, "__init__", 8) == 0) {
        type = TYPE_INITIALIZER;
    }

    function(compiler, type, isAsync);
    emitBytes(compiler, opCode, constant);
}

static void methods(Compiler* compiler) {
    consume(compiler->parser, TOKEN_LEFT_BRACE, "Expect '{' before class/trait body.");
    while (!check(compiler->parser, TOKEN_RIGHT_BRACE) && !check(compiler->parser, TOKEN_EOF)) {
        method(compiler);
    }
    consume(compiler->parser, TOKEN_RIGHT_BRACE, "Expect '}' after class/trait body.");
}

static uint8_t traits(Compiler* compiler, Token* name) {
    uint8_t traitCount = 0;

    do {
        traitCount++;
        if (compiler->function->arity > UINT4_MAX) {
            errorAtCurrent(compiler->parser, "Can't have more than 15 parameters.");
        }

        consume(compiler->parser, TOKEN_IDENTIFIER, "Expect class/trait name.");
        variable(compiler, false);
    } while (match(compiler->parser, TOKEN_COMMA));

    return traitCount;
}

static void behavior(Compiler* compiler, BehaviorType type, Token name) {
    bool isAnonymous = (name.type != TOKEN_IDENTIFIER && name.length == 1);
    if (isAnonymous) {
        emitBytes(compiler, OP_ANONYMOUS, type);
        emitByte(compiler, OP_DUP);
    }

    ClassCompiler* enclosingClass = compiler->parser->vm->currentClass;
    ClassCompiler classCompiler = { .name = name, .enclosing = enclosingClass, .type = type, .superclass = compiler->parser->rootClass };
    compiler->parser->vm->currentClass = &classCompiler;

    if (type == BEHAVIOR_CLASS) {
        if (match(compiler->parser, TOKEN_LESS)) {
            consume(compiler->parser, TOKEN_IDENTIFIER, "Expect super class name.");
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
    addLocal(compiler, syntheticToken("super"));
    defineVariable(compiler, 0, false);

    if (type == BEHAVIOR_CLASS) emitByte(compiler, OP_INHERIT);
    uint8_t traitCount = match(compiler->parser, TOKEN_WITH) ? traits(compiler, &name) : 0;
    if (traitCount > 0) emitBytes(compiler, OP_IMPLEMENT, traitCount);

    methods(compiler);
    endScope(compiler);
    compiler->parser->vm->currentClass = enclosingClass;
}

static void classDeclaration(Compiler* compiler) {
    consume(compiler->parser, TOKEN_IDENTIFIER, "Expect class name.");
    Token className = compiler->parser->previous;
    uint8_t nameConstant = identifierConstant(compiler, &compiler->parser->previous);

    declareVariable(compiler);
    emitBytes(compiler, OP_CLASS, nameConstant);
    behavior(compiler, BEHAVIOR_CLASS, className);
}

static void funDeclaration(Compiler* compiler, bool isAsync) {
    uint8_t global = parseVariable(compiler, "Expect function name.");
    markInitialized(compiler, false);
    function(compiler, TYPE_FUNCTION, isAsync);
    defineVariable(compiler, global, false);
}

static void namespaceDeclaration(Compiler* compiler) {
    uint8_t namespaceDepth = 0;
    do {
        if (namespaceDepth > UINT4_MAX) {
            errorAtCurrent(compiler->parser, "Can't have more than 15 levels of namespace depth.");
        }
        namespace_(compiler, false);
        namespaceDepth++;
    } while (match(compiler->parser, TOKEN_DOT));

    consume(compiler->parser, TOKEN_SEMICOLON, "Expect semicolon after namespace declaration.");
    emitBytes(compiler, OP_DECLARE_NAMESPACE, namespaceDepth);
}

static void traitDeclaration(Compiler* compiler) {
    consume(compiler->parser, TOKEN_IDENTIFIER, "Expect trait name.");
    Token traitName = compiler->parser->previous;
    uint8_t nameConstant = identifierConstant(compiler, &compiler->parser->previous);

    declareVariable(compiler);
    emitBytes(compiler, OP_TRAIT, nameConstant);
    behavior(compiler, BEHAVIOR_TRAIT, traitName);
}

static void varDeclaration(Compiler* compiler, bool isMutable) {
    uint8_t global = parseVariable(compiler, "Expect variable name.");

    if (!isMutable && !check(compiler->parser, TOKEN_EQUAL)) {
        error(compiler->parser, "Immutable variable must be initialized upon declaration.");
    }
    else if (match(compiler->parser, TOKEN_EQUAL)) {
        expression(compiler);
    }
    else {
        emitByte(compiler, OP_NIL);
    }
    consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(compiler, global, isMutable);
}

static void awaitStatement(Compiler* compiler) {
    if (compiler->type == TYPE_SCRIPT) {
        compiler->isAsync = true;
        compiler->function->isAsync = true;
    }
    else if (!compiler->isAsync) error(compiler->parser, "Can only use 'await' in async methods or top level code.");
    
    expression(compiler);
    consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after await value.");
    emitBytes(compiler, OP_AWAIT, OP_POP);
}

static void breakStatement(Compiler* compiler) {
    if (compiler->innermostLoopStart == -1) {
        error(compiler->parser, "Cannot use 'break' outside of a loop.");
    }
    consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after 'break'.");

    discardLocals(compiler);
    emitJump(compiler, OP_END);
}

static void continueStatement(Compiler* compiler) {
    if (compiler->innermostLoopStart == -1) {
        error(compiler->parser, "Cannot use 'continue' outside of a loop.");
    }
    consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after 'continue'.");

    discardLocals(compiler);
    emitLoop(compiler, compiler->innermostLoopStart);
}

static void expressionStatement(Compiler* compiler) {
    expression(compiler);
    if (compiler->type == TYPE_LAMBDA && !check(compiler->parser, TOKEN_SEMICOLON)) {
        emitByte(compiler, OP_RETURN);
    }
    else {
        consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after expression.");
        emitByte(compiler, OP_POP);
    }
}

static void forStatement(Compiler* compiler) {
    beginScope(compiler);
    Token indexToken, valueToken;
    consume(compiler->parser, TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    consume(compiler->parser, TOKEN_VAR, "Expect 'var' keyword after '(' in For loop.");
    if (match(compiler->parser, TOKEN_LEFT_PAREN)) {
        consume(compiler->parser, TOKEN_IDENTIFIER, "Expect first variable name after '('.");
        indexToken = compiler->parser->previous;
        consume(compiler->parser, TOKEN_COMMA, "Expect ',' after first variable declaration.");
        consume(compiler->parser, TOKEN_IDENTIFIER, "Expect second variable name after ','.");
        valueToken = compiler->parser->previous;
        consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after second variable declaration.");
    }
    else {
        consume(compiler->parser, TOKEN_IDENTIFIER, "Expect variable name after 'var'.");
        indexToken = syntheticToken("index ");
        valueToken = compiler->parser->previous;
    }

    consume(compiler->parser, TOKEN_COLON, "Expect ':' after variable name.");
    expression(compiler);
    if (compiler->localCount + 3 > UINT8_MAX) {
        error(compiler->parser, "for loop can only contain up to 252 variables.");
    }

    int collectionSlot = addLocal(compiler, syntheticToken("collection "));
    emitByte(compiler, OP_NIL);
    int indexSlot = addLocal(compiler, indexToken);
    markInitialized(compiler, true);
    consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after loop expression.");

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

static void ifStatement(Compiler* compiler) {
    consume(compiler->parser, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression(compiler);
    consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int thenJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    emitByte(compiler, OP_POP);
    statement(compiler);

    int elseJump = emitJump(compiler, OP_JUMP);
    patchJump(compiler, thenJump);
    emitByte(compiler, OP_POP);

    if (match(compiler->parser, TOKEN_ELSE)) statement(compiler);
    patchJump(compiler, elseJump);
}

static void requireStatement(Compiler* compiler) {
    if (compiler->type != TYPE_SCRIPT) {
        error(compiler->parser, "Can only require source files from top-level code.");
    }

    expression(compiler);
    consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after required file path.");
    emitByte(compiler, OP_REQUIRE);
}

static void returnStatement(Compiler* compiler) {
    if (compiler->type == TYPE_SCRIPT) {
        error(compiler->parser, "Can't return from top-level code.");
    }

    uint8_t depth = 0;
    if (compiler->type == TYPE_LAMBDA) depth = lambdaDepth(compiler);

    if (match(compiler->parser, TOKEN_SEMICOLON)) {
        emitReturn(compiler, depth);
    }
    else {
        if (compiler->type == TYPE_INITIALIZER) {
            error(compiler->parser, "Cannot return value from an initializer.");
        }

        expression(compiler);
        consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after return value.");

        if (compiler->type == TYPE_LAMBDA) {
            emitBytes(compiler, OP_RETURN_NONLOCAL, depth);
        }
        else {
            emitByte(compiler, OP_RETURN);
        }
    }
}

static void switchStatement(Compiler* compiler) {
    consume(compiler->parser, TOKEN_LEFT_PAREN, "Expect '(' after 'switch'.");
    expression(compiler);
    consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after value.");
    consume(compiler->parser, TOKEN_LEFT_BRACE, "Expect '{' before switch cases.");

    int state = 0;
    int caseEnds[MAX_CASES];
    int caseCount = 0;
    int previousCaseSkip = -1;

    while (!match(compiler->parser, TOKEN_RIGHT_BRACE) && !check(compiler->parser, TOKEN_EOF)) {
        if (match(compiler->parser, TOKEN_CASE) || match(compiler->parser, TOKEN_DEFAULT)) {
            TokenSymbol caseType = compiler->parser->previous.type;
            if (state == 2) {
                error(compiler->parser, "Can't have another case or default after the default case.");
            }

            if (state == 1) {
                caseEnds[caseCount++] = emitJump(compiler, OP_JUMP);
                patchJump(compiler, previousCaseSkip);
                emitByte(compiler, OP_POP);
            }

            if (caseType == TOKEN_CASE) {
                state = 1;
                emitByte(compiler, OP_DUP);
                expression(compiler);

                consume(compiler->parser, TOKEN_COLON, "Expect ':' after case value.");
                emitByte(compiler, OP_EQUAL);
                previousCaseSkip = emitJump(compiler, OP_JUMP_IF_FALSE);
                emitByte(compiler, OP_POP);
            }
            else {
                state = 2;
                consume(compiler->parser, TOKEN_COLON, "Expect ':' after default.");
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

static void throwStatement(Compiler* compiler) {
    expression(compiler);
    consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after thrown exception object.");
    emitByte(compiler, OP_THROW);
}

static void tryStatement(Compiler* compiler) {
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

    if (match(compiler->parser, TOKEN_CATCH)) {
        beginScope(compiler);
        consume(compiler->parser, TOKEN_LEFT_PAREN, "Expect '(' after catch");
        consume(compiler->parser, TOKEN_IDENTIFIER, "Expect type name to catch");
        uint8_t name = identifierConstant(compiler, &compiler->parser->previous);
        currentChunk(compiler)->code[exceptionType] = name;
        patchAddress(compiler, handlerAddress);

        if (check(compiler->parser, TOKEN_IDENTIFIER)) {
            consume(compiler->parser, TOKEN_IDENTIFIER, "Expect identifier after exception type.");
            addLocal(compiler, compiler->parser->previous);
            markInitialized(compiler, false);
            uint8_t variable = resolveLocal(compiler, &compiler->parser->previous);
            emitBytes(compiler, OP_SET_LOCAL, variable);
        }

        consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after catch statement");
        emitByte(compiler, OP_CATCH);
        statement(compiler);
        endScope(compiler);
    }
    else {
        errorAtCurrent(compiler->parser, "Must have a catch statement following a try statement.");
    }
    patchJump(compiler, catchJump);

    if (match(compiler->parser, TOKEN_FINALLY)) {
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

static void usingStatement(Compiler* compiler) {
    uint8_t namespaceDepth = 0;
    do {
        consume(compiler->parser, TOKEN_IDENTIFIER, "Expect namespace identifier.");
        uint8_t namespace = identifierConstant(compiler, &compiler->parser->previous);
        emitBytes(compiler, OP_NAMESPACE, namespace);
        namespaceDepth++;
    } while (match(compiler->parser, TOKEN_DOT));

    emitBytes(compiler, OP_GET_NAMESPACE, namespaceDepth);
    uint8_t alias = makeIdentifier(compiler, OBJ_VAL(emptyString(compiler->parser->vm)));

    if (match(compiler->parser, TOKEN_AS)) {
        consume(compiler->parser, TOKEN_IDENTIFIER, "Expect alias after 'as'.");
        Token name = compiler->parser->previous;
        alias = identifierConstant(compiler, &name);
    }
    consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after using statement.");
    emitBytes(compiler, OP_USING_NAMESPACE, alias);
}

static void whileStatement(Compiler* compiler) {
    int loopStart = compiler->innermostLoopStart;
    int scopeDepth = compiler->innermostLoopScopeDepth;
    compiler->innermostLoopStart = currentChunk(compiler)->count;
    compiler->innermostLoopScopeDepth = compiler->scopeDepth;

    consume(compiler->parser, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression(compiler);
    consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

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

static void yieldStatement(Compiler* compiler) {
    if (compiler->type == TYPE_SCRIPT) {
        error(compiler->parser, "Can't yield from top-level code.");
    }
    else if (compiler->type == TYPE_INITIALIZER) {
        error(compiler->parser, "Cannot yield from an initializer.");
    }

    compiler->function->isGenerator = true;
    if (match(compiler->parser, TOKEN_SEMICOLON)) {
        emitBytes(compiler, OP_YIELD, OP_POP);
    }
    else if (match(compiler->parser, TOKEN_WITH)) {
        expression(compiler);
        consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after yield value.");
        emitByte(compiler, OP_YIELD_WITH);
    }
    else {
        expression(compiler);
        consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after yield value.");
        emitBytes(compiler, OP_YIELD, OP_POP);
    }
}

static void declaration(Compiler* compiler) {
    if (check(compiler->parser, TOKEN_ASYNC) && checkNext(compiler->parser, TOKEN_FUN)) {
        advance(compiler->parser);
        advance(compiler->parser);
        funDeclaration(compiler, true);
    }
    else if (check(compiler->parser, TOKEN_CLASS) && checkNext(compiler->parser, TOKEN_IDENTIFIER)) {
        advance(compiler->parser);
        classDeclaration(compiler);
    }
    else if (check(compiler->parser, TOKEN_FUN) && checkNext(compiler->parser, TOKEN_IDENTIFIER)) {
        advance(compiler->parser);
        funDeclaration(compiler, false);
    }
    else if (match(compiler->parser, TOKEN_NAMESPACE)) {
        namespaceDeclaration(compiler);
    }
    else if (check(compiler->parser, TOKEN_TRAIT) && checkNext(compiler->parser, TOKEN_IDENTIFIER)) {
        advance(compiler->parser);
        traitDeclaration(compiler);
    }
    else if (match(compiler->parser, TOKEN_VAL)) {
        varDeclaration(compiler, false);
    }
    else if (match(compiler->parser, TOKEN_VAR)) {
        varDeclaration(compiler, true);
    }
    else {
        statement(compiler);
    }

    if (compiler->parser->panicMode) {
        synchronize(compiler->parser);
    }
}

static void statement(Compiler* compiler) {
    if (match(compiler->parser, TOKEN_AWAIT)) {
        awaitStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_BREAK)) {
        breakStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_CONTINUE)) {
        continueStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_FOR)) {
        forStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_IF)) {
        ifStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_REQUIRE)) {
        requireStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_RETURN)) {
        returnStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_SWITCH)) {
        switchStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_THROW)) {
        throwStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_TRY)) {
        tryStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_USING)) {
        usingStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_WHILE)) {
        whileStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_YIELD)) {
        yieldStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_LEFT_BRACE)) {
        beginScope(compiler);
        block(compiler);
        endScope(compiler);
    }
    else {
        expressionStatement(compiler);
    }
}

ObjFunction* compile(VM* vm, const char* source) {
    Scanner scanner;
    initScanner(&scanner, source);

    Parser parser;
    initParser(&parser, vm, &scanner);

    Compiler compiler;
    initCompiler(&compiler, &parser, NULL, TYPE_SCRIPT, false);

    advance(&parser);
    advance(&parser);
    while (!match(&parser, TOKEN_EOF)) {
        declaration(&compiler);
    }

    ObjFunction* function = endCompiler(&compiler);
    return parser.hadError ? NULL : function;
}

void markCompilerRoots(VM* vm) {
    Compiler* compiler = vm->currentCompiler;
    while (compiler != NULL) {
        markObject(vm, (Obj*)compiler->function);
        markIDMap(vm, &compiler->indexes);
        compiler = compiler->enclosing;
    }
}