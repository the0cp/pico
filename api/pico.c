#include "pico.h"

#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "global_env.h"
#include "object.h"
#include "value.h"
#include "vm.h"

struct PicoCall{
    VM* vm;
    int argCount;
    Value* args;
    Value result;
};

static PicoStatus mapInterpreterStatus(InterpreterStatus status){
    switch(status){
        case VM_OK:
            return PICO_STATUS_OK;
        case VM_COMPILE_ERROR:
            return PICO_STATUS_COMPILE_ERROR;
        case VM_RUNTIME_ERROR:
            return PICO_STATUS_RUNTIME_ERROR;
    }

    return PICO_STATUS_RUNTIME_ERROR;
}

static Value callHostFunc(VM* vm, int argCount, Value* args){
    Value callee = args[-1];
    // the callee is stored immediately before the first arg

    if(!IS_CFUNC(callee)){
        runtimeError(vm, "Invalid host function call.");
        return NULL_VAL;
    }

    ObjectCFunc* func = AS_CFUNC_OBJECT(callee);

    if(func->hostFunc == NULL){
        runtimeError(vm, "Host function callback is not available.");
        return NULL_VAL;
    }

    PicoCall call = {
        .vm = vm,
        .argCount = argCount,
        .args = args,
        .result = NULL_VAL
    };

    func->hostFunc(&call, func->userData);

    return call.result;
}

static bool isValidArgIndex(const PicoCall* call, int index){
    return call != NULL && index >= 0 && index < call->argCount;
}

static void setCallResult(PicoCall* call, Value value){
    if(call == NULL){
        return;
    }

    call->result = value;
}

PicoVM* pico_vm_create(void){
    PicoVM* vm = malloc(sizeof(*vm));

    if(vm == NULL){
        return NULL;
    }

    initVM(vm, 0, NULL);

    /*
     * A script embedded must not be allowed to terminate host process through os.exit().
     * This overrides the initialization of initVM().
    */

    vm->allowProcessExit = false;

    return vm;
}

void pico_vm_destroy(PicoVM* vm){
    if(vm == NULL){
        return;
    }

    freeVM(vm);
    free(vm);
}

PicoStatus pico_vm_eval(
    PicoVM* vm,
    const char* source,
    const char* source_name
){
    if(vm == NULL || source == NULL){
        return PICO_STATUS_INVALID_ARGUMENT;
    }

    if(source_name == NULL){
        source_name = "<embedded>";
    }

    InterpreterStatus status = interpret(vm, source, source_name);
    return mapInterpreterStatus(status);
}

int pico_call_arg_count(const PicoCall* call){
    if(call == NULL){
        return 0;
    }

    return call->argCount;
}

PicoValueType pico_call_arg_type(const PicoCall* call, int index){
    if(!isValidArgIndex(call, index)){
        return PICO_VALUE_OTHER;
    }

    Value value = call->args[index];

    if(IS_NULL(value)){
        return PICO_VALUE_NULL;
    }

    if(IS_BOOL(value)){
        return PICO_VALUE_BOOL;
    }

    if(IS_NUM(value)){
        return PICO_VALUE_NUMBER;
    }

    if(IS_STRING(value)){
        return PICO_VALUE_STRING;
    }

    return PICO_VALUE_OTHER;
}

bool pico_call_get_bool(const PicoCall* call, int index, bool* result){
    if(result == NULL || !isValidArgIndex(call, index)){
        return false;
    }

    Value value = call->args[index];

    if(!IS_BOOL(value)){
        return false;
    }

    *result = AS_BOOL(value);
    return true;
}

bool pico_call_get_number(const PicoCall* call, int index, double* result){
    if(result == NULL || !isValidArgIndex(call, index)){
        return false;
    }

    Value value = call->args[index];

    if(!IS_NUM(value)){
        return false;
    }

    *result = AS_NUM(value);
    return true;
}

const char* pico_call_get_string(const PicoCall* call, int index, size_t* length){
    if(!isValidArgIndex(call, index)){
        return NULL;
    }

    Value value = call->args[index];

    if(!IS_STRING(value)){
        return NULL;
    }

    ObjectString* string = AS_STRING(value);

    if(length != NULL){
        *length = string->length;
    }

    return string->chars;
}

void pico_call_return_null(PicoCall* call){
    setCallResult(call, NULL_VAL);
}

void pico_call_return_bool(PicoCall* call, bool value){
    setCallResult(call, BOOL_VAL(value));
}

void pico_call_return_number(PicoCall* call, double value){
    setCallResult(call, NUM_VAL(value));
}

void pico_call_return_string(PicoCall* call, const char* value, size_t length){
    if(call == NULL){
        return;
    }

    if(value == NULL){
        pico_call_error(call, "Host function returned a null string pointer.");
        return;
    }

    if(length > INT_MAX){
        pico_call_error(call, "Host function returned a string that is too large.");
        return;
    }

    ObjectString* string = copyString(call->vm, value, (int)length);

    setCallResult(call, OBJECT_VAL(string));
}

void pico_call_error(PicoCall* call, const char* message){
    if(call == NULL){
        return;
    }

    if(message == NULL){
        message = "Host function failed.";
    }

    runtimeError(call->vm, "%s", message);

    setCallResult(call, NULL_VAL);
}

PicoStatus pico_vm_register_native(PicoVM* vm, const char* name, PicoNativeFn function, void* user_data){
    if(vm == NULL || name == NULL || name[0] == '\0' || function == NULL){
        return PICO_STATUS_INVALID_ARGUMENT;
    }

    ObjectString* key = copyString(vm, name, (int)strlen(name));

    //Root the key while the function object and global table may allocate.
    push(vm, OBJECT_VAL(key));

    ObjectCFunc* native = newHostCFunc(vm, callHostFunc, function, user_data);

    push(vm, OBJECT_VAL(native));

    bool success = globalSetName(vm, &vm->globals, key, OBJECT_VAL(native));

    pop(vm);
    pop(vm);

    if(!success){
        return PICO_STATUS_RUNTIME_ERROR;
    }

    return PICO_STATUS_OK;
}

const char* pico_vm_last_error(const PicoVM* vm){
    if(vm == NULL || vm->lastError[0] == '\0'){
        return NULL;
    }

    return vm->lastError;
}

const char* pico_status_string(PicoStatus status){
    switch(status){
        case PICO_STATUS_OK:
            return "OK";
        case PICO_STATUS_COMPILE_ERROR:
            return "compile error";
        case PICO_STATUS_RUNTIME_ERROR:
            return "runtime error";
        case PICO_STATUS_INVALID_ARGUMENT:
            return "invalid argument";
    }

    return "unknown status";
}