
#ifndef jp_scanner_h
#define jp_scanner_h

typedef enum {
    // Single-character tokens.
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET,
    TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
    TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,
    // One or two character tokens.
    TOKEN_BANG, TOKEN_BANG_EQUAL,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL,
    TOKEN_AND, TOKEN_OR,
    TOKEN_BINARY_AND, TOKEN_BINARY_OR,
    TOKEN_LEFT_ARROW, TOKEN_RIGHT_ARROW,
    // Op equals 
    TOKEN_PLUS_EQUAL, TOKEN_MINUS_EQUAL,
    TOKEN_STAR_EQUAL, TOKEN_SLASH_EQUAL,
    // Literals.
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
    // Keywords.
    TOKEN_BREAK, TOKEN_BUILD, TOKEN_CLASS, TOKEN_CONST, 
    TOKEN_CONTINUE, TOKEN_DEF, TOKEN_ELSE, TOKEN_FALSE,
    TOKEN_FN, TOKEN_FOR, TOKEN_FROM, TOKEN_IF, TOKEN_IMPORT,
    TOKEN_LET, TOKEN_PRV, TOKEN_PUB, TOKEN_NEW, TOKEN_NULL, 
    TOKEN_RETURN, TOKEN_SUPER, TOKEN_STATIC, TOKEN_THIS, 
    TOKEN_TRUE, TOKEN_UNPACK, TOKEN_VAR, TOKEN_WHILE,

    TOKEN_ERROR, TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

typedef struct {
    const char* start;
    const char* current;
    int line;
} Scanner;

void initScanner(Scanner* scanner, const char* src);
Token scanToken(Scanner* scanner);

#endif
