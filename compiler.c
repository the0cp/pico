#include <stdio.h>

#include "compiler.h"

Parser parser;

static void advance(){
    parser.pre == parser.cur;
    while(true){
        parser.cur = scan();
        if(parser.cur.type != TOKEN_ERROR){
            break;
        }
        error("Unexpected token: %.*s", parser.cur.head);
    }
}

bool compile(const char* code, Chunk* chunk){
    initScanner(code);
    return !parser.hadError;
}