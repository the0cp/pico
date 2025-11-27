#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "common.h"
#include "chunk.h"
#include "vm.h"
#include "compiler.h"
#include "mem.h"
#include "file.h"
#include "modules/modules.h"

#ifdef DEBUG_TRACE
#include "debug.h"
#endif

void resetStack(VM* vm){
    vm->stackTop = vm->stack;
}


void initVM(VM* vm){
    resetStack(vm);
    vm->objects = NULL;
    vm->openUpvalues = NULL;
    vm->frameCount = 0;
    initHashTable(&vm->strings);
    initHashTable(&vm->globals);
    initHashTable(&vm->modules);

    vm->globalCnt = 0;
    vm->curGlobal = &vm->globals;
    vm->globalStack[0] = vm->curGlobal;

    vm->bytesAllocated = 0;
    vm->nextGC = 1024;   // 1KB

    vm->compiler = NULL;

    registerFsModule(vm);
    registerTimeModule(vm);
}

void freeVM(VM* vm){
    freeHashTable(vm, &vm->strings);
    freeHashTable(vm, &vm->globals);
    freeHashTable(vm, &vm->modules);

    vm->globalCnt = 0;
    vm->curGlobal = NULL;
    vm->openUpvalues = NULL;
    freeObjects(vm);
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

Value peek(VM* vm, int distance){
    if(vm->stackTop - vm->stack - 1 - distance < 0){
        runtimeError(vm, "Stack underflow");
        exit(EXIT_FAILURE);
    }
    return vm->stackTop[-1 - distance];
}

static void pushGlobal(VM* vm, HashTable* globals){
    if(vm->globalCnt >= GLOBAL_STATCK_MAX){
        runtimeError(vm, "Too many nested imports, globals stack overflow.");
        return;
    }
    vm->globalStack[vm->globalCnt++] = vm->curGlobal;
    vm->curGlobal = globals;
}

static void popGlobal(VM* vm){
    if(vm->globalCnt <= 0){
        runtimeError(vm, "Global statck underflow.");
        return;
    }
    vm->curGlobal = vm->globalStack[--vm->globalCnt];
}

static bool isTruthy(Value value){
    return !(IS_NULL(value) ||
            (IS_BOOL(value) && !AS_BOOL(value)) ||
            (IS_NUM(value) && AS_NUM(value) == 0));
    // NULL is considered falsy
}

static ObjectUpvalue* captureUpvalue(VM* vm, Value* local){
    ObjectUpvalue* prevUpvalue = NULL;
    ObjectUpvalue* upvalue = vm->openUpvalues;

    while(upvalue != NULL && upvalue->location > local){
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if(upvalue != NULL && upvalue->location == local){
        return upvalue;
    }

    ObjectUpvalue* createdUpvalue = newUpvalue(vm, local);
    createdUpvalue->next = upvalue;

    if(prevUpvalue == NULL){
        vm->openUpvalues = createdUpvalue;
    }else{
        prevUpvalue->next = createdUpvalue;
    }
    return createdUpvalue;
}

static void closeUpvalues(VM* vm, Value* last){
    while(vm->openUpvalues != NULL && vm->openUpvalues->location >= last){
        ObjectUpvalue* upvalue = vm->openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm->openUpvalues = upvalue->next;
    }
}

InterpreterStatus interpret(VM* vm, const char* code, const char* srcName){
    ObjectFunc* func = compile(vm, code, srcName);
    if(func == NULL) return VM_COMPILE_ERROR;

    push(vm, OBJECT_VAL(func));

    ObjectClosure* closure = newClosure(vm, func);
    pop(vm);    // pop func
    push(vm, OBJECT_VAL(closure));

    if(!call(vm, closure, 0)){
        return VM_RUNTIME_ERROR;
    }

    InterpreterStatus status = run(vm);
    return status;
}

static bool checkAccess(VM* vm, ObjectClass* instanceKlass, ObjectString* fieldName){
    // public
    if(isupper(fieldName->chars[0])){
        return true;
    }

    // private check
    if(vm->frameCount > 0){
        CallFrame* frame = &vm->frames[vm->frameCount - 1];
        ObjectFunc* func = frame->closure->func;
        if(func->fieldOwner == instanceKlass){
            return true;
        }
    }

    return false;
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

        [OP_IMPORT]         = &&DO_OP_IMPORT,
        [OP_LIMPORT]        = &&DO_OP_LIMPORT,

        [OP_GET_PROPERTY]   = &&DO_OP_GET_PROPERTY,
        [OP_GET_LPROPERTY]  = &&DO_OP_GET_LPROPERTY,
        [OP_SET_PROPERTY]   = &&DO_OP_SET_PROPERTY,
        [OP_SET_LPROPERTY]  = &&DO_OP_SET_LPROPERTY,

        [OP_CLOSURE]        = &&DO_OP_CLOSURE,
        [OP_LCLOSURE]       = &&DO_OP_LCLOSURE,
        [OP_GET_UPVALUE]    = &&DO_OP_GET_UPVALUE,
        [OP_GET_LUPVALUE]   = &&DO_OP_GET_LUPVALUE,
        [OP_SET_UPVALUE]    = &&DO_OP_SET_UPVALUE,
        [OP_SET_LUPVALUE]   = &&DO_OP_SET_LUPVALUE,
        [OP_CLOSE_UPVALUE]  = &&DO_OP_CLOSE_UPVALUE,

        [OP_CLASS]          = &&DO_OP_CLASS,
        [OP_METHOD]         = &&DO_OP_METHOD,
        [OP_LCLASS]         = &&DO_OP_LCLASS,
        [OP_LMETHOD]        = &&DO_OP_LMETHOD,

        [OP_DEFINE_FIELD]   = &&DO_OP_DEFINE_FIELD,
        [OP_DEFINE_LFIELD]  = &&DO_OP_DEFINE_LFIELD,
    };

    #ifdef DEBUG_TRACE
        #define DISPATCH() \
        do{ \
            printf(">> "); \
            dasmInstruction(&frame->closure->func->chunk, (int)(frame->ip - frame->closure->func->chunk.code)); \
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
        Value constant = frame->closure->func->chunk.constants.values[READ_BYTE()];
        push(vm, constant);
    } DISPATCH();

    DO_OP_LCONSTANT:
    {
        Value constant = frame->closure->func->chunk.constants.values[
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
            ObjectString* str = toString(vm, value);
            push(vm, OBJECT_VAL(str));
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
        Value vb = peek(vm, 0);
        Value va = peek(vm, 1);

        if(IS_STRING(peek(vm, 0)) || IS_STRING(peek(vm, 1))){
            ObjectString* strA = toString(vm, va);
            push(vm, OBJECT_VAL(strA));
            ObjectString* strB = toString(vm, vb);
            
            push(vm, OBJECT_VAL(strB));

            size_t lenA = strA->length;
            size_t lenB = strB->length;
            size_t len = lenA + lenB;

            char* chars = (char*)reallocate(vm, NULL, 0, len + 1);
            if(chars == NULL){
                runtimeError(vm, "Memory allocation failed for string concatenation.");
                return VM_RUNTIME_ERROR;
            }
            memcpy(chars, strA->chars, lenA);
            memcpy(chars + lenA, strB->chars, lenB);
            chars[len] = '\0';

            ObjectString* result = copyString(vm, chars, len);
            reallocate(vm, chars, len + 1, 0);
            pop(vm); pop(vm); pop(vm); pop(vm); // pop strB, strA, vb, va
            push(vm, OBJECT_VAL(result));
        }else if(IS_NUM(va) && IS_NUM(vb)){
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
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[READ_BYTE()]);
        tableSet(vm, vm->curGlobal, name, peek(vm, 0));
        pop(vm);
    } DISPATCH();

    DO_OP_DEFINE_LGLOBAL:
    {
        uint16_t index = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
                         
        frame->ip += 2;
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[index]);
        tableSet(vm, vm->curGlobal, name, peek(vm, 0));
        pop(vm);
    } DISPATCH();

    DO_OP_GET_GLOBAL:
    {
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[READ_BYTE()]);
        Value value;
        if (!tableGet(vm->curGlobal, name, &value)) {
            runtimeError(vm, "Undefined variable '%.*s'.", name->length, name->chars);
            return VM_RUNTIME_ERROR;
        }
        push(vm, value);
    } DISPATCH();

    DO_OP_GET_LGLOBAL:
    {
        uint16_t index = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
        frame->ip += 2;
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[index]);
        Value value;
        if(!tableGet(vm->curGlobal, name, &value)){
            runtimeError(vm, "Undefined variable '%.*s'.", name->length, name->chars);
            return VM_RUNTIME_ERROR;
        }
        push(vm, value);
    } DISPATCH();

    DO_OP_SET_GLOBAL:
    {
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[READ_BYTE()]);
        if(tableSet(vm, vm->curGlobal, name, peek(vm, 0))){
            // empty bucket, undefined
            tableRemove(vm, &vm->globals, name);
            runtimeError(vm, "Undefined variable '%.*s'.", name->length, name->chars);
            return VM_RUNTIME_ERROR;
        }
    } DISPATCH();

    DO_OP_SET_LGLOBAL:
    {
        uint16_t index = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
        frame->ip += 2;
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[index]);
        if(tableSet(vm, vm->curGlobal, name, peek(vm, 0))){
            tableRemove(vm, &vm->globals, name);
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

    DO_OP_IMPORT:
    {
        uint8_t index = READ_BYTE();
        ObjectString* path = AS_STRING(frame->closure->func->chunk.constants.values[index]);
        push(vm, OBJECT_VAL(path));

        Value modVal;
        if(tableGet(&vm->modules, path, &modVal)){
            pop(vm);
            push(vm, modVal);
            DISPATCH(); // next dispatch
        }

        char* source = read(path->chars);
        if(source == NULL){
            pop(vm);
            runtimeError(vm, "Could not read import file '%s'.", path->chars);
            return VM_RUNTIME_ERROR;
        }

        ObjectFunc* func = compile(vm, source, path->chars);
        free(source);
        if(func == NULL){
            pop(vm);
            return VM_COMPILE_ERROR; 
        }

        func->type = TYPE_MODULE;

        push(vm, OBJECT_VAL(func));

        ObjectClosure* closure = newClosure(vm, func);
        pop(vm);    // pop ObjectFunc
        if(closure == NULL){
            pop(vm);
            runtimeError(vm, "Out of memory creating closure");
            return VM_RUNTIME_ERROR;
        }
        push(vm, OBJECT_VAL(closure));

        ObjectModule* module = newModule(vm, path);
        if(module == NULL){
            pop(vm);  // closure
            pop(vm);  // path
            runtimeError(vm, "Out of memory creating module");
            return VM_RUNTIME_ERROR;
        }
        push(vm, OBJECT_VAL(module));
        tableSet(vm, &vm->modules, path, OBJECT_VAL(module));

        pushGlobal(vm, &module->members);
        
        if(!call(vm, closure, 0)){
            popGlobal(vm);
            pop(vm); pop(vm); pop(vm); // module, closure, path
            return VM_RUNTIME_ERROR;
        }

        pop(vm); // path
        frame = &vm->frames[vm->frameCount - 1];

    } DISPATCH();

    DO_OP_LIMPORT:
    {
        uint16_t index = (uint16_t)(frame->ip[0] << 8 | frame->ip[1]);
        frame->ip += 2;
        ObjectString* path = AS_STRING(frame->closure->func->chunk.constants.values[index]);
        
        push(vm, OBJECT_VAL(path));

        Value modVal;
        if(tableGet(&vm->modules, path, &modVal)){
            pop(vm);
            push(vm, modVal);
            DISPATCH(); // next dispatch
        }

        char* source = read(path->chars);
        if(source == NULL){
            pop(vm);
            runtimeError(vm, "Could not read import file '%s'.", path->chars);
            return VM_RUNTIME_ERROR;
        }
        ObjectFunc* func = compile(vm, source, path->chars);
        free(source);
        if(func == NULL){
            pop(vm);
            return VM_COMPILE_ERROR; 
        }

        func->type = TYPE_MODULE;

        push(vm, OBJECT_VAL(func));

        ObjectClosure* closure = newClosure(vm, func);
        pop(vm); // pop ObjectFunc
        push(vm, OBJECT_VAL(closure));

        ObjectModule* module = newModule(vm, path);
        push(vm, OBJECT_VAL(module));
        tableSet(vm, &vm->modules, path, OBJECT_VAL(module));
        pop(vm);

        pushGlobal(vm, &module->members);
        
        if(!call(vm, closure, 0)){
            popGlobal(vm);
            return VM_RUNTIME_ERROR;
        }

        frame = &vm->frames[vm->frameCount - 1];
    }

    DO_OP_GET_PROPERTY:
    {
        if(!IS_OBJECT(peek(vm, 0))){
            runtimeError(vm, "Only modules and objects have properties.");
            return VM_RUNTIME_ERROR;
        }

        if(IS_INSTANCE(peek(vm, 0))){
            ObjectInstance* instance = AS_INSTANCE(peek(vm, 0));
            ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[READ_BYTE()]);

            Value value;
            if(tableGet(&instance->fields, name, &value)){
                if(!checkAccess(vm, instance->klass, name)){
                    runtimeError(vm, "Cannot access private field '%s' of instance of '%s'.", name->chars, instance->klass->name->chars);
                    return VM_RUNTIME_ERROR;
                }
                pop(vm);
                push(vm, value);
                DISPATCH();
            }

            if(tableGet(&instance->klass->methods, name, &value)){
                ObjectBoundMethod* bound = newBoundMethod(vm, peek(vm, 0), AS_CLOSURE(value));
                pop(vm);
                push(vm, OBJECT_VAL(bound));
                DISPATCH();
            }
            runtimeError(vm, "Undefined property '%s' on instance of '%s'.", name->chars, instance->klass->name->chars);
            return VM_RUNTIME_ERROR;
        }

        if(IS_MODULE(peek(vm, 0))){
            ObjectModule* module = AS_MODULE(peek(vm, 0));
            ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[READ_BYTE()]);

            Value value;
            if(!tableGet(&module->members, name, &value)){
                runtimeError(vm, "Undefined property '%s' on module '%s'.", name->chars, module->name->chars);
                return VM_RUNTIME_ERROR;
            }
            
            pop(vm);
            push(vm, value);
        }
    } DISPATCH();

    DO_OP_GET_LPROPERTY:
    {
        if(!IS_OBJECT(peek(vm, 0))){
            runtimeError(vm, "Only modules and objects have properties.");
            return VM_RUNTIME_ERROR;
        }

        if(IS_INSTANCE(peek(vm, 0))){
            ObjectInstance* instance = AS_INSTANCE(peek(vm, 0));
            uint16_t index = (uint16_t)((frame->ip[0] << 8) | frame->ip[1]);
            frame->ip += 2;
            ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[index]);

            Value value;
            if(tableGet(&instance->fields, name, &value)){
                if(!checkAccess(vm, instance->klass, name)){
                    runtimeError(vm, "Cannot access private field '%s' of instance of '%s'.", name->chars, instance->klass->name->chars);
                    return VM_RUNTIME_ERROR;
                }
                pop(vm);
                push(vm, value);
                DISPATCH();
            }

            if(tableGet(&instance->klass->methods, name, &value)){
                ObjectBoundMethod* bound = newBoundMethod(vm, peek(vm, 0), AS_CLOSURE(value));
                pop(vm);
                push(vm, OBJECT_VAL(bound));
                DISPATCH();
            }
            runtimeError(vm, "Undefined property '%s' on instance of '%s'.", name->chars, instance->klass->name->chars);
            return VM_RUNTIME_ERROR;
        }

        if(IS_MODULE(peek(vm, 0))){
            ObjectModule* module = AS_MODULE(peek(vm, 0));
            uint16_t index = (uint16_t)((frame->ip[0] << 8) | frame->ip[1]);
            frame->ip += 2;
            ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[index]);

            Value value;
            if(!tableGet(&module->members, name, &value)){
                runtimeError(vm, "Undefined property '%s' on module '%s'.", name->chars, module->name->chars);
                return VM_RUNTIME_ERROR;
            }
            
            pop(vm);
            push(vm, value);
        }
    } DISPATCH();

    DO_OP_SET_PROPERTY:
    {
        if(!IS_OBJECT(peek(vm, 1))){    // stack top is a value, -1 for object
            runtimeError(vm, "Only modules and instances have properties.");
            return VM_RUNTIME_ERROR;
        }

        if(IS_INSTANCE(peek(vm, 1))){
            ObjectInstance* instance = AS_INSTANCE(peek(vm, 1));
            ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[READ_BYTE()]);
            Value dummy;
            if(!tableGet(&instance->fields, name, &dummy)){
                runtimeError(vm, "Undefined property '%s' on instance of '%s'.", name->chars, instance->klass->name->chars);
                return VM_RUNTIME_ERROR;
            }
            if(!checkAccess(vm, instance->klass, name)){
                runtimeError(vm, "Cannot access private field '%s' of instance of '%s'.", name->chars, instance->klass->name->chars);
                return VM_RUNTIME_ERROR;
            }
            
            tableSet(vm, &instance->fields, name, peek(vm, 0));

            Value value = pop(vm);
            pop(vm);    // pop instance
            push(vm, value);
            DISPATCH();
        }

        if(IS_MODULE(peek(vm, 1))){
            ObjectModule* module = AS_MODULE(peek(vm, 1));
            ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[READ_BYTE()]);
            tableSet(vm, &module->members, name, peek(vm, 0));

            Value value = pop(vm);
            pop(vm);    // pop module
            push(vm, value);
        }
    } DISPATCH();

    DO_OP_SET_LPROPERTY:
    {
        if(!IS_OBJECT(peek(vm, 1))){    // stack top is a value, -1 for object
            runtimeError(vm, "Only modules and objects have properties.");
            return VM_RUNTIME_ERROR;
        }

        if(IS_INSTANCE(peek(vm, 1))){
            ObjectInstance* instance = AS_INSTANCE(peek(vm, 1));
            uint16_t index = (uint16_t)((frame->ip[0] << 8) | frame->ip[1]);
            frame->ip += 2;
            ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[index]);
            Value dummy;
            if(!tableGet(&instance->fields, name, &dummy)){
                runtimeError(vm, "Undefined property '%s' on instance of '%s'.", name->chars, instance->klass->name->chars);
                return VM_RUNTIME_ERROR;
            }
            if(!checkAccess(vm, instance->klass, name)){
                runtimeError(vm, "Cannot access private field '%s' of instance of '%s'.", name->chars, instance->klass->name->chars);
                return VM_RUNTIME_ERROR;
            }
            
            tableSet(vm, &instance->fields, name, peek(vm, 0));

            Value value = pop(vm);
            pop(vm);    // pop instance
            push(vm, value);
            DISPATCH();
        } 

        if(IS_MODULE(peek(vm, 1))){
            ObjectModule* module = AS_MODULE(peek(vm, 1));
            uint16_t index = (uint16_t)((frame->ip[0] << 8) | frame->ip[1]);
            frame->ip += 2;
            ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[index]);
            tableSet(vm, &module->members, name, peek(vm, 0));

            Value value = pop(vm);
            pop(vm);    // pop module
            push(vm, value);
        }
    } DISPATCH();

    DO_OP_CLOSURE:
    {
        uint8_t constIndex = READ_BYTE();
        ObjectFunc* func = AS_FUNC(frame->closure->func->chunk.constants.values[constIndex]);

        ObjectClosure* closure = newClosure(vm, func);
        if(!closure){
            runtimeError(vm, "Out of memory creating closure");
            return VM_RUNTIME_ERROR;
        }
        push(vm, OBJECT_VAL(closure));

        for(int i = 0; i < closure->upvalueCnt; i++){
            uint8_t isLocal = READ_BYTE();
            uint16_t index = (uint16_t)((READ_BYTE() << 8 ) | READ_BYTE());
            if(isLocal){
                closure->upvalues[i] = captureUpvalue(vm, frame->slots + index);
            }else{
                if(index >= frame->closure->upvalueCnt){
                    runtimeError(vm, "Upvalue index out of bounds");
                    return VM_RUNTIME_ERROR;
                }
                closure->upvalues[i] = frame->closure->upvalues[index];
            }
        }
    } DISPATCH();

    DO_OP_LCLOSURE:
    {
        uint16_t constIndex = (uint16_t)((READ_BYTE() << 8) | READ_BYTE());
        if(constIndex >= frame->closure->func->chunk.constants.count){
            runtimeError(vm, "Closure constant index out of bounds");
            return VM_RUNTIME_ERROR;
        }
        ObjectFunc* func = AS_FUNC(frame->closure->func->chunk.constants.values[constIndex]);

        ObjectClosure* closure = newClosure(vm, func);
        if(closure == NULL){
            runtimeError(vm, "Out of memory creating closure");
            return VM_RUNTIME_ERROR;
        }
        push(vm, OBJECT_VAL(closure));

        for(int i = 0; i < closure->upvalueCnt; i++){
            uint8_t isLocal = READ_BYTE();
            uint16_t index = (uint16_t)((READ_BYTE() << 8 ) | READ_BYTE());
            if(isLocal){
                closure->upvalues[i] = captureUpvalue(vm, frame->slots + index);
            }else{
                if(index >= frame->closure->upvalueCnt){
                    runtimeError(vm, "Upvalue index out of bounds");
                    pop(vm);
                    return VM_RUNTIME_ERROR;
                }
                closure->upvalues[i] = frame->closure->upvalues[index];
            }
        }
    } DISPATCH();

    DO_OP_GET_UPVALUE:
    {
        uint8_t index = READ_BYTE();
        push(vm, *frame->closure->upvalues[index]->location);
    } DISPATCH();

    DO_OP_GET_LUPVALUE:
    {
        uint16_t index = (uint16_t)((READ_BYTE() << 8) | READ_BYTE());
        push(vm, *frame->closure->upvalues[index]->location);
    } DISPATCH();

    DO_OP_SET_UPVALUE:
    {
        uint8_t index = READ_BYTE();
        *frame->closure->upvalues[index]->location = peek(vm, 0);
    } DISPATCH();

    DO_OP_SET_LUPVALUE:
    {
        uint16_t index = (uint16_t)((READ_BYTE() << 8) | READ_BYTE());
        *frame->closure->upvalues[index]->location = peek(vm, 0);
    } DISPATCH();

    DO_OP_CLOSE_UPVALUE:
    {
        closeUpvalues(vm, vm->stackTop - 1);
        pop(vm);
    } DISPATCH();

    DO_OP_CLASS:
    {
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[READ_BYTE()]);
        ObjectClass* klass = newClass(vm, name);
        initHashTable(&klass->methods);
        push(vm, OBJECT_VAL(klass));
    } DISPATCH();

    DO_OP_METHOD:
    {
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[READ_BYTE()]);
        Value method = peek(vm, 0);
        Value klassVal = peek(vm, 1);
        if(!IS_CLASS(klassVal)){
            runtimeError(vm, "Receiver must be a class.");
            return VM_RUNTIME_ERROR;
        }
        ObjectClass* klass = AS_CLASS(klassVal);

        ObjectClosure* methodClosure = AS_CLOSURE(method);
        methodClosure->func->fieldOwner = klass;

        tableSet(vm, &klass->methods, name, method);
        pop(vm);
    } DISPATCH();

    DO_OP_LCLASS:
    {
        uint16_t index = (uint16_t)(READ_BYTE() << 8 | READ_BYTE());
        frame->ip += 2;
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[index]);
        ObjectClass* klass = newClass(vm, name);
        push(vm, OBJECT_VAL(klass));
    } DISPATCH();

    DO_OP_LMETHOD:
    {
        uint16_t index = (uint16_t)(READ_BYTE() << 8 | READ_BYTE());
        frame->ip += 2;
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[index]);
        Value method = peek(vm, 0);
        Value klassVal = peek(vm, 1);
        if(!IS_CLASS(klassVal)){
            runtimeError(vm, "Receiver must be a class.");
            return VM_RUNTIME_ERROR;
        }
        ObjectClass* klass = AS_CLASS(klassVal);

        ObjectClosure* methodClosure = AS_CLOSURE(method);
        methodClosure->func->fieldOwner = klass;

        tableSet(vm, &klass->methods, name, method);
        pop(vm);
    } DISPATCH();

    DO_OP_DEFINE_FIELD:
    {
        uint8_t index = READ_BYTE();
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[index]);
        Value defaultValue = pop(vm);
        ObjectClass* klass = AS_CLASS(peek(vm, 0));
        tableSet(vm, &klass->fields, name, defaultValue);
    } DISPATCH();

    DO_OP_DEFINE_LFIELD:
    {
        uint16_t index = (uint16_t)(READ_BYTE() << 8 | READ_BYTE());
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[index]);
        Value defaultValue = pop(vm);
        ObjectClass* klass = AS_CLASS(peek(vm, 0));
        tableSet(vm, &klass->fields, name, defaultValue);
    } DISPATCH();

    DO_OP_RETURN:
    {
        Value result = pop(vm);

        closeUpvalues(vm, frame->slots);

        if(frame->closure->func->type == TYPE_MODULE){
            popGlobal(vm);
            result = frame->slots[0];
        }

        vm->frameCount--;

        if(vm->frameCount == 0){
            vm->stackTop = frame->slots;
            return VM_OK;
        }

        vm->stackTop = frame->slots;
        push(vm, result);

        frame = &vm->frames[vm->frameCount - 1];
    } DISPATCH();
    
    #undef DISPATCH

}

static bool call(VM* vm, ObjectClosure* closure, int argCnt){
    if(argCnt != closure->func->arity){
        runtimeError(vm, "Expected %d args but got %d.", closure->func->arity, argCnt);
        return false;
    }

    if(vm->frameCount == FRAMES_MAX){
        runtimeError(vm, "Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm->frames[vm->frameCount++];   // 0-indexing++ to actual count
    frame->closure = closure;
    frame->ip = closure->func->chunk.code;
    frame->slots = vm->stackTop - argCnt - 1;   // -1 to skip func self

    return true;
}

static bool callValue(VM* vm, Value callee, int argCnt){
    if(IS_OBJECT(callee)){
        switch(OBJECT_TYPE(callee)){
            case OBJECT_CLASS:{
                ObjectClass* klass = AS_CLASS(callee);
                vm->stackTop[-argCnt -1] = OBJECT_VAL(newInstance(vm, klass));
                // init
                return true;
            }
            case OBJECT_BOUND_METHOD:{
                ObjectBoundMethod* bound = AS_BOUND_METHOD(callee);
                vm->stackTop[-argCnt -1] = bound->receiver;
                return call(vm, bound->method, argCnt);
            }
            case OBJECT_CLOSURE:
                return call(vm, AS_CLOSURE(callee), argCnt);
            case OBJECT_CFUNC:{
                CFunc cfunc = AS_CFUNC(callee);
                Value result = cfunc(vm, argCnt, vm->stackTop - argCnt);
                vm->stackTop -= argCnt + 1;
                push(vm, result);
                return true;
            }
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
    if(vm->frameCount > 0){
        CallFrame* frame = &vm->frames[vm->frameCount - 1];
        size_t instructionOffset = frame->ip - frame->closure->func->chunk.code -1;
        
        int line = getLine(&frame->closure->func->chunk, instructionOffset); 

        const char* srcName = frame->closure->func->srcName != NULL 
                                 ? frame->closure->func->srcName->chars 
                                 : "<script>";
        fprintf(stderr, "Runtime error [%s, line %d]\n", srcName, line);
    }else{
        fprintf(stderr, "Runtime error [No stack trace available]\n");
    }
    #endif
    
    resetStack(vm);
}