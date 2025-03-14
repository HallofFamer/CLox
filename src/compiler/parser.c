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
    bool startExpr;
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
    parser->previousNewLine = parser->currentNewLine;
    parser->current = parser->next;
    parser->currentNewLine = false;

    for (;;) {
        parser->next = scanToken(parser->lexer);
        if (parser->next.type == TOKEN_NEW_LINE) {
            parser->currentNewLine = true;
            continue;
        }
        else if (parser->next.type != TOKEN_ERROR) break;
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

static void consumerTerminator(Parser* parser, const char* message) {
    if (parser->current.type == TOKEN_SEMICOLON) {
        advance(parser);
        return;
    }
    else if (parser->previousNewLine || parser->current.type == TOKEN_RIGHT_BRACE || parser->current.type == TOKEN_EOF) {
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

static bool check2(Parser* parser, TokenSymbol type) {
    return check(parser, type) && checkNext(parser, type);
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
    char* result = (char*)realloc(string, newSize);
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
    target[j] = '\0';
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

static Token identifierToken(Parser* parser, const char* message) {
    switch (parser->current.type) {
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
            advance(parser);
            return parser->previous;
        case TOKEN_LEFT_BRACKET:
            advance(parser);
            if (match(parser, TOKEN_RIGHT_BRACKET)) {
                return syntheticToken(match(parser, TOKEN_EQUAL) ? "[]=" : "[]");
            }
        case TOKEN_LEFT_PAREN:
            advance(parser);
            if (match(parser, TOKEN_RIGHT_PAREN)) {
                return syntheticToken("()");
            }
        default:
            break;
    }

    errorAtCurrent(parser, message);
    return parser->previous;
}

static Ast* and_(Parser* parser, Token token, Ast* left, bool canAssign) {
    ParseRule* rule = getRule(parser->previous.type);
    Ast* right = parsePrecedence(parser, PREC_AND);
    return newAst(AST_EXPR_AND, token, 2, left, right);
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
    Token property = identifierToken(parser, "Expect property name after '.'.");

    if (canAssign && match(parser, TOKEN_EQUAL)) {
        Ast* right = expression(parser);
        return newAst(AST_EXPR_PROPERTY_SET, property, 2, left, right);
    }
    else if (match(parser, TOKEN_LEFT_PAREN)) {
        Ast* right = argumentList(parser);
        return newAst(AST_EXPR_INVOKE, property, 2, left, right);
    }
    else {
        return newAst(AST_EXPR_PROPERTY_GET, property, 1, left);
    }
}

static Ast* nil(Parser* parser, Token token, Ast* left, bool canAssign) {
    Ast* right = parsePrecedence(parser, PREC_PRIMARY);
    return newAst(AST_EXPR_NIL, token, 2, left, right);
}

static Ast* or_(Parser* parser, Token token, Ast* left, bool canAssign) {
    ParseRule* rule = getRule(parser->previous.type);
    Ast* right = parsePrecedence(parser, PREC_OR);
    return newAst(AST_EXPR_OR, token, 2, left, right);
}

static Ast* subscript(Parser* parser, Token token, Ast* left, bool canAssign) {
    Ast* index = expression(parser);
    consume(parser, TOKEN_RIGHT_BRACKET, "Expect ']' after subscript.");

    if (canAssign && match(parser, TOKEN_EQUAL)) {
        Ast* right = expression(parser);
        return newAst(AST_EXPR_SUBSCRIPT_SET, token, 3, left, index, right);
    }
    else {
        return newAst(AST_EXPR_SUBSCRIPT_GET, token, 2, left, index);
    }
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
            Ast* str = string(parser, parser->previous, false);
            concatenate = true;
            isString = true;
            astAppendChild(exprs, str);
        }

        Ast* expr = expression(parser);
        astAppendChild(exprs, expr);
        count++;
    } while (match(parser, TOKEN_INTERPOLATION));

    consume(parser, TOKEN_STRING, "Expect end of string interpolation.");
    if (parser->previous.length > 2) {
        Ast* str = string(parser, parser->previous, false);
        astAppendChild(exprs, str);
    }
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
    if (canAssign && match(parser, TOKEN_EQUAL)) {
        Ast* expr = expression(parser);
        return newAst(AST_EXPR_ASSIGN, token, 1, expr);
    }
    return emptyAst(AST_EXPR_VARIABLE, token);
}

static Ast* type_(Parser* parser, const char* message) {
    consume(parser, TOKEN_IDENTIFIER, message);
    return variable(parser, parser->previous, false);
}

static Ast* parameter(Parser* parser, const char* message) {
    bool isMutable = match(parser, TOKEN_VAR);
    Ast* type = NULL;
    if (check2(parser, TOKEN_IDENTIFIER)) type = type_(parser, "Expect type declaration.");
    consume(parser, TOKEN_IDENTIFIER, message);

    Ast* param = emptyAst(AST_EXPR_PARAM, parser->previous);
    if (type != NULL) astAppendChild(param, type);
    param->modifier.isMutable = isMutable;
    return param;
}

static Ast* parameterList(Parser* parser, Token token) {
    Ast* params = emptyAst(AST_LIST_VAR, token);
    int arity = 0;

    if (match(parser, TOKEN_RIGHT_PAREN)) return params;
    if (match(parser, TOKEN_DOT_DOT)) {
        Ast* param = parameter(parser, "Expect variadic parameter name.");
        param->modifier.isVariadic = true;
        astAppendChild(params, param);
        if (match(parser, TOKEN_COMMA)) error(parser, "Cannot have more parameters following variadic parameter.");
        return params;
    }

    do {
        arity++;
        if (arity > UINT8_MAX) errorAtCurrent(parser, "Can't have more than 255 parameters.");
        Ast* param = parameter(parser, "Expect parameter name");
        astAppendChild(params, param);
    } while (match(parser, TOKEN_COMMA));
    return params;
}

static Ast* functionParameters(Parser* parser) {
    Token token = parser->previous;
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after function keyword/name.");
    Ast* params = check(parser, TOKEN_RIGHT_PAREN) ? emptyAst(AST_LIST_VAR, token) : parameterList(parser, token);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    return params;
}

static Ast* lambdaParameters(Parser* parser) {
    Token token = parser->previous;
    if (!match(parser, TOKEN_PIPE)) return emptyAst(AST_LIST_VAR, token);
    Ast* params = parameterList(parser, token);
    consume(parser, TOKEN_PIPE, "Expect '|' after lambda parameters.");
    return params;
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

static Ast* methods(Parser* parser, Token* name) {
    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before class/trait body.");
    Ast* methodList = emptyAst(AST_LIST_METHOD, *name);

    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        bool isAsync = false, isClass = false, isInitializer = false, hasReturnType = false;
        Ast* returnType = NULL;
        if (match(parser, TOKEN_ASYNC)) isAsync = true;
        if (match(parser, TOKEN_CLASS)) isClass = true;
        if (check2(parser, TOKEN_IDENTIFIER) || (check(parser, TOKEN_IDENTIFIER) && tokenIsOperator(parser->next))) {
            hasReturnType = true;
            returnType = type_(parser, "Expect method return type.");
        }

        Token methodName = identifierToken(parser, "Expect method name.");
        if (parser->previous.length == 8 && memcmp(parser->previous.start, "__init__", 8) == 0) {
            isInitializer = true;
        }

        Ast* methodParams = functionParameters(parser);
        Ast* methodBody = block(parser);
        Ast* method = newAst(AST_DECL_METHOD, methodName, 2, methodParams, methodBody);
        method->modifier.isAsync = isAsync;
        method->modifier.isClass = isClass;
        method->modifier.isInitializer = isInitializer;

        if (hasReturnType) {
            astAppendChild(method, returnType);
        }
        astAppendChild(methodList, method);
    }

    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after class/trait body.");
    return methodList;
}

static Ast* superclass_(Parser* parser) {
    if (match(parser, TOKEN_EXTENDS)) {
        return identifier(parser, "Expect super class name.");
    }
    return emptyAst(AST_EXPR_VARIABLE, parser->rootClass);
}

static Ast* traits(Parser* parser, Token* name) {
    Ast* traitList = emptyAst(AST_LIST_VAR, *name);
    if (!match(parser, TOKEN_WITH)) return traitList;
    uint8_t traitCount = 0;

    do {
        traitCount++;
        if (traitCount > UINT4_MAX) {
            errorAtCurrent(parser, "Can't have more than 15 parameters.");
        }

        Ast* trait = identifier(parser, "Expect trait name.");
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
        Ast* arguments = argumentList(parser);
        return newAst(AST_EXPR_SUPER_INVOKE, method, 1, arguments);
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
    if (match(parser, TOKEN_RIGHT_PAREN) || match(parser, TOKEN_RIGHT_BRACKET) || match(parser, TOKEN_RIGHT_BRACE)
        || match(parser, TOKEN_COMMA) || match(parser, TOKEN_SEMICOLON)) 
    {
        return emptyAst(AST_EXPR_YIELD, token);
    }

    bool isWith = match(parser, TOKEN_WITH);
    Ast* expr = expression(parser);
    Ast* ast = newAst(AST_EXPR_YIELD, token, 1, expr);
    ast->modifier.isWith = isWith;
    return ast;
}

static Ast* async(Parser* parser, Token token, bool canAssign) { 
    if (match(parser, TOKEN_FUN)) {
        return function(parser, true, false);
    }
    else if (match(parser, TOKEN_LEFT_BRACE)) {
        return function(parser, true, true);
    }
    else {
        error(parser, "Can only use async as expression modifier for anonymous functions or lambda.");
        return NULL;
    }
}

static Ast* await(Parser* parser, Token token, bool canAssign) { 
    Ast* expr = expression(parser);
    return newAst(AST_EXPR_AWAIT, token, 1, expr);
}

ParseRule parseRules[] = {
    [TOKEN_LEFT_PAREN]     = {grouping,      call,        PREC_CALL,        true},
    [TOKEN_RIGHT_PAREN]    = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_LEFT_BRACKET]   = {collection,    subscript,   PREC_CALL,        true},
    [TOKEN_RIGHT_BRACKET]  = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_LEFT_BRACE]     = {lambda,        NULL,        PREC_NONE,        true},
    [TOKEN_RIGHT_BRACE]    = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_COLON]          = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_COMMA]          = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_MINUS]          = {unary,         binary,      PREC_TERM,        true},
    [TOKEN_MODULO]         = {NULL,          binary,      PREC_FACTOR,      false},
    [TOKEN_PIPE]           = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_PLUS]           = {NULL,          binary,      PREC_TERM,        false},
    [TOKEN_QUESTION]       = {NULL,          question,    PREC_CALL,        false},
    [TOKEN_SEMICOLON]      = {NULL,          NULL,        PREC_NONE,        true},
    [TOKEN_SLASH]          = {NULL,          binary,      PREC_FACTOR,      false},
    [TOKEN_STAR]           = {NULL,          binary,      PREC_FACTOR,      false},
    [TOKEN_BANG]           = {unary,         NULL,        PREC_NONE,        true},
    [TOKEN_BANG_EQUAL]     = {NULL,          binary,      PREC_EQUALITY,    false},   
    [TOKEN_EQUAL]          = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_EQUAL_EQUAL]    = {NULL,          binary,      PREC_EQUALITY,    false},
    [TOKEN_GREATER]        = {NULL,          binary,      PREC_COMPARISON,  false},
    [TOKEN_GREATER_EQUAL]  = {NULL,          binary,      PREC_COMPARISON,  false},
    [TOKEN_LESS]           = {NULL,          binary,      PREC_COMPARISON,  false},
    [TOKEN_LESS_EQUAL]     = {NULL,          binary,      PREC_COMPARISON,  false},
    [TOKEN_DOT]            = {NULL,          dot,         PREC_CALL,        false},
    [TOKEN_DOT_DOT]        = {NULL,          binary,      PREC_CALL,        false},
    [TOKEN_IDENTIFIER]     = {variable,      NULL,        PREC_NONE,        true},
    [TOKEN_STRING]         = {string,        NULL,        PREC_NONE,        true},
    [TOKEN_INTERPOLATION]  = {interpolation, NULL,        PREC_NONE,        true},
    [TOKEN_NUMBER]         = {literal,       NULL,        PREC_NONE,        true},
    [TOKEN_INT]            = {literal,       NULL,        PREC_NONE,        true},
    [TOKEN_AND]            = {NULL,          and_,        PREC_AND,         false},
    [TOKEN_AS]             = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_ASYNC]          = {async,         NULL,        PREC_NONE,        true},
    [TOKEN_AWAIT]          = {await,         NULL,        PREC_NONE,        true},
    [TOKEN_BREAK]          = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_CASE]           = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_CATCH]          = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_CLASS]          = {class_,        NULL,        PREC_NONE,        true},
    [TOKEN_CONTINUE]       = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_DEFAULT]        = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_ELSE]           = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_EXTENDS]        = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_FALSE]          = {literal,       NULL,        PREC_NONE,        true},
    [TOKEN_FINALLY]        = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_FOR]            = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_FUN]            = {closure,       NULL,        PREC_NONE,        true},
    [TOKEN_IF]             = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_NAMESPACE]      = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_NIL]            = {literal,       NULL,        PREC_NONE,        true},
    [TOKEN_OR]             = {NULL,          or_,         PREC_OR,          false},
    [TOKEN_REQUIRE]        = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_RETURN]         = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_SUPER]          = {super_,        NULL,        PREC_NONE,        true},
    [TOKEN_SWITCH]         = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_THIS]           = {this_,         NULL,        PREC_NONE,        true},
    [TOKEN_THROW]          = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_TRAIT]          = {trait,         NULL,        PREC_NONE,        true},
    [TOKEN_TRUE]           = {literal,       NULL,        PREC_NONE,        true},
    [TOKEN_TRY]            = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_USING]          = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_VAL]            = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_VAR]            = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_WHILE]          = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_WITH]           = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_YIELD]          = {yield,         NULL,        PREC_NONE,        true},
    [TOKEN_ERROR]          = {NULL,          NULL,        PREC_NONE,        false},
    [TOKEN_EMPTY]          = {NULL,          NULL,        PREC_NONE,        true},
    [TOKEN_NEW_LINE]       = {NULL,          NULL,        PREC_NONE,        true},
    [TOKEN_EOF]            = {NULL,          NULL,        PREC_NONE,        true},
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
    Ast* stmtList = emptyAst(AST_LIST_STMT, token);
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        Ast* decl = declaration(parser);
        astAppendChild(stmtList, decl);
    }

    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
    return newAst(AST_STMT_BLOCK, token, 1, stmtList);
}

static Ast* awaitStatement(Parser* parser) {
    Token token = parser->previous;
    Ast* expr = expression(parser);
    consumerTerminator(parser, "Expect semicolon or new line after awaiting promise.");
    return newAst(AST_STMT_AWAIT, token, 1, expr);
}

static Ast* breakStatement(Parser* parser) {
    Ast* stmt = emptyAst(AST_STMT_BREAK, parser->previous);
    consumerTerminator(parser, "Expect semicolon or new line after 'break'.");
    return stmt;
}

static Ast* continueStatement(Parser* parser) {
    Ast* stmt = emptyAst(AST_STMT_CONTINUE, parser->previous);
    consumerTerminator(parser, "Expect semicolon or new line after 'continue'.");
    return stmt;
}

static Ast* expressionStatement(Parser* parser) {
    Token token = parser->previous;
    Ast* expr = expression(parser);
    consumerTerminator(parser, "Expect semicolon or new line after expression.");
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
    else {
        astAppendChild(decl, identifier(parser, "Expect variable name after 'var'."));
    }

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
    consumerTerminator(parser, "Expect semicolon or new line after required file path.");
    return newAst(AST_STMT_REQUIRE, token, 1, expr);
}

static Ast* returnStatement(Parser* parser) {
    Token token = parser->previous;
    if (match(parser, TOKEN_SEMICOLON) || !getRule(parser->current.type)->startExpr) {
        return emptyAst(AST_STMT_RETURN, token);
    }
    else {
        Ast* expr = expression(parser);
        consumerTerminator(parser, "Expect semicolon or new line after return value.");
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
                Ast* defaultBody = statement(parser);
                Ast* defaultStmt = newAst(AST_STMT_DEFAULT, syntheticToken("default"), 1, defaultBody);
                astAppendChild(stmt, defaultStmt);
            }
        }
    }

    return stmt;
}

static Ast* throwStatement(Parser* parser) {
    Token token = parser->previous;
    Ast* expr = expression(parser);
    consumerTerminator(parser, "Expect semicolon or new line after thrown exception.");
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
    else {
        errorAtCurrent(parser, "Must have a catch statement following a try statement.");
    }

    if (match(parser, TOKEN_FINALLY)) {
        Ast* finallyBody = statement(parser);
        Ast* finallyStmt = newAst(AST_STMT_FINALLY, syntheticToken("finally"), 1, finallyBody);
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

    consumerTerminator(parser, "Expect semicolon or new-line after using statement.");
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
    if (match(parser, TOKEN_SEMICOLON) || !getRule(parser->current.type)->startExpr) {
        return emptyAst(AST_STMT_YIELD, token);
    }

    bool isWith = match(parser, TOKEN_WITH);
    Ast* expr = expression(parser);
    consumerTerminator(parser, "Expect semicolon or new line after yield value.");

    Ast* ast = newAst(AST_STMT_YIELD, token, 1, expr);
    ast->modifier.isWith = isWith;
    return ast;
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

static Ast* classDeclaration(Parser* parser) {
    consume(parser, TOKEN_IDENTIFIER, "Expect class name.");
    Token name = parser->previous;
    Ast* superClass = superclass_(parser);
    Ast* traitList = traits(parser, &name);
    Ast* methodList = methods(parser, &name);
    Ast* _class = newAst(AST_EXPR_CLASS, name, 3, superClass, traitList, methodList);
    return newAst(AST_DECL_CLASS, name, 1, _class);
}

static Ast* funDeclaration(Parser* parser, bool isAsync, bool hasReturnType) {
    Ast* returnType = hasReturnType ? type_(parser, "Expect function return type.") : NULL;
    consume(parser, TOKEN_IDENTIFIER, "Expect function name.");
    Token name = parser->previous;
    Ast* body = function(parser, isAsync, false);

    Ast* ast = newAst(AST_DECL_FUN, name, 1, body);
    if (returnType != NULL) astAppendChild(ast, returnType);
    return ast;
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

    consumerTerminator(parser, "Expect semicolon or new line after namespace declaration.");
    return newAst(AST_DECL_NAMESPACE, token, 1, _namespace);
}

static Ast* traitDeclaration(Parser* parser) {
    consume(parser, TOKEN_IDENTIFIER, "Expect trait name.");
    Token name = parser->previous;
    Ast* traitList = traits(parser, &name);
    Ast* methodList = methods(parser, &name);
    Ast* trait = newAst(AST_EXPR_TRAIT, name, 2, traitList, methodList);
    return newAst(AST_DECL_TRAIT, name, 1, trait);
}

static Ast* varDeclaration(Parser* parser, bool isMutable) {
    consume(parser, TOKEN_IDENTIFIER, "Expect variable name.");
    Token identifier = parser->previous;
    Ast* varDecl = emptyAst(AST_DECL_VAR, identifier);
    varDecl->modifier.isMutable = isMutable;

    if (match(parser, TOKEN_EQUAL)) {
        Ast* expr = expression(parser);
        astAppendChild(varDecl, expr);
    }

    consumerTerminator(parser, "Expect semicolon or new line after variable declaration.");
    return varDecl;
}

static Ast* declaration(Parser* parser) {
    if (check(parser, TOKEN_ASYNC) && checkNext(parser, TOKEN_FUN)) {
        advance(parser);
        advance(parser);
        return funDeclaration(parser, true, false);
    }
    if (check(parser, TOKEN_ASYNC) && checkNext(parser, TOKEN_IDENTIFIER)) {
        advance(parser);
        return funDeclaration(parser, true, true);
    }
    else if (check(parser, TOKEN_CLASS) && checkNext(parser, TOKEN_IDENTIFIER)) {
        advance(parser);
        return classDeclaration(parser);
    }
    else if ((check(parser, TOKEN_FUN) && checkNext(parser, TOKEN_IDENTIFIER))) {
        advance(parser);
        return funDeclaration(parser, false, false);
    }
    else if (check2(parser, TOKEN_IDENTIFIER)) {
        return funDeclaration(parser, false, true);
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
    else {
        return statement(parser);
    }
}

void initParser(Parser* parser, Lexer* lexer, bool debugAst) {
    parser->lexer = lexer;
    parser->rootClass = syntheticToken("Object");
    parser->debugAst = debugAst;
    parser->previousNewLine = false;
    parser->currentNewLine = true;
    parser->hadError = false;
    parser->panicMode = false;
    advance(parser);
    advance(parser);
}

Ast* parse(Parser* parser) {
    Ast* ast = emptyAst(AST_KIND_NONE, emptyToken());
    while (!match(parser, TOKEN_EOF)) {
        if (setjmp(parser->jumpBuffer) == 0) {
            Ast* decl = declaration(parser);
            astAppendChild(ast, decl);
        }
        else synchronize(parser);
    }

    if (parser->debugAst) {
        astOutput(ast, 0);
        printf("\n");
    }
    return ast;
}
