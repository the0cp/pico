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

void writeChunk(VM* vm, Chunk* chunk, uint8_t byte, int line){
    if(chunk->count + 1 > chunk->capacity){
        size_t oldCapacity = chunk->capacity;
        chunk->capacity = oldCapacity < 8 ? 8 : oldCapacity * 2;
        chunk->code = (uint8_t*)reallocate(
            vm,
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
            chunk->lines = (int*)reallocate(
                vm,
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

void freeChunk(VM* vm, Chunk* chunk){
    reallocate(vm, chunk->code, sizeof(uint8_t) * chunk->capacity, 0);
    freeValueArray(vm, &chunk->constants);
    reallocate(vm, chunk->lines, sizeof(int) * chunk->lineCapacity * 2, 0);
    initChunk(chunk);
}

int addConstant(VM* vm, Chunk* chunk, Value value){
    writeValueArray(vm, &chunk->constants, value);
    return (int)(chunk->constants.count - 1);
}

void writeConstant(VM* vm, Chunk* chunk, Value value, int line){
    int constantIndex = addConstant(vm, chunk, value);
    if(constantIndex < 256){
        writeChunk(vm, chunk, OP_CONSTANT, line);
        writeChunk(vm, chunk, (uint8_t)constantIndex, line);
    }else{
        writeChunk(vm, chunk, OP_LCONSTANT, line);
        writeChunk(vm, chunk, (uint8_t)((constantIndex >> 8) & 0xff), line);
        writeChunk(vm, chunk, (uint8_t)(constantIndex & 0xff), line);
    }
}