#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "common.h"
#include "vm.h"
#include "compiler.h"

#ifdef DEBUG_TRACE
#include "debug.h"
#endif

VM vm;

void resetStack(){
    vm.stackTop = vm.stack;
}

void initVM(){
    resetStack();
}

void freeVM(){

}

void push(Value value){
    if(vm.stackTop - vm.stack >= STACK_MAX){
        runtimeError("Stack overflow");
        exit(EXIT_FAILURE);
    }
    *vm.stackTop++ = value;
}

Value pop(){
    vm.stackTop--;
    return *vm.stackTop;
}

static Value peek(int distance){
    return vm.stackTop[-1 - distance];
}

InterpreterStatus interpret(const char* code){
    Chunk chunk;
    initChunk(&chunk);

    if(!compile(code, &chunk)){
        freeChunk(&chunk);
        return VM_COMPILE_ERROR;
    }

    if(chunk.count == 0){
        freeChunk(&chunk);
        return VM_OK;  // No code to execute
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpreterStatus status = run();

    freeChunk(&chunk);
    return status;
}

static InterpreterStatus run(){
    static void* dispatchTable[] = {
        [OP_CONSTANT]   = &&DO_OP_CONSTANT,
        [OP_LCONSTANT]  = &&DO_OP_LCONSTANT,
        [OP_ADD]        = &&DO_OP_ADD,
        [OP_SUBTRACT]   = &&DO_OP_SUBTRACT,
        [OP_MULTIPLY]   = &&DO_OP_MULTIPLY,
        [OP_DIVIDE]     = &&DO_OP_DIVIDE,
        [OP_NEGATE]     = &&DO_OP_NEGATE,
        [OP_RETURN]     = &&DO_OP_RETURN,
        [OP_NULL]       = &&DO_OP_NULL,
        [OP_TRUE]       = &&DO_OP_TRUE,
        [OP_FALSE]      = &&DO_OP_FALSE,
    };

    #ifdef DEBUG_TRACE
        #define DISPATCH() \
        do{ \
            printf(">> "); \
            dasmInstruction(vm.chunk, (int)(vm.ip - vm.chunk -> code)); \
            goto *dispatchTable[*vm.ip++]; \
        }while(0)
    #else
        #define DISPATCH() \
        do{ \
            goto *dispatchTable[*vm.ip++]; \
        }while(0)
    #endif

    #define BI_OPERATOR(op) \
    do{ \
        if(vm.stackTop - vm.stack < 2){ \
            runtimeError("Stack underflow"); \
            return VM_RUNTIME_ERROR; \
        } \
        if(!IS_NUM(peek(0)) || !IS_NUM(peek(1))){ \
            runtimeError("Operands must be numbers."); \
            return VM_RUNTIME_ERROR; \
        } \
        double b = AS_NUM(pop()); \
        *(vm.stackTop - 1) = NUM_VAL(AS_NUM(*(vm.stackTop - 1)) op b); \
    }while(0)

    DISPATCH();

    DO_OP_CONSTANT:
    {
        Value constant = vm.chunk -> constants.values[*vm.ip++];
        push(constant);
    } DISPATCH();

    DO_OP_LCONSTANT:
    {
        Value constant = vm.chunk -> constants.values[
            (uint32_t)vm.ip[0] | 
            ((uint32_t)vm.ip[1] << 8) | 
            ((uint32_t)vm.ip[2] << 16)
        ];
        vm.ip += 3; // Move past the index
        push(constant);
    } DISPATCH();

    DO_OP_NULL: push(NULL_VAL()); DISPATCH();

    DO_OP_TRUE: push(BOOL_VAL(true)); DISPATCH();

    DO_OP_FALSE: push(BOOL_VAL(false)); DISPATCH();

    DO_OP_ADD: BI_OPERATOR(+); DISPATCH();

    DO_OP_SUBTRACT: BI_OPERATOR(-); DISPATCH();

    DO_OP_MULTIPLY: BI_OPERATOR(*); DISPATCH();

    DO_OP_DIVIDE:
    {
        if(vm.stackTop - vm.stack < 2){
            runtimeError("Stack underflow");
            return VM_RUNTIME_ERROR; \
        }
        if(!IS_NUM(peek(0)) || !IS_NUM(peek(1))){ 
            runtimeError("Operand must be a number.");
            return VM_RUNTIME_ERROR;
        }

        double b = AS_NUM(pop());
        if(b == 0){
            runtimeError("Runtime error: Division by zero");
            return VM_RUNTIME_ERROR;
        }
        *(vm.stackTop - 1) = NUM_VAL(AS_NUM(*(vm.stackTop - 1)) / b);
    } DISPATCH();

    DO_OP_NEGATE:
    {
        if(!IS_NUM(peek(0))){
            runtimeError("Operand must be a number.");
            return VM_RUNTIME_ERROR;
        }
        if(vm.stackTop - vm.stack < 1){
            runtimeError("Stack underflow");
            return VM_RUNTIME_ERROR;
        }
        *(vm.stackTop - 1) = NUM_VAL(-AS_NUM(*(vm.stackTop - 1)));
    } DISPATCH();

    DO_OP_RETURN:
    {
        printValue(pop());
        printf("\n");
        return VM_OK;
    }
    
    #undef DISPATCH

    /*
    while(true){
        uint8_t instruction;
        switch(instruction = *vm.ip++){
            case OP_RETURN:
                return VM_OK;
        }
    }
    */

}

static void runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    #ifdef DEBUG_TRACE
    size_t instructionOffset = vm.ip - vm.chunk -> code;
    int line = getLine(vm.chunk, instructionOffset);
    fprintf(stderr, "Runtime error at line %d\n", line);
    #endif
    resetStack();
}