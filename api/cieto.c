#include "cieto.h"

#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "global_env.h"
#include "object.h"
#include "value.h"
#include "vm.h"

struct CieCall{
    VM* vm;
    int argCount;
    Value* args;
    Value result;
};

static CieStatus mapInterpreterStatus(InterpreterStatus status){
    switch(status){
        case VM_OK:
            return CIE_STATUS_OK;
        case VM_COMPILE_ERROR:
            return CIE_STATUS_COMPILE_ERROR;
        case VM_RUNTIME_ERROR:
            return CIE_STATUS_RUNTIME_ERROR;
    }

    return CIE_STATUS_RUNTIME_ERROR;
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

    CieCall call = {
        .vm = vm,
        .argCount = argCount,
        .args = args,
        .result = NULL_VAL
    };

    func->hostFunc(&call, func->userData);

    return call.result;
}

static bool isValidArgIndex(const CieCall* call, int index){
    return call != NULL && index >= 0 && index < call->argCount;
}

static void setCallResult(CieCall* call, Value value){
    if(call == NULL){
        return;
    }

    call->result = value;
}

static bool toInternalValue(const CieValue* publicValue, Value* internalValue){
    if(publicValue == NULL || internalValue == NULL){
        return false;
    }

    switch(publicValue->type){
        case CIE_VALUE_NULL:
            *internalValue = NULL_VAL;
            return true;
        case CIE_VALUE_BOOL:
            *internalValue = BOOL_VAL(publicValue->as.boolean);
            return true;
        case CIE_VALUE_NUMBER:
            *internalValue = NUM_VAL(publicValue->as.number);
            return true;
        case CIE_VALUE_STRING:
        case CIE_VALUE_OTHER:
        default:
            return false;
    }

    return false;
}

static bool fromInternalValue(Value internalValue, CieValue* publicValue){
    if(publicValue == NULL){
        return false;
    }

    if(IS_NULL(internalValue)){
        *publicValue = cie_value_null();
        return true;
    }

    if(IS_BOOL(internalValue)){
        *publicValue = cie_value_bool(AS_BOOL(internalValue));
        return true;
    }

    if(IS_NUM(internalValue)){
        *publicValue = cie_value_number(AS_NUM(internalValue));
        return true;
    }

    publicValue->type = CIE_VALUE_OTHER;
    return false;
}

static CieStatus setApiError(CieVM* vm, CieStatus status, const char* message){
    if(vm != NULL && message != NULL){
        snprintf(vm->lastError, sizeof(vm->lastError), "%s", message);
    }

    return status;
}

CieValue cie_value_null(void){
    CieValue value = {
        .type = CIE_VALUE_NULL
    };

    return value;
}

CieValue cie_value_bool(bool value){
    CieValue result = {
        .type = CIE_VALUE_BOOL,
        .as.boolean = value
    };

    return result;
}

CieValue cie_value_number(double value){
    CieValue result = {
        .type = CIE_VALUE_NUMBER,
        .as.number = value
    };

    return result;
}

CieVM* cie_vm_create(void){
    CieVM* vm = malloc(sizeof(*vm));

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

void cie_vm_destroy(CieVM* vm){
    if(vm == NULL){
        return;
    }

    freeVM(vm);
    free(vm);
}

CieStatus cie_vm_eval(
    CieVM* vm,
    const char* source,
    const char* source_name
){
    if(vm == NULL || source == NULL){
        return CIE_STATUS_INVALID_ARGUMENT;
    }

    if(source_name == NULL){
        source_name = "<embedded>";
    }

    InterpreterStatus status = interpret(vm, source, source_name);
    return mapInterpreterStatus(status);
}

int cie_call_arg_count(const CieCall* call){
    if(call == NULL){
        return 0;
    }

    return call->argCount;
}

void cie_vm_set_output(CieVM* vm, CieWriteFunc func, void* userData){
    if(vm == NULL){
        return;
    }

    vm->output.write = func;
    vm->output.userData = userData;
}

void cie_vm_set_error_output(CieVM* vm, CieWriteFunc func, void* userData){
    if(vm == NULL){
        return;
    }

    vm->errOutput.write = func;
    vm->errOutput.userData = userData;
}

CieValueType cie_call_arg_type(const CieCall* call, int index){
    if(!isValidArgIndex(call, index)){
        return CIE_VALUE_OTHER;
    }

    Value value = call->args[index];

    if(IS_NULL(value)){
        return CIE_VALUE_NULL;
    }

    if(IS_BOOL(value)){
        return CIE_VALUE_BOOL;
    }

    if(IS_NUM(value)){
        return CIE_VALUE_NUMBER;
    }

    if(IS_STRING(value)){
        return CIE_VALUE_STRING;
    }

    return CIE_VALUE_OTHER;
}

bool cie_call_get_bool(const CieCall* call, int index, bool* result){
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

bool cie_call_get_number(const CieCall* call, int index, double* result){
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

const char* cie_call_get_string(const CieCall* call, int index, size_t* length){
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

void cie_call_return_null(CieCall* call){
    setCallResult(call, NULL_VAL);
}

void cie_call_return_bool(CieCall* call, bool value){
    setCallResult(call, BOOL_VAL(value));
}

void cie_call_return_number(CieCall* call, double value){
    setCallResult(call, NUM_VAL(value));
}

void cie_call_return_string(CieCall* call, const char* value, size_t length){
    if(call == NULL){
        return;
    }

    if(value == NULL){
        cie_call_error(call, "Host function returned a null string pointer.");
        return;
    }

    if(length > INT_MAX){
        cie_call_error(call, "Host function returned a string that is too large.");
        return;
    }

    ObjectString* string = copyString(call->vm, value, (int)length);

    setCallResult(call, OBJECT_VAL(string));
}

void cie_call_error(CieCall* call, const char* message){
    if(call == NULL){
        return;
    }

    if(message == NULL){
        message = "Host function failed.";
    }

    runtimeError(call->vm, "%s", message);

    setCallResult(call, NULL_VAL);
}

CieStatus cie_vm_register_native(CieVM* vm, const char* name, CieNativeFunc function, void* user_data){
    if(vm == NULL || name == NULL || name[0] == '\0' || function == NULL){
        return CIE_STATUS_INVALID_ARGUMENT;
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
        return CIE_STATUS_RUNTIME_ERROR;
    }

    return CIE_STATUS_OK;
}

CieStatus cie_vm_call(
    CieVM* vm,
    const char* name,
    int argCount,
    const CieValue* args,
    CieValue* result
){
    if(vm == NULL || name == NULL || name[0] == '\0' || argCount < 0){
        return CIE_STATUS_INVALID_ARGUMENT;
    }

    if(argCount > 0 && args == NULL){
        return CIE_STATUS_INVALID_ARGUMENT;
    }

    if(vm->frameCount != 0){
        return setApiError(vm, CIE_STATUS_RUNTIME_ERROR, "Cieto VM calls are not reentrant.");
    }

    size_t nameLength = strlen(name);

    if(nameLength > INT_MAX){
        return setApiError(vm, CIE_STATUS_INVALID_ARGUMENT, "Cieto function name is too long.");
    }

    vm->lastError[0] = '\0';

    ObjectString* key = copyString(vm, name, (int)nameLength);
    push(vm, OBJECT_VAL(key));

    Value callee;
    bool found = globalGetName(&vm->globals, key, &callee);

    pop(vm);

    if(!found){
        snprintf(vm->lastError, sizeof(vm->lastError), "Global function '%s' is not defined.", name);
        return CIE_STATUS_RUNTIME_ERROR;
    }

    Value* internalArgs = NULL;

    if(argCount > 0){
        internalArgs = malloc(sizeof(Value) * (size_t)argCount);

        if(internalArgs == NULL){
            return setApiError(vm, CIE_STATUS_OUT_OF_MEMORY, "Could not allocate Cieto call arguments.");
        }
    }

    for(int i = 0; i < argCount; i++){
        if(!toInternalValue(&args[i], &internalArgs[i])){
            free(internalArgs);
            return setApiError(vm, CIE_STATUS_UNSUPPORTED_TYPE, "Arguments contain unsupported value types.");
        }
    }

    Value internalResult = NULL_VAL;
    InterpreterStatus status = vmCallValue(vm, callee, argCount, internalArgs, &internalResult);

    free(internalArgs);

    if(status != VM_OK){
        return mapInterpreterStatus(status);
    }

    if(result == NULL){
        return CIE_STATUS_OK;
    }

    if(!fromInternalValue(internalResult, result)){
        return setApiError(vm, CIE_STATUS_UNSUPPORTED_TYPE, "Cieto function returned an unsupported value type.");
    }

    return CIE_STATUS_OK;
}

const char* cie_vm_last_error(const CieVM* vm){
    if(vm == NULL || vm->lastError[0] == '\0'){
        return NULL;
    }

    return vm->lastError;
}

const char* cie_status_string(CieStatus status){
    switch(status){
        case CIE_STATUS_OK:
            return "OK";
        case CIE_STATUS_COMPILE_ERROR:
            return "compile error";
        case CIE_STATUS_RUNTIME_ERROR:
            return "runtime error";
        case CIE_STATUS_INVALID_ARGUMENT:
            return "invalid argument";
        case CIE_STATUS_OUT_OF_MEMORY:
            return "out of memory";
        case CIE_STATUS_UNSUPPORTED_TYPE:
            return "unsupported type";
    }

    return "unknown status";
}
