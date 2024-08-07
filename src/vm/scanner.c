#include <stdio.h>
#include <string.h>
#include <uv.h>

#include "common.h"
#include "scanner.h"

void initScanner(Scanner* scanner, const char* source) {
    scanner->start = source;
    scanner->current = source;
    scanner->line = 1;
    scanner->interpolationDepth = 0;
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

static char peekPrevious(Scanner* scanner) {
    return scanner->current[-1];
}

static bool match(Scanner* scanner, char expected) {
    if (isAtEnd(scanner)) return false;
    if (*scanner->current != expected) return false;
    scanner->current++;
    return true;
}

static Token makeToken(Scanner* scanner, TokenSymbol type) {
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

static TokenSymbol checkKeyword(Scanner* scanner, int start, int length,
    const char* rest, TokenSymbol type) {
    if (scanner->current - scanner->start == start + length &&
        memcmp(scanner->start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

static TokenSymbol identifierType(Scanner* scanner) {
    if (scanner->start[-1] == '.') return TOKEN_IDENTIFIER;

    switch (scanner->start[0]) {
        case 'a': 
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'n': return checkKeyword(scanner, 2, 1, "d", TOKEN_AND);
                    case 's': { 
                        if (scanner->current - scanner->start > 2) return checkKeyword(scanner, 2, 3, "ync", TOKEN_ASYNC);
                        else return checkKeyword(scanner, 2, 0, "", TOKEN_AS);
                    }
                    case 'w': return checkKeyword(scanner, 2, 3, "ait", TOKEN_AWAIT);
                }
            }          
        case 'b': return checkKeyword(scanner, 1, 4, "reak", TOKEN_BREAK);
        case 'c':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a': 
                        if (scanner->current - scanner->start > 2) {
                            switch (scanner->start[2]) {
                                case 's': return checkKeyword(scanner, 3, 1, "e", TOKEN_CASE);
                                case 't': return checkKeyword(scanner, 3, 2, "ch", TOKEN_CATCH);
                            }
                        }
                    case 'l': return checkKeyword(scanner, 2, 3, "ass", TOKEN_CLASS);
                    case 'o': return checkKeyword(scanner, 2, 6, "ntinue", TOKEN_CONTINUE);
                }
            }
        case 'd': return checkKeyword(scanner, 1, 6, "efault", TOKEN_DEFAULT);
        case 'e': return checkKeyword(scanner, 1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a': return checkKeyword(scanner, 2, 3, "lse", TOKEN_FALSE);
                    case 'i': return checkKeyword(scanner, 2, 5, "nally", TOKEN_FINALLY);
                    case 'o': return checkKeyword(scanner, 2, 1, "r", TOKEN_FOR);
                    case 'u': return checkKeyword(scanner, 2, 1, "n", TOKEN_FUN);
                }
            }
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
            if (scanner->current - scanner->start > 2 && scanner->start[1] == 'e') {
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
        case 't':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'h':
                        if (scanner->current - scanner->start > 2) {
                            switch (scanner->start[2]) {
                                case 'i': return checkKeyword(scanner, 3, 1, "s", TOKEN_THIS);
                                case 'r': return checkKeyword(scanner, 3, 2, "ow", TOKEN_THROW);
                            }
                        }
                    case 'r':
                        if (scanner->current - scanner->start > 2) {
                            switch (scanner->start[2]) {
                                case 'a': return checkKeyword(scanner, 3, 2, "it", TOKEN_TRAIT);
                                case 'u': return checkKeyword(scanner, 3, 1, "e", TOKEN_TRUE);
                                case 'y': return checkKeyword(scanner, 3, 0, "", TOKEN_TRY);
                            }
                        }
                }
            }
            break;
        case 'u': return checkKeyword(scanner, 1, 4, "sing", TOKEN_USING);
        case 'v': 
            if (scanner->current - scanner->start > 2 && scanner->start[1] == 'a') {
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
        case 'y': return checkKeyword(scanner, 1, 4, "ield", TOKEN_YIELD);
    }
    return TOKEN_IDENTIFIER;
}

static Token identifier(Scanner* scanner) {
    while (isAlpha(peek(scanner)) || isDigit(peek(scanner))) advance(scanner);
    return makeToken(scanner, identifierType(scanner));
}

static Token keywordIdentifier(Scanner* scanner) {
    advance(scanner);
    while (isAlpha(peek(scanner)) || isDigit(peek(scanner))) advance(scanner);
    if (peek(scanner) == '`') {
        advance(scanner);
        return makeToken(scanner, TOKEN_IDENTIFIER);
    }
    else return errorToken(scanner, "Keyword identifiers must end with a closing backtick.");
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
    while ((peek(scanner) != '"' || peekPrevious(scanner) == '\\') && !isAtEnd(scanner)) {
        if (peek(scanner) == '\n') scanner->line++;
        else if (peek(scanner) == '$' && peekNext(scanner) == '{') {
            if (scanner->interpolationDepth >= UINT4_MAX) {
                return errorToken(scanner, "Interpolation may only nest 15 levels deep.");
            }
            scanner->interpolationDepth++;
            advance(scanner);
            Token token = makeToken(scanner, TOKEN_INTERPOLATION);
            advance(scanner);
            return token;
        }
        advance(scanner);
    }

    if (isAtEnd(scanner)) return errorToken(scanner, "Unterminated string.");
    advance(scanner);
    return makeToken(scanner, TOKEN_STRING);
}

Token synthesizeToken(const char* text) {
    Token token = { 
        .start = text,
        .length = (int)strlen(text)
    };
    return token;
}

Token scanNextToken(Scanner* scanner) {
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
        case '}': 
            if (scanner->interpolationDepth > 0) {
                scanner->interpolationDepth--;
                return string(scanner);
            }
            return makeToken(scanner, TOKEN_RIGHT_BRACE);
        case ';': return makeToken(scanner, TOKEN_SEMICOLON);
        case ':': return makeToken(scanner, TOKEN_COLON);
        case ',': return makeToken(scanner, TOKEN_COMMA);
        case '?': return makeToken(scanner, TOKEN_QUESTION);
        case '-': return makeToken(scanner, TOKEN_MINUS);
        case '%': return makeToken(scanner, TOKEN_MODULO);
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
        case '`':
            return keywordIdentifier(scanner);
        case '"': return string(scanner);
    }
    return errorToken(scanner, "Unexpected character.");
}
