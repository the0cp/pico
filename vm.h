#ifndef PICO_VM_H
#define PICO_VM_H

#include "hashtable.h"

typedef struct Chunk Chunk;
typedef struct Object Object;

#define STACK_MAX 256

typedef struct VM{
    Chunk* chunk;
    uint8_t* ip;  // Instruction pointer
    Value stack[STACK_MAX];  // Stack for values
    Value* stackTop;
    HashTable strings;
    HashTable globals;
    Object* objects;
}VM;

typedef enum{
    VM_OK,
    VM_COMPILE_ERROR,
    VM_RUNTIME_ERROR
}InterpreterStatus;

void initVM(VM* vm);
void freeVM(VM* vm);
void resetStack(VM* vm);
void push(VM* vm, Value value);
Value pop(VM* vm);
static Value peek(VM* vm, int distance);

static bool isTruthy(Value value);

InterpreterStatus interpret(VM* vm, const char* code);
static InterpreterStatus run(VM* vm);

static void runtimeError(VM* vm, const char* format, ...);

#endif // PICO_VM_H