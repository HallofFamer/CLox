#include <stdio.h>
#include <stdlib.h>

#include "memory.h"
#include "parser.h"

void initParser(Parser* parser, VM* vm, Scanner* scanner) {
    parser->vm = vm;
    parser->scanner = scanner;
    parser->rootClass = syntheticToken("Object");
    parser->hadError = false;
    parser->panicMode = false;
}

static void errorAt(Parser* parser, Token* token, const char* message) {
    if (parser->panicMode) return;
    parser->panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    }
    else if (token->type == TOKEN_ERROR) {

    }
    else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser->hadError = true;
}

void error(Parser* parser, const char* message) {
    errorAt(parser, &parser->previous, message);
}

void errorAtCurrent(Parser* parser, const char* message) {
    errorAt(parser, &parser->current, message);
}

void advance(Parser* parser) {
    parser->previous = parser->current;
    parser->current = parser->next;

    for (;;) {
        parser->next = scanToken(parser->scanner);
        if (parser->next.type != TOKEN_ERROR) break;

        errorAtCurrent(parser, parser->next.start);
    }
}

void consume(Parser* parser, TokenSymbol type, const char* message) {
    if (parser->current.type == type) {
        advance(parser);
        return;
    }
    errorAtCurrent(parser, message);
}

bool check(Parser* parser, TokenSymbol type) {
    return parser->current.type == type;
}

bool checkNext(Parser* parser, TokenSymbol type) {
    return parser->next.type == type;
}

bool match(Parser* parser, TokenSymbol type) {
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

char* parseString(Parser* parser, int* length) {
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
    
    target = (char*)reallocate(parser->vm, target, (size_t)maxLength + 1, (size_t)j + 1);
    target[j] = '\0';
    *length = j;
    return target;
}

void synchronize(Parser* parser) {
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
            case TOKEN_WITH:
            case TOKEN_YIELD:
                return;

            default:
            ;
        }
        advance(parser);
    }
}