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