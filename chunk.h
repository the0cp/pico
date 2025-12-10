#ifndef PICO_CHUNK_H
#define PICO_CHUNK_H

#include "common.h"
#include "value.h"

typedef struct VM VM;

typedef enum {
    OP_CONSTANT, OP_LCONSTANT,
    OP_NULL,
    OP_TRUE, OP_FALSE,
    OP_TO_STRING,
    OP_NOT, OP_NOT_EQUAL, OP_EQUAL, 
    OP_GREATER, OP_LESS, OP_GREATER_EQUAL, OP_LESS_EQUAL,
    OP_ADD, OP_SUBTRACT, OP_MULTIPLY, OP_DIVIDE, OP_NEGATE,
    OP_POP, OP_DUP,
    OP_RETURN,
    OP_PRINT,
    OP_DEFINE_GLOBAL, OP_DEFINE_LGLOBAL, 
    OP_GET_GLOBAL, OP_GET_LGLOBAL, 
    OP_SET_GLOBAL, OP_SET_LGLOBAL,
    OP_GET_LOCAL, OP_GET_LLOCAL,
    OP_SET_LOCAL, OP_SET_LLOCAL,
    OP_JUMP, OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_IMPORT, OP_LIMPORT,
    OP_GET_PROPERTY, OP_GET_LPROPERTY, 
    OP_SET_PROPERTY, OP_SET_LPROPERTY,
    OP_CLOSURE, OP_LCLOSURE, OP_CLOSE_UPVALUE,
    OP_GET_UPVALUE, OP_SET_UPVALUE,
    OP_GET_LUPVALUE, OP_SET_LUPVALUE,
    OP_CLASS, OP_METHOD, OP_LCLASS, OP_LMETHOD, 
    OP_DEFINE_FIELD, OP_DEFINE_LFIELD,
    OP_BUILD_LIST, OP_FILL_LIST, OP_INDEX_GET, OP_INDEX_SET,
    OP_SLICE,
    OP_BUILD_MAP,
    OP_SYSTEM,
} OpCode;

typedef struct Chunk {
    uint8_t* code;
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
void writeConstant(VM* vm, Chunk* chunk, Value value, int line);

#endif  // PICO_CHUNK_H