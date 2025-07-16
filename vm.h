#ifndef PICO_VM_H
#define PICO_VM_H

#include "chunk.h"

#define STACK_MAX 256

typedef struct{
    Chunk* chunk;
    uint8_t* ip;  // Instruction pointer
    Value stack[STACK_MAX];  // Stack for values
    Value* stackTop;
}VM;

typedef enum{
    VM_OK,
    VM_COMPILE_ERROR,
    VM_RUNTIME_ERROR
}InterpreterStatus;

void initVM();
void freeVM();
void resetStack();
void push(Value value);
Value pop();

InterpreterStatus interpret(Chunk* chunk);
static InterpreterStatus run();

#endif // PICO_VM_H