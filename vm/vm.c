#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#include "common.h"
#include "chunk.h"
#include "vm.h"
#include "compiler.h"
#include "mem.h"
#include "file.h"
#include "modules/list.h"
#include "modules/modules.h"
#include "modules/fs.h"
#include "modules/string.h"

#ifdef DEBUG_TRACE
#include "debug.h"
#endif

void resetStack(VM* vm){
    vm->stackTop = vm->stack;
}

void initVM(VM* vm, int argc, const char* argv[]){
    resetStack(vm);
    vm->objects = NULL;
    vm->openUpvalues = NULL;
    vm->frameCount = 0;

    srand((unsigned int)time(NULL));
    uint64_t p1 = (uint64_t)rand();
    uint64_t p2 = (uint64_t)rand();
    vm->hash_seed = (p1 << 32) | p2;
    if(vm->hash_seed == 0) vm->hash_seed = 1;

    initHashTable(&vm->strings);
    initHashTable(&vm->globals);
    initHashTable(&vm->modules);

    vm->globalCnt = 0;
    vm->curGlobal = &vm->globals;
    vm->globalStack[0] = vm->curGlobal;

    vm->bytesAllocated = 0;
    vm->nextGC = 1024 * 1024 * 10;   // 10MB

    vm->compiler = NULL;

    vm->initString = NULL;
    vm->initString = copyString(vm, "init", 4);

    registerFsModule(vm);
    registerTimeModule(vm);
    registerOsModule(vm);
    registerPathModule(vm);
    registerGlobModule(vm);
    registerListModule(vm);
    registerStringModule(vm);
    registerIterModule(vm);

    if(argc > 0 && argv != NULL){
        Value osModVal;
        ObjectString* osName = copyString(vm, "os", 2);
        push(vm, OBJECT_VAL(osName));

        if(tableGet(vm, &vm->modules, OBJECT_VAL(osName), &osModVal)){
            ObjectModule* osMod = AS_MODULE(osModVal);
            ObjectList* argvList = newList(vm);
            push(vm, OBJECT_VAL(argvList));

            for(int i = 0; i < argc; i++){
                ObjectString* arg = copyString(vm, argv[i], (int)strlen(argv[i]));
                push(vm, OBJECT_VAL(arg));
                appendToList(vm, argvList, OBJECT_VAL(arg));
                pop(vm);    // arg
            }

            ObjectString* argvKey = copyString(vm, "argv", 4);
            push(vm, OBJECT_VAL(argvKey));
            tableSet(vm, &osMod->members, OBJECT_VAL(argvKey), OBJECT_VAL(argvList));
            pop(vm);    // argvKey
            pop(vm);    // argvList
        }
        pop(vm);    // osName
    }
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

static bool isValidKey(Value key){
    if(IS_NUM(key)){
        double n = AS_NUM(key);
        return n == (int64_t)n;
    }
    return true;
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

static bool bindListFunc(VM* vm, Value receiver, ObjectString* name){
    CFunc func = NULL;
    switch(name->length){
        case 3:
            if(memcmp(name->chars, "pop", 3) == 0)  func = list_pop;
            break;
        case 4:
            if(memcmp(name->chars, "push", 4) == 0) func = list_push;
            else if(memcmp(name->chars, "size", 4) == 0) func = list_size;
            break;
    }

    if(func){
        ObjectCFunc* cfuncObj = newCFunc(vm, func);
        ObjectBoundMethod* bound = newBoundMethod(vm, receiver, (Object*)cfuncObj);
        pop(vm);    // pop receiver
        push(vm, OBJECT_VAL(bound));
        return true;
    }

    return false;
}

static bool bindFileFunc(VM* vm, Value receiver, ObjectString* name){
    CFunc func = NULL;
    switch(name->length){
        case 4:
            if(memcmp(name->chars, "read", 4) == 0)    func = file_read;
            break;
        case 5:
            if(memcmp(name->chars, "close", 5) == 0)   func = file_close;
            else if(memcmp(name->chars, "write", 5) == 0)   func = file_write;
            break;
        case 8:
            if(memcmp(name->chars, "readLine", 8) == 0)    func = file_readLine;
            break;
    }

    if(func){
        ObjectCFunc* cfuncObj = newCFunc(vm, func);
        ObjectBoundMethod* bound = newBoundMethod(vm, receiver, (Object*)cfuncObj);
        pop(vm);    // pop receiver
        push(vm, OBJECT_VAL(bound));
        return true;
    }

    return false;
}

static bool bindStringFunc(VM* vm, Value receiver, ObjectString* name){
    CFunc func = NULL;
    if(name->length == 3){
        if(memcmp(name->chars, "len", 3) == 0) func = string_len;
        else if(memcmp(name->chars, "sub", 3) == 0) func = string_sub;
    }else if(name->length == 4){
        if(memcmp(name->chars, "trim", 4) == 0) func = string_trim;
        else if(memcmp(name->chars, "find", 4) == 0) func = string_find;
    }else if(name->length == 5){
        if(memcmp(name->chars, "upper", 5) == 0) func = string_upper;
        else if(memcmp(name->chars, "lower", 5) == 0) func = string_lower;
        else if(memcmp(name->chars, "split", 5) == 0) func = string_split;
    }else if(name->length == 7){
        if(memcmp(name->chars, "replace", 7) == 0) func = string_replace;
    }

    if(func){
        ObjectCFunc* cfuncObj = newCFunc(vm, func);
        ObjectBoundMethod* bound = newBoundMethod(vm, receiver, (Object*)cfuncObj);
        pop(vm); // pop receiver
        push(vm, OBJECT_VAL(bound));
        return true;
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
        [OP_MODULO]         = &&DO_OP_MODULO,

        [OP_RETURN]         = &&DO_OP_RETURN,
        [OP_DEFER]          = &&DO_OP_DEFER,
        [OP_DEFER_RETURN]   = &&DO_OP_DEFER_RETURN,
        [OP_SYSTEM]         = &&DO_OP_SYSTEM,
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
        [OP_DUP_2]          = &&DO_OP_DUP_2,
        [OP_SWAP_12]        = &&DO_OP_SWAP_12,
        [OP_SWAP]           = &&DO_OP_SWAP,

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

        [OP_BUILD_LIST]     = &&DO_OP_BUILD_LIST,
        [OP_FILL_LIST]      = &&DO_OP_FILL_LIST,
        [OP_BUILD_MAP]      = &&DO_OP_BUILD_MAP,
        [OP_INDEX_GET]      = &&DO_OP_INDEX_GET,
        [OP_INDEX_SET]      = &&DO_OP_INDEX_SET,
        [OP_SLICE]          = &&DO_OP_SLICE,
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
            return VM_RUNTIME_ERROR;
        }

        Value bVal = peek(vm, 0);
        Value aVal = peek(vm, 1);

        if(IS_NUM(aVal) && IS_NUM(bVal)){ 
            double b = AS_NUM(pop(vm));
            double a = AS_NUM(pop(vm));
            if(b == 0){
                runtimeError(vm, "Runtime error: Division by zero");
                return VM_RUNTIME_ERROR;
            }
            push(vm, NUM_VAL(a / b));
        }else if(IS_STRING(aVal) && IS_STRING(bVal)){
            ObjectString* bStr = AS_STRING(peek(vm, 0));
            ObjectString* aStr = AS_STRING(peek(vm, 1));

            #ifdef _WIN32
                #define PATH_SEP '\\'
                #define IS_SEP(c) ((c) == '\\' || (c) == '/')
            #else
                #define PATH_SEP '/'
                #define IS_SEP(c) ((c) == '/')
            #endif

            ObjectString* checkList[2] = {aStr, bStr};
            for(int j = 0; j < 2; j++){
                ObjectString* str = checkList[j];
                for(int i = 0; i < str->length; i++){
                    char c = str->chars[i];
                    bool invalid = false;
                    if(c < 32) invalid = true;
                    switch(c){
                        case '<': case '>': case '*': case '"': 
                        case '|': case '?': invalid = true; break;
                        case ':': 
                            #ifdef _WIN32
                            if (!(i == 1 && ((str->chars[0] >= 'a' && str->chars[0] <= 'z') || 
                                             (str->chars[0] >= 'A' && str->chars[0] <= 'Z')))) {
                                invalid = true;
                            }
                            #else
                            #endif
                            break;
                    }
                    if(invalid){
                        runtimeError(vm, "Path contains invalid characters.");
                        return VM_RUNTIME_ERROR;
                    }
                }
            }

            bool resetPath = false;
            #ifdef _WIN32
                if(bStr->length >= 2 && bStr->chars[1] == ':' && 
                   ((bStr->chars[0] >= 'a' && bStr->chars[0] <= 'z') || 
                    (bStr->chars[0] >= 'A' && bStr->chars[0] <= 'Z')
                )){
                    resetPath = true;
                }
                if(bStr->length > 0 && IS_SEP(bStr->chars[0])) resetPath = true; 
            #else
            #endif

            if(resetPath || aStr->length == 0){
                push(vm, OBJECT_VAL(bStr));
            }else if(bStr->length == 0){
                push(vm, OBJECT_VAL(aStr));
            }else{
                bool hasSepA = (aStr->length > 0 && IS_SEP(aStr->chars[aStr->length - 1]));
                bool hasSepB = (bStr->length > 0 && IS_SEP(bStr->chars[0]));

                size_t len = aStr->length + bStr->length - hasSepA - hasSepB + 1; 
                // add 1 for sep; it is the length of valid chars
                
                char* chars = (char*)reallocate(vm, NULL, 0, len + 1);  // add 1 for '\0'
                if(chars == NULL){
                    runtimeError(vm, "Memory allocation failed for path join.");
                    return VM_RUNTIME_ERROR;
                }

                memcpy(chars, aStr->chars, aStr->length);

                if(!hasSepA && !hasSepB){
                    chars[aStr->length] = PATH_SEP;
                    memcpy(chars + aStr->length + 1, bStr->chars, bStr->length);
                }else if(hasSepA && hasSepB){
                    memcpy(chars + aStr->length, bStr->chars + 1, bStr->length - 1);
                }else{
                    memcpy(chars + aStr->length, bStr->chars, bStr->length);
                }

                chars[len] = '\0';

                ObjectString* result = copyString(vm, chars, (int)len);
                reallocate(vm, chars, len + 1, 0);
                pop(vm);    
                pop(vm);
                push(vm, OBJECT_VAL(result));
            }
            #undef PATH_SEP
            #undef IS_SEP
        }else{
            runtimeError(vm, "Operands must be numbers or strings.");
            return VM_RUNTIME_ERROR;
        }

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

    DO_OP_MODULO:
    {
        if(vm->stackTop - vm->stack < 2){
            runtimeError(vm, "Stack underflow.");
            return VM_RUNTIME_ERROR;
        }

        if(!IS_NUM(peek(vm, 0)) || !IS_NUM(peek(vm, 1))){
            runtimeError(vm, "Operands must be numbers.");
            return VM_RUNTIME_ERROR;
        }

        double b = AS_NUM(pop(vm));
        double a = AS_NUM(pop(vm));
        push(vm, NUM_VAL(fmod(a, b)));
    } DISPATCH();

    DO_OP_POP:
    {
        pop(vm);
    } DISPATCH();

    DO_OP_DUP:
    {
        push(vm, peek(vm, 0));
    } DISPATCH();

    DO_OP_DUP_2:
    {
        if(vm->stackTop - vm->stack < 2){
            runtimeError(vm, "Stack underflow.");
            return VM_RUNTIME_ERROR;
        }

        Value b = peek(vm, 0);
        Value a = peek(vm, 1);
        push(vm, a);
        push(vm, b);
    } DISPATCH();

    DO_OP_SWAP_12:
    {
        if(vm->stackTop - vm->stack < 3){
            runtimeError(vm, "Stack underflow.");
            return VM_RUNTIME_ERROR;
        }

        Value val1 = peek(vm, 1);
        Value val2 = peek(vm, 2);
        vm->stackTop[-2] = val2;
        vm->stackTop[-3] = val1;
    } DISPATCH();

    DO_OP_SWAP:
    {
        Value a = pop(vm);
        Value b = pop(vm);
        push(vm, a);
        push(vm, b);
    } DISPATCH();

    DO_OP_PRINT:
    {
        printValue(pop(vm));
        printf("\n");
    } DISPATCH();

    DO_OP_DEFINE_GLOBAL:
    {
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[READ_BYTE()]);
        tableSet(vm, vm->curGlobal, OBJECT_VAL(name), peek(vm, 0));
        pop(vm);
    } DISPATCH();

    DO_OP_DEFINE_LGLOBAL:
    {
        uint16_t index = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
                         
        frame->ip += 2;
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[index]);
        tableSet(vm, vm->curGlobal, OBJECT_VAL(name), peek(vm, 0));
        pop(vm);
    } DISPATCH();

    DO_OP_GET_GLOBAL:
    {
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[READ_BYTE()]);
        Value value;
        if (!tableGet(vm, vm->curGlobal, OBJECT_VAL(name), &value)) {
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
        if(!tableGet(vm, vm->curGlobal, OBJECT_VAL(name), &value)){
            runtimeError(vm, "Undefined variable '%.*s'.", name->length, name->chars);
            return VM_RUNTIME_ERROR;
        }
        push(vm, value);
    } DISPATCH();

    DO_OP_SET_GLOBAL:
    {
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[READ_BYTE()]);
        if(tableSet(vm, vm->curGlobal, OBJECT_VAL(name), peek(vm, 0))){
            // empty bucket, undefined
            tableRemove(vm, vm->curGlobal, OBJECT_VAL(name));
            runtimeError(vm, "Undefined variable '%.*s'.", name->length, name->chars);
            return VM_RUNTIME_ERROR;
        }
    } DISPATCH();

    DO_OP_SET_LGLOBAL:
    {
        uint16_t index = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
        frame->ip += 2;
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[index]);
        if(tableSet(vm, vm->curGlobal, OBJECT_VAL(name), peek(vm, 0))){
            tableRemove(vm, vm->curGlobal, OBJECT_VAL(name));
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
        if(tableGet(vm, &vm->modules, OBJECT_VAL(path), &modVal)){
            pop(vm);
            push(vm, modVal);
            DISPATCH(); // next dispatch
        }

        char* source = readScript(path->chars);
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
        tableSet(vm, &vm->modules, OBJECT_VAL(path), OBJECT_VAL(module));

        pushGlobal(vm, &module->members);

        Value moduleValue = pop(vm);
        pop(vm);
        pop(vm);
        push(vm, moduleValue);
        
        if(!call(vm, closure, 0)){
            popGlobal(vm);
            return VM_RUNTIME_ERROR;
        }

        frame = &vm->frames[vm->frameCount - 1];

    } DISPATCH();

    DO_OP_LIMPORT:
    {
        uint16_t index = (uint16_t)(frame->ip[0] << 8 | frame->ip[1]);
        frame->ip += 2;
        ObjectString* path = AS_STRING(frame->closure->func->chunk.constants.values[index]);
        
        push(vm, OBJECT_VAL(path));

        Value modVal;
        if(tableGet(vm, &vm->modules, OBJECT_VAL(path), &modVal)){
            pop(vm);
            push(vm, modVal);
            DISPATCH(); // next dispatch
        }

        char* source = readScript(path->chars);
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
        tableSet(vm, &vm->modules, OBJECT_VAL(path), OBJECT_VAL(module));
        pop(vm);

        pushGlobal(vm, &module->members);

        pop(vm); 
        pop(vm); 
        push(vm, OBJECT_VAL(module));
        
        if(!call(vm, closure, 0)){
            popGlobal(vm);
            return VM_RUNTIME_ERROR;
        }

        frame = &vm->frames[vm->frameCount - 1];
    }

    DO_OP_GET_PROPERTY:
    {
        uint8_t constantIndex = READ_BYTE();
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[constantIndex]);
        Value receiver = peek(vm, 0);

        if(IS_STRING(receiver)){
            if(bindStringFunc(vm, receiver, name)){
                DISPATCH();
            }
            runtimeError(vm, "Undefined property '%s' on string.", name->chars);
            return VM_COMPILE_ERROR;
        }

        if(!IS_OBJECT(receiver)){
            runtimeError(vm, "Only modules and objects have properties.");
            return VM_RUNTIME_ERROR;
        }

        if(IS_LIST(receiver)){
            if(bindListFunc(vm, receiver, name)){
                DISPATCH();
            }
            runtimeError(vm, "Undefined property '%s' on list.", name->chars);
            return VM_RUNTIME_ERROR;
        }

        if(IS_FILE(receiver)){
            if(bindFileFunc(vm, receiver, name)){
                DISPATCH();
            }
            runtimeError(vm, "Undefined property '%s' on file.", name->chars);
            return VM_RUNTIME_ERROR;
        }

        if(IS_INSTANCE(receiver)){
            ObjectInstance* instance = AS_INSTANCE(receiver);

            Value value;
            if(tableGet(vm, &instance->fields, OBJECT_VAL(name), &value)){
                if(!checkAccess(vm, instance->klass, name)){
                    runtimeError(vm, "Cannot access private field '%s' of instance of '%s'.", name->chars, instance->klass->name->chars);
                    return VM_RUNTIME_ERROR;
                }
                pop(vm);
                push(vm, value);
                DISPATCH();
            }

            if(tableGet(vm, &instance->klass->methods, OBJECT_VAL(name), &value)){
                ObjectBoundMethod* bound = newBoundMethod(vm, receiver, (Object*)AS_CLOSURE(value));
                pop(vm);
                push(vm, OBJECT_VAL(bound));
                DISPATCH();
            }
            runtimeError(vm, "Undefined property '%s' on instance of '%s'.", name->chars, instance->klass->name->chars);
            return VM_RUNTIME_ERROR;
        }

        if(IS_MODULE(receiver)){
            ObjectModule* module = AS_MODULE(receiver);

            Value value;
            if(!tableGet(vm, &module->members, OBJECT_VAL(name), &value)){
                runtimeError(vm, "Undefined property '%s' on module '%s'.", name->chars, module->name->chars);
                return VM_RUNTIME_ERROR;
            }
            
            pop(vm);
            push(vm, value);
            DISPATCH();
        }

        return VM_RUNTIME_ERROR;
    } DISPATCH();

    DO_OP_GET_LPROPERTY:
    {
        uint16_t index = (uint16_t)(READ_BYTE() << 8);
        index |= READ_BYTE();   // avoid precedence error
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[index]);
        Value receiver = peek(vm, 0);

        if(IS_STRING(receiver)){
            if(bindStringFunc(vm, receiver, name)){
                DISPATCH();
            }
            runtimeError(vm, "Undefined property '%s' on string.", name->chars);
            return VM_COMPILE_ERROR;
        }

        if(!IS_OBJECT(receiver)){
            runtimeError(vm, "Only modules and objects have properties.");
            return VM_RUNTIME_ERROR;
        }

        if(IS_LIST(receiver)){
            if(bindListFunc(vm, receiver, name)){
                DISPATCH();
            }
            runtimeError(vm, "Undefined property '%s' on list.", name->chars);
            return VM_RUNTIME_ERROR;
        }

        if(IS_FILE(receiver)){
            if(bindFileFunc(vm, receiver, name)){
                DISPATCH();
            }
            runtimeError(vm, "Undefined property '%s' on file.", name->chars);
            return VM_RUNTIME_ERROR;
        }

        if(IS_INSTANCE(receiver)){
            ObjectInstance* instance = AS_INSTANCE(receiver);
            Value value;
            if(tableGet(vm, &instance->fields, OBJECT_VAL(name), &value)){
                if(!checkAccess(vm, instance->klass, name)){
                    runtimeError(vm, "Cannot access private field '%s' of instance of '%s'.", name->chars, instance->klass->name->chars);
                    return VM_RUNTIME_ERROR;
                }
                pop(vm);
                push(vm, value);
                DISPATCH();
            }

            if(tableGet(vm, &instance->klass->methods, OBJECT_VAL(name), &value)){
                ObjectBoundMethod* bound = newBoundMethod(vm, receiver, (Object*)AS_CLOSURE(value));
                pop(vm);
                push(vm, OBJECT_VAL(bound));
                DISPATCH();
            }
            runtimeError(vm, "Undefined property '%s' on instance of '%s'.", name->chars, instance->klass->name->chars);
            return VM_RUNTIME_ERROR;
        }

        if(IS_MODULE(receiver)){
            ObjectModule* module = AS_MODULE(receiver);
            Value value;
            if(!tableGet(vm, &module->members, OBJECT_VAL(name), &value)){
                runtimeError(vm, "Undefined property '%s' on module '%s'.", name->chars, module->name->chars);
                return VM_RUNTIME_ERROR;
            }
            
            pop(vm);
            push(vm, value);

            DISPATCH();
        }

        return VM_RUNTIME_ERROR;
    } DISPATCH();

    DO_OP_SET_PROPERTY:
    {
        uint8_t constantIndex = READ_BYTE();
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[constantIndex]);
        Value receiver = peek(vm, 1);

        if(!IS_OBJECT(receiver)){    // stack top is a value, -1 for object
            runtimeError(vm, "Only modules and instances have properties.");
            return VM_RUNTIME_ERROR;
        }

        if(IS_INSTANCE(receiver)){
            ObjectInstance* instance = AS_INSTANCE(receiver);
            Value dummy;
            if(!tableGet(vm, &instance->fields, OBJECT_VAL(name), &dummy)){
                runtimeError(vm, "Undefined property '%s' on instance of '%s'.", name->chars, instance->klass->name->chars);
                return VM_RUNTIME_ERROR;
            }
            if(!checkAccess(vm, instance->klass, name)){
                runtimeError(vm, "Cannot access private field '%s' of instance of '%s'.", name->chars, instance->klass->name->chars);
                return VM_RUNTIME_ERROR;
            }
            
            tableSet(vm, &instance->fields, OBJECT_VAL(name), peek(vm, 0));

            Value value = pop(vm);
            pop(vm);    // pop instance
            push(vm, value);
            DISPATCH();
        }

        if(IS_MODULE(receiver)){
            ObjectModule* module = AS_MODULE(receiver);
            tableSet(vm, &module->members, OBJECT_VAL(name), peek(vm, 0));

            Value value = pop(vm);
            pop(vm);    // pop module
            push(vm, value);
        }

        if(IS_LIST(receiver)){
            runtimeError(vm, "Cannot set properties on list.");
            return VM_RUNTIME_ERROR;
        }

        return VM_RUNTIME_ERROR;
    } DISPATCH();

    DO_OP_SET_LPROPERTY:
    {
        uint16_t index = (uint16_t)(READ_BYTE() << 8);
        index |= READ_BYTE();   // avoid precedence error
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[index]);
        Value receiver = peek(vm, 1);

        if(!IS_OBJECT(receiver)){    // stack top is a value, -1 for object
            runtimeError(vm, "Only modules and objects have properties.");
            return VM_RUNTIME_ERROR;
        }

        if(IS_INSTANCE(receiver)){
            ObjectInstance* instance = AS_INSTANCE(receiver);
            Value dummy;
            if(!tableGet(vm, &instance->fields, OBJECT_VAL(name), &dummy)){
                runtimeError(vm, "Undefined property '%s' on instance of '%s'.", name->chars, instance->klass->name->chars);
                return VM_RUNTIME_ERROR;
            }
            if(!checkAccess(vm, instance->klass, name)){
                runtimeError(vm, "Cannot access private field '%s' of instance of '%s'.", name->chars, instance->klass->name->chars);
                return VM_RUNTIME_ERROR;
            }
            
            tableSet(vm, &instance->fields, OBJECT_VAL(name), peek(vm, 0));

            Value value = pop(vm);
            pop(vm);    // pop instance
            push(vm, value);
            DISPATCH();
        } 

        if(IS_MODULE(receiver)){
            ObjectModule* module = AS_MODULE(receiver);
            tableSet(vm, &module->members, OBJECT_VAL(name), peek(vm, 0));

            Value value = pop(vm);
            pop(vm);    // pop module
            push(vm, value);
        }

        if(IS_LIST(receiver)){
            runtimeError(vm, "Cannot set properties on list.");
            return VM_RUNTIME_ERROR;
        }

        return VM_RUNTIME_ERROR;
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

        tableSet(vm, &klass->methods, OBJECT_VAL(name), method);
        pop(vm);
    } DISPATCH();

    DO_OP_LCLASS:
    {
        uint16_t index = (uint16_t)(READ_BYTE() << 8 | READ_BYTE());
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[index]);
        ObjectClass* klass = newClass(vm, name);
        push(vm, OBJECT_VAL(klass));
    } DISPATCH();

    DO_OP_LMETHOD:
    {
        uint16_t index = (uint16_t)(READ_BYTE() << 8 | READ_BYTE());
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

        tableSet(vm, &klass->methods, OBJECT_VAL(name), method);
        pop(vm);
    } DISPATCH();

    DO_OP_DEFINE_FIELD:
    {
        uint8_t index = READ_BYTE();
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[index]);
        Value defaultValue = pop(vm);
        ObjectClass* klass = AS_CLASS(peek(vm, 0));
        tableSet(vm, &klass->fields, OBJECT_VAL(name), defaultValue);
    } DISPATCH();

    DO_OP_DEFINE_LFIELD:
    {
        uint16_t index = (uint16_t)(READ_BYTE() << 8 | READ_BYTE());
        ObjectString* name = AS_STRING(frame->closure->func->chunk.constants.values[index]);
        Value defaultValue = pop(vm);
        ObjectClass* klass = AS_CLASS(peek(vm, 0));
        tableSet(vm, &klass->fields, OBJECT_VAL(name), defaultValue);
    } DISPATCH();

    DO_OP_BUILD_LIST:
    {
        uint8_t count = READ_BYTE();
        ObjectList* list = newList(vm);
        push(vm, OBJECT_VAL(list));    // avoid gc

        if(count > 0){
            list->items = (Value*)reallocate(vm, NULL, 0, sizeof(Value) * count);
            list->count = count;
            list->capacity = count;
            for(int i = count - 1; i >= 0; i--){
                list->items[i] = peek(vm, count - i);
            }
        }
        pop(vm);
        vm->stackTop -= count;
        push(vm, OBJECT_VAL(list));
    } DISPATCH();

    DO_OP_FILL_LIST:
    {
        Value cntVal = pop(vm);
        Value itemVal = pop(vm);
        if(!IS_NUM(cntVal)){
            runtimeError(vm, "List fill count must be a number.");
            return VM_RUNTIME_ERROR;
        }
        uint8_t count = (uint8_t)AS_NUM(cntVal);
        ObjectList* list = newList(vm);
        push(vm, OBJECT_VAL(list));    // avoid gc

        for(int i = 0; i < count; i++){
            appendToList(vm, list, itemVal);
        }
    } DISPATCH();

    DO_OP_INDEX_GET:
    {
        Value indexVal = pop(vm);
        Value target = pop(vm);

        if(IS_LIST(target)){
            if(!IS_NUM(indexVal)){
                runtimeError(vm, "List index must be a number.");
                return VM_RUNTIME_ERROR;
            }

            ObjectList* list = AS_LIST(target);
            int index = (int)AS_NUM(indexVal);

            if(index < 0){
                index += list->count;
            }

            if(index < 0 || index >= list->count){
                runtimeError(vm, "List index out of bounds.");
                return VM_RUNTIME_ERROR;
            }

            push(vm, list->items[index]);
        }else if(IS_MAP(target)){
            if(!isValidKey(indexVal)){
                runtimeError(vm, "Map key cannot be an invalid type.");
                return VM_RUNTIME_ERROR;
            }

            ObjectMap* map = AS_MAP(target);
            Value value;

            if(tableGet(vm, &map->table, indexVal, &value)){
                push(vm, value);
            }else{
                push(vm, NULL_VAL);
            }
        }else if(IS_STRING(target)){
            if(!IS_NUM(indexVal)){
                runtimeError(vm, "String index must be a number.");
                return VM_RUNTIME_ERROR;
            }

            ObjectString* str = AS_STRING(target);
            int index = (int)AS_NUM(indexVal);

            if(index < 0){
                index += str->length;
            }

            if(index < 0 || index >= str->length){
                runtimeError(vm, "String index out of bounds.");
                return VM_RUNTIME_ERROR;
            }

            char chars[2] = { str->chars[index], '\0' };
            push(vm, OBJECT_VAL(copyString(vm, chars, 1)));
        }else{
            runtimeError(vm, "Illegal index operation.");
            return VM_RUNTIME_ERROR;
        }

        
    } DISPATCH();

    DO_OP_INDEX_SET:
    {
        Value val = pop(vm);
        Value indexVal = pop(vm);
        Value target = pop(vm);

        if(IS_LIST(target)){
            if(!IS_NUM(indexVal)){
                runtimeError(vm, "List index must be a number.");
                return VM_RUNTIME_ERROR;
            }

            ObjectList* list = AS_LIST(target);
            int index = (int)AS_NUM(indexVal);

            if(index < 0){
                index += list->count;
            }

            if(index < 0 || index >= list->count){
                runtimeError(vm, "List index out of bounds.");
                return VM_RUNTIME_ERROR;
            }

            list->items[index] = val;
            push(vm, val);
        }else if(IS_MAP(target)){
            if(!isValidKey(indexVal)){
                runtimeError(vm, "Map key cannot be a floating point number.");
                return VM_RUNTIME_ERROR;
            }

            ObjectMap* map = AS_MAP(target);
            tableSet(vm, &map->table, indexVal, val);
            push(vm, val);
        }else{
            runtimeError(vm, "Illegal index operation.");
            return VM_RUNTIME_ERROR;
        }        
    } DISPATCH();

    DO_OP_SLICE:
    {
        Value stepVal = pop(vm);
        Value endVal = pop(vm);
        Value stVal = pop(vm);
        Value receiver = peek(vm, 0);

        if(!IS_STRING(receiver) && !IS_LIST(receiver)){
            runtimeError(vm, "Only strings and lists can be sliced");
            return VM_RUNTIME_ERROR;
        }

        int step = 1;
        if(!IS_NULL(stepVal)){
            if(!IS_NUM(stepVal)){
                runtimeError(vm, "Slice step must be a number.");
                return VM_RUNTIME_ERROR;
            }
            step = (int)AS_NUM(stepVal);
        }

        if(step == 0){
            runtimeError(vm, "Slice step cannot be zero.");
            return VM_RUNTIME_ERROR;
        }

        int length = IS_STRING(receiver) ? (int)AS_STRING(receiver)->length : AS_LIST(receiver)->count;

        int start;
        if(IS_NULL(stVal)){
            start = (step > 0) ? 0 : length - 1;
        }else{
            if(!IS_NUM(stVal)){
                runtimeError(vm, "Slice start must be a number.");
                return VM_RUNTIME_ERROR;
            }
            start = (int)AS_NUM(stVal);
            if(start < 0)   start += length;
            if(step > 0){
                if(start < 0)   start = 0;
                if(start > length)  start = length;
            }else{
                if(start < -1)  start = -1;
                if(start > length - 1)  start = length - 1;
            }
        }

        int end;
        if(IS_NULL(endVal)){
            end = (step > 0) ? length : -1;
        }else{
            if(!IS_NUM(endVal)){
                runtimeError(vm, "Slice end must be a number.");
                return VM_RUNTIME_ERROR;
            }
            end = (int)AS_NUM(endVal);
            if(end < 0) end += length;
            if(step > 0){
                if(end < 0) end = 0;
                if(end > length) end = length;
            } else {
                if(end < -1) end = -1;
                if(end > length) end = length;
            }
        }

        if(IS_STRING(receiver)){
            ObjectString* str = AS_STRING(receiver);
            
            int count = 0;
            if(step > 0){
                if(start < end){
                    count = (end - start + step - 1) / step;
                }
            }else{
                if (start > end) {
                    count = (start - end - step - 1) / (-step);
                }
            }

            if(count <= 0){
                pop(vm);
                push(vm, OBJECT_VAL(copyString(vm, "", 0)));
            }else{
                char* chars = (char*)malloc(count + 1);
                
                int dstIdx = 0;
                int srcIdx = start;
                for(int i = 0; i < count; i++){
                    chars[dstIdx++] = str->chars[srcIdx];
                    srcIdx += step;
                }
                chars[count] = '\0';
                
                ObjectString* result = takeString(vm, chars, count);
                pop(vm); // Pop receiver
                push(vm, OBJECT_VAL(result));
            }
        }else if(IS_LIST(receiver)){
            ObjectList* srcList = AS_LIST(receiver);
            
            int count = 0;
            if(step > 0){
                if(start < end){
                    count = (end - start + step - 1) / step;
                }
            }else{
                if(start > end){
                    count = (start - end - step - 1) / (-step);
                }
            }
            
            ObjectList* newListObj = newList(vm);
            push(vm, OBJECT_VAL(newListObj));

            if(count > 0){
                newListObj->items = (Value*)reallocate(vm, NULL, 0, sizeof(Value) * count);
                newListObj->capacity = count;
                newListObj->count = count;
                
                int srcIdx = start;
                for(int i = 0; i < count; i++){
                    newListObj->items[i] = srcList->items[srcIdx];
                    srcIdx += step;
                }
            }
            
            Value resultList = pop(vm); // NewList
            pop(vm); // Receiver
            push(vm, resultList);
        }
    } DISPATCH();

    DO_OP_BUILD_MAP:
    {
        uint8_t itemCnt = READ_BYTE();
        ObjectMap* map = newMap(vm);
        push(vm, OBJECT_VAL(map));

        for(int i = 0; i < itemCnt; i++){
            Value val = peek(vm, 2 * i + 1);
            Value key = peek(vm, 2 * i + 2);
            
            if(!isValidKey(key)){
                runtimeError(vm, "Map key cannot be invalid like floating point number.");
                return VM_RUNTIME_ERROR;
            }

            tableSet(vm, &map->table, key, val);
        }

        vm->stackTop[-1 - 2 * itemCnt] = vm->stackTop[-1];
        vm->stackTop -= 2 * itemCnt;
        
    } DISPATCH();

    DO_OP_SYSTEM:
    {
        if(!IS_STRING(peek(vm, 0))){
            runtimeError(vm, "Expect a string as system command.");
            return VM_RUNTIME_ERROR;
        }

        ObjectString* cmd = AS_STRING(pop(vm));
        int status = system(cmd->chars);
        push(vm, NUM_VAL((double)status));

        ObjectString* exitCodeKey = copyString(vm, "_exit_code", 10);
        tableSet(vm, vm->curGlobal, OBJECT_VAL(exitCodeKey), NUM_VAL((double)status));
    } DISPATCH();

    DO_OP_DEFER:
    {
        Value closureVal = pop(vm);
        ObjectClosure* closure = AS_CLOSURE(closureVal);

        if(frame->deferCnt >= MAX_DEFERS){
            runtimeError(vm, "Too many deferred functions.");
            return VM_RUNTIME_ERROR;
        }

        frame->defers[frame->deferCnt++] = closure;
    } DISPATCH();

    DO_OP_DEFER_RETURN:
    {
        closeUpvalues(vm, frame->slots);
        vm->frameCount--;
        vm->stackTop = frame->slots;

        frame = &vm->frames[vm->frameCount - 1];
    } DISPATCH();

    DO_OP_RETURN:
    {
        if(frame->deferCnt > 0){
            ObjectClosure* deferClosure = frame->defers[--frame->deferCnt];

            push(vm, OBJECT_VAL(deferClosure));
            if(!call(vm, deferClosure, 0)){
                return VM_RUNTIME_ERROR;
            }

            CallFrame* callerFrame = &vm->frames[vm->frameCount - 2];
            callerFrame->ip--;

            frame = &vm->frames[vm->frameCount - 1];

            DISPATCH();
        }

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
    frame->deferCnt = 0;

    return true;
}

static bool callValue(VM* vm, Value callee, int argCnt){
    if(IS_OBJECT(callee)){
        switch(OBJECT_TYPE(callee)){
            case OBJECT_CLASS:{
                ObjectClass* klass = AS_CLASS(callee);
                vm->stackTop[-argCnt - 1] = OBJECT_VAL(newInstance(vm, klass));
                Value initializer;
                if(tableGet(vm, &klass->methods, OBJECT_VAL(vm->initString), &initializer)){
                    if (!callValue(vm, initializer, argCnt)){
                        return false;
                    }
                }else if(argCnt != 0){  // no initializer found but got arguments
                    runtimeError(vm, "Expected 0 arguments but got %d.", argCnt);
                    return false;
                }
                return true;
            }
            case OBJECT_BOUND_METHOD:{
                ObjectBoundMethod* bound = AS_BOUND_METHOD(callee);
                vm->stackTop[-argCnt -1] = bound->receiver;
                Object* method = bound->method;
                if(method->type == OBJECT_CFUNC){
                    CFunc cfunc = AS_CFUNC(OBJECT_VAL(method));
                    Value result = cfunc(vm, argCnt, vm->stackTop - argCnt);
                    vm->stackTop -= argCnt + 1;
                    push(vm, result);
                    return true;
                }else{
                    return call(vm, AS_CLOSURE(OBJECT_VAL(method)), argCnt);
                }
            }
            case OBJECT_CLOSURE:
                return call(vm, AS_CLOSURE(callee), argCnt);
            case OBJECT_CFUNC:{
                CFunc cfunc = AS_CFUNC(callee);
                Value result = cfunc(vm, argCnt, vm->stackTop - argCnt);
                if(vm->stackTop == vm->stack){  
                    // stack is reseted by runtimeError
                    return false;
                }
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

void runtimeError(VM* vm, const char* format, ...){
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