#ifndef PICO_COMPILER_H
#define PICO_COMPILER_H

#include "vm.h"
#include "scanner.h"
#include "common.h"

typedef struct{
    Token pre;  // Previous token
    Token cur;  // Current token
    bool hadError;  // Flag to indicate if there was a compilation error
    bool panic; // Flag to indicate if we are in panic mode
}Parser;

bool compile(const char* code, Chunk* chunk);
static void error(const char* message);

static void advance();

#endif // PICO_COMPILER_H