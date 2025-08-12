#include <stdlib.h>

#include "chunk.h"
#include "mem.h"

void initChunk(Chunk* chunk){
    chunk->code = NULL;
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->lines = NULL;
    chunk->lineCount = 0;
    chunk->lineCapacity = 0;
    initValueArray(&chunk->constants);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line){
    if(chunk->count + 1 > chunk->capacity){
        size_t oldCapacity = chunk->capacity;
        chunk->capacity = oldCapacity < 8 ? 8 : oldCapacity * 2;
        chunk->code = (uint8_t*)resize( 
            chunk->code, 
            sizeof(uint8_t) * oldCapacity, 
            sizeof(uint8_t) * chunk->capacity
        );
    }
    chunk->code[chunk->count++] = byte;
    if(chunk->lineCount == 0 || chunk->lines[chunk->lineCount * 2 - 1] != line){
        if(chunk->lineCount + 1 > chunk->lineCapacity){
            int oldLineCapacity = chunk->lineCapacity;
            chunk->lineCapacity = oldLineCapacity < 8 ? 8 : oldLineCapacity * 2;
            chunk->lines = (int*)resize(
                chunk->lines,
                sizeof(int) * oldLineCapacity * 2,  
                sizeof(int) * chunk->lineCapacity * 2
                // Each (offset, line) pair takes 2 ints
            );
        }
        chunk->lines[chunk->lineCount * 2] = chunk->count - 1;
        chunk->lines[chunk->lineCount * 2 + 1] = line;
        chunk->lineCount++; 
    }
}

void freeChunk(Chunk* chunk){
    resize(chunk->code, sizeof(uint8_t) * chunk->capacity, 0);
    freeValueArray(&chunk->constants);
    resize(chunk->lines, sizeof(int) * chunk->lineCapacity * 2, 0);
    initChunk(chunk);
}

int addConstant(Chunk* chunk, Value value){
    writeValueArray(&chunk->constants, value);
    return (int)(chunk->constants.count - 1);
}

void writeConstant(Chunk* chunk, Value value, int line){
    int constantIndex = addConstant(chunk, value);
    if(constantIndex < 256){
        writeChunk(chunk, OP_CONSTANT, line);
        writeChunk(chunk, (uint8_t)constantIndex, line);
    }else{
        writeChunk(chunk, OP_LCONSTANT, line);
        writeChunk(chunk, (uint8_t)(constantIndex & 0xff), line);
        writeChunk(chunk, (uint8_t)((constantIndex >> 8) & 0xff), line);
        writeChunk(chunk, (uint8_t)((constantIndex >> 16) & 0xff), line);
    }
}