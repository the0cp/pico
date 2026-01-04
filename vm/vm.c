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
#include "registry.h"

#ifdef DEBUG_TRACE
#include "debug.h"
#endif

#define R(n) (frame->base[(n)])
#define K(n) (frame->closure->func->chunk.constants.values[(n)])

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

static Value bindListFunc(VM* vm, Value receiver, ObjectString* name){
    CFunc func = NULL;
    switch(name->length){
        case 3:
            if(memcmp(name->chars, "pop", 3) == 0){
                func = list_pop;
            }
            break;
        case 4:
            if(memcmp(name->chars, "push", 4) == 0){
                func = list_push;
            }else if(memcmp(name->chars, "size", 4) == 0){
                func = list_size;
            }
            break;
    }

    if(func){
        ObjectCFunc* cfuncObj = newCFunc(vm, func);
        ObjectBoundMethod* bound = newBoundMethod(vm, receiver, (Object*)cfuncObj);
        return OBJECT_VAL(bound);
    }

    return NULL_VAL;
}

static bool bindFileFunc(VM* vm, Value receiver, ObjectString* name){
    CFunc func = NULL;
    switch(name->length){
        case 4:
            if(memcmp(name->chars, "read", 4) == 0){
                func = file_read;
            }
            break;
        case 5:
            if(memcmp(name->chars, "close", 5) == 0){
                func = file_close;
            }else if(memcmp(name->chars, "write", 5) == 0){
                func = file_write;
            }
            break;
        case 8:
            if(memcmp(name->chars, "readLine", 8) == 0){
                func = file_readLine;
            }
            break;
    }

    if(func){
        ObjectCFunc* cfuncObj = newCFunc(vm, func);
        ObjectBoundMethod* bound = newBoundMethod(vm, receiver, (Object*)cfuncObj);
        return OBJECT_VAL(bound);
    }

    return NULL_VAL;
}

static Value bindStringFunc(VM* vm, Value receiver, ObjectString* name){
    CFunc func = NULL;
    if(name->length == 3){
        if(memcmp(name->chars, "len", 3) == 0){
            func = string_len;
        }else if(memcmp(name->chars, "sub", 3) == 0){
            func = string_sub;
        }
    }else if(name->length == 4){
        if(memcmp(name->chars, "trim", 4) == 0){
            func = string_trim;
        }else if(memcmp(name->chars, "find", 4) == 0){
            func = string_find;
        }
    }else if(name->length == 5){
        if(memcmp(name->chars, "upper", 5) == 0){
            func = string_upper;
        }else if(memcmp(name->chars, "lower", 5) == 0){
            func = string_lower;
        }else if(memcmp(name->chars, "split", 5) == 0){
            func = string_split;
        }
    }else if(name->length == 7){
        if(memcmp(name->chars, "replace", 7) == 0){
            func = string_replace;
        }
    }

    if(func){
        ObjectCFunc* cfuncObj = newCFunc(vm, func);
        ObjectBoundMethod* bound = newBoundMethod(vm, receiver, (Object*)cfuncObj);
        return OBJECT_VAL(bound);
    }
    return NULL_VAL;
}

static InterpreterStatus run(VM* vm){
    CallFrame* frame = &vm->frames[vm->frameCount - 1];

    Instruction instruction;

    static void* dispatchTable[] = {
        [OP_MOVE]           = &&DO_OP_MOVE,
        [OP_LOADK]          = &&DO_OP_LOADK,
        [OP_LOADNULL]       = &&DO_OP_LOADNULL,
        [OP_LOADBOOL]       = &&DO_OP_LOADBOOL,

        [OP_GET_GLOBAL]     = &&DO_OP_GET_GLOBAL,
        [OP_SET_GLOBAL]     = &&DO_OP_SET_GLOBAL,
        [OP_GET_UPVAL]      = &&DO_OP_GET_UPVAL,
        [OP_SET_UPVAL]      = &&DO_OP_SET_UPVAL,

        [OP_GET_INDEX]      = &&DO_OP_GET_INDEX,
        [OP_SET_INDEX]      = &&DO_OP_SET_INDEX,
        [OP_GET_PROPERTY]   = &&DO_OP_GET_PROPERTY,
        [OP_SET_PROPERTY]   = &&DO_OP_SET_PROPERTY,

        [OP_ADD]            = &&DO_OP_ADD,
        [OP_SUB]            = &&DO_OP_SUB,
        [OP_MUL]            = &&DO_OP_MUL,
        [OP_DIV]            = &&DO_OP_DIV,
        [OP_MOD]            = &&DO_OP_MOD,
        [OP_NEG]            = &&DO_OP_NEG,
        [OP_NOT]            = &&DO_OP_NOT,

        [OP_EQ]             = &&DO_OP_EQ,
        [OP_LT]             = &&DO_OP_LT,
        [OP_LE]             = &&DO_OP_LE,

        [OP_JMP]            = &&DO_OP_JMP,
        [OP_CALL]           = &&DO_OP_CALL,
        [OP_RETURN]         = &&DO_OP_RETURN,

        [OP_CLOSURE]        = &&DO_OP_CLOSURE,
        [OP_CLOSE_UPVAL]    = &&DO_OP_CLOSE_UPVAL,

        [OP_CLASS]          = &&DO_OP_CLASS,
        [OP_METHOD]         = &&DO_OP_METHOD,

        [OP_IMPORT]         = &&DO_OP_IMPORT,

        [OP_PRINT]          = &&DO_OP_PRINT,
        
        [OP_DEFER]          = &&DO_OP_DEFER,
        [OP_SYSTEM]         = &&DO_OP_SYSTEM,

        [OP_TO_STRING]      = &&DO_OP_TO_STRING,

        [OP_JMP]            = &&DO_OP_JMP,

        [OP_CALL]           = &&DO_OP_CALL,

        [OP_IMPORT]         = &&DO_OP_IMPORT,

        [OP_CLASS]          = &&DO_OP_CLASS,
        [OP_METHOD]         = &&DO_OP_METHOD,

        [OP_BUILD_LIST]     = &&DO_OP_BUILD_LIST,
        [OP_INIT_LIST]      = &&DO_OP_INIT_LIST,
        [OP_FILL_LIST]      = &&DO_OP_FILL_LIST,
        [OP_BUILD_MAP]      = &&DO_OP_BUILD_MAP,
        [OP_SLICE]          = &&DO_OP_SLICE,
    };

    #ifdef DEBUG_TRACE
        #define DISPATCH() \
            do { \
                instruction = *frame->ip++; \
                dasmInstruction(&frame->closure->func->chunk, \
                    (int)(frame->ip - frame->closure->func->chunk.code - 1)); \
                goto *dispatchTable[GET_OPCODE(instruction)]; \
            } while (0)
    #else
        #define DISPATCH() \
            do { \
                instruction = *frame->ip++; \
                goto *dispatchTable[GET_OPCODE(instruction)]; \
            } while (0)
    #endif

    #define BI_OP(type, op) \
        do { \
            Value b = R(GET_ARG_B(instruction)); \
            Value c = R(GET_ARG_C(instruction)); \
            if(!IS_NUM(b) || !IS_NUM(c)){ \
                runtimeError(vm, "Operands must be numbers."); \
                return VM_RUNTIME_ERROR; \
            } \
            R(GET_ARG_A(instruction)) = type(AS_NUM(b) op AS_NUM(c)); \
        } while(false)

    DISPATCH();

    DO_OP_MOVE:
    {
        R(GET_ARG_A(instruction)) = R(GET_ARG_B(instruction));
    } DISPATCH();

    DO_OP_LOADK:
    {
        R(GET_ARG_A(instruction)) = K(GET_ARG_Bx(instruction));
    } DISPATCH();

    DO_OP_LOADNULL:
    {
        int a = GET_ARG_A(instruction);
        int b = GET_ARG_B(instruction);
        for(int i = 0; i <= b; i++){
            R(a + i) = NULL_VAL;
        }
    } DISPATCH();

    DO_OP_LOADBOOL:
    {
        R(GET_ARG_A(instruction)) = BOOL_VAL(GET_ARG_B(instruction));
        if(GET_ARG_C(instruction)) frame->ip++; // skip next instruction if C != 0
    } DISPATCH();

    DO_OP_GET_GLOBAL:
    {
        ObjectString* name = AS_STRING(K(GET_ARG_Bx(instruction)));
        Value value;
        if(!tableGet(vm, vm->curGlobal, OBJECT_VAL(name), &value)){
            runtimeError(vm, "Undefined global variable '%s'.", name->chars);
            return VM_RUNTIME_ERROR;
        }
        R(GET_ARG_A(instruction)) = value;
    } DISPATCH();

    DO_OP_SET_GLOBAL:
    {
        ObjectString* name = AS_STRING(K(GET_ARG_Bx(instruction)));
        tableSet(vm, vm->curGlobal, OBJECT_VAL(name), R(GET_ARG_A(instruction)));
    } DISPATCH();

    DO_OP_GET_UPVAL:
    {
        R(GET_ARG_A(instruction)) = *frame->closure->upvalues[GET_ARG_B(instruction)]->location;
    } DISPATCH();

    DO_OP_SET_UPVAL:
    {
        *frame->closure->upvalues[GET_ARG_B(instruction)]->location = R(GET_ARG_A(instruction));
    } DISPATCH();

    DO_OP_GET_INDEX:
    {
        Value val = R(GET_ARG_B(instruction));
        Value key = R(GET_ARG_C(instruction));
        Value result = NULL_VAL;

        if(IS_LIST(val)){
            if(!IS_NUM(key)){
                runtimeError(vm, "List index must be a number.");
                return VM_RUNTIME_ERROR;
            }

            ObjectList* list = AS_LIST(val);

            int index = (int)AS_NUM(key);
            if(index < 0){
                index += list->count;
            }
            if(index < 0 || index >= list->count){
                runtimeError(vm, "List index out of range.");
                return VM_RUNTIME_ERROR;
            }
            result = list->items[index];
        }else if(IS_MAP(val)){
            ObjectMap* map = AS_MAP(val);
            if(!isValidKey(key)){
                runtimeError(vm, "Invalid map key.");
                return VM_RUNTIME_ERROR;
            }
            if(!tableGet(vm, &map->table, key, &result)){
                runtimeError(vm, "Key not found in map.");
                return VM_RUNTIME_ERROR;
            }
        }else if(IS_STRING(val)){
            if(!IS_NUM(key)){
                runtimeError(vm, "String index must be a number.");
                return VM_RUNTIME_ERROR;
            }

            ObjectString* str = AS_STRING(val);

            int index = (int)AS_NUM(key);
            if(index < 0){
                index += str->length;
            }
            if(index < 0 || index >= str->length){
                runtimeError(vm, "String index out of range.");
                return VM_RUNTIME_ERROR;
            }
            char chars[2] = {str->chars[index], '\0'};
            ObjectString* charStr = copyString(vm, chars, 1);
            result = OBJECT_VAL(charStr);
        }else{
            runtimeError(vm, "Only list and map type support indexing.");
            return VM_RUNTIME_ERROR;
        }

        R(GET_ARG_A(instruction)) = result;
    } DISPATCH();

    DO_OP_SET_INDEX:
    {
        Value val = R(GET_ARG_B(instruction));
        Value key = R(GET_ARG_C(instruction));
        Value newVal = R(GET_ARG_A(instruction));

        if(IS_LIST(val)){
            if(!IS_NUM(key)){
                runtimeError(vm, "List index must be a number.");
                return VM_RUNTIME_ERROR;
            }

            ObjectList* list = AS_LIST(val);

            int index = (int)AS_NUM(key);
            if(index < 0){
                index += list->count;
            }
            if(index < 0 || index >= list->count){
                runtimeError(vm, "List index out of range.");
                return VM_RUNTIME_ERROR;
            }
            list->items[index] = newVal;
        }else if(IS_MAP(val)){
            ObjectMap* map = AS_MAP(val);
            if(!isValidKey(key)){
                runtimeError(vm, "Invalid map key.");
                return VM_RUNTIME_ERROR;
            }

            if(!tableSet(vm, &map->table, key, newVal)){
                runtimeError(vm, "Out of memory.");
                return VM_RUNTIME_ERROR;
            }
        }else{
            runtimeError(vm, "Only map type support key-value assignment.");
            return VM_RUNTIME_ERROR;
        }
    } DISPATCH();

    DO_OP_GET_PROPERTY:
    {
        Value instanceVal = R(GET_ARG_B(instruction));
        ObjectString* key = AS_STRING(K(GET_ARG_C(instruction)));
        Value result = NULL_VAL;

        if(IS_INSTANCE(instanceVal)){
            ObjectInstance* instance = AS_INSTANCE(instanceVal);
            if(tableGet(vm, &instance->fields, OBJECT_VAL(key), &result)){
                if(!checkAccess(vm, instance->klass, key)){
                    runtimeError(vm, "Cannot access private field '%s'.", key->chars);
                    return VM_RUNTIME_ERROR;
                }
                R(GET_ARG_A(instruction)) = result;
            }else{
                Value methodVal;
                if(tableGet(vm, &instance->klass->methods, OBJECT_VAL(key), &methodVal)){
                    ObjectBoundMethod* bound = newBoundMethod(vm, instanceVal, AS_OBJECT(methodVal));
                    R(GET_ARG_A(instruction)) = OBJECT_VAL(bound);
                }else{
                    runtimeError(vm, "Undefined field '%s'.", key->chars);
                    return VM_RUNTIME_ERROR;
                }
            }
        }else if(IS_MODULE(instanceVal)){
            ObjectModule* module = AS_MODULE(instanceVal);
            if(!tableGet(vm, &module->members, OBJECT_VAL(key), &result)){
                runtimeError(vm, "Module has no member '%s'.", key->chars);
                return VM_RUNTIME_ERROR;
            }
            R(GET_ARG_A(instruction)) = result;
        }else{
            Value bound = NULL_VAL;
            
            if(IS_STRING(instanceVal)) bound = bindStringFunc(vm, instanceVal, key);
            else if(IS_LIST(instanceVal)) bound = bindListFunc(vm, instanceVal, key);
            else if(IS_FILE(instanceVal)) bound = bindFileFunc(vm, instanceVal, key);
            
            if(!IS_NULL(bound)){
                R(GET_ARG_A(instruction)) = pop(vm);
            }else{
                runtimeError(vm, "Property '%s' not found on object.", key->chars); 
                return VM_RUNTIME_ERROR;
            }
        }
        
    } DISPATCH();

    DO_OP_SET_PROPERTY:
    {
        Value instanceVal = R(GET_ARG_B(instruction));
        ObjectString* key = AS_STRING(K(GET_ARG_C(instruction)));
        Value newVal = R(GET_ARG_A(instruction));

        if(IS_INSTANCE(instanceVal)){
            ObjectInstance* instance = AS_INSTANCE(instanceVal);
            if(!checkAccess(vm, instance->klass, key)){
                runtimeError(vm, "Cannot access private field '%s'.", key->chars);
                return VM_RUNTIME_ERROR;
            }
            tableSet(vm, &instance->fields, OBJECT_VAL(key), newVal);
        }else if(IS_MODULE(instanceVal)){
            ObjectModule* module = AS_MODULE(instanceVal);
            tableSet(vm, &module->members, OBJECT_VAL(key), newVal);
        }else{
            runtimeError(vm, "Only instance and module support field assignment.");
            return VM_RUNTIME_ERROR;
        }
    } DISPATCH();

    DO_OP_TO_STRING:
    {
        int a = GET_ARG_A(instruction);
        int b = GET_ARG_B(instruction);
        Value val = R(b);

        if(IS_STRING(val)){
            R(a) = val;
        }else{
            ObjectString* str = toString(vm, val);
            R(a) = OBJECT_VAL(str);
        }
    } DISPATCH();

    DO_OP_NULL: push(vm, NULL_VAL); DISPATCH();

    DO_OP_TRUE: push(vm, BOOL_VAL(true)); DISPATCH();

    DO_OP_FALSE: push(vm, BOOL_VAL(false)); DISPATCH();

    DO_OP_NOT: {
        Value b = R(GET_ARG_B(instruction));
        R(GET_ARG_A(instruction)) = BOOL_VAL(!isTruthy(b));
    } DISPATCH();

    DO_OP_EQ:
    {
        Value b = R(GET_ARG_B(instruction));
        Value c = R(GET_ARG_C(instruction));
        int a = GET_ARG_A(instruction);
        if(isEqual(b, c) != a){
            frame->ip++;
        }
    } DISPATCH();

    DO_OP_LT:
    {
        Value b = R(GET_ARG_B(instruction));
        Value c = R(GET_ARG_C(instruction));
        int expect = GET_ARG_A(instruction);

        if(!IS_NUM(b) || !IS_NUM(c)){
            runtimeError(vm, "Operands must be numbers.");
            return VM_RUNTIME_ERROR;
        }

        if((AS_NUM(b) < AS_NUM(c)) != expect){
            frame->ip++;
        }
    } DISPATCH();

    DO_OP_LE:
    {
        Value b = R(GET_ARG_B(instruction));
        Value c = R(GET_ARG_C(instruction));
        int expect = GET_ARG_A(instruction);

        if(!IS_NUM(b) || !IS_NUM(c)){
            runtimeError(vm, "Operands must be numbers.");
            return VM_RUNTIME_ERROR;
        }

        if((AS_NUM(b) <= AS_NUM(c)) != expect){
            frame->ip++;
        }
    } DISPATCH();

    DO_OP_ADD: 
    {
        Value b = R(GET_ARG_B(instruction));
        Value c = R(GET_ARG_C(instruction));

        if(IS_NUM(b) && IS_NUM(c)){
            double result = AS_NUM(b) + AS_NUM(c);
            R(GET_ARG_A(instruction)) = NUM_VAL(result);
        }else if(IS_STRING(b) && IS_STRING(c)){
            ObjectString* bStr = AS_STRING(b);
            ObjectString* cStr = AS_STRING(c);

            size_t len = bStr->length + cStr->length;
            char* chars = (char*)reallocate(vm, NULL, 0, len + 1);  // add 1 for '\0'
            if(chars == NULL){
                runtimeError(vm, "Memory allocation failed for string concatenation.");
                return VM_RUNTIME_ERROR;
            }
            memcpy(chars, bStr->chars, bStr->length);
            memcpy(chars + bStr->length, cStr->chars, cStr->length);
            chars[len] = '\0';
            R(GET_ARG_A(instruction)) = OBJECT_VAL(copyString(vm, chars, (int)len));
        }else{
            runtimeError(vm, "Operands must be two numbers or two strings.");
            return VM_RUNTIME_ERROR;
        }
    } DISPATCH();

    DO_OP_SUB: BI_OP(NUM_VAL, -); DISPATCH();

    DO_OP_MUL: BI_OP(NUM_VAL, *); DISPATCH();

    DO_OP_DIV:
    {
        Value b = R(GET_ARG_B(instruction));
        Value c = R(GET_ARG_C(instruction));

        if(IS_NUM(b) && IS_NUM(c)){ 
            double v = AS_NUM(c);
            if(v == 0){
                runtimeError(vm, "Runtime error: Division by zero");
                return VM_RUNTIME_ERROR;
            }
            R(GET_ARG_A(instruction)) = NUM_VAL(AS_NUM(b) / v);
        }else if(IS_STRING(b) && IS_STRING(c)){
            ObjectString* aStr = AS_STRING(b);
            ObjectString* bStr = AS_STRING(c);

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
                if(bStr->length > 0 && IS_SEP(bStr->chars[0])) resetPath = true;
            #endif

            if(resetPath || aStr->length == 0){
                R(GET_ARG_A(instruction)) = OBJECT_VAL(bStr);
            }else if(bStr->length == 0){
                R(GET_ARG_A(instruction)) = OBJECT_VAL(aStr);
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

                R(GET_ARG_A(instruction)) = OBJECT_VAL(copyString(vm, chars, (int)len));
            }
            #undef PATH_SEP
            #undef IS_SEP
        }else{
            runtimeError(vm, "Operands must be numbers or strings.");
            return VM_RUNTIME_ERROR;
        }

    } DISPATCH();

    DO_OP_NEG:
    {
        Value b = R(GET_ARG_B(instruction));
        if(!IS_NUM(b)){
            runtimeError(vm, "Operand must be a number.");
            return VM_RUNTIME_ERROR;
        }
        R(GET_ARG_A(instruction)) = NUM_VAL(-AS_NUM(b));
    } DISPATCH();

    DO_OP_MOD:
    {
        Value b = R(GET_ARG_B(instruction));
        Value c = R(GET_ARG_C(instruction));
        if(!IS_NUM(b) || !IS_NUM(c)){
            runtimeError(vm, "Operands must be numbers.");
            return VM_RUNTIME_ERROR;
        }
        double divisor = AS_NUM(c);
        if(divisor == 0){
            runtimeError(vm, "Runtime error: Division by zero");
            return VM_RUNTIME_ERROR;
        }
        R(GET_ARG_A(instruction)) = NUM_VAL(fmod(AS_NUM(b), divisor));
    } DISPATCH();

    DO_OP_PRINT:
    {
        int a = GET_ARG_A(instruction);
        printValue(R(a));
        printf("\n");
    } DISPATCH();

    DO_OP_JMP:
    {
        int sBx = GET_ARG_sBx(instruction);
        frame->ip += sBx;
    } DISPATCH();

    DO_OP_CALL:
    {
        int a = GET_ARG_A(instruction);
        int b = GET_ARG_B(instruction);

        Value callee = R(a);
        int argCount = b - 1;
        
        vm->stackTop = &R(a + b);

        if(!callValue(vm, callee, argCount)){
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

    DO_OP_CLOSURE:
    {
        int a = GET_ARG_A(instruction);
        int bx = GET_ARG_Bx(instruction);
        ObjectFunc* func = AS_FUNC(K(bx));
        ObjectClosure* closure = newClosure(vm, func);
        if(closure == NULL){
            runtimeError(vm, "Out of memory creating closure");
            return VM_RUNTIME_ERROR;
        }
        R(a) = OBJECT_VAL(closure);

        for(int i = 0; i < closure->upvalueCnt; i++){
            Instruction nextInstruction = *frame->ip++;
            int isLocal = GET_ARG_B(nextInstruction);
            int index = GET_ARG_C(nextInstruction);
            if(isLocal){
                closure->upvalues[i] = captureUpvalue(vm, &R(index));
            }else{
                closure->upvalues[i] = frame->closure->upvalues[index];
            }
        }
    } DISPATCH();

    DO_OP_CLOSE_UPVAL:
    {
        int a = GET_ARG_A(instruction);
        closeUpvalues(vm, &R(a));
    } DISPATCH();

    DO_OP_CLASS:
    {
        int a = GET_ARG_A(instruction);
        int bx = GET_ARG_Bx(instruction);
        ObjectString* name = AS_STRING(K(bx));
        ObjectClass* klass = newClass(vm, name);
        if(klass == NULL){
            runtimeError(vm, "Out of memory creating class.");
            return VM_RUNTIME_ERROR;
        }
        R(a) = OBJECT_VAL(klass);
    } DISPATCH();

    DO_OP_METHOD:
    {
        int a = GET_ARG_A(instruction);
        int b = GET_ARG_B(instruction);
        int c = GET_ARG_C(instruction);

        ObjectClass* klass = AS_CLASS(R(a));
        ObjectString* name = AS_STRING(K(b));
        Value method = R(c);

        AS_CLOSURE(method)->func->fieldOwner = klass;
        tableSet(vm, &klass->methods, OBJECT_VAL(name), method);
    } DISPATCH();

    DO_OP_BUILD_LIST:
    {
        int a = GET_ARG_A(instruction);
        int size = GET_ARG_B(instruction);

        ObjectList* list = newList(vm);
        
        if(size > 0){
            list->items = (Value*)reallocate(vm, NULL, 0, sizeof(Value) * size);
            list->capacity = size;
        }
        
        R(a) = OBJECT_VAL(list);
    } DISPATCH();

    DO_OP_INIT_LIST:
    {
        int a = GET_ARG_A(instruction);
        int startReg = GET_ARG_B(instruction);
        int count = GET_ARG_C(instruction);

        Value listVal = R(a);
        if(!IS_LIST(listVal)){
            runtimeError(vm, "Operand must be a list.");
            return VM_RUNTIME_ERROR;
        }
        
        ObjectList* list = AS_LIST(listVal);

        if(count > 0){
            int needed = list->count + count;
            
            if(list->capacity < needed){
                int oldCapacity = list->capacity;
                int newCapacity = oldCapacity;
                
                while(newCapacity < needed){
                    newCapacity = GROW_CAPACITY(newCapacity);
                }
                
                list->items = (Value*)reallocate(vm, list->items, 
                    sizeof(Value) * oldCapacity, 
                    sizeof(Value) * newCapacity);
                list->capacity = newCapacity;
            }

            for(int k = 0; k < count; k++){
                list->items[list->count++] = R(startReg + k);
            }
        }
    } DISPATCH();

    DO_OP_FILL_LIST:
    {
        int a = GET_ARG_A(instruction);
        Value item = R(GET_ARG_B(instruction));
        Value countVal = R(GET_ARG_C(instruction));

        if(!IS_NUM(countVal)){
            runtimeError(vm, "List fill count must be a number.");
            return VM_RUNTIME_ERROR;
        }

        int count = (int)AS_NUM(countVal);
        if(count < 0){
            runtimeError(vm, "List fill count cannot be negative.");
            return VM_RUNTIME_ERROR;
        }

        ObjectList* list = newList(vm);
        
        if(count > 0){
            list->items = (Value*)reallocate(vm, NULL, 0, sizeof(Value) * count);
            list->capacity = count;
            list->count = count;
            
            for(int k = 0; k < count; k++){
                list->items[k] = item;
            }
        }
        
        R(a) = OBJECT_VAL(list);
    } DISPATCH();

    // R(A) = slice(R(B), start=R(C), end=R(C+1), step=R(C+2))
    // obj[start:end:step]
    DO_OP_SLICE: {
        int a = GET_ARG_A(instruction);
        int b = GET_ARG_B(instruction);
        int c = GET_ARG_C(instruction);

        Value receiver = R(b);
        Value startVal = R(c);
        Value endVal   = R(c + 1);
        Value stepVal  = R(c + 2);

        int length = 0;
        if(IS_STRING(receiver)){
            length = AS_STRING(receiver)->length;
        }else if (IS_LIST(receiver)){
            length = AS_LIST(receiver)->count;
        }else{
            runtimeError(vm, "Only lists and strings can be sliced.");
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

        int start;
        if(IS_NULL(startVal)){
            start = (step > 0) ? 0 : length - 1;
        }else{
            if(!IS_NUM(startVal)){
                runtimeError(vm, "Slice start must be a number."); 
                return VM_RUNTIME_ERROR; 
            }
            start = (int)AS_NUM(startVal);

            if(start < 0){
                start += length;
            }

            if(start < 0){
                #ifdef DEBUG_TRACE
                fprintf(stderr, "[Warn] Slice start %d underflow, clamped to %d.\n", originalStart, (step > 0) ? 0 : -1);
                #endif
                start = (step > 0) ? 0 : -1; 
            }else if(start >= length){
                #ifdef DEBUG_TRACE
                fprintf(stderr, "[Warn] Slice start %d overflow, clamped to %d.\n", originalStart, (step > 0) ? length : length - 1);
                #endif
                start = (step > 0) ? length : length - 1;
            }
        }

        int end;
        if(IS_NIL(endVal)){
            end = (step > 0) ? length : -1;
        }else{
            if(!IS_NUM(endVal)){
                runtimeError(vm, "Slice end must be a number."); 
                return VM_RUNTIME_ERROR; 
            }
            end = (int)AS_NUM(endVal);

            if(end < 0){
                end += length;
            }

            if(end < 0){
                #ifdef DEBUG_TRACE
                fprintf(stderr, "[Warn] Slice end %d underflow, clamped to -1.\n", originalEnd);
                #endif
                end = -1;
            }else if(end > length){
                #ifdef DEBUG_TRACE
                fprintf(stderr, "[Warn] Slice end %d overflow, clamped to %d.\n", originalEnd, length);
                #endif
                end = length;
            }
        }

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

        if(IS_STRING(receiver)){
            ObjectString* str = AS_STRING(receiver);
            if(count <= 0){
                R(a) = OBJECT_VAL(copyString(vm, "", 0));
            }else{
                char* chars = (char*)reallocate(vm, NULL, 0, count + 1);
                int destIdx = 0;
                int current = start;
                
                for(int k = 0; k < count; k++){
                    chars[destIdx++] = str->chars[current];
                    current += step;
                }
                chars[count] = '\0';
                R(a) = OBJECT_VAL(takeString(vm, chars, count));
            }
        }else{
            // List
            ObjectList* list = AS_LIST(receiver);
            ObjectList* newListObj = newList(vm);
            
            if(count > 0){
                newListObj->items = (Value*)reallocate(vm, NULL, 0, sizeof(Value) * count);
                newListObj->capacity = count;
                newListObj->count = count;
                
                int destIdx = 0;
                int current = start;
                
                for(int k = 0; k < count; k++){
                    newListObj->items[destIdx++] = list->items[current];
                    current += step;
                }
            }
            R(a) = OBJECT_VAL(newListObj);
        }

    } DISPATCH();

    DO_OP_BUILD_MAP:
    {
        int a = GET_ARG_A(instruction);
        R(a) = OBJECT_VAL(newMap(vm));        
    } DISPATCH();

    DO_OP_SYSTEM:
    {
        int a = GET_ARG_A(instruction);
        int b = GET_ARG_B(instruction);
        
        Value cmd = R(b);
        if(!IS_STRING(cmd)){
            runtimeError(vm, "System command expected to be a string.");
            return VM_RUNTIME_ERROR;
        }
        
        int status = system(AS_STRING(cmd)->chars);
        Value statusVal = NUM_VAL((double)status);
        R(a) = statusVal;

        ObjectString* exitKey = copyString(vm, "_exit_code", 10);
        tableSet(vm, vm->curGlobal, OBJECT_VAL(exitKey), statusVal);
    } DISPATCH();

    DO_OP_DEFER:
    {
        Value closureVal = R(GET_ARG_A(instruction));
        if(frame->deferCnt >= MAX_DEFERS){
            runtimeError(vm, "Exceeded maximum defer count (%d).", MAX_DEFERS);
            return VM_RUNTIME_ERROR;
        }
        if(!IS_CLOSURE(closureVal)){
            runtimeError(vm, "Defer operand must be a closure.");
            return VM_RUNTIME_ERROR;
        }
        frame->defers[frame->deferCnt++] = AS_CLOSURE(closureVal);
    } DISPATCH();

    DO_OP_RETURN:
    {
        int a = GET_ARG_A(instruction);
        int b = GET_ARG_B(instruction);

        if(frame->deferCnt > 0){
            ObjectClosure* deferClosure = frame->defers[--frame->deferCnt];
            call(vm, deferClosure, 0);
            frame = &vm->frames[vm->frameCount - 1];
            DISPATCH();
            frame = &vm->frames[vm->frameCount - 2];
            frame->ip--;
            DISPATCH();
        }

        closeUpvalues(vm, &R(0));

        Value result = (b > 1) ? R(a) : NULL_VAL;

        vm->frameCount--;

        if(vm->frameCount == 0){
            pop(vm);
            return VM_OK;
        }

        vm->stackTop = frame->base;

        frame = &vm->frames[vm->frameCount - 1];

        Instruction callerInstance = frame->ip[-1];
        int callerA = GET_ARG_A(callerInstance);
        int callerC = GET_ARG_C(callerInstance);

        if(callerC > 1){
            R(callerA) = result;
        }
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
    frame->base = vm->stackTop - argCnt - 1;   // -1 to skip func self
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
                    if (!call(vm, AS_CLOSURE(initializer), argCnt)){
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