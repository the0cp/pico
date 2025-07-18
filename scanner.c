#include <stdio.h>
#include <string.h>

#include "scanner.h"
#include "common.h"

Scanner sc;

void initScanner(const char* code){
    sc.head = code;
    sc.cur = code;
    sc.line = 1;
}

static inline Token pack(TokenType type, const char* head, int len, int line){
    Token token;
    token.type = type;
    token.head = head;
    token.len = len;
    token.line = line;
    return token;
}

static inline Token error(const char* message, int line){
    fprintf(stderr, "Error at line %d: %s\n", line, message);
    return pack(TOKEN_ERROR, message, (int)strlen(message), line);
}

static inline char* next(){
    if(*sc.cur == '\0') return NULL;
    return sc.cur++;
}

static inline bool is_next(char c){
    if(*sc.cur == '\0') return false;
    if(*sc.cur == c){
        sc.cur++;
        return true;
    }
    return false;
}

Token scan(){
    sc.head = sc.cur;
    if(*sc.cur == '\0') {
        return pack(TOKEN_EOF, sc.head, 0, sc.line);
    }

    char c = next();

    switch(c){
        case '+': return pack(TOKEN_PLUS, sc.head, 1, sc.line);
        case '-': return pack(TOKEN_MINUS, sc.head, 1, sc.line);
        case '*': return pack(TOKEN_STAR, sc.head, 1, sc.line);
        case '/': return pack(TOKEN_SLASH, sc.head, 1, sc.line);
        case '(': return pack(TOKEN_LEFT_PAREN, sc.head, 1, sc.line);
        case ')': return pack(TOKEN_RIGHT_PAREN, sc.head, 1, sc.line);
        case '{': return pack(TOKEN_LEFT_BRACE, sc.head, 1, sc.line);
        case '}': return pack(TOKEN_RIGHT_BRACE, sc.head, 1, sc.line);
        case ',': return pack(TOKEN_COMMA, sc.head, 1, sc.line);
        case ';': return pack(TOKEN_SEMICOLON, sc.head, 1, sc.line);
        case '*':
            return pack(is_next('=') ? TOKEN_EQUAL : TOKEN_STAR, sc.head, 1 + is_next('='), sc.line);
    }
    return error("Unrecognized character", sc.line);
}