#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "parser.h"
#include "../vm/debug.h"

typedef enum {
    COMPILE_TYPE_FUNCTION,
    COMPILE_TYPE_INITIALIZER,
    COMPILE_TYPE_LAMBDA,
    COMPILE_TYPE_METHOD,
    COMPILE_TYPE_SCRIPT
} CompileType;

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

typedef struct SwitchCompiler {
    struct SwitchCompiler* enclosing;
    int state;
    int caseEnds[MAX_CASES];
    int caseCount;
    int previousCaseSkip;
} SwitchCompiler;

typedef struct LoopCompiler {
    struct LoopCompiler* enclosing;
    int start;
    int exitJump;
    int scopeDepth;
} LoopCompiler;

typedef struct ClassCompiler {
    struct ClassCompiler* enclosing;
    Token name;
    Token superclass;
    BehaviorType type;
} ClassCompiler;

struct Compiler {
    VM* vm;
    struct Compiler* enclosing;
    CompileType type;
    ObjFunction* function;

    Local locals[UINT8_COUNT];
    int localCount;
    Upvalue upvalues[UINT8_COUNT];
    IDMap indexes;
    Token currentToken;
    ClassCompiler* currentClass;
    LoopCompiler* currentLoop;
    SwitchCompiler* currentSwitch;

    Token rootClass;
    int scopeDepth;
    bool isAsync;
    bool hadError;
};

static Chunk* currentChunk(Compiler* compiler) {
    return &compiler->function->chunk;
}

static int currentLine(Compiler* compiler) {
    return compiler->currentToken.line;
}

static void compileError(Compiler* compiler, const char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[line %d] Compile Error: ", currentLine(compiler));
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);
    compiler->hadError = true;
}

static void emitByte(Compiler* compiler, uint8_t byte) {
    writeChunk(compiler->vm, currentChunk(compiler), byte, currentLine(compiler));
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

static void emitLoop(Compiler* compiler) {
    emitByte(compiler, OP_LOOP);

    int offset = currentChunk(compiler)->count - compiler->currentLoop->start + 2;
    if (offset > UINT16_MAX) compileError(compiler, "Loop body too large.");

    emitByte(compiler, (offset >> 8) & 0xff);
    emitByte(compiler, offset & 0xff);
}

static void emitReturn(Compiler* compiler, uint8_t depth) {
    if (compiler->type == COMPILE_TYPE_INITIALIZER) emitBytes(compiler, OP_GET_LOCAL, 0);
    else emitByte(compiler, OP_NIL);

    if (depth == 0) emitByte(compiler, OP_RETURN);
    else emitBytes(compiler, OP_RETURN_NONLOCAL, depth);
}

static uint8_t makeConstant(Compiler* compiler, Value value) {
    int constant = addConstant(compiler->vm, currentChunk(compiler), value);
    if (constant > UINT8_MAX) {
        compileError(compiler, "Too many constants in one chunk.");
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
        compileError(compiler, "Too much code to jump over.");
    }

    currentChunk(compiler)->code[offset] = (jump >> 8) & 0xff;
    currentChunk(compiler)->code[offset + 1] = jump & 0xff;
}

static void patchAddress(Compiler* compiler, int offset) {
    currentChunk(compiler)->code[offset] = (currentChunk(compiler)->count >> 8) & 0xff;
    currentChunk(compiler)->code[offset + 1] = currentChunk(compiler)->count & 0xff;
}

static void initClassCompiler(Compiler* compiler, ClassCompiler* klass, Token name, BehaviorType type) {
    klass->enclosing = compiler->currentClass;
    klass->name = name;
    klass->type = type;
    klass->superclass = compiler->rootClass;
    compiler->currentClass = klass;
}

static void endClassCompiler(Compiler* compiler) {
    compiler->currentClass = compiler->currentClass->enclosing;
}

static void initLoopCompiler(Compiler* compiler, LoopCompiler* loop) {
    loop->enclosing = compiler->currentLoop;
    loop->start = currentChunk(compiler)->count;
    loop->exitJump = -1;
    loop->scopeDepth = compiler->scopeDepth;
    compiler->currentLoop = loop;
}

static void endLoopCompiler(Compiler* compiler) {
    int offset = compiler->currentLoop->start;
    Chunk* chunk = currentChunk(compiler);
    while (offset < chunk->count) {
        if (chunk->code[offset] == OP_END) {
            chunk->code[offset] = OP_JUMP;
            patchJump(compiler, offset + 1);
        }
        else offset += opCodeOffset(chunk, offset);
    }
    compiler->currentLoop = compiler->currentLoop->enclosing;
}

static void initSwitchCompiler(Compiler* compiler, SwitchCompiler* _switch) {
    _switch->enclosing = compiler->currentSwitch;
    _switch->state = 1;
    _switch->caseCount = 0;
    _switch->previousCaseSkip = -1;
    compiler->currentSwitch = _switch;
}

static void endSwitchCompiler(Compiler* compiler) {
    if (compiler->currentSwitch->state == 1) {
        compiler->currentSwitch->caseEnds[compiler->currentSwitch->caseCount++] = emitJump(compiler, OP_JUMP);
        patchJump(compiler, compiler->currentSwitch->previousCaseSkip);
        emitByte(compiler, OP_POP);
    }

    for (int i = 0; i < compiler->currentSwitch->caseCount; i++) {
        patchJump(compiler, compiler->currentSwitch->caseEnds[i]);
    }

    emitByte(compiler, OP_POP);
    compiler->currentSwitch = compiler->currentSwitch->enclosing;
}

static void initCompiler(VM* vm, Compiler* compiler, Compiler* enclosing, CompileType type, Token* name, bool isAsync) {
    compiler->vm = vm;
    compiler->enclosing = enclosing;
    compiler->type = type;
    compiler->function = NULL;
    compiler->currentClass = NULL;
    compiler->currentLoop = NULL;
    compiler->currentSwitch = NULL;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->hadError = false;
    compiler->rootClass = syntheticToken("Object");

    compiler->isAsync = isAsync;
    compiler->function = newFunction(vm);
    compiler->function->isAsync = isAsync;
    if (type != COMPILE_TYPE_SCRIPT) compiler->function->name = copyString(vm, name->start, name->length);
    initIDMap(&compiler->indexes);
    vm->compiler = compiler;

    if (enclosing != NULL) {
        compiler->currentClass = enclosing->currentClass;
        compiler->currentLoop = enclosing->currentLoop;
        compiler->currentSwitch = enclosing->currentSwitch;
    }

    Local* local = &compiler->locals[compiler->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    local->isMutable = false;

    if (type != COMPILE_TYPE_FUNCTION && type != COMPILE_TYPE_LAMBDA) {
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
    if (!compiler->hadError) {
        disassembleChunk(currentChunk(compiler), function->name != NULL ? function->name->chars : "<script>");
    }
#endif

    freeIDMap(compiler->vm, &compiler->indexes);
    compiler->vm->compiler = compiler->enclosing;
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
        else emitByte(compiler, OP_POP);
        compiler->localCount--;
    }
}

static uint8_t makeIdentifier(Compiler* compiler, Value value) {
    ObjString* name = AS_STRING(value);
    int identifier;
    if (!idMapGet(&compiler->indexes, name, &identifier)) {
        identifier = addIdentifier(compiler->vm, currentChunk(compiler), value);
        if (identifier > UINT8_MAX) {
            compileError(compiler, "Too many identifiers in one chunk.");
            return 0;
        }
        idMapSet(compiler->vm, &compiler->indexes, name, identifier);
    }

    return (uint8_t)identifier;
}

static uint8_t identifierConstant(Compiler* compiler, Token* name) {
    const char* start = name->start[0] != '`' ? name->start : name->start + 1;
    int length = name->start[0] != '`' ? name->length : name->length - 2;
    return makeIdentifier(compiler, OBJ_VAL(copyString(compiler->vm, start, length)));
}

static ObjString* identifierName(Compiler* compiler, uint8_t arg) {
    return AS_STRING(currentChunk(compiler)->identifiers.values[arg]);
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
                compileError(compiler, "Can't read local variable in its own initializer.");
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
        compileError(compiler, "Too many closure variables in function.");
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
        compileError(compiler, "Too many local variables in function.");
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
    for (; i >= 0 && compiler->locals[i].depth > compiler->currentLoop->scopeDepth; i--) {
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
    int slot = makeIdentifier(compiler, OBJ_VAL(copyString(compiler->vm, name, length)));
    emitByte(compiler, OP_INVOKE);
    emitByte(compiler, slot);
    emitByte(compiler, args);
}

static void declareVariable(Compiler* compiler, Token* name) {
    if (compiler->scopeDepth == 0) return;

    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (local->depth != -1 && local->depth < compiler->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            compileError(compiler, "Already a variable with this name in this scope.");
        }
    }
    addLocal(compiler, *name);
}

static uint8_t makeVariable(Compiler* compiler, Token* name, const char* errorMessage) {
    declareVariable(compiler, name);
    if (compiler->scopeDepth > 0) return 0;
    return identifierConstant(compiler, name);
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
        if (idMapGet(&compiler->vm->currentModule->varIndexes, name, &index)) {
            compileError(compiler, "Cannot redeclare global variable.");
        }

        if (isMutable) {
            idMapSet(compiler->vm, &compiler->vm->currentModule->varIndexes, name, compiler->vm->currentModule->varFields.count);
            valueArrayWrite(compiler->vm, &compiler->vm->currentModule->varFields, NIL_VAL);
            emitBytes(compiler, OP_DEFINE_GLOBAL_VAR, global);
        }
        else {
            idMapSet(compiler->vm, &compiler->vm->currentModule->valIndexes, name, compiler->vm->currentModule->valFields.count);
            valueArrayWrite(compiler->vm, &compiler->vm->currentModule->valFields, NIL_VAL);
            emitBytes(compiler, OP_DEFINE_GLOBAL_VAL, global);
        }
    }
}

static uint8_t argumentList(Compiler* compiler, Ast* ast) {
    uint8_t argCount = 0;
    int numChild = astNumChild(ast);
    while (argCount < numChild) {
        compileChild(compiler, ast, argCount);
        argCount++;
    }
    return argCount;
}

static void integer(Compiler* compiler, Token token) {
    int value = strtol(token.start, NULL, 10);
    emitConstant(compiler, INT_VAL(value));
}

static void number(Compiler* compiler, Token token) {
    double value = strtod(token.start, NULL);
    emitConstant(compiler, NUMBER_VAL(value));
}

static void string(Compiler* compiler, Token token) {
    char* string = tokenToCString(token);
    emitConstant(compiler, OBJ_VAL(takeString(compiler->vm, string, token.length)));
}

static void checkMutability(Compiler* compiler, int arg, uint8_t opCode) {
    switch (opCode) {
        case OP_SET_LOCAL:
            if (!compiler->locals[arg].isMutable) {
                compileError(compiler, "Cannot assign to immutable local variable.");
            }
            break;
        case OP_SET_UPVALUE:
            if (!compiler->upvalues[arg].isMutable) {
               compileError(compiler, "Cannot assign to immutable captured upvalue.");
            }
            break;
        case OP_SET_GLOBAL: {
            ObjString* name = identifierName(compiler, arg);
            int index;
            if (idMapGet(&compiler->vm->currentModule->valIndexes, name, &index)) {
                compileError(compiler, "Cannot assign to immutable global variables.");
            }
            break;
        }
        default: break;
    }
}

static void getVariable(Compiler* compiler, Token* token) {
    int arg = resolveLocal(compiler, token);
    if (arg != -1) {
        emitBytes(compiler, OP_GET_LOCAL, (uint8_t)arg);
    }
    else if ((arg = resolveUpvalue(compiler, token)) != -1) {
        emitBytes(compiler, OP_GET_UPVALUE, (uint8_t)arg);
    }
    else {
        arg = identifierConstant(compiler, token);
        emitBytes(compiler, OP_GET_GLOBAL, (uint8_t)arg);
    }
}

static void parameters(Compiler* compiler, Ast* ast) {
    if (!astHasChild(ast)) return;
    for (int i = 0; i < ast->children->count; i++) {
        compileChild(compiler, ast, i);
    }
}

static uint8_t lambdaDepth(Compiler* compiler) {
    uint8_t depth = 1;
    Compiler* current = compiler->enclosing;
    while (current->type == COMPILE_TYPE_LAMBDA) {
        depth++;
        current = current->enclosing;
    }
    return depth;
}

static void block(Compiler* compiler, Ast* ast) {
    Ast* stmts = astGetChild(ast, 0);
    for (int i = 0; i < stmts->children->count; i++) {
        compileChild(compiler, stmts, i);
    }
}

static void function(Compiler* enclosing, CompileType type, Ast* ast, bool isAsync) {
    Compiler compiler;
    initCompiler(enclosing->vm, &compiler, enclosing, type, &ast->token, isAsync);
    beginScope(&compiler);

    parameters(&compiler, astGetChild(ast, 0));
    block(&compiler, astGetChild(ast, 1));
    ObjFunction* function = endCompiler(&compiler);
    emitBytes(enclosing, OP_CLOSURE, makeIdentifier(enclosing, OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(enclosing, compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(enclosing, compiler.upvalues[i].index);
    }
}

static void behavior(Compiler* compiler, BehaviorType type, Ast* ast) {
    Token name = ast->token;
    bool isAnonymous = (name.type == TOKEN_EMPTY && name.length == 1);
    if (isAnonymous) {
        emitBytes(compiler, OP_ANONYMOUS, type);
        emitByte(compiler, OP_DUP);
    }

    ClassCompiler classCompiler;
    initClassCompiler(compiler, &classCompiler, name, type);
    int childIndex = 0;

    if (type == BEHAVIOR_CLASS) {
        Ast* superClass = astGetChild(ast, childIndex);
        compiler->currentClass->superclass = superClass->token;
        compileChild(compiler, ast, childIndex);
        childIndex++;
        if (identifiersEqual(&name, &compiler->rootClass)) {
            compileError(compiler, "Cannot redeclare root class Object.");
        }
        if (identifiersEqual(&name, &superClass->token)) {
            compileError(compiler, "A class cannot inherit from itself.");
        }
    }

    beginScope(compiler);
    addLocal(compiler, syntheticToken("super"));
    defineVariable(compiler, 0, false);
    if (type == BEHAVIOR_CLASS) emitByte(compiler, OP_INHERIT);

    Ast* traitList = astGetChild(ast, childIndex);
    uint8_t traitCount = astNumChild(traitList);
    if (traitCount > 0) {
        compileChild(compiler, ast, childIndex);
        emitBytes(compiler, OP_IMPLEMENT, traitCount);
    }

    childIndex++; 
    compileChild(compiler, ast, childIndex);
    endScope(compiler);
    endClassCompiler(compiler);
}

static uint8_t super_(Compiler* compiler, Ast* ast) {
    if (compiler->currentClass == NULL) {
        compileError(compiler, "Cannot use 'super' outside of a class.");
    }
    uint8_t index = identifierConstant(compiler, &ast->token);
    Token _this = syntheticToken("this");
    getVariable(compiler, &_this);
    return index;
}

static void compileAnd(Compiler* compiler, Ast* ast) { 
    compileChild(compiler, ast, 0);
    int endJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    emitByte(compiler, OP_POP);

    compileChild(compiler, ast, 1);
    patchJump(compiler, endJump);
}

static void compileArray(Compiler* compiler, Ast* ast) {
    uint8_t elementCount = 0;
    if (astHasChild(ast)) {
        Ast* elements = astGetChild(ast, 0);
        while (elementCount < elements->children->count) {
            compileChild(compiler, elements, elementCount);
            elementCount++;
        }
    }
    emitBytes(compiler, OP_ARRAY, elementCount);
}

static void compileAssign(Compiler* compiler, Ast* ast) {
    uint8_t setOp;
    int arg = resolveLocal(compiler, &ast->token);
    if (arg != -1) {
        setOp = OP_SET_LOCAL;
    }
    else if ((arg = resolveUpvalue(compiler, &ast->token)) != -1) {
        setOp = OP_SET_UPVALUE;
    }
    else {
        arg = identifierConstant(compiler, &ast->token);
        setOp = OP_SET_GLOBAL;
    }

    checkMutability(compiler, arg, setOp);
    compileChild(compiler, ast, 0);
    emitBytes(compiler, setOp, (uint8_t)arg);
}

static void compileAwait(Compiler* compiler, Ast* ast) {
    if (compiler->type == COMPILE_TYPE_SCRIPT) {
        compiler->isAsync = true;
        compiler->function->isAsync = true;
    }
    else if (!compiler->isAsync) compileError(compiler, "Cannot use await unless in top level code or inside async functions/methods.");
    compileChild(compiler, ast, 0);
    emitByte(compiler, OP_AWAIT);
}

static void compileBinary(Compiler* compiler, Ast* ast) {
    compileChild(compiler, ast, 0);
    compileChild(compiler, ast, 1);
    TokenSymbol operatorType = ast->token.type;
    
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

static void compileCall(Compiler* compiler, Ast* ast) {
    compileChild(compiler, ast, 0);
    Ast* args = astGetChild(ast, 1);
    uint8_t argCount = argumentList(compiler, args);
    emitBytes(compiler, OP_CALL, argCount);
}

static void compileClass(Compiler* compiler, Ast* ast) {
    behavior(compiler, BEHAVIOR_CLASS, ast);
}

static void compileDictionary(Compiler* compiler, Ast* ast) {
    uint8_t entryCount = 0;
    Ast* keys = astGetChild(ast, 0);
    Ast* values = astGetChild(ast, 1);
    while (entryCount < keys->children->count) {
        compileChild(compiler, keys, entryCount);
        compileChild(compiler, values, entryCount);
        entryCount++;
    }
    emitBytes(compiler, OP_DICTIONARY, entryCount);
}

static void compileFunction(Compiler* compiler, Ast* ast) {
    CompileType type = ast->modifier.isLambda ? COMPILE_TYPE_LAMBDA : COMPILE_TYPE_FUNCTION;
    bool isAsync = ast->modifier.isAsync;
    function(compiler, type, ast, isAsync);
}

static void compileGrouping(Compiler* compiler, Ast* ast) {
    compileChild(compiler, ast, 0);
}

static void compileInterpolation(Compiler* compiler, Ast* ast) {
    Ast* exprs = astGetChild(ast, 0);
    int count = 0;
    while (count < exprs->children->count) {
        bool concatenate = false;
        bool isString = false;
        Ast* expr = astGetChild(exprs, count);

        if (expr->type == AST_EXPR_LITERAL && expr->token.type == TOKEN_STRING) {
            compileChild(compiler, exprs, count);
            if (count > 0) emitByte(compiler, OP_ADD);
            concatenate = true;
            isString = true;
            count++;            
            if (count >= exprs->children->count) break;
        }

        compileChild(compiler, exprs, count);
        invokeMethod(compiler, 0, "toString", 8);
        if (concatenate || (count >= 1 && !isString)) {
            emitByte(compiler, OP_ADD);
        }
        count++;
    }
}

static void compileInvoke(Compiler* compiler, Ast* ast) {
    compileChild(compiler, ast, 0);
    Ast* args = astGetChild(ast, 1);
    uint8_t methodIndex = identifierConstant(compiler, &ast->token);
    uint8_t argCount = argumentList(compiler, args);
    emitBytes(compiler, OP_INVOKE, methodIndex);
    emitByte(compiler, argCount);
}

static void compileLiteral(Compiler* compiler, Ast* ast) {
    switch (ast->token.type) {
        case TOKEN_NIL: emitByte(compiler, OP_NIL); break;
        case TOKEN_TRUE: emitByte(compiler, OP_TRUE); break;
        case TOKEN_FALSE: emitByte(compiler, OP_FALSE); break;
        case TOKEN_INT: integer(compiler, ast->token); break;
        case TOKEN_NUMBER: number(compiler, ast->token); break;
        case TOKEN_STRING: string(compiler, ast->token); break;
        default: compileError(compiler, "Invalid AST literal type.");
    }
}

static void compileNil(Compiler* compiler, Ast* ast) {
    // To be implemented
}

static void compileOr(Compiler* compiler, Ast* ast) {
    compileChild(compiler, ast, 0);
    int elseJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    int endJump = emitJump(compiler, OP_JUMP);
    patchJump(compiler, elseJump);

    emitByte(compiler, OP_POP);
    compileChild(compiler, ast, 1);
    patchJump(compiler, endJump);
}

static void compileParam(Compiler* compiler, Ast* ast) {
    if (ast->modifier.isVariadic) compiler->function->arity = -1;
    else compiler->function->arity++;
    uint8_t constant = makeVariable(compiler, &ast->token, "Expect parameter name.");
    defineVariable(compiler, constant, ast->modifier.isMutable);
}

static void compilePropertyGet(Compiler* compiler, Ast* ast) {
    compileChild(compiler, ast, 0);
    uint8_t index = identifierConstant(compiler, &ast->token);
    emitBytes(compiler, OP_GET_PROPERTY, index);
}

static void compilePropertySet(Compiler* compiler, Ast* ast) {
    compileChild(compiler, ast, 0);
    uint8_t index = identifierConstant(compiler, &ast->token);
    compileChild(compiler, ast, 1);
    emitBytes(compiler, OP_SET_PROPERTY, index);
}

static void compileSubscriptGet(Compiler* compiler, Ast* ast) {
    compileChild(compiler, ast, 0);
    compileChild(compiler, ast, 1);
    emitByte(compiler, OP_GET_SUBSCRIPT);
}

static void compileSubscriptSet(Compiler* compiler, Ast* ast) {
    compileChild(compiler, ast, 0);
    compileChild(compiler, ast, 1);
    compileChild(compiler, ast, 2);
    emitByte(compiler, OP_SET_SUBSCRIPT);
}

static void compileSuperGet(Compiler* compiler, Ast* ast) {
    uint8_t index = super_(compiler, ast);
    getVariable(compiler, &compiler->currentClass->superclass);
    emitBytes(compiler, OP_GET_SUPER, index);
}

static void compileSuperInvoke(Compiler* compiler, Ast* ast) {
    uint8_t index = super_(compiler, ast);
    Ast* args = astGetChild(ast, 0);
    uint8_t argCount = argumentList(compiler, args);
    getVariable(compiler, &compiler->currentClass->superclass);
    emitBytes(compiler, OP_SUPER_INVOKE, index);
    emitByte(compiler, argCount);
}

static void compileThis(Compiler* compiler, Ast* ast) {
    if (compiler->currentClass == NULL) {
        compileError(compiler, "Cannot use 'this' outside of a class.");
    }
    getVariable(compiler, &ast->token);
}

static void compileTrait(Compiler* compiler, Ast* ast) {
    behavior(compiler, BEHAVIOR_TRAIT, ast);
}

static void compileUnary(Compiler* compiler, Ast* ast) {
    compileChild(compiler, ast, 0);
    TokenSymbol operatorType = ast->token.type;
    switch (operatorType) {
        case TOKEN_BANG: emitByte(compiler, OP_NOT); break;
        case TOKEN_MINUS: emitByte(compiler, OP_NEGATE); break;
        default: return;
    }
}

static void compileVariable(Compiler* compiler, Ast* ast) {
    getVariable(compiler, &ast->token);
}

static void compileYield(Compiler* compiler, Ast* ast) {
    // To be implemented
}

static void compileExpression(Compiler* compiler, Ast* ast) {
    switch (ast->type) {
        case AST_EXPR_AND:
            compileAnd(compiler, ast);
            break;
        case AST_EXPR_ARRAY:
            compileArray(compiler, ast);
            break;
        case AST_EXPR_ASSIGN:
            compileAssign(compiler, ast);
            break;
        case AST_EXPR_AWAIT:
            compileAwait(compiler, ast);
            break;
        case AST_EXPR_BINARY:
            compileBinary(compiler, ast);
            break;
        case AST_EXPR_CALL:
            compileCall(compiler, ast);
            break;
        case AST_EXPR_CLASS:
            compileClass(compiler, ast);
            break;
        case AST_EXPR_DICTIONARY:
            compileDictionary(compiler, ast);
            break;
        case AST_EXPR_FUNCTION:
            compileFunction(compiler, ast);
            break;
        case AST_EXPR_GROUPING:
            compileGrouping(compiler, ast);
            break;
        case AST_EXPR_INTERPOLATION:
            compileInterpolation(compiler, ast);
            break;
        case AST_EXPR_INVOKE:
            compileInvoke(compiler, ast);
            break;
        case AST_EXPR_LITERAL:
            compileLiteral(compiler, ast);
            break;
        case AST_EXPR_NIL:
            compileNil(compiler, ast);
            break;
        case AST_EXPR_OR:
            compileOr(compiler, ast);
            break;
        case AST_EXPR_PARAM:
            compileParam(compiler, ast);
            break;
        case AST_EXPR_PROPERTY_GET:
            compilePropertyGet(compiler, ast);
            break;
        case AST_EXPR_PROPERTY_SET:
            compilePropertySet(compiler, ast);
            break;
        case AST_EXPR_SUBSCRIPT_GET:
            compileSubscriptGet(compiler, ast);
            break;
        case AST_EXPR_SUBSCRIPT_SET:
            compileSubscriptSet(compiler, ast);
            break;
        case AST_EXPR_SUPER_GET:
            compileSuperGet(compiler, ast);
            break;
        case AST_EXPR_SUPER_INVOKE:
            compileSuperInvoke(compiler, ast);
            break;
        case AST_EXPR_THIS:
            compileThis(compiler, ast);
            break;
        case AST_EXPR_TRAIT:
            compileTrait(compiler, ast);
            break;
        case AST_EXPR_UNARY:
            compileUnary(compiler, ast);
            break;
        case AST_EXPR_VARIABLE:
            compileVariable(compiler, ast);
            break;
        case AST_EXPR_YIELD:
            compileYield(compiler, ast);
            break;
        default:
            compileError(compiler, "Invalid AST expression type.");
    }
}

static void compileAwaitStatement(Compiler* compiler, Ast* ast) {
    if (compiler->type == COMPILE_TYPE_SCRIPT) {
        compiler->isAsync = true;
        compiler->function->isAsync = true;
    }
    else if (!compiler->isAsync) {
        compileError(compiler, "Can only use 'await' in async methods or top level code.");
    }

    compileChild(compiler, ast, 0);
    emitBytes(compiler, OP_AWAIT, OP_POP);
}

static void compileBlockStatement(Compiler* compiler, Ast* ast) {
    beginScope(compiler);
    block(compiler, ast);
    endScope(compiler);
}

static void compileBreakStatement(Compiler* compiler, Ast* ast) {
    if (compiler->currentLoop == NULL) {
        compileError(compiler, "Cannot use 'break' outside of a loop.");
    }
    discardLocals(compiler);
    emitJump(compiler, OP_END);
}

static void compileCaseStatement(Compiler* compiler, Ast* ast) {
    if (compiler->currentSwitch == NULL) {
        compileError(compiler, "Cannot use 'case' outside of a switch.");
        return;
    }

    emitByte(compiler, OP_DUP);
    compileChild(compiler, ast, 0);
    emitByte(compiler, OP_EQUAL);
    compiler->currentSwitch->previousCaseSkip = emitJump(compiler, OP_JUMP_IF_FALSE);
    emitByte(compiler, OP_POP);
    compileChild(compiler, ast, 1);

    compiler->currentSwitch->caseEnds[compiler->currentSwitch->caseCount++] = emitJump(compiler, OP_JUMP);
    patchJump(compiler, compiler->currentSwitch->previousCaseSkip);
    emitByte(compiler, OP_POP);
}

static void compileCatchStatement(Compiler* compiler, Ast* ast) {
    // To be implemented
}

static void compileContinueStatement(Compiler* compiler, Ast* ast) {
    if (compiler->currentLoop == NULL) {
        compileError(compiler, "Cannot use 'continue' outside of a loop.");
    }
    discardLocals(compiler);
    emitLoop(compiler);
}

static void compileDefaultStatement(Compiler* compiler, Ast* ast) {
    compileChild(compiler, ast, 0);
    compiler->currentSwitch->state = 2;
    compiler->currentSwitch->previousCaseSkip = -1;
}

static void compileExpressionStatement(Compiler* compiler, Ast* ast) {
    compileChild(compiler, ast, 0);
    if (compiler->type == COMPILE_TYPE_LAMBDA && ast->sibling == NULL) {
        emitByte(compiler, OP_RETURN);
    }
    else emitByte(compiler, OP_POP);
}

static void compileFinallyStatement(Compiler* compiler, Ast* ast) {
    // To be implemented
}

static void compileForStatement(Compiler* compiler, Ast* ast) {
    beginScope(compiler);
    Token indexToken, valueToken;
    Ast* decl = astGetChild(ast, 0);

    if (decl->children->count > 1) {
        indexToken = decl->children->elements[0]->token;
        valueToken = decl->children->elements[1]->token;
    }
    else {
        indexToken = syntheticToken("index ");
        valueToken = decl->children->elements[0]->token;
    }

    compileChild(compiler, ast, 1);
    if (compiler->localCount + 3 > UINT8_MAX) {
        compileError(compiler, "for loop can only contain up to 252 variables.");
    }

    int collectionSlot = addLocal(compiler, syntheticToken("collection "));
    emitByte(compiler, OP_NIL);
    int indexSlot = addLocal(compiler, indexToken);
    markInitialized(compiler, true);

    LoopCompiler* outerLoop = compiler->currentLoop;
    LoopCompiler innerLoop;
    initLoopCompiler(compiler, &innerLoop);
    getLocal(compiler, collectionSlot);
    getLocal(compiler, indexSlot);

    invokeMethod(compiler, 1, "next", 4);
    setLocal(compiler, indexSlot);
    emitByte(compiler, OP_POP);
    compiler->currentLoop->exitJump = emitJump(compiler, OP_JUMP_IF_EMPTY);

    getLocal(compiler, collectionSlot);
    getLocal(compiler, indexSlot);
    invokeMethod(compiler, 1, "nextValue", 9);

    beginScope(compiler);
    int valueSlot = addLocal(compiler, valueToken);
    markInitialized(compiler, false);
    setLocal(compiler, valueSlot);
    compileChild(compiler, ast, 2);
    endScope(compiler);

    emitLoop(compiler);
    patchJump(compiler, compiler->currentLoop->exitJump);
    endLoopCompiler(compiler);
    emitByte(compiler, OP_POP);
    emitByte(compiler, OP_POP);

    compiler->localCount -= 2;
    compiler->currentLoop = outerLoop;
    endScope(compiler);
}

static void compileIfStatement(Compiler* compiler, Ast* ast) {
    compileChild(compiler, ast, 0);
    int thenJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    emitByte(compiler, OP_POP);
    compileChild(compiler, ast, 1);

    int elseJump = emitJump(compiler, OP_JUMP);
    patchJump(compiler, thenJump);
    emitByte(compiler, OP_POP);

    if (ast->children->count > 2) compileChild(compiler, ast, 2);
    patchJump(compiler, elseJump);
}

static void compileRequireStatement(Compiler* compiler, Ast* ast) {
    if (compiler->type != COMPILE_TYPE_SCRIPT) {
        compileError(compiler, "Can only require source files from top-level code.");
    }
    compileChild(compiler, ast, 0);
    emitByte(compiler, OP_REQUIRE);
}

static void compileReturnStatement(Compiler* compiler, Ast* ast) {
    if (compiler->type == COMPILE_TYPE_SCRIPT) {
        compileError(compiler, "Can't return from top-level code.");
    }
    else if (compiler->type == COMPILE_TYPE_INITIALIZER) {
        compileError(compiler, "Cannot return value from an initializer.");
    }

    uint8_t depth = 0;
    if (compiler->type == COMPILE_TYPE_LAMBDA) depth = lambdaDepth(compiler);
    if (astHasChild(ast)) {
        compileChild(compiler, ast, 0);
        if (compiler->type == COMPILE_TYPE_LAMBDA) emitBytes(compiler, OP_RETURN_NONLOCAL, depth);
        else emitByte(compiler, OP_RETURN);
    }
    else emitReturn(compiler, depth);
}

static void compileSwitchStatement(Compiler* compiler, Ast* ast) {
    compileChild(compiler, ast, 0);
    Ast* caseList = astGetChild(ast, 1);
    SwitchCompiler innerSwitch;
    initSwitchCompiler(compiler, &innerSwitch);

    for (int i = 0; i < caseList->children->count; i++) {
        compileChild(compiler, caseList, i);
    }

    if (caseList->children->count > 2) {
        compileChild(compiler, ast, 2);
    }

    endSwitchCompiler(compiler);
}

static void compileThrowStatement(Compiler* compiler, Ast* ast) {
    compileChild(compiler, ast, 0);
    emitByte(compiler, OP_THROW);
}

static void compileTryStatement(Compiler* compiler, Ast* ast) {
    // To be implemented
}

static void compileUsingStatement(Compiler* compiler, Ast* ast) {
    Ast* _namespace = astGetChild(ast, 0);
    int namespaceDepth = astNumChild(_namespace);
    uint8_t index;
    for (int i = 0; i < namespaceDepth; i++) {
        Ast* subNamespace = astGetChild(_namespace, i);
        index = identifierConstant(compiler, &subNamespace->token);
        emitBytes(compiler, OP_NAMESPACE, index);
    }
    emitBytes(compiler, OP_GET_NAMESPACE, namespaceDepth);

    index = makeIdentifier(compiler, OBJ_VAL(emptyString(compiler->vm)));
    if (astNumChild(ast) > 1) {
        Ast* alias = astGetChild(ast, 1);
        index = identifierConstant(compiler, &alias->token);
    }
    emitBytes(compiler, OP_USING_NAMESPACE, index);
}

static void compileWhileStatement(Compiler* compiler, Ast* ast) {
    LoopCompiler* outerLoop = compiler->currentLoop;
    LoopCompiler innerLoop;
    initLoopCompiler(compiler, &innerLoop);

    compileChild(compiler, ast, 0);
    compiler->currentLoop->exitJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    emitByte(compiler, OP_POP);
    compileChild(compiler, ast, 1);
    emitLoop(compiler);

    patchJump(compiler, compiler->currentLoop->exitJump);
    emitByte(compiler, OP_POP);
    endLoopCompiler(compiler);
    compiler->currentLoop = outerLoop;
}

static void compileYieldStatement(Compiler* compiler, Ast* ast) {
    // To be implemented
}

static void compileStatement(Compiler* compiler, Ast* ast) {
    switch (ast->type) {
        case AST_STMT_AWAIT:
            compileAwaitStatement(compiler, ast);
            break;
        case AST_STMT_BLOCK:
            compileBlockStatement(compiler, ast);
            break;
        case AST_STMT_BREAK:
            compileBreakStatement(compiler, ast);
            break;
        case AST_STMT_CASE:
            compileCaseStatement(compiler, ast);
            break;
        case AST_STMT_CATCH:
            compileCatchStatement(compiler, ast);
            break;
        case AST_STMT_CONTINUE:
            compileContinueStatement(compiler, ast);
            break;
        case AST_STMT_DEFAULT:
            compileDefaultStatement(compiler, ast);
            break;
        case AST_STMT_EXPRESSION:
            compileExpressionStatement(compiler, ast);
            break;
        case AST_STMT_FINALLY:
            compileFinallyStatement(compiler, ast);
            break;
        case AST_STMT_FOR:
            compileForStatement(compiler, ast);
            break;
        case AST_STMT_IF:
            compileIfStatement(compiler, ast);
            break;
        case AST_STMT_REQUIRE:
            compileRequireStatement(compiler, ast);
            break;
        case AST_STMT_RETURN:
            compileReturnStatement(compiler, ast);
            break;
        case AST_STMT_SWITCH:
            compileSwitchStatement(compiler, ast);
            break;
        case AST_STMT_THROW:
            compileThrowStatement(compiler, ast);
            break;
        case AST_STMT_TRY:
            compileTryStatement(compiler, ast);
            break;
        case AST_STMT_USING:
            compileUsingStatement(compiler, ast);
            break;
        case AST_STMT_WHILE:
            compileWhileStatement(compiler, ast);
            break;
        case AST_STMT_YIELD:
            compileYieldStatement(compiler, ast);
            break;
        default:
            compileError(compiler, "Invalid AST statement type.");
    }
}

static void compileClassDeclaration(Compiler* compiler, Ast* ast) {
    Token* name = &ast->token;
    uint8_t index = identifierConstant(compiler, name);
    declareVariable(compiler, name);
    emitBytes(compiler, OP_CLASS, index);
    compileChild(compiler, ast, 0);
}

static void compileFunDeclaration(Compiler* compiler, Ast* ast) {
    uint8_t index = makeVariable(compiler, &ast->token, "Expect function name.");
    markInitialized(compiler, false);
    compileChild(compiler, ast, 0);
    defineVariable(compiler, index, false);
}

static void compileMethodDeclaration(Compiler* compiler, Ast* ast) {
    uint8_t index = identifierConstant(compiler, &ast->token);
    CompileType type = ast->modifier.isInitializer ? COMPILE_TYPE_INITIALIZER : COMPILE_TYPE_METHOD;
    function(compiler, type, ast, ast->modifier.isAsync);
    emitBytes(compiler, ast->modifier.isClass ? OP_CLASS_METHOD : OP_INSTANCE_METHOD, index);
}

static void compileNamespaceDeclaration(Compiler* compiler, Ast* ast) {
    uint8_t namespaceDepth = 0;
    Ast* identifiers = astGetChild(ast, 0);
    while (namespaceDepth < identifiers->children->count) {
        Ast* identifier = astGetChild(identifiers, namespaceDepth);
        ObjString* name = copyString(compiler->vm, identifier->token.start, identifier->token.length);
        emitBytes(compiler, OP_NAMESPACE, makeIdentifier(compiler, OBJ_VAL(name)));
        namespaceDepth++;
    }
    emitBytes(compiler, OP_DECLARE_NAMESPACE, namespaceDepth);
}

static void compileTraitDeclaration(Compiler* compiler, Ast* ast) {
    Token* name = &ast->token;
    uint8_t index = identifierConstant(compiler, name);
    declareVariable(compiler, name);
    emitBytes(compiler, OP_TRAIT, index);
    compileChild(compiler, ast, 0);
}

static void compileVarDeclaration(Compiler* compiler, Ast* ast) {
    uint8_t index = makeVariable(compiler, &ast->token, "Expect variable name.");
    bool hasValue = astHasChild(ast);

    if (hasValue) {
        compileChild(compiler, ast, 0);
    }
    else if (!ast->modifier.isMutable) {
        compileError(compiler, "Immutable variable must be initialized upon declaration.");
    }
    else emitByte(compiler, OP_NIL);

    defineVariable(compiler, index, ast->modifier.isMutable);
}

static void compileDeclaration(Compiler* compiler, Ast* ast) {
    switch (ast->type) {
        case AST_DECL_CLASS:
            compileClassDeclaration(compiler, ast);
            break;
        case AST_DECL_FUN:
            compileFunDeclaration(compiler, ast);
            break;
        case AST_DECL_METHOD:
            compileMethodDeclaration(compiler, ast);
            break;
        case AST_DECL_NAMESPACE:
            compileNamespaceDeclaration(compiler, ast);
            break;
        case AST_DECL_TRAIT:
            compileTraitDeclaration(compiler, ast);
            break;
        case AST_DECL_VAR:
            compileVarDeclaration(compiler, ast);
            break;
        default: 
            compileError(compiler, "Invalid AST declaration type.");
    }
}

void compileAst(Compiler* compiler, Ast* ast) {
    if (compiler->hadError) return;
    for (int i = 0; i < ast->children->count; i++) {
        compileChild(compiler, ast, i);
    }
}

void compileChild(Compiler* compiler, Ast* ast, int index) {
    Ast* child = astGetChild(ast, index);
    compiler->currentToken = child->token;
    switch (child->category) {
        case AST_CATEGORY_SCRIPT:
        case AST_CATEGORY_OTHER:
            compileAst(compiler, child);
            break;
        case AST_CATEGORY_EXPR:
            compileExpression(compiler, child);
            break;
        case AST_CATEGORY_STMT:
            compileStatement(compiler, child);
            break;
        case AST_CATEGORY_DECL:
            compileDeclaration(compiler, child);
            break;
        default:
            compileError(compiler, "Invalid AST category.");
    }
}

ObjFunction* compile(VM* vm, const char* source) {
    Lexer lexer;
    initLexer(&lexer, source);

    Parser parser;
    initParser(&parser, &lexer);
    Ast* ast = parse(&parser);
    if (parser.hadError) return NULL;
    
    Compiler compiler;
    initCompiler(vm, &compiler, NULL, COMPILE_TYPE_SCRIPT, NULL, false);
    compileAst(&compiler, ast);
    ObjFunction* function = endCompiler(&compiler);

    freeAst(ast, true);
    if (compiler.hadError) return NULL;
    return function;
}