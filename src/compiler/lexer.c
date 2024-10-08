#include <stdio.h>
#include <string.h>

#include "lexer.h"

void initLexer(Lexer* lexer, const char* source, bool debugToken) {
    lexer->start = source;
    lexer->current = source;
    lexer->line = 1;
    lexer->interpolationDepth = 0;
    lexer->debugToken = debugToken;
}

static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static bool isAtEnd(Lexer* lexer) {
    return *lexer->current == '\0';
}

static char advance(Lexer* lexer) {
    lexer->current++;
    return lexer->current[-1];
}

static char peek(Lexer* lexer) {
    return *lexer->current;
}

static char peekNext(Lexer* lexer) {
    if (isAtEnd(lexer)) return '\0';
    return lexer->current[1];
}

static char peekPrevious(Lexer* lexer) {
    return lexer->current[-1];
}

static bool match(Lexer* lexer, char expected) {
    if (isAtEnd(lexer)) return false;
    if (*lexer->current != expected) return false;
    lexer->current++;
    return true;
}

static Token makeToken(Lexer* lexer, TokenSymbol type) {
    Token token = {
        .type = type,
        .start = lexer->start,
        .length = (int)(lexer->current - lexer->start),
        .line = lexer->line
    };

    if (lexer->debugToken) outputToken(token);
    return token;
}

static Token errorToken(Lexer* lexer, const char* message) {
    Token token = {
        .type = TOKEN_ERROR,
        .start = message,
        .length = (int)strlen(message),
        .line = lexer->line
    };
    return token;
}

static void skipLineComment(Lexer* lexer) {
    while (peek(lexer) != '\n' && !isAtEnd(lexer)) {
        advance(lexer);
    }
}

static void skipBlockComment(Lexer* lexer) {
    int nesting = 1;
    while (nesting > 0) {
        if (isAtEnd(lexer)) return;

        if (peek(lexer) == '/' && peekNext(lexer) == '*') {
            advance(lexer);
            advance(lexer);
            nesting++;
            continue;
        }

        if (peek(lexer) == '*' && peekNext(lexer) == '/') {
            advance(lexer);
            advance(lexer);
            nesting--;
            continue;
        }

        if (peek(lexer) == '\n') lexer->line++;
        advance(lexer);
    }
}

static void skipWhitespace(Lexer* lexer) {
    for (;;) {
        char c = peek(lexer);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(lexer);
                break;
            case '\n':
                lexer->line++;
                advance(lexer);
                break;
            case '/':
                if (peekNext(lexer) == '/') {
                    skipLineComment(lexer);
                }
                else if (peekNext(lexer) == '*') {
                    advance(lexer);
                    skipBlockComment(lexer);
                }
                else return;
                break;
            default:
                return;
        }
    }
}

static TokenSymbol checkKeyword(Lexer* lexer, int start, int length, const char* rest, TokenSymbol type) {
    if (lexer->current - lexer->start == start + length &&
        memcmp(lexer->start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

static TokenSymbol identifierType(Lexer* lexer) {
    if (lexer->start[-1] == '.') return TOKEN_IDENTIFIER;

    switch (lexer->start[0]) {
        case 'a':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'n': return checkKeyword(lexer, 2, 1, "d", TOKEN_AND);
                    case 's': {
                        if (lexer->current - lexer->start > 2) return checkKeyword(lexer, 2, 3, "ync", TOKEN_ASYNC);
                        else return checkKeyword(lexer, 2, 0, "", TOKEN_AS);
                    }
                    case 'w': return checkKeyword(lexer, 2, 3, "ait", TOKEN_AWAIT);
                }
            }
        case 'b': return checkKeyword(lexer, 1, 4, "reak", TOKEN_BREAK);
        case 'c':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'a':
                        if (lexer->current - lexer->start > 2) {
                            switch (lexer->start[2]) {
                                case 's': return checkKeyword(lexer, 3, 1, "e", TOKEN_CASE);
                                case 't': return checkKeyword(lexer, 3, 2, "ch", TOKEN_CATCH);
                            }
                        }
                    case 'l': return checkKeyword(lexer, 2, 3, "ass", TOKEN_CLASS);
                    case 'o': return checkKeyword(lexer, 2, 6, "ntinue", TOKEN_CONTINUE);
                }
            }
        case 'd': return checkKeyword(lexer, 1, 6, "efault", TOKEN_DEFAULT);
        case 'e': 
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'l': return checkKeyword(lexer, 2, 2, "se", TOKEN_ELSE);
                    case 'x': return checkKeyword(lexer, 2, 5, "tends", TOKEN_EXTENDS);
                }
            }
        case 'f':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'a': return checkKeyword(lexer, 2, 3, "lse", TOKEN_FALSE);
                    case 'i': return checkKeyword(lexer, 2, 5, "nally", TOKEN_FINALLY);
                    case 'o': return checkKeyword(lexer, 2, 1, "r", TOKEN_FOR);
                    case 'u': return checkKeyword(lexer, 2, 1, "n", TOKEN_FUN);
                }
            }
        case 'i': return checkKeyword(lexer, 1, 1, "f", TOKEN_IF);
        case 'n':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'a': return checkKeyword(lexer, 2, 7, "mespace", TOKEN_NAMESPACE);
                    case 'i': return checkKeyword(lexer, 2, 1, "l", TOKEN_NIL);
                }
            }
        case 'o': return checkKeyword(lexer, 1, 1, "r", TOKEN_OR);
        case 'r':
            if (lexer->current - lexer->start > 2 && lexer->start[1] == 'e') {
                switch (lexer->start[2]) {
                    case 'q': return checkKeyword(lexer, 3, 4, "uire", TOKEN_REQUIRE);
                    case 't': return checkKeyword(lexer, 3, 3, "urn", TOKEN_RETURN);
                }
            }
        case 's':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'u': return checkKeyword(lexer, 2, 3, "per", TOKEN_SUPER);
                    case 'w': return checkKeyword(lexer, 2, 4, "itch", TOKEN_SWITCH);
                }
            }
        case 't':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'h':
                        if (lexer->current - lexer->start > 2) {
                            switch (lexer->start[2]) {
                                case 'i': return checkKeyword(lexer, 3, 1, "s", TOKEN_THIS);
                                case 'r': return checkKeyword(lexer, 3, 2, "ow", TOKEN_THROW);
                            }
                        }
                   case 'r':
                        if (lexer->current - lexer->start > 2) {
                            switch (lexer->start[2]) {
                                case 'a': return checkKeyword(lexer, 3, 2, "it", TOKEN_TRAIT);
                                case 'u': return checkKeyword(lexer, 3, 1, "e", TOKEN_TRUE);
                                case 'y': return checkKeyword(lexer, 3, 0, "", TOKEN_TRY);
                            }
                        }
                }
            }
            break;
        case 'u': return checkKeyword(lexer, 1, 4, "sing", TOKEN_USING);
        case 'v':
            if (lexer->current - lexer->start > 2 && lexer->start[1] == 'a') {
                switch (lexer->start[2]) {
                    case 'l': return checkKeyword(lexer, 3, 0, "", TOKEN_VAL);
                    case 'r': return checkKeyword(lexer, 3, 0, "", TOKEN_VAR);
                }
            }
            break;
        case 'w':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'h': return checkKeyword(lexer, 2, 3, "ile", TOKEN_WHILE);
                    case 'i': return checkKeyword(lexer, 2, 2, "th", TOKEN_WITH);
                }
            }
            break;
        case 'y': return checkKeyword(lexer, 1, 4, "ield", TOKEN_YIELD);
    }
    return TOKEN_IDENTIFIER;
}

static Token identifier(Lexer* lexer) {
    while (isAlpha(peek(lexer)) || isDigit(peek(lexer))) advance(lexer);
    return makeToken(lexer, identifierType(lexer));
}

static Token keywordIdentifier(Lexer* lexer) {
    advance(lexer);
    while (isAlpha(peek(lexer)) || isDigit(peek(lexer))) advance(lexer);
    if (peek(lexer) == '`') {
        advance(lexer);
        return makeToken(lexer, TOKEN_IDENTIFIER);
    }
    else return errorToken(lexer, "Keyword identifiers must end with a closing backtick.");
}

static Token number(Lexer* lexer) {
    while (isDigit(peek(lexer))) advance(lexer);

    if (peek(lexer) == '.' && isDigit(peekNext(lexer))) {
        advance(lexer);
        while (isDigit(peek(lexer))) advance(lexer);
        return makeToken(lexer, TOKEN_NUMBER);
    }
    return makeToken(lexer, TOKEN_INT);
}

static Token string(Lexer* lexer) {
    while ((peek(lexer) != '"' || peekPrevious(lexer) == '\\') && !isAtEnd(lexer)) {
        if (peek(lexer) == '\n') lexer->line++;
        else if (peek(lexer) == '$' && peekNext(lexer) == '{') {
            if (lexer->interpolationDepth >= UINT4_MAX) {
                return errorToken(lexer, "Interpolation may only nest 15 levels deep.");
            }
            lexer->interpolationDepth++;
            advance(lexer);

            Token token = makeToken(lexer, TOKEN_INTERPOLATION);
            advance(lexer);
            return token;
        }
        advance(lexer);
    }

    if (isAtEnd(lexer)) return errorToken(lexer, "Unterminated string.");
    advance(lexer);
    return makeToken(lexer, TOKEN_STRING);
}

Token scanToken(Lexer* lexer) {
    skipWhitespace(lexer);
    lexer->start = lexer->current;

    if (isAtEnd(lexer)) return makeToken(lexer, TOKEN_EOF);

    char c = advance(lexer);
    if (isAlpha(c)) return identifier(lexer);
    if (isDigit(c)) return number(lexer);

    switch (c) {
        case '(': return makeToken(lexer, TOKEN_LEFT_PAREN);
        case ')': return makeToken(lexer, TOKEN_RIGHT_PAREN);
        case '[': return makeToken(lexer, TOKEN_LEFT_BRACKET);
        case ']': return makeToken(lexer, TOKEN_RIGHT_BRACKET);
        case '{': return makeToken(lexer, TOKEN_LEFT_BRACE);
        case '}':
            if (lexer->interpolationDepth > 0) {
                lexer->interpolationDepth--;
                return string(lexer);
            }
            return makeToken(lexer, TOKEN_RIGHT_BRACE);
        case ';': return makeToken(lexer, TOKEN_SEMICOLON);
        case ':': return makeToken(lexer, TOKEN_COLON);
        case ',': return makeToken(lexer, TOKEN_COMMA);
        case '?': return makeToken(lexer, TOKEN_QUESTION);
        case '-': return makeToken(lexer, TOKEN_MINUS);
        case '%': return makeToken(lexer, TOKEN_MODULO);
        case '|': return makeToken(lexer, TOKEN_PIPE);
        case '+': return makeToken(lexer, TOKEN_PLUS);
        case '/': return makeToken(lexer, TOKEN_SLASH);
        case '*': return makeToken(lexer, TOKEN_STAR);
        case '!': 
            return makeToken(lexer, match(lexer, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=': 
            return makeToken(lexer, match(lexer, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '>':
            return makeToken(lexer, match(lexer, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '<':
            return makeToken(lexer, match(lexer, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '.':
            return makeToken(lexer, match(lexer, '.') ? TOKEN_DOT_DOT : TOKEN_DOT);
        case '`': 
            return keywordIdentifier(lexer);
        case '"':
            return string(lexer);
    }
    return errorToken(lexer, "Unexpected character.");
}
