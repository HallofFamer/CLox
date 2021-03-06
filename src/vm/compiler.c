#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "memory.h"
#include "parser.h"
#include "scanner.h"
#include "string.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,
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
} Local;

typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;

typedef enum {
    TYPE_FUNCTION,
    TYPE_INITIALIZER,
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
    int scopeDepth;
};

struct ClassCompiler {
    struct ClassCompiler* enclosing;
    Token name;
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

static void emitLoop(Compiler* compiler, int loopStart) {
    emitByte(compiler, OP_LOOP);

    int offset = currentChunk(compiler)->count - loopStart + 2;
    if (offset > UINT16_MAX) error(compiler->parser, "Loop body too large.");

    emitByte(compiler, (offset >> 8) & 0xff);
    emitByte(compiler, offset & 0xff);
}

static int emitJump(Compiler* compiler, uint8_t instruction) {
    emitByte(compiler, instruction);
    emitByte(compiler, 0xff);
    emitByte(compiler, 0xff);
    return currentChunk(compiler)->count - 2;
}

static void emitReturn(Compiler* compiler) {
    if (compiler->type == TYPE_INITIALIZER) {
        emitBytes(compiler, OP_GET_LOCAL, 0);
    }
    else {
        emitByte(compiler, OP_NIL);
    }
    emitByte(compiler, OP_RETURN);
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

static void initCompiler(Compiler* compiler, Parser* parser, Compiler* enclosing, FunctionType type) {
    compiler->parser = parser;
    compiler->enclosing = enclosing;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction(parser->vm);
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
    if (type != TYPE_FUNCTION) {
        local->name.start = "this";
        local->name.length = 4;
    }
    else {
        local->name.start = "";
        local->name.length = 0;
    }
}

static ObjFunction* endCompiler(Compiler* compiler) {
    emitReturn(compiler);
    ObjFunction* function = compiler->function;

#ifdef DEBUG_PRINT_CODE
    if (!compiler->parser->hadError) {
        disassembleChunk(currentChunk(compiler), function->name != NULL ? function->name->chars : "<script>");
    }
#endif

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
static void function(Compiler* enclosing, FunctionType type);
static void declaration(Compiler* compiler);
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Compiler* compiler, Precedence precedence);

static uint8_t identifierConstant(Compiler* compiler, Token* name) {
    return makeConstant(compiler, OBJ_VAL(copyString(compiler->parser->vm, name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
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

static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
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
    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token* name) {
    if (compiler->enclosing == NULL) return -1;

    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return addUpvalue(compiler, (uint8_t)upvalue, false);
    }
    return -1;
}

static void addLocal(Compiler* compiler, Token name) {
    if (compiler->localCount == UINT8_COUNT) {
        error(compiler->parser, "Too many local variables in function.");
        return;
    }

    Local* local = &compiler->locals[compiler->localCount++];
    local->name = name;
    local->depth = -1;
    local->isCaptured = false;
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

static void markInitialized(Compiler* compiler) {
    if (compiler->scopeDepth == 0) return;
    compiler->locals[compiler->localCount - 1].depth = compiler->scopeDepth;
}

static void defineVariable(Compiler* compiler, uint8_t global) {
    if (compiler->scopeDepth > 0) {
        markInitialized(compiler);
        return;
    }
    emitBytes(compiler, OP_DEFINE_GLOBAL, global);
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
        defineVariable(compiler, constant);
        return;
    }

    do {
        compiler->function->arity++;
        if (compiler->function->arity > UINT8_MAX) {
            errorAtCurrent(compiler->parser, "Can't have more than 255 parameters.");
        }
        uint8_t constant = parseVariable(compiler, "Expect parameter name.");
        defineVariable(compiler, constant);
    } while (match(compiler->parser, TOKEN_COMMA));
}

static void and_(Compiler* compiler, bool canAssign) {
    int endJump = emitJump(compiler, OP_JUMP_IF_FALSE);

    emitByte(compiler, OP_POP);
    parsePrecedence(compiler, PREC_AND);

    patchJump(compiler, endJump);
}

static void binary(Compiler* compiler, bool canAssign) {
    TokenType operatorType = compiler->parser->previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence(compiler, (Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL:    emitBytes(compiler, OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:   emitByte(compiler, OP_EQUAL); break;
        case TOKEN_GREATER:       emitByte(compiler, OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emitBytes(compiler, OP_LESS, OP_NOT); break;
        case TOKEN_LESS:          emitByte(compiler, OP_LESS); break;
        case TOKEN_LESS_EQUAL:    emitBytes(compiler, OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS:          emitByte(compiler, OP_ADD); break;
        case TOKEN_MINUS:         emitByte(compiler, OP_SUBTRACT); break;
        case TOKEN_STAR:          emitByte(compiler, OP_MULTIPLY); break;
        case TOKEN_SLASH:         emitByte(compiler, OP_DIVIDE); break;
        default: return;
    }
}

static void call(Compiler* compiler, bool canAssign) {
    uint8_t argCount = argumentList(compiler);
    emitBytes(compiler, OP_CALL, argCount);
}

static void dot(Compiler* compiler, bool canAssign) {
    consume(compiler->parser, TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint8_t name = identifierConstant(compiler, &compiler->parser->previous);

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
    emitConstant(compiler, OBJ_VAL(copyString(compiler->parser->vm, compiler->parser->previous.start + 1, compiler->parser->previous.length - 2)));
}

static void list(Compiler* compiler) {
    uint8_t elementCount = 1;
    while (match(compiler->parser, TOKEN_COMMA)) {
        expression(compiler);
        if (elementCount == UINT8_MAX) {
            error(compiler->parser, "Cannot have more than 255 elements.");
        }
        elementCount++;
    }

    consume(compiler->parser, TOKEN_RIGHT_BRACKET, "Expect ']' after elements.");
    emitBytes(compiler, OP_LIST, elementCount);
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
        emitBytes(compiler, OP_LIST, 0);
    }
    else {
        expression(compiler);
        if (match(compiler->parser, TOKEN_COLON)) {
            expression(compiler);
            dictionary(compiler);
        }
        else list(compiler);
    }
}

static void closure(Compiler* compiler, bool canAssign) {
    function(compiler, TYPE_FUNCTION);
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

static void super_(Compiler* compiler, bool canAssign) {
    if (compiler->parser->vm->currentClass == NULL) {
        error(compiler->parser, "Cannot use 'super' outside of a class.");
    }

    consume(compiler->parser, TOKEN_DOT, "Expect '.' after 'super'.");
    consume(compiler->parser, TOKEN_IDENTIFIER, "Expect superclass method name.");
    uint8_t name = identifierConstant(compiler, &compiler->parser->previous);

    namedVariable(compiler, syntheticToken("this"), false);
    if (match(compiler->parser, TOKEN_LEFT_PAREN)) {
        uint8_t argCount = argumentList(compiler);
        namedVariable(compiler, syntheticToken("super"), false);
        emitBytes(compiler, OP_SUPER_INVOKE, name);
        emitByte(compiler, argCount);
    }
    else {
        namedVariable(compiler, syntheticToken("super"), false);
        emitBytes(compiler, OP_GET_SUPER, name);
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
    TokenType operatorType = compiler->parser->previous.type;
    parsePrecedence(compiler, PREC_UNARY);

    switch (operatorType) {
        case TOKEN_BANG: emitByte(compiler, OP_NOT); break;
        case TOKEN_MINUS: emitByte(compiler, OP_NEGATE); break;
        default: return;
    }
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]    = {grouping,   call,        PREC_CALL},
    [TOKEN_RIGHT_PAREN]   = {NULL,       NULL,        PREC_NONE},
    [TOKEN_LEFT_BRACKET]  = {collection, subscript,   PREC_CALL},
    [TOKEN_RIGHT_BRACKET] = {NULL,       NULL,        PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {NULL,       NULL,        PREC_NONE}, 
    [TOKEN_RIGHT_BRACE]   = {NULL,       NULL,        PREC_NONE},
    [TOKEN_COMMA]         = {NULL,       NULL,        PREC_NONE},
    [TOKEN_MINUS]         = {unary,      binary,      PREC_TERM},
    [TOKEN_PLUS]          = {NULL,       binary,      PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,       NULL,        PREC_NONE},
    [TOKEN_SLASH]         = {NULL,       binary,      PREC_FACTOR},
    [TOKEN_STAR]          = {NULL,       binary,      PREC_FACTOR},
    [TOKEN_BANG]          = {unary,      NULL,        PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,       binary,      PREC_EQUALITY},
    [TOKEN_EQUAL]         = {NULL,       NULL,        PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,       binary,      PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,       binary,      PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,       binary,      PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,       binary,      PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,       binary,      PREC_COMPARISON},
    [TOKEN_DOT]           = {NULL,       dot,         PREC_CALL},
    [TOKEN_DOT_DOT]       = {NULL,       NULL,        PREC_NONE},
    [TOKEN_IDENTIFIER]    = {variable,   NULL,        PREC_NONE},
    [TOKEN_STRING]        = {string,     NULL,        PREC_NONE},
    [TOKEN_NUMBER]        = {number,     NULL,        PREC_NONE},
    [TOKEN_INT]           = {integer,    NULL,        PREC_NONE},
    [TOKEN_AND]           = {NULL,       and_,        PREC_AND},
    [TOKEN_CLASS]         = {NULL,       NULL,        PREC_NONE},
    [TOKEN_ELSE]          = {NULL,       NULL,        PREC_NONE},
    [TOKEN_FALSE]         = {literal,    NULL,        PREC_NONE},
    [TOKEN_FOR]           = {NULL,       NULL,        PREC_NONE},
    [TOKEN_FUN]           = {closure,    NULL,        PREC_NONE},
    [TOKEN_IF]            = {NULL,       NULL,        PREC_NONE},
    [TOKEN_NIL]           = {literal,    NULL,        PREC_NONE},
    [TOKEN_OR]            = {NULL,       or_,         PREC_OR},
    [TOKEN_RETURN]        = {NULL,       NULL,        PREC_NONE},
    [TOKEN_SUPER]         = {super_,     NULL,        PREC_NONE},
    [TOKEN_THIS]          = {this_,      NULL,        PREC_NONE},
    [TOKEN_TRUE]          = {literal,    NULL,        PREC_NONE},
    [TOKEN_VAR]           = {NULL,       NULL,        PREC_NONE},
    [TOKEN_WHILE]         = {NULL,       NULL,        PREC_NONE},
    [TOKEN_ERROR]         = {NULL,       NULL,        PREC_NONE},
    [TOKEN_EOF]           = {NULL,       NULL,        PREC_NONE},
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

static ParseRule* getRule(TokenType type) {
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

static void function(Compiler* enclosing, FunctionType type) {
    Compiler compiler;
    initCompiler(&compiler, enclosing->parser, enclosing, type);
    beginScope(&compiler);

    consume(compiler.parser, TOKEN_LEFT_PAREN, "Expect '(' after function keyword/name.");
    if (!check(compiler.parser, TOKEN_RIGHT_PAREN)) {
        parameterList(&compiler);
    }

    consume(compiler.parser, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(compiler.parser, TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block(&compiler);

    ObjFunction* function = endCompiler(&compiler);
    emitBytes(enclosing, OP_CLOSURE, makeConstant(enclosing, OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(enclosing, compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(enclosing, compiler.upvalues[i].index);
    }
}

static void method(Compiler* compiler) {
    consume(compiler->parser, TOKEN_IDENTIFIER, "Expect method name.");
    uint8_t constant = identifierConstant(compiler, &compiler->parser->previous);

    FunctionType type = TYPE_METHOD;
    if (compiler->parser->previous.length == 4 && memcmp(compiler->parser->previous.start, "init", 4) == 0) {
        type = TYPE_INITIALIZER;
    }

    function(compiler, type);
    emitBytes(compiler, OP_METHOD, constant);
}

static void classDeclaration(Compiler* compiler) {
    consume(compiler->parser, TOKEN_IDENTIFIER, "Expect class name.");
    Token className = compiler->parser->previous;
    uint8_t nameConstant = identifierConstant(compiler, &compiler->parser->previous);

    declareVariable(compiler);
    emitBytes(compiler, OP_CLASS, nameConstant);
    defineVariable(compiler, nameConstant);

    ClassCompiler* enclosingClass = compiler->parser->vm->currentClass;
    ClassCompiler classCompiler = { .name = compiler->parser->previous, .enclosing = enclosingClass };
    compiler->parser->vm->currentClass = &classCompiler;

    if (match(compiler->parser, TOKEN_LESS)) {
        consume(compiler->parser, TOKEN_IDENTIFIER, "Expect super class name.");
        variable(compiler, false);
        if (identifiersEqual(&className, &compiler->parser->previous)) {
            error(compiler->parser, "A class cannot inherit from itself.");
        }
    }
    else {
        namedVariable(compiler, compiler->parser->rootClass, false);
        if (identifiersEqual(&className, &compiler->parser->rootClass)) {
            error(compiler->parser, "Cannot redeclare root class Object.");
        }
    }

    beginScope(compiler);
    addLocal(compiler, syntheticToken("super"));
    defineVariable(compiler, 0);
    namedVariable(compiler, className, false);
    emitByte(compiler, OP_INHERIT);

    namedVariable(compiler, className, false);
    consume(compiler->parser, TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check(compiler->parser, TOKEN_RIGHT_BRACE) && !check(compiler->parser, TOKEN_EOF)) {
        method(compiler);
    }

    consume(compiler->parser, TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emitByte(compiler, OP_POP);
    endScope(compiler);
    compiler->parser->vm->currentClass = enclosingClass;
}

static void funDeclaration(Compiler* compiler) {
    uint8_t global = parseVariable(compiler, "Expect function name.");
    markInitialized(compiler);
    function(compiler, TYPE_FUNCTION);
    defineVariable(compiler, global);
}

static void varDeclaration(Compiler* compiler) {
    uint8_t global = parseVariable(compiler, "Expect variable name.");

    if (match(compiler->parser, TOKEN_EQUAL)) {
        expression(compiler);
    }
    else {
        emitByte(compiler, OP_NIL);
    }
    consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(compiler, global);
}

static void expressionStatement(Compiler* compiler) {
    expression(compiler);
    consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(compiler, OP_POP);
}

static void forStatement(Compiler* compiler) {
    beginScope(compiler);
    consume(compiler->parser, TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(compiler->parser, TOKEN_SEMICOLON)) {

    }
    else if (match(compiler->parser, TOKEN_VAR)) {
        varDeclaration(compiler);
    }
    else {
        expressionStatement(compiler);
    }

    int loopStart = currentChunk(compiler)->count;
    int exitJump = -1;
    if (!match(compiler->parser, TOKEN_SEMICOLON)) {
        expression(compiler);
        consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after loop condition.");
        exitJump = emitJump(compiler, OP_JUMP_IF_FALSE);
        emitByte(compiler, OP_POP);
    }

    if (!match(compiler->parser, TOKEN_RIGHT_PAREN)) {
        int bodyJump = emitJump(compiler, OP_JUMP);
        int incrementStart = currentChunk(compiler)->count;
        expression(compiler);
        emitByte(compiler, OP_POP);
        consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(compiler, loopStart);
        loopStart = incrementStart;
        patchJump(compiler, bodyJump);
    }

    statement(compiler);
    emitLoop(compiler, loopStart);

    if (exitJump != -1) {
        patchJump(compiler, exitJump);
        emitByte(compiler, OP_POP);
    }
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

static void returnStatement(Compiler* compiler) {
    if (compiler->type == TYPE_SCRIPT) {
        error(compiler->parser, "Can't return from top-level code.");
    }

    if (match(compiler->parser, TOKEN_SEMICOLON)) {
        emitReturn(compiler);
    }
    else {
        if (compiler->type == TYPE_INITIALIZER) {
            error(compiler->parser, "Cannot return value from an initializer.");
        }

        expression(compiler);
        consume(compiler->parser, TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(compiler, OP_RETURN);
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
            TokenType caseType = compiler->parser->previous.type;
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
        patchJump(compiler, previousCaseSkip);
        emitByte(compiler, OP_POP);
    }

    for (int i = 0; i < caseCount; i++) {
        patchJump(compiler, caseEnds[i]);
    }

    emitByte(compiler, OP_POP);
}

static void whileStatement(Compiler* compiler) {
    int loopStart = currentChunk(compiler)->count;
    consume(compiler->parser, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression(compiler);
    consume(compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    emitByte(compiler, OP_POP);
    statement(compiler);
    emitLoop(compiler, loopStart);

    patchJump(compiler, exitJump);
    emitByte(compiler, OP_POP);
}

static void declaration(Compiler* compiler) {
    if (match(compiler->parser, TOKEN_CLASS)) {
        classDeclaration(compiler);
    }
    else if (check(compiler->parser, TOKEN_FUN) && checkNext(compiler->parser, TOKEN_IDENTIFIER)) {
        advance(compiler->parser);
        funDeclaration(compiler);
    }
    else if (match(compiler->parser, TOKEN_VAR)) {
        varDeclaration(compiler);
    }
    else {
        statement(compiler);
    }
    if (compiler->parser->panicMode) synchronize(compiler->parser);
}

static void statement(Compiler* compiler) {
    if (match(compiler->parser, TOKEN_FOR)) {
        forStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_IF)) {
        ifStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_RETURN)) {
        returnStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_SWITCH)) {
        switchStatement(compiler);
    }
    else if (match(compiler->parser, TOKEN_WHILE)) {
        whileStatement(compiler);
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
    initCompiler(&compiler, &parser, NULL, TYPE_SCRIPT);

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
        compiler = compiler->enclosing;
    }
}