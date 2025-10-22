#ifndef PICO_VM_H
#define PICO_VM_H

#include "hashtable.h"
#include "object.h"

typedef struct Chunk Chunk;
typedef struct Object Object;

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * 256)

typedef struct CallFrame{
    ObjectFunc* func;
    uint8_t* ip;
    Value* slots;
}CallFrame;

typedef struct VM{
    // Chunk* chunk;
    // uint8_t* ip;  // Instruction pointer
    Value stack[STACK_MAX];  // Stack for values
    Value* stackTop;
    HashTable strings;
    HashTable globals;
    Object* objects;
    CallFrame frames[FRAMES_MAX];
    int frameCount;
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

InterpreterStatus interpret(VM* vm, const char* code, const char* srcName);
static InterpreterStatus run(VM* vm);

static bool call(VM* vm, ObjectFunc* func, int argCnt);
static bool callValue(VM* vm, Value callee, int argCnt);

static void runtimeError(VM* vm, const char* format, ...);

#endif // PICO_VM_H