#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "parser.h"

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
    COMPILE_TYPE_FUNCTION,
    COMPILE_TYPE_INITIALIZER,
    COMPILE_TYPE_LAMBDA,
    COMPILE_TYPE_METHOD,
    COMPILE_TYPE_SCRIPT
} CompileType;

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

    int scopeDepth;
    int innermostLoopStart;
    int innermostLoopScopeDepth;
    bool isAsync;
    bool hadError;
};

static Chunk* currentChunk(Compiler* compiler) {
    return &compiler->function->chunk;
}

static int currentLine(Compiler* compiler) {
    return compiler->currentToken.length;
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

static void emitLoop(Compiler* compiler, int loopStart) {
    emitByte(compiler, OP_LOOP);

    int offset = currentChunk(compiler)->count - loopStart + 2;
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

static void emitConstant(CompilerV1* compiler, Value value) {
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

static void endLoop(Compiler* compiler) {
    int offset = compiler->innermostLoopStart;
    Chunk* chunk = currentChunk(compiler);
    while (offset < chunk->count) {
        if (chunk->code[offset] == OP_END) {
            chunk->code[offset] = OP_JUMP;
            patchJump(compiler, offset + 1);
        }
        else offset += opCodeOffset(chunk, offset);
    }
}

static void initCompiler(VM* vm, Compiler* compiler, Compiler* enclosing, CompileType type, const char* name, bool isAsync) {
    compiler->vm = vm;
    compiler->enclosing = enclosing;
    compiler->type = type;
    compiler->function = NULL;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->innermostLoopStart = -1;
    compiler->innermostLoopScopeDepth = 0;
    compiler->hadError = false;

    compiler->isAsync = isAsync;
    compiler->function = newFunction(vm);
    compiler->function->isAsync = isAsync;
    if (type != COMPILE_TYPE_SCRIPT) compiler->function->name = newString(vm, name);
    initIDMap(&compiler->indexes);
    vm->currentCompiler = compiler;

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
    if (!compiler->parser->hadError) {
        disassembleChunk(currentChunk(compiler), function->name != NULL ? function->name->chars : "<script>");
    }
#endif

    freeIDMap(compiler->vm, &compiler->indexes);
    compiler->vm->currentCompiler = compiler->enclosing;
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

static ObjString* identifierName(CompilerV1* compiler, uint8_t arg) {
    return AS_STRING(currentChunk(compiler)->identifiers.values[arg]);
}

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static uint8_t propertyConstant(Compiler* compiler, const char* message) {
    // To be implemented
    return NULL;
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

static uint8_t compileVariable(Compiler* compiler, Token* name, const char* errorMessage) {
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

static void compileExpression(Compiler* compiler, Ast* ast) {
    // To be implemented
    return NULL;
}

static void compileStatement(Compiler* compiler, Ast* ast) {
    // To be implemented
    return NULL;
}

static void compileClassDeclaration(Compiler* compiler, Ast* ast) {
    // To be implemented
    return NULL;
}

static void compileFunDeclaration(Compiler* compiler, Ast* ast) {
    // To be implemented
    return NULL;
}

static void compileMethodDeclaration(Compiler* compiler, Ast* ast) {
    // To be implemented
    return NULL;
}

static void compileNamespaceDeclaration(Compiler* compiler, Ast* ast) {
    // To be implemented
    return NULL;
}

static void compileTraitDeclaration(Compiler* compiler, Ast* ast) {
    // To be implemented
    return NULL;
}

static void compileVarDeclaration(Compiler* compiler, Ast* ast) {
    // To be implemented
    return NULL;
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
            compileError(compiler, "Invalid declaration type.");
    }
}

static void compileChild(Compiler* compiler, Ast* ast) {
    switch (ast->category) {
        case AST_CATEGORY_SCRIPT:
            compileAst(compiler, ast);
            break;
        case AST_CATEGORY_EXPR:
            compileExpression(compiler, ast);
            break;
        case AST_CATEGORY_STMT:
            compileStatement(compiler, ast);
        case AST_CATEGORY_DECL:
            compileDeclaration(compiler, ast);
            break;
        default:
            break;
    }
}

void compileAst(Compiler* compiler, Ast* ast) {
    if (compiler->hadError || !AST_IS_ROOT(ast)) return;
    for (int i = 0; i < ast->children->count; i++) {
        Ast* decl = ast->children->elements[i];
        compileChild(compiler, decl);
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
    return function;
}