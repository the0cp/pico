#ifndef PICO_VM_H
#define PICO_VM_H

#include "hashtable.h"
#include "object.h"

typedef struct Chunk Chunk;
typedef struct Object Object;
typedef struct Compiler Compiler;

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * 256)
#define GLOBAL_STATCK_MAX 64

typedef struct CallFrame{
    // ObjectFunc* func;
    ObjectClosure* closure;
    uint8_t* ip;
    Value* slots;
}CallFrame;

typedef struct VM{
    Value stack[STACK_MAX];  // Stack for values
    Value* stackTop;
    HashTable strings;
    HashTable globals;
    HashTable* curGlobal;
    HashTable* globalStack[GLOBAL_STATCK_MAX];
    int globalCnt;
    HashTable modules;
    Object* objects;
    ObjectString* initString;
    ObjectUpvalue* openUpvalues;    // descending locations
    CallFrame frames[FRAMES_MAX];
    int frameCount;
    size_t bytesAllocated;
    size_t nextGC;
    Compiler* compiler;
    uint64_t hash_seed;
}VM;

typedef enum{
    VM_OK,
    VM_COMPILE_ERROR,
    VM_RUNTIME_ERROR
}InterpreterStatus;

void initVM(VM* vm, int argc, const char* argv[]);
void freeVM(VM* vm);
void resetStack(VM* vm);
void push(VM* vm, Value value);
Value pop(VM* vm);
Value peek(VM* vm, int distance);

static bool isTruthy(Value value);
static bool checkAccess(VM* vm, ObjectClass* instanceKlass, ObjectString* fieldName);

InterpreterStatus interpret(VM* vm, const char* code, const char* srcName);
static InterpreterStatus run(VM* vm);

static bool call(VM* vm, ObjectClosure* closure, int argCnt);
static bool callValue(VM* vm, Value callee, int argCnt);

void runtimeError(VM* vm, const char* format, ...);

#endif // PICO_VM_H