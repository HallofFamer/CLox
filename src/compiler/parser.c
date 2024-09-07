#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "../inc/utf8.h"

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

typedef Ast* (*ParsePrefixFn)(Parser* parser, Token token, bool canAssign);
typedef Ast* (*ParseInfixFn)(Parser* parser, Token token, Ast* left, bool canAssign);

typedef struct {
    ParsePrefixFn prefix;
    ParseInfixFn infix;
    Precedence precedence;
} ParseRule;

static void parseError(Parser* parser, Token* token, const char* message) {
    if (parser->panicMode) return;
    parser->panicMode = true;
    fprintf(stderr, "[line %d] Parse Error", token->line);

    if (token->type == TOKEN_EOF) fprintf(stderr, " at end");
    else if (token->type == TOKEN_ERROR) { }
    else fprintf(stderr, " at '%.*s'", token->length, token->start);

    fprintf(stderr, ": %s\n", message);
    parser->hadError = true;
    longjmp(parser->jumpBuffer, 1);
}

static void error(Parser* parser, const char* message) {
    parseError(parser, &parser->previous, message);
}

static void errorAtCurrent(Parser* parser, const char* message) {
    parseError(parser, &parser->current, message);
}

static void advance(Parser* parser) {
    parser->previous = parser->current;
    parser->current = parser->next;

    for (;;) {
        parser->next = scanToken(parser->lexer);
        if (parser->next.type != TOKEN_ERROR) break;
        errorAtCurrent(parser, parser->next.start);
    }
}

static void consume(Parser* parser, TokenSymbol type, const char* message) {
    if (parser->current.type == type) {
        advance(parser);
        return;
    }
    errorAtCurrent(parser, message);
}

static bool check(Parser* parser, TokenSymbol type) {
    return parser->current.type == type;
}

static bool checkNext(Parser* parser, TokenSymbol type) {
    return parser->next.type == type;
}

static bool match(Parser* parser, TokenSymbol type) {
    if (!check(parser, type)) return false;
    advance(parser);
    return true;
}

static int hexDigit(Parser* parser, char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;

    error(parser, "Invalid hex escape sequence.");
    return -1;
}

static int hexEscape(Parser* parser, const char* source, int base, int startIndex) {
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

static int unicodeEscape(Parser* parser, const char* source, char* target, int base, int startIndex, int currentLength) {
    int value = hexEscape(parser, source, base, startIndex);
    int numBytes = utf8_num_bytes(value);
    if (numBytes < 0) error(parser, "Negative unicode character specified.");
    if (value > 0xffff) numBytes++;

    if (numBytes > 0) {
        char* utfChar = utf8_encode(value);
        if (utfChar == NULL) error(parser, "Invalid unicode character specified.");
        else {
            memcpy(target + currentLength, utfChar, (size_t)numBytes + 1);
            free(utfChar);
        }
    }
    return numBytes;
}

static void* resizeString(char* string, size_t newSize) {
    void* result = realloc(string, newSize);
    if (result == NULL) exit(1);
    return result;
}

static char* parseString(Parser* parser, int* length) {
    int maxLength = parser->previous.length - 2;
    const char* source = parser->previous.start + 1;
    char* target = (char*)malloc((size_t)maxLength + 1);
    if (target == NULL) {
        fprintf(stderr, "Not enough memory to parser string token.");
        exit(1);
    }

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

    target = resizeString(target, (size_t)j + 1);
    *length = j;
    return target;
}

static void synchronize(Parser* parser) {
    parser->panicMode = false;

    while (parser->current.type != TOKEN_EOF) {
        if (parser->previous.type == TOKEN_SEMICOLON) return;

        switch (parser->current.type) {
            case TOKEN_ASYNC:
            case TOKEN_AWAIT:
            case TOKEN_CLASS:
            case TOKEN_FOR:
            case TOKEN_FUN:
            case TOKEN_IF:
            case TOKEN_NAMESPACE:
            case TOKEN_RETURN:
            case TOKEN_SWITCH:
            case TOKEN_TRAIT:
            case TOKEN_THROW:
            case TOKEN_USING:
            case TOKEN_VAL:
            case TOKEN_VAR:
            case TOKEN_WHILE:
            case TOKEN_YIELD:
                return;

            default: ;
        }
        advance(parser);
    }
}

static Ast* expression(Parser* parser);
static Ast* statement(Parser* parser);
static Ast* block(Parser* parser);
static Ast* function(Parser* parser, bool isAsync, bool isLambda);
static Ast* declaration(Parser* parser);
static ParseRule* getRule(TokenSymbol type);
static Ast* parsePrecedence(Parser* parser, Precedence precedence);

static Ast* argumentList(Parser* parser) {
    Ast* argList = emptyAst(AST_LIST_EXPR, parser->previous);
    uint8_t argCount = 0;

    if (!check(parser, TOKEN_RIGHT_PAREN)) {
        do {
            Ast* child = expression(parser);
            if (argCount == UINT8_MAX) error(parser, "Can't have more than 255 arguments.");
            astAppendChild(argList, child);
            argCount++;
        } while (match(parser, TOKEN_COMMA));
    }

    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argList;
}

static Ast* identifier(Parser* parser, const char* message) {
    consume(parser, TOKEN_IDENTIFIER, message);
    return emptyAst(AST_EXPR_VARIABLE, parser->previous);
}

static Ast* binary(Parser* parser, Token token, Ast* left, bool canAssign) {
    ParseRule* rule = getRule(parser->previous.type);
    Ast* right = parsePrecedence(parser, (Precedence)(rule->precedence + 1));
    return newAst(AST_EXPR_BINARY, token, 2, left, right);
}

static Ast* call(Parser* parser, Token token, Ast* left, bool canAssign) { 
    Ast* right = argumentList(parser);
    return newAst(AST_EXPR_CALL, token, 2, left, right);
}

static Ast* dot(Parser* parser, Token token, Ast* left, bool canAssign) { 
    Ast* property = identifier(parser, "Expect property name after '.'.");
    Ast* expr = NULL;

    if (canAssign && match(parser, TOKEN_EQUAL)) {
        Ast* right = expression(parser);
        expr = newAst(AST_EXPR_PROPERTY_SET, token, 3, left, property, right);
    }
    else if (match(parser, TOKEN_LEFT_PAREN)) {
        Ast* right = argumentList(parser);
        expr = newAst(AST_EXPR_INVOKE, token, 3, left, property, right);
    }
    else expr = newAst(AST_EXPR_PROPERTY_GET, token, 2, left, property);

    return expr;
}

static Ast* logical(Parser* parser, Token token, Ast* left, bool canAssign) {
    ParseRule* rule = getRule(parser->previous.type);
    Ast* right = parsePrecedence(parser, (Precedence)(rule->precedence + 1));
    return newAst(AST_EXPR_LOGICAL, token, 2, left, right);
}

static Ast* nil(Parser* parser, Token token, Ast* left, bool canAssign) {
    Ast* right = parsePrecedence(parser, PREC_PRIMARY);
    return newAst(AST_EXPR_NIL, token, 2, left, right);
}

static Ast* subscript(Parser* parser, Token token, Ast* left, bool canAssign) {
    Ast* expr = NULL;
    Ast* index = expression(parser);
    consume(parser, TOKEN_RIGHT_BRACKET, "Expect ']' after subscript.");

    if (canAssign && match(parser, TOKEN_EQUAL)) {
        Ast* right = expression(parser);
        expr = newAst(AST_EXPR_SUBSCRIPT_SET, token, 3, left, index, right);
    }
    else expr = newAst(AST_EXPR_SUBSCRIPT_GET, token, 2, left, index);
    return expr;
}

static Ast* question(Parser* parser, Token token, Ast* left, bool canAssign) { 
    Ast* expr = NULL;

    if (match(parser, TOKEN_DOT)) {
        expr = dot(parser, token, left, canAssign);
    }
    else if (match(parser, TOKEN_LEFT_BRACKET)) {
        expr = subscript(parser, token, left, canAssign);
    }
    else if (match(parser, TOKEN_LEFT_PAREN)) {
        expr = call(parser, token, left, canAssign);
    }
    else if (match(parser, TOKEN_QUESTION) || match(parser, TOKEN_COLON)) {
        expr = nil(parser, token, left, canAssign);
    }

    if (expr != NULL) expr->modifier.isOptional = true;
    return expr;
}

static Ast* literal(Parser* parser, Token token, bool canAssign) {
    return emptyAst(AST_EXPR_LITERAL, token);
}

static Ast* grouping(Parser* parser, Token token, bool canAssign) { 
    Ast* expr = emptyAst(AST_EXPR_GROUPING, token);
    Ast* child = expression(parser);
    astAppendChild(expr, child);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
    return expr;
}

static Ast* string(Parser* parser, Token token, bool canAssign) {
    int length = 0;
    char* str = parseString(parser, &length);
    Token strToken = {
        .length = length,
        .line = token.line,
        .start = str,
        .type = TOKEN_STRING
    };
    return emptyAst(AST_EXPR_LITERAL, strToken);
}

static Ast* interpolation(Parser* parser, Token token, bool canAssign) { 
    Ast* exprs = emptyAst(AST_LIST_EXPR, token);
    int count = 0;
    do {
        bool concatenate = false;
        bool isString = false;

        if (parser->previous.length > 2) {
            int length = 0;
            char* str = parseString(parser, &length);
            Ast* string = emptyAst(AST_EXPR_LITERAL, syntheticToken(str));
            concatenate = true;
            isString = true;
            astAppendChild(exprs, string);
        }

        Ast* expr = expression(parser);
        astAppendChild(exprs, expr);
        count++;
    } while (match(parser, TOKEN_INTERPOLATION));

    consume(parser, TOKEN_STRING, "Expect end of string interpolation.");
    return newAst(AST_EXPR_INTERPOLATION, token, 1, exprs);
}

static Ast* array(Parser* parser, Token token, Ast* element) {
    Ast* elements = newAst(AST_LIST_EXPR, token, 1, element);
    uint8_t elementCount = 1;
    while (match(parser, TOKEN_COMMA)) {
        element = expression(parser);
        astAppendChild(elements, element);

        if (elementCount == UINT8_MAX) {
            error(parser, "Cannot have more than 255 elements.");
        }
        elementCount++;
    }

    consume(parser, TOKEN_RIGHT_BRACKET, "Expect ']' after elements.");
    return newAst(AST_EXPR_ARRAY, token, 1, elements);
}

static Ast* dictionary(Parser* parser, Token token, Ast* key, Ast* value) {
    Ast* keys = newAst(AST_LIST_EXPR, token, 1, key);
    Ast* values = newAst(AST_LIST_EXPR, token, 1, value);
    uint8_t entryCount = 1;
    while (match(parser, TOKEN_COMMA)) {
        Ast* key = expression(parser);
        astAppendChild(keys, key);
        consume(parser, TOKEN_COLON, "Expect ':' after entry key.");
        Ast* value = expression(parser);
        astAppendChild(values, value);

        if (entryCount == UINT8_MAX) {
            error(parser, "Cannot have more than 255 entries.");
        }
        entryCount++;
    }

    consume(parser, TOKEN_RIGHT_BRACKET, "Expect ']' after entries.");
    return newAst(AST_EXPR_DICTIONARY, token, 2, keys, values);
}

static Ast* collection(Parser* parser, Token token, bool canAssign) { 
    if (match(parser, TOKEN_RIGHT_BRACKET)) return emptyAst(AST_EXPR_ARRAY, token);
    else {
        Ast* first = expression(parser);
        if (match(parser, TOKEN_COLON)) {
            Ast* firstValue = expression(parser);
            return dictionary(parser, token, first, firstValue);
        }
        else return array(parser, token, first);
    }
}

static Ast* closure(Parser* parser, Token token, bool canAssign) {
    return function(parser, false, false);
}

static Ast* lambda(Parser* parser, Token token, bool canAssign) {
    return function(parser, false, true);
}

static Ast* variable(Parser* parser, Token token, bool canAssign) {
    return emptyAst(AST_EXPR_VARIABLE, token);
}

static Ast* methods(Parser* parser, Token* name) {
    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before class/trait body.");
    Ast* methodList = emptyAst(AST_LIST_METHOD, *name);

    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        bool isAsync = false, isClass = false, isInitializer = false;
        if (match(parser, TOKEN_ASYNC)) isAsync = true;
        if (match(parser, TOKEN_CLASS)) isClass = true;
        consume(parser, TOKEN_IDENTIFIER, "Expect method name.");

        if (parser->previous.length == 8 && memcmp(parser->previous.start, "__init__", 8) == 0) {
            isInitializer = true;
        }
        Ast* method = function(parser, isAsync, false);
        method->modifier.isAsync = isAsync;
        method->modifier.isClass = isClass;
        method->modifier.isInitializer = isInitializer;
        astAppendChild(methodList, method);
    }

    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after class/trait body.");
    return methodList;
}

static Ast* superclass_(Parser* parser) {
    if (match(parser, TOKEN_LESS)) {
        return identifier(parser, "Expect super class name.");
    }
    return emptyAst(AST_EXPR_VARIABLE, parser->rootClass);
}

static Ast* traits(Parser* parser, Token* name) {
    Ast* traitList = emptyAst(AST_LIST_VAR, *name);
    uint8_t traitCount = 0;

    do {
        traitCount++;
        if (traitCount > UINT4_MAX) {
            errorAtCurrent(parser, "Can't have more than 15 parameters.");
        }

        Ast* trait = identifier(parser, "Expect class/trait name.");
        astAppendChild(traitList, trait);
    } while (match(parser, TOKEN_COMMA));

    return traitList;
}

static Ast* class_(Parser* parser, Token token, bool canAssign) {
    Token className = syntheticToken("@");
    Ast* superClass = superclass_(parser);
    Ast* traitList = traits(parser, &className);
    Ast* methodList = methods(parser, &className);
    return newAst(AST_EXPR_CLASS, className, 3, superClass, traitList, methodList);
}

static Ast* trait(Parser* parser, Token token, bool canAssign) {
    Token traitName = syntheticToken("@");
    Ast* traitList = traits(parser, &traitName);
    Ast* methodList = methods(parser, &traitName);
    return newAst(AST_EXPR_TRAIT, traitName, 2, traitList, methodList);
}

static Ast* super_(Parser* parser, Token token, bool canAssign) {
    consume(parser, TOKEN_DOT, "Expect '.' after 'super'.");
    consume(parser, TOKEN_IDENTIFIER, "Expect superclass method name.");
    Token method = parser->previous;

    if (match(parser, TOKEN_LEFT_PAREN)) {
        Ast* value = argumentList(parser);
        return newAst(AST_EXPR_SUPER_INVOKE, method, 1, value);
    }
    else return emptyAst(AST_EXPR_SUPER_GET, method);
}

static Ast* this_(Parser* parser, Token token, bool canAssign) { 
    return emptyAst(AST_EXPR_THIS, token);
}

static Ast* unary(Parser* parser, Token token, bool canAssign) {
    Ast* expr = parsePrecedence(parser, PREC_UNARY);
    return newAst(AST_EXPR_UNARY, token, 1, expr);
}

static Ast* yield(Parser* parser, Token token, bool canAssign) {
    Ast* expr = expression(parser);
    return newAst(AST_EXPR_YIELD, token, 1, expr);
}

static Ast* async(Parser* parser, Token token, bool canAssign) { 
    Ast* func = NULL;
    if (match(parser, TOKEN_FUN)) {
        func = function(parser, true, false);
    }
    else if (match(parser, TOKEN_LEFT_BRACE)) {
        func = function(parser, true, true);
    }
    else {
        error(parser, "Can only use async as expression modifier for anonymous functions or lambda.");
    }
    return func;
}

static Ast* await(Parser* parser, Token token, bool canAssign) { 
    Ast* expr = expression(parser);
    return newAst(AST_EXPR_AWAIT, token, 1, expr);
}

ParseRule parseRules[] = {
    [TOKEN_LEFT_PAREN]     = {grouping,      call,        PREC_CALL},
    [TOKEN_RIGHT_PAREN]    = {NULL,          NULL,        PREC_NONE},
    [TOKEN_LEFT_BRACKET]   = {collection,    subscript,   PREC_CALL},
    [TOKEN_RIGHT_BRACKET]  = {NULL,          NULL,        PREC_NONE},
    [TOKEN_LEFT_BRACE]     = {lambda,        NULL,        PREC_NONE},
    [TOKEN_RIGHT_BRACE]    = {NULL,          NULL,        PREC_NONE},
    [TOKEN_COLON]          = {NULL,          NULL,        PREC_NONE},
    [TOKEN_COMMA]          = {NULL,          NULL,        PREC_NONE},
    [TOKEN_MINUS]          = {unary,         binary,      PREC_TERM},
    [TOKEN_MODULO]         = {NULL,          binary,      PREC_FACTOR},
    [TOKEN_PIPE]           = {NULL,          NULL,        PREC_NONE},
    [TOKEN_PLUS]           = {NULL,          binary,      PREC_TERM},
    [TOKEN_QUESTION]       = {NULL,          question,    PREC_CALL},
    [TOKEN_SEMICOLON]      = {NULL,          NULL,        PREC_NONE},
    [TOKEN_SLASH]          = {NULL,          binary,      PREC_FACTOR},
    [TOKEN_STAR]           = {NULL,          binary,      PREC_FACTOR},
    [TOKEN_BANG]           = {unary,         NULL,        PREC_NONE},
    [TOKEN_BANG_EQUAL]     = {NULL,          binary,      PREC_EQUALITY},
    [TOKEN_EQUAL]          = {NULL,          NULL,        PREC_NONE},
    [TOKEN_EQUAL_EQUAL]    = {NULL,          binary,      PREC_EQUALITY},
    [TOKEN_GREATER]        = {NULL,          binary,      PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL]  = {NULL,          binary,      PREC_COMPARISON},
    [TOKEN_LESS]           = {NULL,          binary,      PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]     = {NULL,          binary,      PREC_COMPARISON},
    [TOKEN_DOT]            = {NULL,          dot,         PREC_CALL},
    [TOKEN_DOT_DOT]        = {NULL,          binary,      PREC_CALL},
    [TOKEN_IDENTIFIER]     = {variable,      NULL,        PREC_NONE},
    [TOKEN_STRING]         = {string,        NULL,        PREC_NONE},
    [TOKEN_INTERPOLATION]  = {interpolation, NULL,        PREC_NONE},
    [TOKEN_NUMBER]         = {literal,       NULL,        PREC_NONE},
    [TOKEN_INT]            = {literal,       NULL,        PREC_NONE},
    [TOKEN_AND]            = {NULL,          logical,     PREC_AND},
    [TOKEN_AS]             = {NULL,          NULL,        PREC_NONE},
    [TOKEN_ASYNC]          = {async,         NULL,        PREC_NONE},
    [TOKEN_AWAIT]          = {await,         NULL,        PREC_NONE},
    [TOKEN_BREAK]          = {NULL,          NULL,        PREC_NONE},
    [TOKEN_CASE]           = {NULL,          NULL,        PREC_NONE},
    [TOKEN_CATCH]          = {NULL,          NULL,        PREC_NONE},
    [TOKEN_CLASS]          = {class_,        NULL,        PREC_NONE},
    [TOKEN_CONTINUE]       = {NULL,          NULL,        PREC_NONE},
    [TOKEN_DEFAULT]        = {NULL,          NULL,        PREC_NONE},
    [TOKEN_ELSE]           = {NULL,          NULL,        PREC_NONE},
    [TOKEN_FALSE]          = {literal,       NULL,        PREC_NONE},
    [TOKEN_FINALLY]        = {NULL,          NULL,        PREC_NONE},
    [TOKEN_FOR]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_FUN]            = {closure,       NULL,        PREC_NONE},
    [TOKEN_IF]             = {NULL,          NULL,        PREC_NONE},
    [TOKEN_NAMESPACE]      = {NULL,          NULL,        PREC_NONE},
    [TOKEN_NIL]            = {literal,       NULL,        PREC_NONE},
    [TOKEN_OR]             = {NULL,          logical,     PREC_OR},
    [TOKEN_REQUIRE]        = {NULL,          NULL,        PREC_NONE},
    [TOKEN_RETURN]         = {NULL,          NULL,        PREC_NONE},
    [TOKEN_SUPER]          = {super_,        NULL,        PREC_NONE},
    [TOKEN_SWITCH]         = {NULL,          NULL,        PREC_NONE},
    [TOKEN_THIS]           = {this_,         NULL,        PREC_NONE},
    [TOKEN_THROW]          = {NULL,          NULL,        PREC_NONE},
    [TOKEN_TRAIT]          = {trait,         NULL,        PREC_NONE},
    [TOKEN_TRUE]           = {literal,       NULL,        PREC_NONE},
    [TOKEN_TRY]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_USING]          = {NULL,          NULL,        PREC_NONE},
    [TOKEN_VAL]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_VAR]            = {NULL,          NULL,        PREC_NONE},
    [TOKEN_WHILE]          = {NULL,          NULL,        PREC_NONE},
    [TOKEN_WITH]           = {NULL,          NULL,        PREC_NONE},
    [TOKEN_YIELD]          = {yield,         NULL,        PREC_NONE},
    [TOKEN_ERROR]          = {NULL,          NULL,        PREC_NONE},
    [TOKEN_EMPTY]          = {NULL,          NULL,        PREC_NONE},
    [TOKEN_EOF]            = {NULL,          NULL,        PREC_NONE},
};

static Ast* parsePrefix(Parser* parser, Precedence precedence, bool canAssign) {
    ParsePrefixFn prefixRule = getRule(parser->previous.type)->prefix;
    if (prefixRule == NULL) {
        error(parser, "Expect expression.");
        return NULL;
    }
    return prefixRule(parser, parser->previous, canAssign);
}

static Ast* parseInfix(Parser* parser, Precedence precedence, Ast* left, bool canAssign) {
    while (precedence <= getRule(parser->current.type)->precedence) {
        advance(parser);
        ParseInfixFn infixRule = getRule(parser->previous.type)->infix;
        left = infixRule(parser, parser->previous, left, canAssign);
    }

    if (canAssign && match(parser, TOKEN_EQUAL)) {
        error(parser, "Invalid assignment target.");
    }
    return left;
}

static Ast* parsePrecedence(Parser* parser, Precedence precedence) {
    advance(parser);
    bool canAssign = precedence <= PREC_ASSIGNMENT;
    Ast* left = parsePrefix(parser, precedence, canAssign);
    return parseInfix(parser, precedence, left, canAssign);
}

static ParseRule* getRule(TokenSymbol type) {
    return &parseRules[type];
}

static Ast* expression(Parser* parser) {
    return parsePrecedence(parser, PREC_ASSIGNMENT);
}

static Ast* block(Parser* parser) {
    Token token = parser->previous;
    Ast* stmtList = emptyAst(AST_STMT_BLOCK, token);
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        Ast* decl = declaration(parser);
        astAppendChild(stmtList, decl);
    }
    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
    return newAst(AST_LIST_STMT, token, 1, stmtList);
}

static Ast* awaitStatement(Parser* parser) {
    Token token = parser->previous;
    Ast* expr = expression(parser);
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after await value.");
    return newAst(AST_STMT_AWAIT, token, 1, expr);
}

static Ast* breakStatement(Parser* parser) {
    Ast* stmt = emptyAst(AST_STMT_BREAK, parser->previous);
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after 'break'.");
    return stmt;
}

static Ast* continueStatement(Parser* parser) {
    Ast* stmt = emptyAst(AST_STMT_CONTINUE, parser->previous);
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after 'continue'.");
    return stmt;
}

static Ast* expressionStatement(Parser* parser) {
    Token token = parser->previous;
    Ast* expr = expression(parser);
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after expression.");
    return newAst(AST_STMT_EXPRESSION, token, 1, expr);
}

static Ast* forStatement(Parser* parser) {
    Token token = parser->previous;
    Ast* stmt = emptyAst(AST_STMT_FOR, token);
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    consume(parser, TOKEN_VAR, "Expect 'var' keyword after '(' in For loop.");
    Ast* decl = emptyAst(AST_LIST_VAR, parser->previous);

    if (match(parser, TOKEN_LEFT_PAREN)) { 
        astAppendChild(decl, identifier(parser, "Expect first variable name after '('."));
        consume(parser, TOKEN_COMMA, "Expect ',' after first variable declaration.");
        astAppendChild(decl, identifier(parser, "Expect second variable name after ','."));
        consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after second variable declaration.");
    }
    else astAppendChild(decl, identifier(parser, "Expect variable name after 'var'."));

    consume(parser, TOKEN_COLON, "Expect ':' after variable name.");
    Ast* expr = expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after loop expression.");
    Ast* body = statement(parser);
    return newAst(AST_STMT_FOR, token, 3, decl, expr, body);
}

static Ast* ifStatement(Parser* parser) {
    Token token = parser->previous;
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    Ast* condition = expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    Ast* thenBranch = statement(parser);

    Ast* stmt = newAst(AST_STMT_IF, token, 2, condition, thenBranch);
    if (match(parser, TOKEN_ELSE)) {
        Ast* elseBranch = statement(parser);
        astAppendChild(stmt, elseBranch);
    }
    return stmt;
}

static Ast* requireStatement(Parser* parser) {
    Token token = parser->previous;
    Ast* expr = expression(parser);
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after required file path.");
    return newAst(AST_STMT_REQUIRE, token, 1, expr);
}

static Ast* returnStatement(Parser* parser) {
    Token token = parser->previous;
    if (match(parser, TOKEN_SEMICOLON)) return emptyAst(AST_STMT_RETURN, token);
    else {
        Ast* expr = expression(parser);
        consume(parser, TOKEN_SEMICOLON, "Expect ';' after return value.");
        return newAst(AST_STMT_RETURN, token, 1, expr);
    }
}

static Ast* switchStatement(Parser* parser) {
    Ast* stmt = emptyAst(AST_STMT_SWITCH, parser->previous);
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'switch'.");
    Ast* expr = expression(parser);
    astAppendChild(stmt, expr);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after value.");
    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before switch cases.");
    Ast* caseListStmt = emptyAst(AST_LIST_STMT, parser->previous);
    astAppendChild(stmt, caseListStmt);

    int state = 0;
    int caseCount = 0;
    while (!match(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        if (match(parser, TOKEN_CASE) || match(parser, TOKEN_DEFAULT)) {
            Token caseToken = parser->previous;
            if (state == 1) caseCount++;
            if (state == 2) error(parser, "Can't have another case or default after the default case.");

            if (caseToken.type == TOKEN_CASE) {
                state = 1;
                Ast* caseLabel = expression(parser);
                consume(parser, TOKEN_COLON, "Expect ':' after case value.");
                Token token = parser->previous;
                Ast* caseBody = statement(parser);
                Ast* caseStmt = newAst(AST_STMT_CASE, token, 2, caseLabel, caseBody);
                astAppendChild(caseListStmt, caseStmt);
            }
            else {
                state = 2;
                consume(parser, TOKEN_COLON, "Expect ':' after default.");
                Ast* defaultStmt = statement(parser);
                astAppendChild(stmt, defaultStmt);
            }
        }
    }

    return stmt;
}

static Ast* throwStatement(Parser* parser) {
    Token token = parser->previous;
    Ast* expr = expression(parser);
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after thrown exception object.");
    return newAst(AST_STMT_THROW, token, 1, expr);
}

static Ast* tryStatement(Parser* parser) {
    Ast* stmt = emptyAst(AST_STMT_TRY, parser->previous);
    Ast* tryStmt = statement(parser);
    astAppendChild(stmt, tryStmt);

    if (match(parser, TOKEN_CATCH)) {
        consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after catch");
        consume(parser, TOKEN_IDENTIFIER, "Expect type name to catch");
        Token exceptionType = parser->previous;
        Ast* exceptionVar = identifier(parser, "Expect identifier after exception type.");

        consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after catch statement");
        Ast* catchBody = statement(parser);
        Ast* catchStmt = newAst(AST_STMT_CATCH, exceptionType, 2, exceptionVar, catchBody);
        astAppendChild(stmt, catchStmt);
    }
    else errorAtCurrent(parser, "Must have a catch statement following a try statement.");

    if (match(parser, TOKEN_FINALLY)) {
        Ast* finallyStmt = statement(parser);
        astAppendChild(stmt, finallyStmt);
    }
    return stmt;
}

static Ast* usingStatement(Parser* parser) {
    Token token = parser->previous;
    Ast* stmt = emptyAst(AST_STMT_USING, token);
    Ast* _namespace = emptyAst(AST_LIST_VAR, token);
    Ast* subNamespace = NULL;
    Ast* alias = NULL;

    do {
        consume(parser, TOKEN_IDENTIFIER, "Expect namespace identifier.");
        subNamespace = emptyAst(AST_EXPR_VARIABLE, parser->previous);
        astAppendChild(_namespace, subNamespace);
    } while (match(parser, TOKEN_DOT));
    astAppendChild(stmt, _namespace);

    if (match(parser, TOKEN_AS)) {
        consume(parser, TOKEN_IDENTIFIER, "Expect alias after 'as'.");
        alias = emptyAst(AST_EXPR_VARIABLE, parser->previous);
        astAppendChild(stmt, alias);
    }
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after using statement.");
    return stmt;
}

static Ast* whileStatement(Parser* parser) {
    Token token = parser->previous;
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    Ast* condition = expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    Ast* body = statement(parser);
    return newAst(AST_STMT_WHILE, token, 2, condition, body);
}

static Ast* yieldStatement(Parser* parser) {
    Token token = parser->previous;
    Ast* expr = expression(parser);
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after yield value.");
    return newAst(AST_STMT_YIELD, token, 1, expr);
}

static Ast* statement(Parser* parser) {
    if (match(parser, TOKEN_AWAIT)) {
        return awaitStatement(parser);
    }
    else if (match(parser, TOKEN_BREAK)) {
        return breakStatement(parser);
    }
    else if (match(parser, TOKEN_CONTINUE)) {
        return continueStatement(parser);
    }
    else if (match(parser, TOKEN_FOR)) {
        return forStatement(parser);
    }
    else if (match(parser, TOKEN_IF)) {
        return ifStatement(parser);
    }
    else if (match(parser, TOKEN_REQUIRE)) {
        return requireStatement(parser);
    }
    else if (match(parser, TOKEN_RETURN)) {
        return returnStatement(parser);
    }
    else if (match(parser, TOKEN_SWITCH)) {
        return switchStatement(parser);
    }
    else if (match(parser, TOKEN_THROW)) {
        return throwStatement(parser);
    }
    else if (match(parser, TOKEN_TRY)) {
        return tryStatement(parser);
    }
    else if (match(parser, TOKEN_USING)) {
        return usingStatement(parser);
    }
    else if (match(parser, TOKEN_WHILE)) {
        return whileStatement(parser);
    }
    else if (match(parser, TOKEN_YIELD)) {
        return yieldStatement(parser);
    }
    else if (match(parser, TOKEN_LEFT_BRACE)) {
        return block(parser);
    }
    else {
        return expressionStatement(parser);
    }
}

static Ast* parameterList(Parser* parser, Token token) {
    Ast* params = emptyAst(AST_LIST_VAR, token);
    int arity = 0;

    if (match(parser, TOKEN_DOT_DOT)) {
        Ast* param = identifier(parser, "Expect parameter name.");
        param->modifier.isVariadic = true;
        astAppendChild(params, param);
    }

    do {
        arity++;
        if (arity > UINT8_MAX) {
            errorAtCurrent(parser, "Can't have more than 255 parameters.");
        }

        bool isMutable = match(parser, TOKEN_VAR);
        Ast* param = identifier(parser, "Expect parameter name.");
        param->modifier.isMutable = isMutable;
        astAppendChild(params, param);
    } while (match(parser, TOKEN_COMMA));

    return params;
}

static Ast* functionParameters(Parser* parser) {
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after function keyword/name.");
    Ast* params = parameterList(parser, parser->previous);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    return params;
}

static Ast* lambdaParameters(Parser* parser) {
    Token token = parser->previous;
    if (!match(parser, TOKEN_PIPE)) return emptyAst(AST_LIST_VAR, token);
    return parameterList(parser, token);
}

static Ast* function(Parser* parser, bool isAsync, bool isLambda) {
    Token token = parser->previous;
    Ast* params = isLambda ? lambdaParameters(parser) : functionParameters(parser);
    Ast* body = block(parser);
    Ast* func = newAst(AST_EXPR_FUNCTION, token, 2, params, body);
    func->modifier.isAsync = isAsync;
    func->modifier.isLambda = isLambda;
    return func;
}

static Ast* classDeclaration(Parser* parser) {
    consume(parser, TOKEN_IDENTIFIER, "Expect class name.");
    Token name = parser->previous;
    Ast* superClass = superclass_(parser);
    Ast* traitList = traits(parser, &name);
    Ast* methodList = methods(parser, &name);
    return newAst(AST_DECL_CLASS, name, 3, superClass, traitList, methodList);
}

static Ast* funDeclaration(Parser* parser, bool isAsync) {
    consume(parser, TOKEN_IDENTIFIER, "Expect function name.");
    Token name = parser->previous;
    Ast* body = function(parser, isAsync, false);
    return newAst(AST_DECL_FUN, name, 1, body);
}

static Ast* namespaceDeclaration(Parser* parser) {
    Token token = parser->previous;
    Ast* _namespace = emptyAst(AST_LIST_VAR, token);
    uint8_t namespaceDepth = 0;
    do {
        if (namespaceDepth > UINT4_MAX) {
            errorAtCurrent(parser, "Can't have more than 15 levels of namespace depth.");
        }
        astAppendChild(_namespace, identifier(parser, "Expect Namespace identifier."));
        namespaceDepth++;
    } while (match(parser, TOKEN_DOT));

    consume(parser, TOKEN_SEMICOLON, "Expect semicolon after namespace declaration.");
    return newAst(AST_DECL_NAMESPACE, token, 1, _namespace);
}

static Ast* traitDeclaration(Parser* parser) {
    consume(parser, TOKEN_IDENTIFIER, "Expect trait name.");
    Token traitName = parser->previous;
    Ast* traitList = traits(parser, &traitName);
    Ast* methodList = methods(parser, &traitName);
    return newAst(AST_DECL_TRAIT, traitName, 2, traitList, methodList);
}

static Ast* varDeclaration(Parser* parser, bool isMutable) {
    consume(parser, TOKEN_IDENTIFIER, "Expect variable name.");
    Token identifier = parser->previous;
    Ast* varDecl = emptyAst(AST_DECL_VAR, identifier);
    varDecl->modifier.isMutable = isMutable;
    if (!isMutable && !check(parser, TOKEN_EQUAL)) {
        error(parser, "Immutable variable must be initialized upon declaration.");
    }
    else if (match(parser, TOKEN_EQUAL)) {
        Ast* expr = expression(parser);
        astAppendChild(varDecl, expr);
    }

    consume(parser, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
    return varDecl;
}

static Ast* declaration(Parser* parser) {
    if (check(parser, TOKEN_ASYNC) && checkNext(parser, TOKEN_FUN)) {
        advance(parser);
        advance(parser);
        return funDeclaration(parser, true);
    }
    else if (check(parser, TOKEN_CLASS) && checkNext(parser, TOKEN_IDENTIFIER)) {
        advance(parser);
        return classDeclaration(parser);
    }
    else if (check(parser, TOKEN_FUN) && checkNext(parser, TOKEN_IDENTIFIER)) {
        advance(parser);
        return funDeclaration(parser, false);
    }
    else if (match(parser, TOKEN_NAMESPACE)) {
        return namespaceDeclaration(parser);
    }
    else if (check(parser, TOKEN_TRAIT) && checkNext(parser, TOKEN_IDENTIFIER)) {
        advance(parser);
        return traitDeclaration(parser);
    }
    else if (match(parser, TOKEN_VAL)) {
        return varDeclaration(parser, false);
    }
    else if (match(parser, TOKEN_VAR)) {
        return varDeclaration(parser, true);
    }
    else return statement(parser);
}

void initParser(Parser* parser, Lexer* lexer) {
    parser->lexer = lexer;
    parser->rootClass = syntheticToken("Object");
    parser->hadError = false;
    parser->panicMode = false;
    advance(parser);
    advance(parser);
}

Ast* parse(Parser* parser) {
    Ast* ast = emptyAst(AST_TYPE_NONE, emptyToken());
    while (!match(parser, TOKEN_EOF)) {
        if (setjmp(parser->jumpBuffer) == 0) {
            Ast* decl = declaration(parser);
            astAppendChild(ast, decl);
        }
        else synchronize(parser);
    }

#ifdef DEBUG_PRINT_AST
    astOutput(ast, 0);
#endif

    return ast;
}
