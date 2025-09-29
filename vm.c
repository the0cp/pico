#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "common.h"
#include "chunk.h"
#include "vm.h"
#include "compiler.h"
#include "mem.h"

#ifdef DEBUG_TRACE
#include "debug.h"
#endif

void resetStack(VM* vm){
    vm->stackTop = vm->stack;
}

void initVM(VM* vm){
    resetStack(vm);
    vm->objects = NULL;
    initHashTable(&vm->strings);
    initHashTable(&vm->globals);
}

void freeVM(VM* vm){
    freeHashTable(&vm->strings);
    freeHashTable(&vm->globals);
    // freeObjects(vm);
}

void push(VM* vm, Value value){
    if(vm->stackTop - vm->stack >= STACK_MAX){
        runtimeError(vm, "Stack overflow");
        exit(EXIT_FAILURE);
    }
    *vm->stackTop++ = value;
}

Value pop(VM* vm){
    vm->stackTop--;
    return *vm->stackTop;
}

static Value peek(VM* vm, int distance){
    return vm->stackTop[-1 - distance];
}

static bool isTruthy(Value value){
    return !(IS_NULL(value) ||
            (IS_BOOL(value) && !AS_BOOL(value)) ||
            (IS_NUM(value) && AS_NUM(value) == 0));
    // NULL is considered falsy
}

InterpreterStatus interpret(VM* vm, const char* code){
    Chunk chunk;
    initChunk(&chunk);

    if(!compile(vm, code, &chunk)){
        freeChunk(&chunk);
        return VM_COMPILE_ERROR;
    }

    if(chunk.count == 0){
        freeChunk(&chunk);
        return VM_OK;  // No code to execute
    }

    vm->chunk = &chunk;
    vm->ip = vm->chunk->code;

    InterpreterStatus status = run(vm);

    freeChunk(&chunk);
    return status;
}

static InterpreterStatus run(VM* vm){
    static void* dispatchTable[] = {
        [OP_CONSTANT]       = &&DO_OP_CONSTANT,
        [OP_LCONSTANT]      = &&DO_OP_LCONSTANT,

        [OP_ADD]            = &&DO_OP_ADD,
        [OP_SUBTRACT]       = &&DO_OP_SUBTRACT,
        [OP_MULTIPLY]       = &&DO_OP_MULTIPLY,
        [OP_DIVIDE]         = &&DO_OP_DIVIDE,
        [OP_NEGATE]         = &&DO_OP_NEGATE,

        [OP_RETURN]         = &&DO_OP_RETURN,
        [OP_NULL]           = &&DO_OP_NULL,
        [OP_TRUE]           = &&DO_OP_TRUE,
        [OP_FALSE]          = &&DO_OP_FALSE,
        [OP_NOT]            = &&DO_OP_NOT,
        [OP_EQUAL]          = &&DO_OP_EQUAL,
        [OP_NOT_EQUAL]      = &&DO_OP_NOT_EQUAL,
        [OP_GREATER]        = &&DO_OP_GREATER,
        [OP_LESS]           = &&DO_OP_LESS,
        [OP_GREATER_EQUAL]  = &&DO_OP_GREATER_EQUAL,
        [OP_LESS_EQUAL]     = &&DO_OP_LESS_EQUAL,

        [OP_TO_STRING]      = &&DO_OP_TO_STRING,

        [OP_POP]            = &&DO_OP_POP,

        [OP_PRINT]          = &&DO_OP_PRINT,
        [OP_DEFINE_GLOBAL]  = &&DO_OP_DEFINE_GLOBAL,
        [OP_DEFINE_LGLOBAL] = &&DO_OP_DEFINE_LGLOBAL,
        [OP_GET_GLOBAL]     = &&DO_OP_GET_GLOBAL,
        [OP_GET_LGLOBAL]    = &&DO_OP_GET_LGLOBAL,
        [OP_SET_GLOBAL]     = &&DO_OP_SET_GLOBAL,
        [OP_SET_LGLOBAL]    = &&DO_OP_SET_LGLOBAL,

        [OP_GET_LOCAL]      = &&DO_OP_GET_LOCAL,
        [OP_SET_LOCAL]      = &&DO_OP_SET_LOCAL,
        [OP_GET_LLOCAL]     = &&DO_OP_GET_LLOCAL,
        [OP_SET_LLOCAL]     = &&DO_OP_SET_LLOCAL,

        [OP_JUMP]           = &&DO_OP_JUMP,
        [OP_JUMP_IF_FALSE]  = &&DO_OP_JUMP_IF_FALSE,

        [OP_LOOP]           = &&DO_OP_LOOP,
    };

    #ifdef DEBUG_TRACE
        #define DISPATCH() \
        do{ \
            printf(">> "); \
            dasmInstruction(vm->chunk, (int)(vm->ip - vm->chunk->code)); \
            goto *dispatchTable[*vm->ip++]; \
        }while(0)
    #else
        #define DISPATCH() \
        do{ \
            goto *dispatchTable[*vm->ip++]; \
        }while(0)
    #endif

    #define BI_OPERATOR(op) \
    do{ \
        if(vm->stackTop - vm->stack < 2){ \
            runtimeError(vm, "Stack underflow"); \
            return VM_RUNTIME_ERROR; \
        } \
        if(!IS_NUM(peek(vm, 0)) || !IS_NUM(peek(vm, 1))){ \
            runtimeError(vm, "Operands must be numbers."); \
            return VM_RUNTIME_ERROR; \
        } \
        double b = AS_NUM(pop(vm)); \
        *(vm->stackTop - 1) = NUM_VAL(AS_NUM(*(vm->stackTop - 1)) op b); \
    }while(0)

    DISPATCH();

    DO_OP_CONSTANT:
    {
        Value constant = vm->chunk->constants.values[*vm->ip++];
        push(vm, constant);
    } DISPATCH();

    DO_OP_LCONSTANT:
    {
        Value constant = vm->chunk->constants.values[
            (uint16_t)(vm->ip[0] << 8) | 
            vm->ip[1]
        ];
        vm->ip += 2; // Move past the index
        push(vm, constant);
    } DISPATCH();

    DO_OP_TO_STRING:
    {
        if(!IS_STRING(peek(vm, 0))){
            Value value = pop(vm);
            char* str = valueToString(value);
            push(vm, OBJECT_VAL(copyString(vm, str, strlen(str))));
        }
    } DISPATCH();

    DO_OP_NULL: push(vm, NULL_VAL); DISPATCH();

    DO_OP_TRUE: push(vm, BOOL_VAL(true)); DISPATCH();

    DO_OP_FALSE: push(vm, BOOL_VAL(false)); DISPATCH();

    DO_OP_NOT: push(vm, BOOL_VAL(!isTruthy(pop(vm)))); DISPATCH();

    DO_OP_EQUAL:
    {
        if(vm->stackTop - vm->stack < 2){
            runtimeError(vm, "Stack underflow");
            return VM_RUNTIME_ERROR;
        }
        Value b = pop(vm);
        Value a = pop(vm);
        push(vm, BOOL_VAL(isEqual(a, b)));
    } DISPATCH();

    DO_OP_NOT_EQUAL:
    {
        if(vm->stackTop - vm->stack < 2){
            runtimeError(vm, "Stack underflow");
            return VM_RUNTIME_ERROR;
        }
        Value b = pop(vm);
        *(vm->stackTop - 1) = BOOL_VAL(!isEqual(peek(vm, 0), b));
    } DISPATCH();

    DO_OP_GREATER:
    {
        if(vm->stackTop - vm->stack < 2){
            runtimeError(vm, "Stack underflow");
            return VM_RUNTIME_ERROR;
        }
        if(!IS_NUM(peek(vm, 0)) || !IS_NUM(peek(vm, 1))){ 
            runtimeError(vm, "Operands must be numbers.");
            return VM_RUNTIME_ERROR;
        }
        double b = AS_NUM(pop(vm));
        *(vm->stackTop - 1) = BOOL_VAL(AS_NUM(peek(vm, 0)) > b);
    } DISPATCH();

    DO_OP_LESS:
    {
        if(vm->stackTop - vm->stack < 2){
            runtimeError(vm, "Stack underflow");
            return VM_RUNTIME_ERROR;
        }
        if(!IS_NUM(peek(vm, 0)) || !IS_NUM(peek(vm, 1))){ 
            runtimeError(vm, "Operands must be numbers.");
            return VM_RUNTIME_ERROR;
        }
        double b = AS_NUM(pop(vm));
        *(vm->stackTop - 1) = BOOL_VAL(AS_NUM(peek(vm, 0)) < b);
    } DISPATCH();

    DO_OP_GREATER_EQUAL:
    {
        if(vm->stackTop - vm->stack < 2){
            runtimeError(vm, "Stack underflow");
            return VM_RUNTIME_ERROR;
        }
        if(!IS_NUM(peek(vm, 0)) || !IS_NUM(peek(vm, 1))){ 
            runtimeError(vm, "Operands must be numbers.");
            return VM_RUNTIME_ERROR;
        }
        double b = AS_NUM(pop(vm));
        *(vm->stackTop - 1) = BOOL_VAL(AS_NUM(peek(vm, 0)) >= b);
    } DISPATCH();

    DO_OP_LESS_EQUAL:
    {
        if(vm->stackTop - vm->stack < 2){
            runtimeError(vm, "Stack underflow");
            return VM_RUNTIME_ERROR;
        }
        if(!IS_NUM(peek(vm, 0)) || !IS_NUM(peek(vm, 1))){ 
            runtimeError(vm, "Operands must be numbers.");
            return VM_RUNTIME_ERROR;
        }
        double b = AS_NUM(pop(vm));
        *(vm->stackTop - 1) = BOOL_VAL(AS_NUM(peek(vm, 0)) <= b);
    } DISPATCH();

    DO_OP_ADD: 
    {
        if(IS_STRING(peek(vm, 0)) || IS_STRING(peek(vm, 1))){
            Value b = pop(vm);
            Value a = pop(vm);

            char* strA = valueToString(a);
            char* strB = valueToString(b);

            size_t lenA = strlen(strA);
            size_t lenB = strlen(strB);
            size_t len = lenA + lenB;

            char* chars = (char*)resize(NULL, 0, len + 1);
            memcpy(chars, strA, lenA);
            memcpy(chars + lenA, strB, lenB);
            chars[len] = '\0';

            push(vm, OBJECT_VAL(copyString(vm, chars, len)));
            resize(chars, len + 1, 0);
        }else if(IS_NUM(peek(vm, 0)) && IS_NUM(peek(vm, 1))){
            double b = AS_NUM(pop(vm));
            double a =AS_NUM(pop(vm));
            push(vm, NUM_VAL(a + b));
        }else{
            runtimeError(vm, "Unknown operands.");
            return VM_RUNTIME_ERROR;
        }
    } DISPATCH();

    DO_OP_SUBTRACT: BI_OPERATOR(-); DISPATCH();

    DO_OP_MULTIPLY: BI_OPERATOR(*); DISPATCH();

    DO_OP_DIVIDE:
    {
        if(vm->stackTop - vm->stack < 2){
            runtimeError(vm, "Stack underflow");
            return VM_RUNTIME_ERROR; \
        }
        if(!IS_NUM(peek(vm, 0)) || !IS_NUM(peek(vm, 1))){ 
            runtimeError(vm, "Operand must be a number.");
            return VM_RUNTIME_ERROR;
        }

        double b = AS_NUM(pop(vm));
        if(b == 0){
            runtimeError(vm, "Runtime error: Division by zero");
            return VM_RUNTIME_ERROR;
        }
        *(vm->stackTop - 1) = NUM_VAL(AS_NUM(*(vm->stackTop - 1)) / b);
    } DISPATCH();

    DO_OP_NEGATE:
    {
        if(!IS_NUM(peek(vm, 0))){
            runtimeError(vm, "Operand must be a number.");
            return VM_RUNTIME_ERROR;
        }
        if(vm->stackTop - vm->stack < 1){
            runtimeError(vm, "Stack underflow");
            return VM_RUNTIME_ERROR;
        }
        *(vm->stackTop - 1) = NUM_VAL(-AS_NUM(*(vm->stackTop - 1)));
    } DISPATCH();

    DO_OP_POP:
    {
        pop(vm);
    } DISPATCH();

    DO_OP_PRINT:
    {
        printValue(pop(vm));
        printf("\n");
    } DISPATCH();

    DO_OP_DEFINE_GLOBAL:
    {
        ObjectString* name = AS_STRING(vm->chunk->constants.values[*vm->ip++]);
        tableSet(&vm->globals, name, peek(vm, 0));
        pop(vm);
    } DISPATCH();

    DO_OP_DEFINE_LGLOBAL:
    {
        uint16_t index = (uint16_t)(vm->ip[0] << 8) | vm->ip[1];
                         
        vm->ip += 2;
        ObjectString* name = AS_STRING(vm->chunk->constants.values[index]);
        tableSet(&vm->globals, name, peek(vm, 0));
        pop(vm);
    } DISPATCH();

    DO_OP_GET_GLOBAL:
    {
        ObjectString* name = AS_STRING(vm->chunk->constants.values[*vm->ip++]);
        Value value;
        if (!tableGet(&vm->globals, name, &value)) {
            runtimeError(vm, "Undefined variable '%.*s'.", name->length, name->chars);
            return VM_RUNTIME_ERROR;
        }
        push(vm, value);
    } DISPATCH();

    DO_OP_GET_LGLOBAL:
    {
        uint16_t index = (uint16_t)(vm->ip[0] << 8) | vm->ip[1];
        vm->ip += 2;
        ObjectString* name = AS_STRING(vm->chunk->constants.values[index]);
        Value value;
        if(!tableGet(&vm->globals, name, &value)){
            runtimeError(vm, "Undefined variable '%.*s'.", name->length, name->chars);
            return VM_RUNTIME_ERROR;
        }
        push(vm, value);
    } DISPATCH();

    DO_OP_SET_GLOBAL:
    {
        ObjectString* name = AS_STRING(vm->chunk->constants.values[*vm->ip++]);
        if(tableSet(&vm->globals, name, peek(vm, 0))){
            // empty bucket, undefined
            tableRemove(&vm->globals, name);
            runtimeError(vm, "Undefined variable '%.*s'.", name->length, name->chars);
            return VM_RUNTIME_ERROR;
        }
    } DISPATCH();

    DO_OP_SET_LGLOBAL:
    {
        uint16_t index = (uint16_t)(vm->ip[0] << 8) | vm->ip[1];
        vm->ip += 2;
        ObjectString* name = AS_STRING(vm->chunk->constants.values[index]);
        if(tableSet(&vm->globals, name, peek(vm, 0))){
            tableRemove(&vm->globals, name);
            runtimeError(vm, "Undefined variable '%.*s'.", name->length, name->chars);
            return VM_RUNTIME_ERROR;
        }
    } DISPATCH();

    DO_OP_GET_LOCAL:
    {
        uint8_t slot = *vm->ip++;
        push(vm, vm->stack[slot]);
    } DISPATCH();

    DO_OP_SET_LOCAL:
    {
        uint8_t slot = *vm->ip++;
        vm->stack[slot] = peek(vm, 0);
    } DISPATCH();

    DO_OP_GET_LLOCAL:
    {
        uint16_t slot = (uint16_t)(vm->ip[0] << 8) | vm->ip[1];
        vm->ip += 2;
        push(vm, vm->stack[slot]);
    } DISPATCH();

    DO_OP_SET_LLOCAL:
    {
        uint16_t slot = (uint16_t)(vm->ip[0] << 8) | vm->ip[1];
        vm->ip += 2;
        vm->stack[slot] = peek(vm, 0);
    } DISPATCH();

    DO_OP_JUMP:
    {
        uint16_t offset = (uint16_t)(vm->ip[0] << 8) | vm->ip[1];
        vm->ip += 2;
        vm->ip += offset;
    } DISPATCH();

    DO_OP_JUMP_IF_FALSE:
    {
        uint16_t offset = (uint16_t)(vm->ip[0] << 8) | vm->ip[1];
        vm->ip += 2;
        if(!isTruthy(peek(vm, 0))){
            vm->ip += offset;
        }
        
    } DISPATCH();

    DO_OP_LOOP:
    {
        uint16_t offset = (uint16_t)(vm->ip[0] << 8) | vm->ip[1];
        vm->ip += 2;
        vm->ip -= offset;
    } DISPATCH();

    DO_OP_RETURN:
    {
        // printValue(pop(vm));
        // printf("\n");
        return VM_OK;
    }
    
    #undef DISPATCH

    /*
    while(true){
        uint8_t instruction;
        switch(instruction = *vm->ip++){
            case OP_RETURN:
                return VM_OK;
        }
    }
    */

}

static void runtimeError(VM* vm, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    #ifdef DEBUG_TRACE
    size_t instructionOffset = vm->ip - vm->chunk->code;
    int line = getLine(vm->chunk, instructionOffset);
    fprintf(stderr, "Runtime error at line %d\n", line);
    #endif
    
    resetStack(vm);
}