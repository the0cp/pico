#ifndef PICO_SCANNER_H
#define PICO_SCANNER_H

typedef enum{
    TOKEN_EOF, TOKEN_ERROR,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_PLUS, TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_EQUAL,
    TOKEN_NOT_EQUAL,
    TOKEN_LESS, TOKEN_GREATER,
    TOKEN_LESS_EQUAL, TOKEN_GREATER_EQUAL,
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_COMMA, TOKEN_SEMICOLON,
    TOKEN_IF, TOKEN_ELSE,
    TOKEN_WHILE, TOKEN_FOR,
    TOKEN_RETURN,
}TokenType;

typedef struct{
    const char* head;
    const char* cur;
    int line;
}Scanner;

typedef struct{
    TokenType type;
    const char* head;
    int len;
    int line;
}Token;

void initScanner(const char* code);
Token scan();
static inline char* next();
static inline bool is_next(char c);
static inline Token pack(TokenType type, const char* head, int len, int line);
static inline Token error(const char* message, int line);

#endif // PICO_SCANNER_H