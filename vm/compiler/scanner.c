#include <stdio.h>
#include <string.h>

#include "../util/common.h"
#include "scanner.h"

void initScanner(Scanner* scanner, const char* src) {
    scanner->start = src;
    scanner->current = src;
    scanner->line = 1;
}

static bool isAtEnd(Scanner* scanner) {
    return *scanner->current == '\0';
}

static Token makeToken(Scanner* scanner, TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner->start;
    token.length = (int) (scanner->current - scanner->start);
    token.line = scanner->line;
    return token;
}

static Token errorToken(Scanner* scanner, const char* msg) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = msg;
    token.length = (int) strlen(msg);
    token.line = scanner->line;
    return token;
}

static char advance(Scanner* scanner) {
    scanner->current++;
    return scanner->current[-1];
}

static bool match(Scanner* scanner, char c) {
    if (isAtEnd(scanner)) return false;
    if (*scanner->current != c) return false;
    scanner->current++;
    return true;
}

static char peek(Scanner* scanner) {
    return *scanner->current;
}

static char peekNext(Scanner* scanner) {
    if (isAtEnd(scanner)) return '\0';
    return scanner->current[1];
}

static void skipWhitespace(Scanner* scanner) {
    for (;;) {
        char c = peek(scanner);
        switch (c) {
            case '\n':
                scanner->line++;
            case ' ':
            case '\r':
            case '\t':
                advance(scanner);
                break;
            case '/':
                if (peekNext(scanner) != '/') 
                    return;
                while (peek(scanner) != '\n' && !isAtEnd(scanner)) 
                    advance(scanner);
                break;
            default:
                return;
        }
    }
}

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        c == '_';
}

static Token stringToken(Scanner* scanner) {
    while (peek(scanner) != '"' && !isAtEnd(scanner)) {
        if (peek(scanner) == '\n') scanner->line++;
        if (peek(scanner) == '\\') advance(scanner);
        advance(scanner);
    }

    if (isAtEnd(scanner)) return errorToken(scanner, "Unterminated string.");

    advance(scanner);
    return makeToken(scanner, TOKEN_STRING);
}

static Token numberToken(Scanner* scanner) {
    while (isDigit(peek(scanner))) 
        advance(scanner);

    if (peek(scanner) == '.' && isDigit(peekNext(scanner))) {
        advance(scanner);
        while (isDigit(peek(scanner))) 
            advance(scanner);
    }

    return makeToken(scanner, TOKEN_NUMBER);
}

static TokenType checkKeyword(Scanner* scanner, int start, int length, 
        const char* rest, TokenType type) {
    if (scanner->current - scanner->start == start + length && 
        memcmp(scanner->start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

static TokenType identifierTokenType(Scanner* scanner) {
    switch (scanner->start[0]) {
        case 'd': return checkKeyword(scanner, 1, 2, "ef", TOKEN_DEF);
        case 'e': return checkKeyword(scanner, 1, 3, "lse", TOKEN_ELSE);
        case 'l': return checkKeyword(scanner, 1, 2, "et", TOKEN_LET);
        case 'r': return checkKeyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
        case 's': return checkKeyword(scanner, 1, 4, "uper", TOKEN_SUPER);
        case 'u': return checkKeyword(scanner, 1, 5, "npack", TOKEN_UNPACK);
        case 'v': return checkKeyword(scanner, 1, 2, "ar", TOKEN_VAR);
        case 'w': return checkKeyword(scanner, 1, 4, "hile", TOKEN_WHILE);

        case 'b':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'r': return checkKeyword(scanner, 2, 3, "eak", TOKEN_BREAK);
                    case 'u': return checkKeyword(scanner, 2, 3, "ild", TOKEN_BUILD);
                }
            }
        case 'c':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'l': return checkKeyword(scanner, 2, 3, "ass", TOKEN_CLASS);
                    case 'o': 
                        if (checkKeyword(scanner, 2, 3, "nst", TOKEN_CONST) == TOKEN_IDENTIFIER)
                            return checkKeyword(scanner, 2, 6, "ntinue", TOKEN_CONTINUE);
                        return TOKEN_CONST;
                }
            }
            break;
        case 'f':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a': return checkKeyword(scanner, 2, 3, "lse", TOKEN_FALSE);
                    case 'o': return checkKeyword(scanner, 2, 1, "r", TOKEN_FOR);
                    case 'u': return checkKeyword(scanner, 2, 2, "nc", TOKEN_FN);
                    case 'r': return checkKeyword(scanner, 2, 2, "om", TOKEN_FROM);
                }
            }
            break;
        case 'i':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'f': return scanner->current - scanner->start == 2 ? TOKEN_IF : TOKEN_IDENTIFIER;
                    case 'm': return checkKeyword(scanner, 2, 4, "port", TOKEN_IMPORT);
                }
            }
            break;
        case 'n':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'e': return checkKeyword(scanner, 2, 1, "w", TOKEN_NEW);
                    case 'u': return checkKeyword(scanner, 2, 2, "ll", TOKEN_NULL);
                }
            }
        case 't':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'h': return checkKeyword(scanner, 2, 2, "is", TOKEN_THIS);
                    case 'r': return checkKeyword(scanner, 2, 2, "ue", TOKEN_TRUE);
                }
            }
    }
    return TOKEN_IDENTIFIER;
}

static Token identifierToken(Scanner* scanner) {
    while (isAlpha(peek(scanner)) || isDigit(peek(scanner)))
        advance(scanner);
    return makeToken(scanner, identifierTokenType(scanner));
}

Token scanToken(Scanner* scanner) {
    skipWhitespace(scanner);

    scanner->start = scanner->current;

    if (isAtEnd(scanner)) 
        return makeToken(scanner, TOKEN_EOF);

    char c = advance(scanner);
    if (isAlpha(c))
        return identifierToken(scanner);
    if (isDigit(c)) 
        return numberToken(scanner);
    
    switch (c) {
        case '(': return makeToken(scanner, TOKEN_LEFT_PAREN);
        case ')': return makeToken(scanner, TOKEN_RIGHT_PAREN);
        case '{': return makeToken(scanner, TOKEN_LEFT_BRACE);
        case '}': return makeToken(scanner, TOKEN_RIGHT_BRACE);
        case '[': return makeToken(scanner, TOKEN_LEFT_BRACKET);
        case ']': return makeToken(scanner, TOKEN_RIGHT_BRACKET);
        case ';': return makeToken(scanner, TOKEN_SEMICOLON);
        case ',': return makeToken(scanner, TOKEN_COMMA);
        case '.': return makeToken(scanner, TOKEN_DOT);
        case '+': 
            return makeToken(scanner, 
                match(scanner, '=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS);
        case '/': 
            return makeToken(scanner, 
                match(scanner, '=') ? TOKEN_SLASH_EQUAL : TOKEN_SLASH);
        case '*': 
            return makeToken(scanner, 
                match(scanner, '=') ? TOKEN_STAR_EQUAL : TOKEN_STAR);

        case '-': 
            return makeToken(scanner, 
                match(scanner, '>') ? TOKEN_RIGHT_ARROW : match(scanner, '=') ? TOKEN_MINUS_EQUAL : TOKEN_MINUS);
        case '!':
            return makeToken(scanner, 
                match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return makeToken(scanner, 
                match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<':
            return makeToken(scanner, 
                match(scanner, '=') ? TOKEN_LESS_EQUAL : 
                (match(scanner, '-') ? TOKEN_LEFT_ARROW : TOKEN_LESS));
        case '>':
            return makeToken(scanner, 
                match(scanner, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '&':
            return makeToken(scanner, 
                match(scanner, '&') ? TOKEN_AND : TOKEN_BINARY_AND);
        case '|':
            return makeToken(scanner, 
                match(scanner, '|') ? TOKEN_OR : TOKEN_BINARY_OR);
        
        case '"':
            return stringToken(scanner);
    }

    return errorToken(scanner, "Unexpected character.");
}
