#include <stdio.h>
#include <string.h>
#include <uv.h>

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

static TokenV1 makeToken(Scanner* scanner, TokenSymbolV1 type) {
    TokenV1 token = {
        .type = type, 
        .start = scanner->start, 
        .length = (int)(scanner->current - scanner->start), 
        .line = scanner->line
    };
    return token;
}

static TokenV1 errorToken(Scanner* scanner, const char* message) {
    TokenV1 token = {
        .type = TOKEN_ERROR_V1,
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

static TokenSymbolV1 checkKeyword(Scanner* scanner, int start, int length,
    const char* rest, TokenSymbolV1 type) {
    if (scanner->current - scanner->start == start + length &&
        memcmp(scanner->start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER_V1;
}

static TokenSymbolV1 identifierType(Scanner* scanner) {
    if (scanner->start[-1] == '.') return TOKEN_IDENTIFIER_V1;

    switch (scanner->start[0]) {
        case 'a': 
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'n': return checkKeyword(scanner, 2, 1, "d", TOKEN_AND_V1);
                    case 's': { 
                        if (scanner->current - scanner->start > 2) return checkKeyword(scanner, 2, 3, "ync", TOKEN_ASYNC_V1);
                        else return checkKeyword(scanner, 2, 0, "", TOKEN_AS_V1);
                    }
                    case 'w': return checkKeyword(scanner, 2, 3, "ait", TOKEN_AWAIT_V1);
                }
            }          
        case 'b': return checkKeyword(scanner, 1, 4, "reak", TOKEN_BREAK_V1);
        case 'c':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a': 
                        if (scanner->current - scanner->start > 2) {
                            switch (scanner->start[2]) {
                                case 's': return checkKeyword(scanner, 3, 1, "e", TOKEN_CASE_V1);
                                case 't': return checkKeyword(scanner, 3, 2, "ch", TOKEN_CATCH_V1);
                            }
                        }
                    case 'l': return checkKeyword(scanner, 2, 3, "ass", TOKEN_CLASS_V1);
                    case 'o': return checkKeyword(scanner, 2, 6, "ntinue", TOKEN_CONTINUE_V1);
                }
            }
        case 'd': return checkKeyword(scanner, 1, 6, "efault", TOKEN_DEFAULT_V1);
        case 'e': return checkKeyword(scanner, 1, 3, "lse", TOKEN_ELSE_V1);
        case 'f':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a': return checkKeyword(scanner, 2, 3, "lse", TOKEN_FALSE_V1);
                    case 'i': return checkKeyword(scanner, 2, 5, "nally", TOKEN_FINALLY_V1);
                    case 'o': return checkKeyword(scanner, 2, 1, "r", TOKEN_FOR_V1);
                    case 'u': return checkKeyword(scanner, 2, 1, "n", TOKEN_FUN_V1);
                }
            }
        case 'i': return checkKeyword(scanner, 1, 1, "f", TOKEN_IF_V1);
        case 'n': 
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a': return checkKeyword(scanner, 2, 7, "mespace", TOKEN_NAMESPACE_V1);
                    case 'i': return checkKeyword(scanner, 2, 1, "l", TOKEN_NIL_V1);
                }
            }
        case 'o': return checkKeyword(scanner, 1, 1, "r", TOKEN_OR_V1);
        case 'r': 
            if (scanner->current - scanner->start > 2 && scanner->start[1] == 'e') {
                switch (scanner->start[2]) {
                    case 'q': return checkKeyword(scanner, 3, 4, "uire", TOKEN_REQUIRE_V1);
                    case 't': return checkKeyword(scanner, 3, 3, "urn", TOKEN_RETURN_V1);
                }
            }
        case 's':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'u': return checkKeyword(scanner, 2, 3, "per", TOKEN_SUPER_V1);
                    case 'w': return checkKeyword(scanner, 2, 4, "itch", TOKEN_SWITCH_V1);
                }
            }
        case 't':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'h':
                        if (scanner->current - scanner->start > 2) {
                            switch (scanner->start[2]) {
                                case 'i': return checkKeyword(scanner, 3, 1, "s", TOKEN_THIS_V1);
                                case 'r': return checkKeyword(scanner, 3, 2, "ow", TOKEN_THROW_V1);
                            }
                        }
                    case 'r':
                        if (scanner->current - scanner->start > 2) {
                            switch (scanner->start[2]) {
                                case 'a': return checkKeyword(scanner, 3, 2, "it", TOKEN_TRAIT_V1);
                                case 'u': return checkKeyword(scanner, 3, 1, "e", TOKEN_TRUE_V1);
                                case 'y': return checkKeyword(scanner, 3, 0, "", TOKEN_TRY_V1);
                            }
                        }
                }
            }
            break;
        case 'u': return checkKeyword(scanner, 1, 4, "sing", TOKEN_USING_V1);
        case 'v': 
            if (scanner->current - scanner->start > 2 && scanner->start[1] == 'a') {
                switch (scanner->start[2]) {
                    case 'l': return checkKeyword(scanner, 3, 0, "", TOKEN_VAL_V1);
                    case 'r': return checkKeyword(scanner, 3, 0, "", TOKEN_VAR_V1);
                }
            }
            break;
        case 'w':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                case 'h': return checkKeyword(scanner, 2, 3, "ile", TOKEN_WHILE_V1);
                case 'i': return checkKeyword(scanner, 2, 2, "th", TOKEN_WITH_V1);
                }
            }
            break;
        case 'y': return checkKeyword(scanner, 1, 4, "ield", TOKEN_YIELD_V1);
    }
    return TOKEN_IDENTIFIER_V1;
}

static TokenV1 identifier(Scanner* scanner) {
    while (isAlpha(peek(scanner)) || isDigit(peek(scanner))) advance(scanner);
    return makeToken(scanner, identifierType(scanner));
}

static TokenV1 keywordIdentifier(Scanner* scanner) {
    advance(scanner);
    while (isAlpha(peek(scanner)) || isDigit(peek(scanner))) advance(scanner);
    if (peek(scanner) == '`') {
        advance(scanner);
        return makeToken(scanner, TOKEN_IDENTIFIER_V1);
    }
    else return errorToken(scanner, "Keyword identifiers must end with a closing backtick.");
}

static TokenV1 number(Scanner* scanner) {
    while (isDigit(peek(scanner))) advance(scanner);

    if (peek(scanner) == '.' && isDigit(peekNext(scanner))) {
        advance(scanner);

        while (isDigit(peek(scanner))) advance(scanner);
        return makeToken(scanner, TOKEN_NUMBER_V1);
    }
    return makeToken(scanner, TOKEN_INT_V1);
}

static TokenV1 string(Scanner* scanner) {
    while ((peek(scanner) != '"' || peekPrevious(scanner) == '\\') && !isAtEnd(scanner)) {
        if (peek(scanner) == '\n') scanner->line++;
        else if (peek(scanner) == '$' && peekNext(scanner) == '{') {
            if (scanner->interpolationDepth >= UINT4_MAX) {
                return errorToken(scanner, "Interpolation may only nest 15 levels deep.");
            }
            scanner->interpolationDepth++;
            advance(scanner);
            TokenV1 token = makeToken(scanner, TOKEN_INTERPOLATION_V1);
            advance(scanner);
            return token;
        }
        advance(scanner);
    }

    if (isAtEnd(scanner)) return errorToken(scanner, "Unterminated string.");
    advance(scanner);
    return makeToken(scanner, TOKEN_STRING_V1);
}

TokenV1 synthesizeToken(const char* text) {
    TokenV1 token = { 
        .start = text,
        .length = (int)strlen(text)
    };
    return token;
}

TokenV1 scanNextToken(Scanner* scanner) {
    skipWhitespace(scanner);
    scanner->start = scanner->current;

    if (isAtEnd(scanner)) return makeToken(scanner, TOKEN_EOF_V1);

    char c = advance(scanner);
    if (isAlpha(c)) return identifier(scanner);
    if (isDigit(c)) return number(scanner);

    switch (c) {
        case '(': return makeToken(scanner, TOKEN_LEFT_PAREN_V1);
        case ')': return makeToken(scanner, TOKEN_RIGHT_PAREN_V1);
        case '[': return makeToken(scanner, TOKEN_LEFT_BRACKET_V1);
        case ']': return makeToken(scanner, TOKEN_RIGHT_BRACKET_V1);
        case '{': return makeToken(scanner, TOKEN_LEFT_BRACE_V1);
        case '}': 
            if (scanner->interpolationDepth > 0) {
                scanner->interpolationDepth--;
                return string(scanner);
            }
            return makeToken(scanner, TOKEN_RIGHT_BRACE_V1);
        case ';': return makeToken(scanner, TOKEN_SEMICOLON_V1);
        case ':': return makeToken(scanner, TOKEN_COLON_V1);
        case ',': return makeToken(scanner, TOKEN_COMMA_V1);
        case '?': return makeToken(scanner, TOKEN_QUESTION_V1);
        case '-': return makeToken(scanner, TOKEN_MINUS_V1);
        case '%': return makeToken(scanner, TOKEN_MODULO_V1);
        case '|': return makeToken(scanner, TOKEN_PIPE_V1);
        case '+': return makeToken(scanner, TOKEN_PLUS_V1);
        case '/': return makeToken(scanner, TOKEN_SLASH_V1);
        case '*': return makeToken(scanner, TOKEN_STAR_V1);
        case '!':
            return makeToken(scanner, match(scanner, '=') ? TOKEN_BANG_EQUAL_V1 : TOKEN_BANG_V1);
        case '=':
            return makeToken(scanner, match(scanner, '=') ? TOKEN_EQUAL_EQUAL_V1 : TOKEN_EQUAL_V1);
        case '>':
            return makeToken(scanner, match(scanner, '=') ? TOKEN_GREATER_EQUAL_V1 : TOKEN_GREATER_V1);
        case '<':
            return makeToken(scanner, match(scanner, '=') ? TOKEN_LESS_EQUAL_V1 : TOKEN_LESS_V1);
        case '.': 
            return makeToken(scanner, match(scanner, '.') ? TOKEN_DOT_DOT_V1 : TOKEN_DOT_V1);
        case '`':
            return keywordIdentifier(scanner);
        case '"': return string(scanner);
    }
    return errorToken(scanner, "Unexpected character.");
}
