#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "vm.h"

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
        fprintf(stderr, "Stack overflow\n");
        exit(EXIT_FAILURE);
    }
    *vm.stackTop++ = value;
}

Value pop(){
    vm.stackTop--;
    return *vm.stackTop;
}

InterpreterStatus interpret(const char* code){
    Chunk chunk;
    initChunk(&chunk);

    if(!compile(code, &chunk)){
        freeChunk(&chunk);
        return VM_COMPILE_ERROR;
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
            double b = pop(); \
            *(vm.stackTop - 1) = *(vm.stackTop - 1) op b; \
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

    DO_OP_ADD: BI_OPERATOR(+); DISPATCH();

    DO_OP_SUBTRACT: BI_OPERATOR(-); DISPATCH();

    DO_OP_MULTIPLY: BI_OPERATOR(*); DISPATCH();

    DO_OP_DIVIDE:
    {
        double b = pop();
        if(b == 0){
            fprintf(stderr, "Runtime error: Division by zero\n");
            return VM_RUNTIME_ERROR;
        }
        double a = pop();
        push(a / b);
    } DISPATCH();

    DO_OP_NEGATE:
    {
        if(vm.stackTop - vm.stack < 1){
            fprintf(stderr, "Stack underflow\n");
            return VM_RUNTIME_ERROR;
        }
        *(vm.stackTop - 1) = -(*(vm.stackTop - 1));
    } DISPATCH();

    DO_OP_RETURN:
    {
        printf("Top of stack: %g\n", pop());
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