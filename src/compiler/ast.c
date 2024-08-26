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

AstNodeCategory astNodeCategory(AstNodeType type) {
    if (type == AST_TYPE_NONE) return AST_CATEGORY_SCRIPT;
    else if (type >= AST_EXPR_ASSIGN && type <= AST_EXPR_YIELD) return AST_CATEGORY_EXPR;
    else if (type >= AST_STMT_AWAIT && type <= AST_STMT_YIELD) return AST_CATEGORY_STMT;
    else if (type >= AST_DECL_CLASS && type <= AST_DECL_VAR) return AST_CATEGORY_DECL;
    else return AST_CATEGORY_OTHER;
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

static char* astGetChildOutput(Ast* ast, int indentLevel, int index) {
    if (ast->children == NULL || ast->children->count < index) {
        fprintf(stderr, "Ast has no children or invalid child index specified.");
        exit(1);
    }
    Ast* expr = ast->children->elements[index];
    return astToString(expr, indentLevel);
}

static char* astExprArrayToString(Ast* ast, int indentLevel) {
    size_t length = 8;
    char* buffer = bufferNewCharArray(length);
    Ast* argList = ast->children->elements[1];
    sprintf_s(buffer, length, "(array [");

    for (int i = 0; i < argList->children->count; i++) {
        char* arg = astGetChildOutput(ast, indentLevel, i);
        buffer = astConcatOutput(buffer, arg);
        buffer = astConcatOutput(buffer, " ");
    }
    buffer = astConcatOutput(buffer, "]");
    return buffer;
}

static char* astExprAssignToString(Ast* ast, int indentLevel) {
    char* left = astGetChildOutput(ast, indentLevel, 0);
    char* right = astGetChildOutput(ast, indentLevel, 1);
    size_t length = strlen(left) + strlen(right) + 10;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(assign %s %s)", left, right);
    return buffer;
}

static char* astExprAwaitToString(Ast* ast, int indentLevel) {
    char* exprOutput = astGetChildOutput(ast, indentLevel, 0);;
    size_t length = (size_t)ast->token.length + 8;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(await %s)", exprOutput);
    return buffer;
}

static char* astExprBinaryToString(Ast* ast, int indentLevel) {
    char* op = tokenToString(ast->token);
    char* left = astGetChildOutput(ast, indentLevel, 0);
    char* right = astGetChildOutput(ast, indentLevel, 1);
    size_t length = strlen(op) + strlen(left) + strlen(right) + 4;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(%s %s %s)", left, op, right);
    return buffer;
}

static char* astExprCallToString(Ast* ast, int indentLevel) {
    char* expr = astGetChildOutput(ast, indentLevel, 0);
    size_t length = (size_t)ast->token.length + 8;
    char* buffer = bufferNewCharArray(length);
    Ast* argList = ast->children->elements[1];
    sprintf_s(buffer, length, "(call %s (", expr);

    for (int i = 0; i < argList->children->count; i++) {
        char* arg = astGetChildOutput(ast, indentLevel, i);
        buffer = astConcatOutput(buffer, arg);
        buffer = astConcatOutput(buffer, " ");
    }
    buffer = astConcatOutput(buffer, ")");
    return buffer;
}

static char* astExprClassToString(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* superClassName = astGetChildOutput(ast, indentLevel + 1, 0);
    char* traitList = astGetChildOutput(ast, indentLevel + 1, 1);
    char* methodList = astGetChildOutput(ast, indentLevel + 1, 2);
    size_t length = strlen(indent) + strlen(superClassName) + strlen(traitList) + strlen(methodList) + 18;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(class < %s\nwith %s\n%s)\n", indent, superClassName, traitList, methodList);
    return buffer;
}

static char* astExprDictionaryToString(Ast* ast, int indentLevel) {
    size_t length = 13;
    char* buffer = bufferNewCharArray(length);
    Ast* argList = ast->children->elements[1];
    sprintf_s(buffer, length, "(dictionary [");

    for (int i = 0; i < argList->children->count; i += 2) {
        char* key = astGetChildOutput(ast, indentLevel, i);
        char* value = astGetChildOutput(ast, indentLevel, i + 1);
        buffer = astConcatOutput(buffer, key);
        buffer = astConcatOutput(buffer, ": ");
        buffer = astConcatOutput(buffer, value);
        buffer = astConcatOutput(buffer, " ");
    }
    buffer = astConcatOutput(buffer, "]");
    return buffer;
}

static char* astExprFunctionToString(Ast* ast, int indentLevel) {
    char* params = astGetChildOutput(ast, indentLevel, 0);
    char* block = astGetChildOutput(ast, indentLevel + 1, 1);
    size_t length = strlen(params) + strlen(block) + 14;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(function (%s) %s)", params, block);
    return buffer;
}

static char* astExprGroupingToString(Ast* ast, int indentLevel) {
    char* expr = astGetChildOutput(ast, indentLevel, 0);
    size_t length = (size_t)ast->token.length + 8;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(group %s)", expr);
    return buffer;
}

static char* astExprInvokeToString(Ast* ast, int indentLevel) {
    char* receiver = astGetChildOutput(ast, indentLevel, 0);
    char* method = astGetChildOutput(ast, indentLevel, 1);
    size_t length = strlen(receiver) + strlen(method) + 10;
    char* buffer = bufferNewCharArray(length);
    Ast* argList = ast->children->elements[2];
    sprintf_s(buffer, length, "(invoke %s.%s(", receiver, method);

    for (int i = 0; i < argList->children->count; i++) {
        char* arg = astGetChildOutput(ast, indentLevel, i);
        buffer = astConcatOutput(buffer, arg);
        buffer = astConcatOutput(buffer, " ");
    }
    buffer = astConcatOutput(buffer, ")");
    return buffer;
}

static char* astExprInterpolationToString(Ast* ast, int indentLevel) {
    char* exprs = astGetChildOutput(ast, indentLevel, 0);
    size_t length = strlen(exprs) + 16;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(interpolation %s)", exprs);
    return buffer;
}

static char* astExprLiteralToString(Ast* ast, int indentLevel) {
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

static char* astExprLogicalToString(Ast* ast, int indentLevel) {
    return astExprBinaryToString(ast, indentLevel);
}

static char* astExprPropertyGetToString(Ast* ast, int indentLevel) {
    char* expr = astGetChildOutput(ast, indentLevel, 0);
    char* prop = tokenToString(ast->token);
    size_t length = strlen(expr) + strlen(prop) + 15;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(propertyGet %s.%s)", expr, prop);
    return buffer;
}

static char* astExprPropertySetToString(Ast* ast, int indentLevel) {
    char* left = astGetChildOutput(ast, indentLevel, 0);
    char* prop = tokenToString(ast->token);
    char* right = astGetChildOutput(ast, indentLevel, 1);
    size_t length = strlen(left) + strlen(prop) + strlen(right) + 18;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(propertySet %s.%s = %s)", left, prop, right);
    return buffer;
}

static char* astExprSubscriptGetToString(Ast* ast, int indentLevel) {
    char* expr = astGetChildOutput(ast, indentLevel, 0);
    char* index = astGetChildOutput(ast, indentLevel, 1);
    size_t length = strlen(expr) + strlen(index) + 17;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(subscriptGet %s[%s])", expr, index);
    return buffer;
}

static char* astExprSubscriptSetToString(Ast* ast, int indentLevel) {
    char* left = astGetChildOutput(ast, indentLevel, 0);
    char* index = astGetChildOutput(ast, indentLevel, 1);
    char* right = astGetChildOutput(ast, indentLevel, 2);
    size_t length = strlen(left) + strlen(index) + strlen(right) + 20;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(subscriptSet %s[%s] = %s)", left, index, right);
    return buffer;
}

static char* astExprSuperGetToString(Ast* ast, int indentLevel) {
    char* prop = tokenToString(ast->token);
    size_t length = strlen(prop) + 17;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(superGet super.%s)", prop);
    return buffer;
}

static char* astExprSuperInvokeToString(Ast* ast, int indentLevel) {
    char* method = tokenToString(ast->token);
    size_t length = strlen(method) + 15;
    char* buffer = bufferNewCharArray(length);
    Ast* argList = ast->children->elements[0];
    sprintf_s(buffer, length, "(invoke super.%s(", method);

    for (int i = 0; i < argList->children->count; i++) {
        char* arg = astGetChildOutput(ast, indentLevel, i);
        buffer = astConcatOutput(buffer, arg);
        buffer = astConcatOutput(buffer, " ");
    }

    buffer = astConcatOutput(buffer, ")");
    return buffer;
}

static char* astExprThisToString(Ast* ast, int indentLevel) {
    return "(this)";
}

static char* astExprTraitToString(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* traitList = astGetChildOutput(ast, indentLevel + 1, 0);
    char* methodList = astGetChildOutput(ast, indentLevel + 1, 1);
    size_t length = strlen(indent) + strlen(traitList) + strlen(methodList) + 16;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(trait \nwith %s\n%s)\n", indent, traitList, methodList);
    return buffer;
}

static char* astExprUnaryToString(Ast* ast, int indentLevel) {
    char* op = tokenToString(ast->token);
    char* child = astGetChildOutput(ast, indentLevel, 0);
    size_t length = strlen(op) + strlen(child) + 3;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(%s %s)", op, child);
    return buffer;
}

static char* astExprVariableToString(Ast* ast, int indentLevel) {
    const char* format = ast->modifier.isMutable ? "(var %s)" : "(val %s)";
    const char* name = tokenToString(ast->token);
    size_t length = strlen(name) + 6;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, format, name);
    return buffer;
}

static char* astExprYieldToString(Ast* ast, int indentLevel) {
    char* expr = astGetChildOutput(ast, indentLevel, 0);;
    size_t length = (size_t)ast->token.length + 8;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "(yield %s)", expr);
    return buffer;
}

static char* astStmtAwaitToString(Ast* ast, int indentLevel) {
    char* expr = astGetChildOutput(ast, indentLevel, 0);
    char* indent = astIndent(indentLevel);
    size_t length = strlen(expr) + strlen(indent) + 9;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(await %s)\n", indent, expr);
    return buffer;
}

static char* astStmtBlockToString(Ast* ast, int indentLevel) {
    char* buffer;
    size_t length;
    char* indent = astIndent(indentLevel);
    Ast* stmtList = astGetChild(ast, 0);

    if (astHasChild(stmtList)) {
        length = strlen(indent) + 7;
        buffer = bufferNewCharArray(length);
        sprintf_s(buffer, length, "%s(block\n", indent);

        char* stmtListOutput = astToString(stmtList, indentLevel + 1);
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

static char* astStmtBreakToString(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    size_t length = strlen(indent) + 8;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(break)\n", indent);
    return buffer;
}

static char* astStmtCaseToString(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* expr = astGetChildOutput(ast, indentLevel, 0);
    char* body = astGetChildOutput(ast, indentLevel, 1);
    size_t length = strlen(indent) + strlen(expr) + strlen(body) + 10;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(case %s: %s)\n", indent, expr, body);
    return buffer;
}

static char* astStmtCatchToString(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* exceptionType = tokenToString(ast->token);
    char* exceptionVar = astGetChildOutput(ast, indentLevel, 0);
    char* catchBody = astGetChildOutput(ast, indentLevel, 1);
    size_t length = strlen(indent) + strlen(exceptionType) + strlen(exceptionVar) + strlen(catchBody) + 11;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(catch %s %s %s)\n", indent, exceptionType, exceptionVar, catchBody);
    return buffer;
}

static char* astStmtContinueToString(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    size_t length = strlen(indent) + 11;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(continue)\n", indent);
    return buffer;
}

static char* astStmtExpressionToString(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* expr = astToString(ast, indentLevel);
    size_t length = strlen(indent) + strlen(expr) + 12;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(exprStmt %s)\n", indent, expr);
    return buffer;
}

static char* astStmtForToString(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* decl = astGetChildOutput(ast, indentLevel, 0);
    char* expr = astGetChildOutput(ast, indentLevel, 1);
    char* body = astGetChildOutput(ast, indentLevel + 1, 2);
    size_t length = strlen(indent) + strlen(decl) + strlen(expr) + strlen(body)  + 13;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(for %s : %s \n%s\n)\n", indent, decl, expr, body);
    return buffer;
}

static char* astStmtIfToString(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* condition = astGetChildOutput(ast, indentLevel, 0);
    char* thenBranch = astGetChildOutput(ast, indentLevel + 1, 1);
    size_t length = strlen(indent) + strlen(condition) + 5;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(if %s\n%s", indent, condition, thenBranch);

    if (ast->children->count > 2) {
        char* elseBranch = astGetChildOutput(ast, indentLevel + 1, 2);
        size_t length2 = strlen(indent) + 7;
        char* buffer2 = bufferNewCharArray(length);
        sprintf_s(buffer2, length2, "%s(else %s\n", indent, condition);
        buffer = astConcatOutput(buffer, buffer2);
        free(buffer2);
    }

    buffer = astConcatOutput(buffer, ")\n");
    return buffer;
}

static char* astStmtRequireToString(Ast* ast, int indentLevel) {
    char* expr = astGetChildOutput(ast, indentLevel, 0);
    char* indent = astIndent(indentLevel);
    size_t length = strlen(expr) + strlen(indent) + 10;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(require %s)\n", indent, expr);
    return buffer;
}

static char* astStmtReturnToString(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    size_t length = strlen(indent) + 7;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(return", indent);

    if (ast->children != NULL && ast->children->count > 0) {
        char* expr = astGetChildOutput(ast, indentLevel, 0);
        buffer = astConcatOutput(buffer, " ");
        buffer = astConcatOutput(buffer, expr);
    }
    buffer = astConcatOutput(buffer, ")\n");
    return buffer;
}

static char* astStmtSwitchToString(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* expr = astGetChildOutput(ast, indentLevel, 0);
    size_t length = strlen(indent) + strlen(expr) + 9;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(switch %s\n", indent, expr);

    char* caseList = astGetChildOutput(ast, indentLevel + 1, 1);
    size_t length2 = strlen(caseList) + 12;
    char* buffer2 = bufferNewCharArray(length2);
    sprintf_s(buffer, length, "%s(caseList %s)\n", indent, caseList);
    astConcatOutput(buffer, buffer2);
    free(buffer2);

    if (ast->children != NULL && ast->children->count > 2) {
        char* defaultStmt = astGetChildOutput(ast, indentLevel + 1, 2);
        size_t length3 = strlen(defaultStmt) + 11;
        char* buffer3 = bufferNewCharArray(length3);
        sprintf_s(buffer, length, "%s(default %s)\n", indent, defaultStmt);
        astConcatOutput(buffer, buffer3);
        free(buffer3);
    }

    buffer = astConcatOutput(buffer, ")\n");
    return buffer;
}

static char* astStmtThrowToString(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* expr = astGetChildOutput(ast, indentLevel, 0);
    size_t length = strlen(indent) + strlen(expr) + 9;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(throw %s)\n", indent, expr);
    return buffer;
}

static char* astStmtTryToString(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* tryStmt = astGetChildOutput(ast, indentLevel + 1, 0);
    char* catchStmt = astGetChildOutput(ast, indentLevel + 1, 1);
    size_t length = strlen(indent) + strlen(tryStmt) + strlen(catchStmt) + 14;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(try \n%s\ncatch \n%s", indent, tryStmt, catchStmt);

    if (ast->children->count > 2) {
        char* finallyStmt = astGetChildOutput(ast, indentLevel + 1, 2);
        size_t length2 = strlen(indent) + 10;
        char* buffer2 = bufferNewCharArray(length);
        sprintf_s(buffer2, length2, "%s(finally %s\n", indent, finallyStmt);
        buffer = astConcatOutput(buffer, buffer2);
        free(buffer2);
    }

    buffer = astConcatOutput(buffer, ")\n");
    return buffer;
}

static char* astStmtUsingToString(Ast* ast, int indentLevel) {
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

static char* astStmtWhileToString(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* expr = astGetChildOutput(ast, indentLevel, 0);
    char* body = astGetChildOutput(ast, indentLevel + 1, 1);
    size_t length = strlen(indent) + strlen(expr) + 13;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(while %s\n%s\n)\n", indent, expr, body);
    return buffer;
}

static char* astStmtYieldToString(Ast* ast, int indentLevel) {
    char* expr = astGetChildOutput(ast, indentLevel, 0);
    char* indent = astIndent(indentLevel);
    size_t length = strlen(expr) + strlen(indent) + 9;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(yield %s)\n", indent, expr);
    return buffer;
}

static char* astDeclClassToString(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* className = tokenToString(ast->token);
    char* superClassName = astGetChildOutput(ast, indentLevel + 1, 0);
    char* traitList = astGetChildOutput(ast, indentLevel + 1, 1);
    char* methodList = astGetChildOutput(ast, indentLevel + 1, 2);
    size_t length = strlen(indent) + strlen(className) + strlen(superClassName) + strlen(traitList) + strlen(methodList) + 19;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(classDecl %s < %s \n%s\n%s)\n", indent, className, superClassName, traitList, methodList);
    return buffer;
}

static char* astDeclFunToString(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* async = ast->modifier.isAsync ? "async " : "";
    char* name = tokenToString(ast->token);
    char* body = astGetChildOutput(ast, indentLevel + 1, 0);
    size_t length = strlen(indent) + strlen(async) + strlen(name) + strlen(body) + 12;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(funDecl %s%s\n%s)\n", indent, async, name, body);
    return buffer;
}

static char* astDeclMethodToString(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* async = ast->modifier.isAsync ? "async " : "";
    char* _class = ast->modifier.isClass ? "class " : "";
    char* name = tokenToString(ast->token);
    char* body = astGetChildOutput(ast, indentLevel + 1, 0);
    size_t length = strlen(indent) + strlen(async) + strlen(_class) + strlen(name) + strlen(body) + 15;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(methodDecl %s%s%s %s)\n", indent, async, _class, name, body);
    return buffer;
}

static char* astDeclNamespaceToString(Ast* ast, int indentLevel) {
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

static char* astDeclTraitToString(Ast* ast, int indentLevel) {
    // To be implemented
    return NULL;
}

static char* astDeclVarToString(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* keyword = ast->modifier.isMutable ? "var" : "val";
    char* name = tokenToString(ast->token);
    char* expr = "";

    if (ast->children->count > 0) {
        expr = astGetChildOutput(ast, indentLevel, 0);
        expr = astConcatOutput(" = ", expr);
    }

    size_t length = strlen(indent) + strlen(name) + strlen(expr) + 12;
    char* buffer = bufferNewCharArray(length);
    sprintf_s(buffer, length, "%s(varDecl %s %s%s)\n", indent, keyword, name, expr);
    return buffer;
}

static char* astListExprToString(Ast* ast, int indentLevel) {
    char* buffer = "(exprList\n";
    for (int i = 0; i < ast->children->count; i++) {
        char* stmt = astToString(astGetChild(ast, i), indentLevel);
        buffer = astConcatOutput(buffer, stmt);
    }
    buffer = astConcatOutput(buffer, ")\n");
    return buffer;
}

static char* astListMethodToString(Ast* ast, int indentLevel) {
    char* buffer = "(methodList\n";
    for (int i = 0; i < ast->children->count; i++) {
        char* stmt = astToString(ast->children->elements[i], indentLevel);
        buffer = astConcatOutput(buffer, stmt);
    }
    buffer = astConcatOutput(buffer, ")\n");
    return buffer;
}

static char* astListStmtToString(Ast* ast, int indentLevel) {
    char* buffer = "(stmtList\n";
    for (int i = 0; i < ast->children->count; i++) {
        char* stmt = astToString(ast->children->elements[i], indentLevel);
        buffer = astConcatOutput(buffer, stmt);
    }
    buffer = astConcatOutput(buffer, ")\n");
    return buffer;
}

static char* astListVarToString(Ast* ast, int indentLevel) {
    char* buffer = "(varList\n";
    for (int i = 0; i < ast->children->count; i++) {
        char* stmt = astToString(ast->children->elements[i], indentLevel);
        buffer = astConcatOutput(buffer, stmt);
    }
    buffer = astConcatOutput(buffer, ")\n");
    return buffer;
}

static char* astOutputScript(Ast* ast, int indentLevel) {
    char* buffer = "(program \n";
    if (ast->children != NULL && ast->children->count > 0) {
        for (int i = 0; i < ast->children->count; i++) {
            char* stmt = astToString(ast->children->elements[i], indentLevel + 1);
            buffer = astConcatOutput(buffer, stmt);
        }
    }
    buffer = astConcatOutput(buffer, ")\n");
    return buffer;
}

char* astToString(Ast* ast, int indentLevel) {
    switch (ast->category) {
        case AST_CATEGORY_SCRIPT: return astOutputScript(ast, indentLevel);
        case AST_CATEGORY_EXPR: {
            switch (ast->type) {
                case AST_EXPR_ARRAY:
                    return astExprArrayToString(ast, indentLevel);
                case AST_EXPR_ASSIGN:
                    return astExprAssignToString(ast, indentLevel);
                case AST_EXPR_AWAIT:
                    return astExprAwaitToString(ast, indentLevel);
                case AST_EXPR_BINARY: 
                    return astExprBinaryToString(ast, indentLevel);
                case AST_EXPR_CALL:
                    return astExprCallToString(ast, indentLevel);
                case AST_EXPR_CLASS:
                    return astExprClassToString(ast, indentLevel);
                case AST_EXPR_DICTIONARY:
                    return astExprDictionaryToString(ast, indentLevel);
                case AST_EXPR_FUNCTION:
                    return astExprFunctionToString(ast, indentLevel);
                case AST_EXPR_GROUPING:
                    return astExprGroupingToString(ast, indentLevel);
                case AST_EXPR_INVOKE:
                    return astExprInvokeToString(ast, indentLevel);
                case AST_EXPR_INTERPOLATION:
                    return astExprInterpolationToString(ast, indentLevel);
                case AST_EXPR_LITERAL:
                    return astExprLiteralToString(ast, indentLevel);
                case AST_EXPR_LOGICAL:
                    return astExprLogicalToString(ast, indentLevel);
                case AST_EXPR_PROPERTY_GET:
                    return astExprPropertyGetToString(ast, indentLevel);
                case AST_EXPR_PROPERTY_SET:
                    return astExprPropertySetToString(ast, indentLevel);
                case AST_EXPR_SUBSCRIPT_GET:
                    return astExprSubscriptGetToString(ast, indentLevel);
                case AST_EXPR_SUBSCRIPT_SET:
                    return astExprSubscriptSetToString(ast, indentLevel);
                case AST_EXPR_SUPER_GET:
                    return astExprSuperGetToString(ast, indentLevel);
                case AST_EXPR_SUPER_INVOKE:
                    return astExprSuperInvokeToString(ast, indentLevel);
                case AST_EXPR_THIS: 
                    return astExprThisToString(ast, indentLevel);
                case AST_EXPR_TRAIT:
                    return astExprTraitToString(ast, indentLevel);
                case AST_EXPR_UNARY:
                    return astExprUnaryToString(ast, indentLevel);
                case AST_EXPR_VARIABLE:
                    return astExprVariableToString(ast, indentLevel);
                case AST_EXPR_YIELD:
                    return astExprYieldToString(ast, indentLevel);
                default: return NULL;
            }
        }
        case AST_CATEGORY_STMT: {
            switch (ast->type) {
                case AST_STMT_AWAIT:
                    return astStmtAwaitToString(ast, indentLevel);
                case AST_STMT_BLOCK:
                    return astStmtBlockToString(ast, indentLevel);
                case AST_STMT_BREAK:
                    return astStmtBreakToString(ast, indentLevel);
                case AST_STMT_CASE:
                    return astStmtCaseToString(ast, indentLevel);
                case AST_STMT_CATCH:
                    return astStmtCatchToString(ast, indentLevel);
                case AST_STMT_CONTINUE:
                    return astStmtContinueToString(ast, indentLevel);
                case AST_STMT_EXPRESSION:
                    return astStmtExpressionToString(ast, indentLevel);
                case AST_STMT_FOR:
                    return astStmtForToString(ast, indentLevel);
                case AST_STMT_IF:
                    return astStmtIfToString(ast, indentLevel);
                case AST_STMT_REQUIRE:
                    return astStmtRequireToString(ast, indentLevel);
                case AST_STMT_RETURN:
                    return astStmtReturnToString(ast, indentLevel);
                case AST_STMT_SWITCH:
                    return astStmtSwitchToString(ast, indentLevel);
                case AST_STMT_THROW:
                    return astStmtThrowToString(ast, indentLevel);
                case AST_STMT_TRY:
                    return astStmtTryToString(ast, indentLevel);
                case AST_STMT_USING:
                    return astStmtUsingToString(ast, indentLevel);
                case AST_STMT_WHILE:
                    return astStmtUsingToString(ast, indentLevel);
                case AST_STMT_YIELD:
                    return astStmtYieldToString(ast, indentLevel);
                default: return NULL;
            }
        }
        case AST_CATEGORY_DECL: {
            switch (ast->type) {
                case AST_DECL_CLASS:
                    return astDeclClassToString(ast, indentLevel);
                case AST_DECL_FUN:
                    return astDeclFunToString(ast, indentLevel);
                case AST_DECL_METHOD:
                    return astDeclMethodToString(ast, indentLevel);
                case AST_DECL_NAMESPACE:
                    return astDeclNamespaceToString(ast, indentLevel);
                case AST_DECL_TRAIT:
                    return astDeclTraitToString(ast, indentLevel);
                case AST_DECL_VAR:
                    return astDeclVarToString(ast, indentLevel);
                default: return NULL;
            }
        }
        case AST_CATEGORY_OTHER: {
            switch (ast->type) {
                case AST_LIST_EXPR:
                    return astListExprToString(ast, indentLevel);
                case AST_LIST_METHOD:
                    return astListMethodToString(ast, indentLevel);
                case AST_LIST_STMT:
                    return astListStmtToString(ast, indentLevel);
                case AST_LIST_VAR:
                    return astListVarToString(ast, indentLevel);
                default: return NULL;
            }
        }
        default:
            return NULL;
    }
    return NULL;
}
