#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

void initScanner(Scanner* scanner, const char* source) {
    scanner->start = source;
    scanner->current = source;
    scanner->line = 1;
}

static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static bool isAtEnd(Scanner* scanner) {
    return *scanner->current == '\0';
}

static char advance(Scanner* scanner) {
    scanner->current++;
    return scanner->current[-1];
}

static char peek(Scanner* scanner) {
    return *scanner->current;
}

static char peekNext(Scanner* scanner) {
    if (isAtEnd(scanner)) return '\0';
    return scanner->current[1];
}

static bool match(Scanner* scanner, char expected) {
    if (isAtEnd(scanner)) return false;
    if (*scanner->current != expected) return false;
    scanner->current++;
    return true;
}

static Token makeToken(Scanner* scanner, TokenType type) {
    Token token = {
        .type = type, 
        .start = scanner->start, 
        .length = (int)(scanner->current - scanner->start), 
        .line = scanner->line
    };
    return token;
}

static Token errorToken(Scanner* scanner, const char* message) {
    Token token = {
        .type = TOKEN_ERROR, 
        .start = message, 
        .length = (int)strlen(message), 
        .line = scanner->line
    };
    return token;
}

static void skipLineComment(Scanner* scanner) {
    while (peek(scanner) != '\n' && !isAtEnd(scanner)) {
        advance(scanner);
    }
}

static void skipBlockComment(Scanner* scanner) {
    int nesting = 1;
    while (nesting > 0) {
        if (isAtEnd(scanner)) return;

        if (peek(scanner) == '/' && peekNext(scanner) == '*') {
            advance(scanner);
            advance(scanner);
            nesting++;
            continue;
        }

        if (peek(scanner) == '*' && peekNext(scanner) == '/') {
            advance(scanner);
            advance(scanner);
            nesting--;
            continue;
        }

        if (peek(scanner) == '\n') scanner->line++;
        advance(scanner);
    }
}

static void skipWhitespace(Scanner* scanner) {
    for (;;) {
        char c = peek(scanner);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(scanner);
                break;
            case '\n':
                scanner->line++;
                advance(scanner);
                break;
            case '/':
                if (peekNext(scanner) == '/') {
                    skipLineComment(scanner);
                } 
                else if (peekNext(scanner) == '*') {
                    advance(scanner);
                    skipBlockComment(scanner);
                }
                else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static TokenType checkKeyword(Scanner* scanner, int start, int length,
    const char* rest, TokenType type) {
    if (scanner->current - scanner->start == start + length &&
        memcmp(scanner->start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

static TokenType identifierType(Scanner* scanner) {
    switch (scanner->start[0]) {
        case 'a': 
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'n': return checkKeyword(scanner, 2, 1, "d", TOKEN_AND);
                    case 's': return checkKeyword(scanner, 2, 0, "", TOKEN_AS);
                }
            }          
        case 'b': return checkKeyword(scanner, 1, 4, "reak", TOKEN_BREAK);
        case 'c':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a': return checkKeyword(scanner, 2, 2, "se", TOKEN_CASE);
                    case 'l': return checkKeyword(scanner, 2, 3, "ass", TOKEN_CLASS);
                }
            }
            break;
        case 'd': return checkKeyword(scanner, 1, 6, "efault", TOKEN_DEFAULT);
        case 'e': return checkKeyword(scanner, 1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a': return checkKeyword(scanner, 2, 3, "lse", TOKEN_FALSE);
                    case 'o': return checkKeyword(scanner, 2, 1, "r", TOKEN_FOR);
                    case 'u': return checkKeyword(scanner, 2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i': return checkKeyword(scanner, 1, 1, "f", TOKEN_IF);
        case 'n': 
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a': return checkKeyword(scanner, 2, 7, "mespace", TOKEN_NAMESPACE);
                    case 'i': return checkKeyword(scanner, 2, 1, "l", TOKEN_NIL);
                }
            }
        case 'o': return checkKeyword(scanner, 1, 1, "r", TOKEN_OR);
        case 'r': 
            if (scanner->current - scanner->start > 2) {
                switch (scanner->start[2]) {
                    case 'q': return checkKeyword(scanner, 3, 4, "uire", TOKEN_REQUIRE);
                    case 't': return checkKeyword(scanner, 3, 3, "urn", TOKEN_RETURN);
                }
            }
        case 's':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'u': return checkKeyword(scanner, 2, 3, "per", TOKEN_SUPER);
                    case 'w': return checkKeyword(scanner, 2, 4, "itch", TOKEN_SWITCH);
                }
            }
            break;
        case 't':
            if(scanner->current - scanner->start > 2) {
                switch (scanner->start[2]) {
                    case 'a': return checkKeyword(scanner, 3, 2, "it", TOKEN_TRAIT);
                    case 'i': return checkKeyword(scanner, 3, 1, "s", TOKEN_THIS);
                    case 'u': return checkKeyword(scanner, 3, 1, "e", TOKEN_TRUE);
                }
            }
            break;
        case 'u': return checkKeyword(scanner, 1, 1, "sing", TOKEN_USING);
        case 'v': 
            if (scanner->current - scanner->start > 2) {
                switch (scanner->start[2]) {
                    case 'l': return checkKeyword(scanner, 3, 0, "", TOKEN_VAL);
                    case 'r': return checkKeyword(scanner, 3, 0, "", TOKEN_VAR);
                }
            }
            break;
        case 'w':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                case 'h': return checkKeyword(scanner, 2, 3, "ile", TOKEN_WHILE);
                case 'i': return checkKeyword(scanner, 2, 2, "th", TOKEN_WITH);
                }
            }
            break;
    }
    return TOKEN_IDENTIFIER;
}

static Token identifier(Scanner* scanner) {
    while (isAlpha(peek(scanner)) || isDigit(peek(scanner))) advance(scanner);
    return makeToken(scanner, identifierType(scanner));
}

static Token number(Scanner* scanner) {
    while (isDigit(peek(scanner))) advance(scanner);

    if (peek(scanner) == '.' && isDigit(peekNext(scanner))) {
        advance(scanner);

        while (isDigit(peek(scanner))) advance(scanner);
        return makeToken(scanner, TOKEN_NUMBER);
    }
    return makeToken(scanner, TOKEN_INT);
}

static Token string(Scanner* scanner) {
    while (peek(scanner) != '"' && !isAtEnd(scanner)) {
        if (peek(scanner) == '\n') scanner->line++;
        advance(scanner);
    }

    if (isAtEnd(scanner)) return errorToken(scanner, "Unterminated string.");

    advance(scanner);
    return makeToken(scanner, TOKEN_STRING);
}

Token syntheticToken(const char* text) {
    Token token = { 
        .start = text,
        .length = (int)strlen(text)
    };
    return token;
}

Token scanToken(Scanner* scanner) {
    skipWhitespace(scanner);
    scanner->start = scanner->current;

    if (isAtEnd(scanner)) return makeToken(scanner, TOKEN_EOF);

    char c = advance(scanner);
    if (isAlpha(c)) return identifier(scanner);
    if (isDigit(c)) return number(scanner);

    switch (c) {
        case '(': return makeToken(scanner, TOKEN_LEFT_PAREN);
        case ')': return makeToken(scanner, TOKEN_RIGHT_PAREN);
        case '[': return makeToken(scanner, TOKEN_LEFT_BRACKET);
        case ']': return makeToken(scanner, TOKEN_RIGHT_BRACKET);
        case '{': return makeToken(scanner, TOKEN_LEFT_BRACE);
        case '}': return makeToken(scanner, TOKEN_RIGHT_BRACE);
        case ';': return makeToken(scanner, TOKEN_SEMICOLON);
        case ':': return makeToken(scanner, TOKEN_COLON);
        case ',': return makeToken(scanner, TOKEN_COMMA);
        case '-': return makeToken(scanner, TOKEN_MINUS);
        case '|': return makeToken(scanner, TOKEN_PIPE);
        case '+': return makeToken(scanner, TOKEN_PLUS);
        case '/': return makeToken(scanner, TOKEN_SLASH);
        case '*': return makeToken(scanner, TOKEN_STAR);
        case '!':
            return makeToken(scanner, match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return makeToken(scanner, match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '>':
            return makeToken(scanner, match(scanner, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '<':
            return makeToken(scanner, match(scanner, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '.': 
            return makeToken(scanner, match(scanner, '.') ? TOKEN_DOT_DOT : TOKEN_DOT);
        case '"': return string(scanner);
    }
    return errorToken(scanner, "Unexpected character.");
}
