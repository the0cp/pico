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

static bool toInternalValue(const PicoValue* publicValue, Value* internalValue){
    if(publicValue == NULL || internalValue == NULL){
        return false;
    }

    switch(publicValue->type){
        case PICO_VALUE_NULL:
            *internalValue = NULL_VAL;
            return true;
        case PICO_VALUE_BOOL:
            *internalValue = BOOL_VAL(publicValue->as.boolean);
            return true;
        case PICO_VALUE_NUMBER:
            *internalValue = NUM_VAL(publicValue->as.number);
            return true;
        case PICO_VALUE_STRING:
        case PICO_VALUE_OTHER:
        default:
            return false;
    }

    return false;
}

static bool fromInternalValue(Value internalValue, PicoValue* publicValue){
    if(publicValue == NULL){
        return false;
    }

    if(IS_NULL(internalValue)){
        *publicValue = pico_value_null();
        return true;
    }

    if(IS_BOOL(internalValue)){
        *publicValue = pico_value_bool(AS_BOOL(internalValue));
        return true;
    }

    if(IS_NUM(internalValue)){
        *publicValue = pico_value_number(AS_NUM(internalValue));
        return true;
    }

    publicValue->type = PICO_VALUE_OTHER;
    return false;
}

static PicoStatus setApiError(PicoVM* vm, PicoStatus status, const char* message){
    if(vm != NULL && message != NULL){
        snprintf(vm->lastError, sizeof(vm->lastError), "%s", message);
    }

    return status;
}

PicoValue pico_value_null(void){
    PicoValue value = {
        .type = PICO_VALUE_NULL
    };

    return value;
}

PicoValue pico_value_bool(bool value){
    PicoValue result = {
        .type = PICO_VALUE_BOOL,
        .as.boolean = value
    };

    return result;
}

PicoValue pico_value_number(double value){
    PicoValue result = {
        .type = PICO_VALUE_NUMBER,
        .as.number = value
    };

    return result;
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

void pico_vm_set_output(PicoVM* vm, PicoWriteFunc func, void* userData){
    if(vm == NULL){
        return;
    }

    vm->output.write = func;
    vm->output.userData = userData;
}

void pico_vm_set_error_output(PicoVM* vm, PicoWriteFunc func, void* userData){
    if(vm == NULL){
        return;
    }

    vm->errOutput.write = func;
    vm->errOutput.userData = userData;
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

PicoStatus pico_vm_register_native(PicoVM* vm, const char* name, PicoNativeFunc function, void* user_data){
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

PicoStatus pico_vm_call(
    PicoVM* vm, 
    const char* name, 
    int argCount,
    const PicoValue* args, 
    PicoValue* result
){
    if(vm == NULL || name == NULL || name[0] == '\0' || argCount < 0){
        return PICO_STATUS_INVALID_ARGUMENT;
    }

    if(argCount > 0 && args == NULL){
        return PICO_STATUS_INVALID_ARGUMENT;
    }

    if(vm->frameCount != 0){
        return setApiError(vm, PICO_STATUS_RUNTIME_ERROR, "PiCo VM calls are not reentrant.");
    }

    size_t nameLength = strlen(name);

    if(nameLength > INT_MAX){
        return setApiError(vm, PICO_STATUS_INVALID_ARGUMENT, "PiCo function name is too long.");
    }

    vm->lastError[0] = '\0';

    ObjectString* key = copyString(vm, name, (int)nameLength);
    push(vm, OBJECT_VAL(key));

    Value callee;
    bool found = globalGetName(&vm->globals, key, &callee);

    pop(vm);

    if(!found){
        snprintf(vm->lastError, sizeof(vm->lastError), "Global function '%s' is not defined.", name);
        return PICO_STATUS_RUNTIME_ERROR;
    }

    Value* internalArgs = NULL;

    if(argCount > 0){
        internalArgs = malloc(sizeof(Value) * (size_t)argCount);

        if(internalArgs == NULL){
            return setApiError(vm, PICO_STATUS_OUT_OF_MEMORY, "Could not allocate PiCo call arguments.");
        }
    }

    for(int i = 0; i < argCount; i++){
        if(!toInternalValue(&args[i], &internalArgs[i])){
            free(internalArgs);
            return setApiError(vm, PICO_STATUS_UNSUPPORTED_TYPE, "Arguments contain unsupported value types.");
        }
    }

    Value internalResult = NULL_VAL;
    InterpreterStatus status = vmCallValue(vm, callee, argCount, internalArgs, &internalResult);

    free(internalArgs);

    if(status != VM_OK){
        return mapInterpreterStatus(status);
    }

    if(result == NULL){
        return PICO_STATUS_OK;
    }

    if(!fromInternalValue(internalResult, result)){
        return setApiError(vm, PICO_STATUS_UNSUPPORTED_TYPE, "PiCo function returned an unsupported value type.");
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
        case PICO_STATUS_OUT_OF_MEMORY:
            return "out of memory";
        case PICO_STATUS_UNSUPPORTED_TYPE:
            return "unsupported type";
    }

    return "unknown status";
}