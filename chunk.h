#ifndef PICO_CHUNK_H
#define PICO_CHUNK_H

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT, OP_LCONSTANT,
    OP_NULL,
    OP_TRUE, OP_FALSE,
    OP_NOT, OP_NOT_EQUAL, OP_EQUAL, OP_GREATER, OP_LESS, OP_GREATER_EQUAL, OP_LESS_EQUAL,
    OP_ADD, OP_SUBTRACT, OP_MULTIPLY, OP_DIVIDE, OP_NEGATE,
    OP_RETURN
} OpCode;

typedef struct {
    uint8_t* code;
    size_t count;
    size_t capacity;
    ValueArray constants;
    int* lines;
    int lineCount;
    int lineCapacity;
} Chunk;

void initChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
void freeChunk(Chunk* chunk);

int addConstant(Chunk* chunk, Value value);
void writeConstant(Chunk* chunk, Value value, int line);

#endif  // PICO_CHUNK_H