#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

DEFINE_BUFFER(AstArray, Ast*)

static void freeAstChildren(AstArray* children, bool freeChildren) {
    for (int i = 0; i < children->count; i++) {
        freeAst(children->elements[i], freeChildren);
    }
}

Ast* emptyAst(AstNodeType type, Token token) {
    Ast* ast = (Ast*)malloc(sizeof(Ast));
    if (ast != NULL) {
        ast->category = astNodeCategory(type);
        ast->type = type;
        ast->modifier = astInitModifier();
        ast->token = token;
        ast->parent = NULL;
        ast->children = (AstArray*)malloc(sizeof(AstArray));
        if (ast->children != NULL) AstArrayInit(ast->children);
    }
    return ast;
}

Ast* newAst(AstNodeType type, Token token, int numChildren, ...) {
    Ast* ast = emptyAst(type, token);
    va_list children;
    va_start(children, numChildren);
    for (int i = 0; i < numChildren; i++) {
        astAppendChild(ast, va_arg(children, Ast*));
    }
    va_end(children);
    return ast;
}

Ast* newAstWithChildren(AstNodeType type, Token token, AstArray* children) {
    Ast* ast = (Ast*)malloc(sizeof(Ast));
    if (ast != NULL) {
        ast->category = astNodeCategory(type);
        ast->type = type;
        ast->modifier = astInitModifier();
        ast->token = token;
        ast->parent = NULL;
        if (children == NULL) {
            ast->children = (AstArray*)malloc(sizeof(AstArray));
            if (ast->children != NULL) AstArrayInit(ast->children);
        }
        else ast->children = children;
    }
    return ast;
}

void freeAst(Ast* ast, bool freeChildren) {
    if (ast->children != NULL) {
        if (freeChildren) freeAstChildren(ast->children, freeChildren);
        AstArrayFree(ast->children);
        free(ast->children);
    }
    free(ast);
}

void astAppendChild(Ast* ast, Ast* child) {
    if (ast->children == NULL) {
        fprintf(stderr, "Not enough memory to add child AST node to parent.");
        exit(1);
    }
    AstArrayAdd(ast->children, child);
    if (child != NULL) child->parent = ast;
}

static char* astConcatOutput(char* source, char* dest) {
    size_t srcLength = strlen(source);
    size_t destLength = strlen(dest);
    char* result = bufferNewCharArray(srcLength + destLength);
    memcpy(result, source, srcLength);
    memcpy(result + srcLength, dest, destLength);
    result[srcLength + destLength] = '\0';
    return result;
}

static Ast* astGetChild(Ast* ast, int index) {
    if (ast->children == NULL || ast->children->count < index) {
        fprintf(stderr, "Ast has no children or invalid child index specified.");
        exit(1);
    }
    return ast->children->elements[index];
}

static bool astHasChild(Ast* ast) {
    return (ast->children != NULL && ast->children->count > 0);
}

static char* astIndent(int indentLevel) {
    char* buffer = "";
    for (int i = 0; i < indentLevel; i++) {
        buffer = astConcatOutput(buffer, "  ");
    }
    return buffer;
}

static char* astOutputChild(Ast* ast, int indentLevel, int index) {
    if (ast->children == NULL || ast->children->count < index) {
        fprintf(stderr, "Ast has no children or invalid child index specified.");
        exit(1);
    }
    Ast* expr = ast->children->elements[index];
    return astOutput(expr, indentLevel);
}

static char* astOutputExprArray(Ast* ast, int indentLevel) {
    size_t length = 8;
    char* buffer = bufferNewCharArray(length);
    Ast* argList = ast->children->elements[1];
    sprintf_s(buffer, length, "(array [");

    for (int i = 0; i < argList->children->count; i++) {
        char* arg = astOutputChild(ast, indentLevel, i);
        buffer = astConcatOutput(buffer, arg);
        buffer = astConcatOutput(buffer, " ");
    }
    buffer = astConcatOutput(buffer, "]");
    return buffer;
}

static char* astOutputExprAssign(Ast* ast, int indentLevel) {
    char* left = astOutputChild(ast, indentLevel, 0);
    char* right = astOutputChild(ast, indentLevel, 1);
    size_t length = strlen(left) + strlen(right) + 10;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(assign %s %s)", left, right);
    return buffer;
}

static char* astOutputExprAwait(Ast* ast, int indentLevel) {
    char* exprOutput = astOutputChild(ast, indentLevel, 0);;
    size_t length = (size_t)ast->token.length + 8;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(await %s)", exprOutput);
    return buffer;
}

static char* astOutputExprBinary(Ast* ast, int indentLevel) {
    char* op = tokenToString(ast->token);
    char* left = astOutputChild(ast, indentLevel, 0);
    char* right = astOutputChild(ast, indentLevel, 1);
    size_t length = strlen(op) + strlen(left) + strlen(right) + 4;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(%s %s %s)", left, op, right);
    return buffer;
}

static char* astOutputExprCall(Ast* ast, int indentLevel) {
    char* expr = astOutputChild(ast, indentLevel, 0);
    size_t length = (size_t)ast->token.length + 8;
    char* buffer = bufferNewCharArray(length);
    Ast* argList = ast->children->elements[1];
    sprintf_s(buffer, length, "(call %s (", expr);

    for (int i = 0; i < argList->children->count; i++) {
        char* arg = astOutputChild(ast, indentLevel, i);
        buffer = astConcatOutput(buffer, arg);
        buffer = astConcatOutput(buffer, " ");
    }
    buffer = astConcatOutput(buffer, ")");
    return buffer;
}

static char* astOutputExprClass(Ast* ast, int indentLevel) {
    char* superClassName = astOutputChild(ast, indentLevel + 1, 0);
    char* traitList = astOutputChild(ast, indentLevel + 1, 1);
    char* methodList = astOutputChild(ast, indentLevel + 1, 2);
    size_t length = strlen(superClassName) + strlen(traitList) + strlen(methodList) + 18;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(class < %s\nwith %s\n%s)\n", superClassName, traitList, methodList);
    return buffer;
}

static char* astOutputExprDictionary(Ast* ast, int indentLevel) {
    size_t length = 13;
    char* buffer = bufferNewCharArray(length);
    Ast* argList = ast->children->elements[1];
    sprintf_s(buffer, length, "(dictionary [");

    for (int i = 0; i < argList->children->count; i += 2) {
        char* key = astOutputChild(ast, indentLevel, i);
        char* value = astOutputChild(ast, indentLevel, i + 1);
        buffer = astConcatOutput(buffer, key);
        buffer = astConcatOutput(buffer, ": ");
        buffer = astConcatOutput(buffer, value);
        buffer = astConcatOutput(buffer, " ");
    }
    buffer = astConcatOutput(buffer, "]");
    return buffer;
}

static char* astOutputExprFunction(Ast* ast, int indentLevel) {
    char* params = astOutputChild(ast, indentLevel, 0);
    char* block = astOutputChild(ast, indentLevel + 1, 1);
    size_t length = strlen(params) + strlen(block) + 14;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(function (%s) %s)", params, block);
    return buffer;
}

static char* astOutputExprGrouping(Ast* ast, int indentLevel) {
    char* expr = astOutputChild(ast, indentLevel, 0);
    size_t length = (size_t)ast->token.length + 8;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(group %s)", expr);
    return buffer;
}

static char* astOutputExprInvoke(Ast* ast, int indentLevel) {
    char* receiver = astOutputChild(ast, indentLevel, 0);
    char* method = astOutputChild(ast, indentLevel, 1);
    size_t length = strlen(receiver) + strlen(method) + 10;
    char* buffer = bufferNewCharArray(length);
    Ast* argList = ast->children->elements[2];
    sprintf_s(buffer, length, "(invoke %s.%s(", receiver, method);

    for (int i = 0; i < argList->children->count; i++) {
        char* arg = astOutputChild(ast, indentLevel, i);
        buffer = astConcatOutput(buffer, arg);
        buffer = astConcatOutput(buffer, " ");
    }
    buffer = astConcatOutput(buffer, ")");
    return buffer;
}

static char* astOutputExprInterpolation(Ast* ast, int indentLevel) {
    char* exprs = astOutputChild(ast, indentLevel, 0);
    size_t length = strlen(exprs) + 16;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(interpolation %s)", exprs);
    return buffer;
}

static char* astOutputExprLiteral(Ast* ast, int indentLevel) {
    switch (ast->token.type) {
        case TOKEN_NIL:
        case TOKEN_TRUE:
        case TOKEN_FALSE:
        case TOKEN_INT:
        case TOKEN_NUMBER: {
            size_t length = (size_t)ast->token.length;
            char* buffer = bufferNewCharArray(length);
            snprintf(buffer, length, "%s", ast->token.start);
            return buffer;
        }
        case TOKEN_STRING: {
            size_t length = (size_t)ast->token.length + 2;
            char* buffer = bufferNewCharArray(length);
            snprintf(buffer, length, "\"%s\"", ast->token.start);
            return buffer;
        }
        default: return NULL;
    }
    return NULL;
}

static char* astOutputExprLogical(Ast* ast, int indentLevel) {
    return astOutputExprBinary(ast, indentLevel);
}

static char* astOutputExprNil(Ast* ast, int indentLevel) {
    char* op = tokenToString(ast->token);
    char* left = astOutputChild(ast, indentLevel, 0);
    char* right = astOutputChild(ast, indentLevel, 1);
    size_t length = strlen(op) + strlen(left) + strlen(right) + 5;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(%s ?%s %s)", left, op, right);
    return buffer;
}

static char* astOutputExprPropertyGet(Ast* ast, int indentLevel) {
    char* expr = astOutputChild(ast, indentLevel, 0);
    char* prop = tokenToString(ast->token);
    size_t length = strlen(expr) + strlen(prop) + 15;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(propertyGet %s.%s)", expr, prop);
    return buffer;
}

static char* astOutputExprPropertySet(Ast* ast, int indentLevel) {
    char* left = astOutputChild(ast, indentLevel, 0);
    char* prop = tokenToString(ast->token);
    char* right = astOutputChild(ast, indentLevel, 1);
    size_t length = strlen(left) + strlen(prop) + strlen(right) + 18;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(propertySet %s.%s = %s)", left, prop, right);
    return buffer;
}

static char* astOutputExprSubscriptGet(Ast* ast, int indentLevel) {
    char* expr = astOutputChild(ast, indentLevel, 0);
    char* index = astOutputChild(ast, indentLevel, 1);
    size_t length = strlen(expr) + strlen(index) + 17;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(subscriptGet %s[%s])", expr, index);
    return buffer;
}

static char* astOutputExprSubscriptSet(Ast* ast, int indentLevel) {
    char* left = astOutputChild(ast, indentLevel, 0);
    char* index = astOutputChild(ast, indentLevel, 1);
    char* right = astOutputChild(ast, indentLevel, 2);
    size_t length = strlen(left) + strlen(index) + strlen(right) + 20;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(subscriptSet %s[%s] = %s)", left, index, right);
    return buffer;
}

static char* astOutputExprSuperGet(Ast* ast, int indentLevel) {
    char* prop = tokenToString(ast->token);
    size_t length = strlen(prop) + 17;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(superGet super.%s)", prop);
    return buffer;
}

static char* astOutputExprSuperInvoke(Ast* ast, int indentLevel) {
    char* method = tokenToString(ast->token);
    size_t length = strlen(method) + 15;
    char* buffer = bufferNewCharArray(length);
    Ast* argList = ast->children->elements[0];
    sprintf_s(buffer, length, "(invoke super.%s(", method);

    for (int i = 0; i < argList->children->count; i++) {
        char* arg = astOutputChild(ast, indentLevel, i);
        buffer = astConcatOutput(buffer, arg);
        buffer = astConcatOutput(buffer, " ");
    }

    buffer = astConcatOutput(buffer, ")");
    return buffer;
}

static char* astOutputExprThis(Ast* ast, int indentLevel) {
    return "(this)";
}

static char* astOutputExprTrait(Ast* ast, int indentLevel) {
    char* traitList = astOutputChild(ast, indentLevel + 1, 0);
    char* methodList = astOutputChild(ast, indentLevel + 1, 1);
    size_t length = strlen(traitList) + strlen(methodList) + 16;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(trait \nwith %s\n%s)\n", traitList, methodList);
    return buffer;
}

static char* astOutputExprUnary(Ast* ast, int indentLevel) {
    char* op = tokenToString(ast->token);
    char* child = astOutputChild(ast, indentLevel, 0);
    size_t length = strlen(op) + strlen(child) + 3;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(%s %s)", op, child);
    return buffer;
}

static char* astOutputExprVariable(Ast* ast, int indentLevel) {
    const char* format = ast->modifier.isMutable ? "(var %s)" : "(val %s)";
    const char* name = tokenToString(ast->token);
    size_t length = strlen(name) + 6;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, format, name);
    return buffer;
}

static char* astOutputExprYield(Ast* ast, int indentLevel) {
    char* expr = astOutputChild(ast, indentLevel, 0);;
    size_t length = (size_t)ast->token.length + 8;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(yield %s)", expr);
    return buffer;
}

static char* astOutputStmtAwait(Ast* ast, int indentLevel) {
    char* expr = astOutputChild(ast, indentLevel, 0);
    char* indent = astIndent(indentLevel);
    size_t length = strlen(expr) + strlen(indent) + 9;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(await %s)\n", indent, expr);
    return buffer;
}

static char* astOutputStmtBlock(Ast* ast, int indentLevel) {
    char* buffer;
    size_t length;
    char* indent = astIndent(indentLevel);
    Ast* stmtList = astGetChild(ast, 0);

    if (astHasChild(stmtList)) {
        length = strlen(indent) + 7;
        buffer = bufferNewCharArray(length);
        sprintf_s(buffer, length, "%s(block\n", indent);

        char* stmtListOutput = astOutput(stmtList, indentLevel + 1);
        buffer = astConcatOutput(buffer, stmtListOutput);
        buffer = astConcatOutput(buffer, indent);
        buffer = astConcatOutput(buffer, ")\n");
    }
    else {
        length = strlen(indent) + 8;
        buffer = bufferNewCharArray(strlen(indent) + 8);
        sprintf_s(buffer, length, "%s(block)\n", indent);
    }

    return buffer;
}

static char* astOutputStmtBreak(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    size_t length = strlen(indent) + 8;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(break)\n", indent);
    return buffer;
}

static char* astOutputStmtCase(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* expr = astOutputChild(ast, indentLevel, 0);
    char* body = astOutputChild(ast, indentLevel, 1);
    size_t length = strlen(indent) + strlen(expr) + strlen(body) + 10;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(case %s: %s)\n", indent, expr, body);
    return buffer;
}

static char* astOutputStmtCatch(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* exceptionType = tokenToString(ast->token);
    char* exceptionVar = astOutputChild(ast, indentLevel, 0);
    char* catchBody = astOutputChild(ast, indentLevel, 1);
    size_t length = strlen(indent) + strlen(exceptionType) + strlen(exceptionVar) + strlen(catchBody) + 11;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(catch %s %s %s)\n", indent, exceptionType, exceptionVar, catchBody);
    return buffer;
}

static char* astOutputStmtContinue(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    size_t length = strlen(indent) + 11;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(continue)\n", indent);
    return buffer;
}

static char* astOutputStmtExpression(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* expr = astOutput(ast, indentLevel);
    size_t length = strlen(indent) + strlen(expr) + 12;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(exprStmt %s)\n", indent, expr);
    return buffer;
}

static char* astOutputStmtFor(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* decl = astOutputChild(ast, indentLevel, 0);
    char* expr = astOutputChild(ast, indentLevel, 1);
    char* body = astOutputChild(ast, indentLevel + 1, 2);
    size_t length = strlen(indent) + strlen(decl) + strlen(expr) + strlen(body)  + 13;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(for %s : %s \n%s\n)\n", indent, decl, expr, body);
    return buffer;
}

static char* astOutputStmtIf(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* condition = astOutputChild(ast, indentLevel, 0);
    char* thenBranch = astOutputChild(ast, indentLevel + 1, 1);
    size_t length = strlen(indent) + strlen(condition) + 5;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(if %s\n%s", indent, condition, thenBranch);

    if (ast->children->count > 2) {
        char* elseBranch = astOutputChild(ast, indentLevel + 1, 2);
        size_t length2 = strlen(indent) + 7;
        char* buffer2 = bufferNewCharArray(length);
        sprintf_s(buffer2, length2, "%s(else %s\n", indent, condition);
        buffer = astConcatOutput(buffer, buffer2);
        free(buffer2);
    }

    buffer = astConcatOutput(buffer, ")\n");
    return buffer;
}

static char* astOutputStmtRequire(Ast* ast, int indentLevel) {
    char* expr = astOutputChild(ast, indentLevel, 0);
    char* indent = astIndent(indentLevel);
    size_t length = strlen(expr) + strlen(indent) + 10;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(require %s)\n", indent, expr);
    return buffer;
}

static char* astOutputStmtReturn(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    size_t length = strlen(indent) + 7;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(return", indent);

    if (ast->children != NULL && ast->children->count > 0) {
        char* expr = astOutputChild(ast, indentLevel, 0);
        buffer = astConcatOutput(buffer, " ");
        buffer = astConcatOutput(buffer, expr);
    }
    buffer = astConcatOutput(buffer, ")\n");
    return buffer;
}

static char* astOutputStmtSwitch(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* expr = astOutputChild(ast, indentLevel, 0);
    size_t length = strlen(indent) + strlen(expr) + 9;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(switch %s\n", indent, expr);

    char* caseList = astOutputChild(ast, indentLevel + 1, 1);
    size_t length2 = strlen(caseList) + 12;
    char* buffer2 = bufferNewCharArray(length2);
    sprintf_s(buffer, length, "%s(caseList %s)\n", indent, caseList);
    astConcatOutput(buffer, buffer2);
    free(buffer2);

    if (ast->children != NULL && ast->children->count > 2) {
        char* defaultStmt = astOutputChild(ast, indentLevel + 1, 2);
        size_t length3 = strlen(defaultStmt) + 11;
        char* buffer3 = bufferNewCharArray(length3);
        sprintf_s(buffer, length, "%s(default %s)\n", indent, defaultStmt);
        astConcatOutput(buffer, buffer3);
        free(buffer3);
    }

    buffer = astConcatOutput(buffer, ")\n");
    return buffer;
}

static char* astOutputStmtThrow(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* expr = astOutputChild(ast, indentLevel, 0);
    size_t length = strlen(indent) + strlen(expr) + 9;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(throw %s)\n", indent, expr);
    return buffer;
}

static char* astOutputStmtTry(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* tryStmt = astOutputChild(ast, indentLevel + 1, 0);
    char* catchStmt = astOutputChild(ast, indentLevel + 1, 1);
    size_t length = strlen(indent) + strlen(tryStmt) + strlen(catchStmt) + 14;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(try \n%s\ncatch \n%s", indent, tryStmt, catchStmt);

    if (ast->children->count > 2) {
        char* finallyStmt = astOutputChild(ast, indentLevel + 1, 2);
        size_t length2 = strlen(indent) + 10;
        char* buffer2 = bufferNewCharArray(length);
        sprintf_s(buffer2, length2, "%s(finally %s\n", indent, finallyStmt);
        buffer = astConcatOutput(buffer, buffer2);
        free(buffer2);
    }

    buffer = astConcatOutput(buffer, ")\n");
    return buffer;
}

static char* astOutputStmtUsing(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    Ast* identifiers = astGetChild(ast, 0);
    Ast* identifier = astGetChild(identifiers, 0);
    char* name = tokenToString(identifier->token);
    size_t length = strlen(indent) + strlen(name) + 15;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(using %s", indent, name);

    for (int i = 1; i < identifiers->children->count; i++) {
        identifier = astGetChild(identifiers, i);
        name = tokenToString(identifier->token);
        buffer = astConcatOutput(buffer, ".");
        buffer = astConcatOutput(buffer, name);
    }

    if (ast->children->count > 1) {
        Ast* alias = astGetChild(ast, 1);
        name = tokenToString(alias->token);
        buffer = astConcatOutput(buffer, " as ");
        buffer = astConcatOutput(buffer, name);
    }

    buffer = astConcatOutput(buffer, ")\n");
    return buffer;
}

static char* astOutputStmtWhile(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* expr = astOutputChild(ast, indentLevel, 0);
    char* body = astOutputChild(ast, indentLevel + 1, 1);
    size_t length = strlen(indent) + strlen(expr) + 13;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(while %s\n%s\n)\n", indent, expr, body);
    return buffer;
}

static char* astOutputStmtYield(Ast* ast, int indentLevel) {
    char* expr = astOutputChild(ast, indentLevel, 0);
    char* indent = astIndent(indentLevel);
    size_t length = strlen(expr) + strlen(indent) + 9;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(yield %s)\n", indent, expr);
    return buffer;
}

static char* astOutputDeclClass(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* className = tokenToString(ast->token);
    char* superClassName = astOutputChild(ast, indentLevel + 1, 0);
    char* traitList = astOutputChild(ast, indentLevel + 1, 1);
    char* methodList = astOutputChild(ast, indentLevel + 1, 2);
    size_t length = strlen(indent) + strlen(className) + strlen(superClassName) + strlen(traitList) + strlen(methodList) + 19;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(classDecl %s < %s \n%s\n%s)\n", indent, className, superClassName, traitList, methodList);
    return buffer;
}

static char* astOutputDeclFun(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* async = ast->modifier.isAsync ? "async " : "";
    char* name = tokenToString(ast->token);
    char* body = astOutputChild(ast, indentLevel + 1, 0);
    size_t length = strlen(indent) + strlen(async) + strlen(name) + strlen(body) + 12;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(funDecl %s%s\n%s)\n", indent, async, name, body);
    return buffer;
}

static char* astOutputDeclMethod(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* async = ast->modifier.isAsync ? "async " : "";
    char* _class = ast->modifier.isClass ? "class " : "";
    char* name = tokenToString(ast->token);
    char* body = astOutputChild(ast, indentLevel + 1, 0);
    size_t length = strlen(indent) + strlen(async) + strlen(_class) + strlen(name) + strlen(body) + 15;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(methodDecl %s%s%s %s)\n", indent, async, _class, name, body);
    return buffer;
}

static char* astOutputDeclNamespace(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    Ast* identifiers = astGetChild(ast, 0);
    Ast* identifier = astGetChild(identifiers, 0);
    char* name = tokenToString(identifier->token);
    size_t length = strlen(indent) + strlen(name) + 15;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(namespaceDecl %s", indent, name);

    for (int i = 1; i < identifiers->children->count; i++) {
        identifier = astGetChild(identifiers, i);
        char* name = tokenToString(identifier->token);
        buffer = astConcatOutput(buffer, ".");
        buffer = astConcatOutput(buffer, name);
    }

    buffer = astConcatOutput(buffer, ")\n");
    return buffer;
}

static char* astOutputDeclTrait(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* traitName = tokenToString(ast->token);
    char* traitList = astOutputChild(ast, indentLevel + 1, 0);
    char* methodList = astOutputChild(ast, indentLevel + 1, 1);
    size_t length = strlen(indent) + strlen(traitName) + strlen(traitList) + strlen(methodList) + 20;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(traitDecl %s\nwith %s\n%s)\n", indent, traitName, traitList, methodList);
    return buffer;
}

static char* astOutputDeclVar(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* keyword = ast->modifier.isMutable ? "var" : "val";
    char* name = tokenToString(ast->token);
    char* expr = "";

    if (ast->children->count > 0) {
        expr = astOutputChild(ast, indentLevel, 0);
        expr = astConcatOutput(" = ", expr);
    }

    size_t length = strlen(indent) + strlen(name) + strlen(expr) + 12;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(varDecl %s %s%s)\n", indent, keyword, name, expr);
    return buffer;
}

static char* astOutputListExpr(Ast* ast, int indentLevel) {
    char* buffer = "(listExpr\n";
    for (int i = 0; i < ast->children->count; i++) {
        char* stmt = astOutput(astGetChild(ast, i), indentLevel);
        buffer = astConcatOutput(buffer, stmt);
    }
    buffer = astConcatOutput(buffer, ")\n");
    return buffer;
}

static char* astOutputListMethod(Ast* ast, int indentLevel) {
    char* buffer = "(listMethod\n";
    for (int i = 0; i < ast->children->count; i++) {
        char* stmt = astOutput(ast->children->elements[i], indentLevel);
        buffer = astConcatOutput(buffer, stmt);
    }
    buffer = astConcatOutput(buffer, ")\n");
    return buffer;
}

static char* astOutputListStmt(Ast* ast, int indentLevel) {
    char* buffer = "(listStmt\n";
    for (int i = 0; i < ast->children->count; i++) {
        char* stmt = astOutput(ast->children->elements[i], indentLevel);
        buffer = astConcatOutput(buffer, stmt);
    }
    buffer = astConcatOutput(buffer, ")\n");
    return buffer;
}

static char* astOutputListVar(Ast* ast, int indentLevel) {
    char* buffer = "(listVar\n";
    for (int i = 0; i < ast->children->count; i++) {
        char* stmt = astOutput(ast->children->elements[i], indentLevel);
        buffer = astConcatOutput(buffer, stmt);
    }
    buffer = astConcatOutput(buffer, ")\n");
    return buffer;
}

static char* astOutputScript(Ast* ast, int indentLevel) {
    char* buffer = "(program \n";
    if (ast->children != NULL && ast->children->count > 0) {
        for (int i = 0; i < ast->children->count; i++) {
            char* stmt = astOutput(ast->children->elements[i], indentLevel + 1);
            buffer = astConcatOutput(buffer, stmt);
        }
    }
    buffer = astConcatOutput(buffer, ")\n");
    return buffer;
}

char* astOutput(Ast* ast, int indentLevel) {
    switch (ast->category) {
        case AST_CATEGORY_SCRIPT: return astOutputScript(ast, indentLevel);
        case AST_CATEGORY_EXPR: {
            switch (ast->type) {
                case AST_EXPR_ARRAY:
                    return astOutputExprArray(ast, indentLevel);
                case AST_EXPR_ASSIGN:
                    return astOutputExprAssign(ast, indentLevel);
                case AST_EXPR_AWAIT:
                    return astOutputExprAwait(ast, indentLevel);
                case AST_EXPR_BINARY: 
                    return astOutputExprBinary(ast, indentLevel);
                case AST_EXPR_CALL:
                    return astOutputExprCall(ast, indentLevel);
                case AST_EXPR_CLASS:
                    return astOutputExprClass(ast, indentLevel);
                case AST_EXPR_DICTIONARY:
                    return astOutputExprDictionary(ast, indentLevel);
                case AST_EXPR_FUNCTION:
                    return astOutputExprFunction(ast, indentLevel);
                case AST_EXPR_GROUPING:
                    return astOutputExprGrouping(ast, indentLevel);
                case AST_EXPR_INVOKE:
                    return astOutputExprInvoke(ast, indentLevel);
                case AST_EXPR_INTERPOLATION:
                    return astOutputExprInterpolation(ast, indentLevel);
                case AST_EXPR_LITERAL:
                    return astOutputExprLiteral(ast, indentLevel);
                case AST_EXPR_LOGICAL:
                    return astOutputExprLogical(ast, indentLevel);
                case AST_EXPR_NIL:
                    return astOutputExprNil(ast, indentLevel);
                case AST_EXPR_PROPERTY_GET:
                    return astOutputExprPropertyGet(ast, indentLevel);
                case AST_EXPR_PROPERTY_SET:
                    return astOutputExprPropertySet(ast, indentLevel);
                case AST_EXPR_SUBSCRIPT_GET:
                    return astOutputExprSubscriptGet(ast, indentLevel);
                case AST_EXPR_SUBSCRIPT_SET:
                    return astOutputExprSubscriptSet(ast, indentLevel);
                case AST_EXPR_SUPER_GET:
                    return astOutputExprSuperGet(ast, indentLevel);
                case AST_EXPR_SUPER_INVOKE:
                    return astOutputExprSuperInvoke(ast, indentLevel);
                case AST_EXPR_THIS: 
                    return astOutputExprThis(ast, indentLevel);
                case AST_EXPR_TRAIT:
                    return astOutputExprTrait(ast, indentLevel);
                case AST_EXPR_UNARY:
                    return astOutputExprUnary(ast, indentLevel);
                case AST_EXPR_VARIABLE:
                    return astOutputExprVariable(ast, indentLevel);
                case AST_EXPR_YIELD:
                    return astOutputExprYield(ast, indentLevel);
                default: return NULL;
            }
        }
        case AST_CATEGORY_STMT: {
            switch (ast->type) {
                case AST_STMT_AWAIT:
                    return astOutputStmtAwait(ast, indentLevel);
                case AST_STMT_BLOCK:
                    return astOutputStmtBlock(ast, indentLevel);
                case AST_STMT_BREAK:
                    return astOutputStmtBreak(ast, indentLevel);
                case AST_STMT_CASE:
                    return astOutputStmtCase(ast, indentLevel);
                case AST_STMT_CATCH:
                    return astOutputStmtCatch(ast, indentLevel);
                case AST_STMT_CONTINUE:
                    return astOutputStmtContinue(ast, indentLevel);
                case AST_STMT_EXPRESSION:
                    return astOutputStmtExpression(ast, indentLevel);
                case AST_STMT_FOR:
                    return astOutputStmtFor(ast, indentLevel);
                case AST_STMT_IF:
                    return astOutputStmtIf(ast, indentLevel);
                case AST_STMT_REQUIRE:
                    return astOutputStmtRequire(ast, indentLevel);
                case AST_STMT_RETURN:
                    return astOutputStmtReturn(ast, indentLevel);
                case AST_STMT_SWITCH:
                    return astOutputStmtSwitch(ast, indentLevel);
                case AST_STMT_THROW:
                    return astOutputStmtThrow(ast, indentLevel);
                case AST_STMT_TRY:
                    return astOutputStmtTry(ast, indentLevel);
                case AST_STMT_USING:
                    return astOutputStmtUsing(ast, indentLevel);
                case AST_STMT_WHILE:
                    return astOutputStmtUsing(ast, indentLevel);
                case AST_STMT_YIELD:
                    return astOutputStmtYield(ast, indentLevel);
                default: return NULL;
            }
        }
        case AST_CATEGORY_DECL: {
            switch (ast->type) {
                case AST_DECL_CLASS:
                    return astOutputDeclClass(ast, indentLevel);
                case AST_DECL_FUN:
                    return astOutputDeclFun(ast, indentLevel);
                case AST_DECL_METHOD:
                    return astOutputDeclMethod(ast, indentLevel);
                case AST_DECL_NAMESPACE:
                    return astOutputDeclNamespace(ast, indentLevel);
                case AST_DECL_TRAIT:
                    return astOutputDeclTrait(ast, indentLevel);
                case AST_DECL_VAR:
                    return astOutputDeclVar(ast, indentLevel);
                default: return NULL;
            }
        }
        case AST_CATEGORY_OTHER: {
            switch (ast->type) {
                case AST_LIST_EXPR:
                    return astOutputListExpr(ast, indentLevel);
                case AST_LIST_METHOD:
                    return astOutputListMethod(ast, indentLevel);
                case AST_LIST_STMT:
                    return astOutputListStmt(ast, indentLevel);
                case AST_LIST_VAR:
                    return astOutputListVar(ast, indentLevel);
                default: return NULL;
            }
        }
        default: return NULL;
    }
    return NULL;
}
