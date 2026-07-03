#include "pico.h"

#include <stdlib.h>

#include "vm.h"

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

PicoVM* pico_vm_create(void){
    PicoVM* vm = malloc(sizeof(*vm));

    if(vm == NULL){
        return NULL;
    }

    initVM(vm, 0, NULL);
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