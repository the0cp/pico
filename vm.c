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
    vm->frameCount = 0;
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
    ObjectFunc* function = compile(vm, code);
    if (function == NULL) return VM_COMPILE_ERROR;

    push(vm, OBJECT_VAL(function));
    call(vm, function, 0);

    InterpreterStatus status = run(vm);
    return status;
}

static InterpreterStatus run(VM* vm){
    CallFrame* frame = &vm->frames[vm->frameCount - 1];

    #define READ_BYTE() (*frame->ip++)

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
        [OP_DUP]            = &&DO_OP_DUP,

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
        [OP_CALL]           = &&DO_OP_CALL,
    };

    #ifdef DEBUG_TRACE
        #define DISPATCH() \
        do{ \
            printf(">> "); \
            dasmInstruction(&frame->func->chunk, (int)(frame->ip - frame->function->chunk.code)); \
            goto *dispatchTable[READ_BYTE()]; \
        }while(0)
    #else
        #define DISPATCH() \
        do{ \
            goto *dispatchTable[READ_BYTE()]; \
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
        Value constant = frame->func->chunk.constants.values[READ_BYTE()];
        push(vm, constant);
    } DISPATCH();

    DO_OP_LCONSTANT:
    {
        Value constant = frame->func->chunk.constants.values[
            (uint16_t)(frame->ip[0] << 8) | 
            frame->ip[1]
        ];
        frame->ip += 2; // Move past the index
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

    DO_OP_DUP:
    {
        push(vm, peek(vm, 0));
    } DISPATCH();

    DO_OP_PRINT:
    {
        printValue(pop(vm));
        printf("\n");
    } DISPATCH();

    DO_OP_DEFINE_GLOBAL:
    {
        ObjectString* name = AS_STRING(frame->func->chunk.constants.values[READ_BYTE()]);
        tableSet(&vm->globals, name, peek(vm, 0));
        pop(vm);
    } DISPATCH();

    DO_OP_DEFINE_LGLOBAL:
    {
        uint16_t index = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
                         
        frame->ip += 2;
        ObjectString* name = AS_STRING(frame->func->chunk.constants.values[index]);
        tableSet(&vm->globals, name, peek(vm, 0));
        pop(vm);
    } DISPATCH();

    DO_OP_GET_GLOBAL:
    {
        ObjectString* name = AS_STRING(frame->func->chunk.constants.values[READ_BYTE()]);
        Value value;
        if (!tableGet(&vm->globals, name, &value)) {
            runtimeError(vm, "Undefined variable '%.*s'.", name->length, name->chars);
            return VM_RUNTIME_ERROR;
        }
        push(vm, value);
    } DISPATCH();

    DO_OP_GET_LGLOBAL:
    {
        uint16_t index = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
        frame->ip += 2;
        ObjectString* name = AS_STRING(frame->func->chunk.constants.values[index]);
        Value value;
        if(!tableGet(&vm->globals, name, &value)){
            runtimeError(vm, "Undefined variable '%.*s'.", name->length, name->chars);
            return VM_RUNTIME_ERROR;
        }
        push(vm, value);
    } DISPATCH();

    DO_OP_SET_GLOBAL:
    {
        ObjectString* name = AS_STRING(frame->func->chunk.constants.values[READ_BYTE()]);
        if(tableSet(&vm->globals, name, peek(vm, 0))){
            // empty bucket, undefined
            tableRemove(&vm->globals, name);
            runtimeError(vm, "Undefined variable '%.*s'.", name->length, name->chars);
            return VM_RUNTIME_ERROR;
        }
    } DISPATCH();

    DO_OP_SET_LGLOBAL:
    {
        uint16_t index = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
        frame->ip += 2;
        ObjectString* name = AS_STRING(frame->func->chunk.constants.values[index]);
        if(tableSet(&vm->globals, name, peek(vm, 0))){
            tableRemove(&vm->globals, name);
            runtimeError(vm, "Undefined variable '%.*s'.", name->length, name->chars);
            return VM_RUNTIME_ERROR;
        }
    } DISPATCH();

    DO_OP_GET_LOCAL:
    {
        uint8_t slot = READ_BYTE();
        push(vm, frame->slots[slot]);
    } DISPATCH();

    DO_OP_SET_LOCAL:
    {
        uint8_t slot = READ_BYTE();
        frame->slots[slot] = peek(vm, 0);
    } DISPATCH();

    DO_OP_GET_LLOCAL:
    {
        uint16_t slot = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
        frame->ip += 2;
        push(vm, frame->slots[slot]);
    } DISPATCH();

    DO_OP_SET_LLOCAL:
    {
        uint16_t slot = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
        frame->ip += 2;
        frame->slots[slot] = peek(vm, 0);
    } DISPATCH();

    DO_OP_JUMP:
    {
        uint16_t offset = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
        frame->ip += 2;
        frame->ip += offset;
    } DISPATCH();

    DO_OP_JUMP_IF_FALSE:
    {
        uint16_t offset = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
        frame->ip += 2;
        if(!isTruthy(peek(vm, 0))){
            frame->ip += offset;
        }
        //pop(vm);
    } DISPATCH();

    DO_OP_LOOP:
    {
        uint16_t offset = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
        frame->ip += 2;
        frame->ip -= offset;
    } DISPATCH();

    DO_OP_CALL:
    {
        int argCount = READ_BYTE();
        if(!callValue(vm, peek(vm, argCount), argCount)){
            return VM_RUNTIME_ERROR;
        }
        frame = &vm->frames[vm->frameCount - 1];
    } DISPATCH();

    DO_OP_RETURN:
    {
        Value result = pop(vm);
        vm->frameCount--;

        if(vm->frameCount == 0){
            pop(vm);
            return VM_OK;
        }

        vm->stackTop = frame->slots;
        push(vm, result);

        frame = &vm->frames[vm->frameCount - 1];
    } DISPATCH();
    
    #undef DISPATCH

    /*
    while(true){
        uint8_t instruction;
        switch(instruction = READ_BYTE()){
            case OP_RETURN:
                return VM_OK;
        }
    }
    */

}

static bool call(VM* vm, ObjectFunc* func, int argCnt){
    if(argCnt != func->arity){
        runtimeError(vm, "Expected %d args but got %d.", func->arity, argCnt);
        return false;
    }

    if(vm->frameCount == FRAMES_MAX){
        runtimeError(vm, "Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm->frames[vm->frameCount++];   // 0-indexing++ to actual count
    frame->func = func;
    frame->ip = func->chunk.code;

    frame->slots = vm->stackTop - argCnt - 1;   // -1 to skip func self
    return true;
}

static bool callValue(VM* vm, Value callee, int argCnt){
    if(IS_OBJECT(callee)){
        switch(OBJECT_TYPE(callee)){
            case OBJECT_FUNC:
                return call(vm, AS_FUNC(callee), argCnt);
            default:
                break;
        }
    }
    runtimeError(vm, "Can only call functions and classes.");
    return false;
}

static void runtimeError(VM* vm, const char* format, ...){
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    #ifdef DEBUG_TRACE
    CallFrame* frame = &vm->frames[vm->frameCount - 1];
    size_t instructionOffset = frame->ip - frame->func->chunk.code -1;
    int line = getLine(vm->chunk, instructionOffset);
    fprintf(stderr, "Runtime error at line %d\n", line);
    #endif
    
    resetStack(vm);
}