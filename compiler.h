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
static void stopCompiler();
static void emitByte(uint8_t byte);
static void emitPair(uint8_t byte1, uint8_t byte2);
static void consume(TokenType type, const char* message);
static void errorAt(Token* token, const char* message);

static void handleNum();
static void emitConstant(Value value);

static void handleGrouping();

static void handleUnary();
static void handleBinary();

static void handleLiteral();

static Chunk* getCurChunk();
static void advance();

static void expression();

#endif // PICO_COMPILER_H