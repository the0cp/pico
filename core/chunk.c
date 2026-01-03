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

void writeChunk(VM* vm, Chunk* chunk, Instruction instruction, int line){
    if(chunk->count + 1 > chunk->capacity){
        size_t oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(
            vm,
            Instruction,
            chunk->code,
            oldCapacity,
            chunk->capacity
        );
        chunk->lines = GROW_ARRAY(
            vm,
            int,
            chunk->lines,
            oldCapacity * 2,
            chunk->capacity * 2
        );
    }
    chunk->code[chunk->count] = instruction;
    chunk->lines[chunk->count] = line;
    chunk->count++;
    
}

void freeChunk(VM* vm, Chunk* chunk){
    FREE_ARRAY(vm, Instruction, chunk->code, chunk->capacity);
    FREE_ARRAY(vm, int, chunk->lines, chunk->lineCapacity * 2);
    freeValueArray(vm, &chunk->constants);
    initChunk(chunk);
}

int addConstant(VM* vm, Chunk* chunk, Value value){
    writeValueArray(vm, &chunk->constants, value);
    return (int)(chunk->constants.count - 1);
}