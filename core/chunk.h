#ifndef PICO_CHUNK_H
#define PICO_CHUNK_H

#include "common.h"
#include "value.h"
#include "instruction.h"

typedef struct Chunk {
    Instruction* code;
    size_t count;
    size_t capacity;
    ValueArray constants;
    int* lines;
    int lineCount;
    int lineCapacity;
} Chunk;

void initChunk(Chunk* chunk);
void writeChunk(VM* vm, Chunk* chunk, uint8_t byte, int line);
void freeChunk(VM* vm, Chunk* chunk);

int addConstant(VM* vm, Chunk* chunk, Value value);

#endif  // PICO_CHUNK_H