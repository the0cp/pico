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

bool compile(VM* vm, const char* code, Chunk* chunk);
static void stopCompiler(VM* vm);
static void emitByte(uint8_t byte);
static void emitPair(uint8_t byte1, uint8_t byte2);
static void consume(TokenType type, const char* errMsg);
static void errorAt(Token* token, const char* message);

static void handleNum(VM* vm);
static void emitConstant(VM* vm, Value value);

static void handleGrouping(VM* vm);

static void handleUnary(VM* vm);
static void handleBinary(VM* vm);

static void handleLiteral(VM* vm);

static void handleString(VM* vm);

static Chunk* getCurChunk();
static void advance();

static void expression(VM* vm);

#endif // PICO_COMPILER_H