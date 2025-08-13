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

typedef struct Compiler{
    Parser parser;
    Chunk* chunk;
    VM* vm;
}Compiler;

bool compile(VM* vm, const char* code, Chunk* chunk);
static void stopCompiler(Compiler* compiler);
static void emitByte(Compiler* compiler, uint8_t byte);
static void emitPair(Compiler* compiler, uint8_t byte1, uint8_t byte2);
static void consume(Compiler* compiler, TokenType type, const char* errMsg);
static void errorAt(Compiler* compiler, Token* token, const char* message);

static void handleNum(Compiler* compiler);
static void emitConstant(Compiler* compiler, Value value);

static void handleGrouping(Compiler* compiler);

static void handleUnary(Compiler* compiler);
static void handleBinary(Compiler* compiler);

static void handleLiteral(Compiler* compiler);

static void handleString(Compiler* compiler);

static void advance(Compiler* compiler);

static void expression(Compiler* compiler);

#endif // PICO_COMPILER_H