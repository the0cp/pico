#include <stdio.h>
#include <string.h>

#include <cieto.h>

typedef struct HostState {
    int addCalls;
    int recordCalls;
    double recordedValue;
} HostState;

static void hostAdd(CieCall* call, void* userData) {
    HostState* state = (HostState*)userData;

    if(cie_call_arg_count(call) != 2){
        cie_call_error(call, "hostAdd() expects exactly two arguments.");
        return;
    }

    double left;
    double right;

    if(!cie_call_get_number(call, 0, &left) || !cie_call_get_number(call, 1, &right)){
        cie_call_error(call, "hostAdd() expects two numbers.");
        return;
    }

    state->addCalls++;
    cie_call_return_number(call, left + right);
}

static void hostRecord(CieCall* call, void* userData) {
    HostState* state = (HostState*)userData;

    if(cie_call_arg_count(call) != 1){
        cie_call_error(call, "hostRecord() expects exactly one argument.");
        return;
    }

    double value;

    if(!cie_call_get_number(call, 0, &value)){
        cie_call_error(call, "hostRecord() expects a number.");
        return;
    }

    state->recordCalls++;
    state->recordedValue = value;

    cie_call_return_bool(call, true);
}

static int reportStatus(CieVM* vm, const char* operation, CieStatus status) {
    const char* error = cie_vm_last_error(vm);

    fprintf(stderr, "%s failed: %s\n", operation, error != NULL ? error : cie_status_string(status));

    return 1;
}

int main(void) {
    CieVM* vm = cie_vm_create();

    if(vm == NULL){
        fprintf(stderr, "Could not create Cieto VM.\n");
        return 1;
    }

    HostState state = {0};

    CieStatus status = cie_vm_register_native(vm, "hostAdd", hostAdd, &state);

    if(status != CIE_STATUS_OK){
        int result = reportStatus(vm, "Registering hostAdd", status);
        cie_vm_destroy(vm);
        return result;
    }

    status = cie_vm_register_native(vm, "hostRecord", hostRecord, &state);

    if(status != CIE_STATUS_OK){
        int result = reportStatus(vm, "Registering hostRecord", status);
        cie_vm_destroy(vm);
        return result;
    }

    const char* source =
        "var result = hostAdd(20, 22);\n"
        "hostRecord(result);\n";

    status = cie_vm_eval(vm, source, "embedding_native_function.cies");

    if(status != CIE_STATUS_OK){
        int result = reportStatus(vm, "Execution", status);
        cie_vm_destroy(vm);
        return result;
    }

    if(state.addCalls != 1){
        fprintf(stderr, "Expected hostAdd() to be called once, got %d calls.\n", state.addCalls);
        cie_vm_destroy(vm);
        return 1;
    }

    if(state.recordCalls != 1){
        fprintf(stderr, "Expected hostRecord() to be called once, got %d calls.\n", state.recordCalls);
        cie_vm_destroy(vm);
        return 1;
    }

    if(state.recordedValue != 42.0){
        fprintf(stderr, "Expected recorded value 42, got %.14g.\n", state.recordedValue);
        cie_vm_destroy(vm);
        return 1;
    }

    printf("Host function result: %.14g\n", state.recordedValue);

    status = cie_vm_eval(vm,
        "hostAdd(\"invalid\", 1);\n",
        "embedding_native_error.cies"
    );

    if(status != CIE_STATUS_RUNTIME_ERROR){
        fprintf(stderr, "Expected runtime error, got %s.\n", cie_status_string(status));
        cie_vm_destroy(vm);
        return 1;
    }

    const char* error = cie_vm_last_error(vm);

    if(error == NULL || strstr(error, "expects two numbers") == NULL){
        fprintf(stderr, "Unexpected callback error: %s\n", error != NULL ? error : "<none>");
        cie_vm_destroy(vm);
        return 1;
    }

    printf("Captured callback error: %s\n", error);

    status = cie_vm_eval(vm,
        "var nestedResult = hostAdd(10, 5);\n"
        "hostRecord(hostAdd(nestedResult, 27));\n",
        "embedding_nested_native_function.cies"
    );

    if(status != CIE_STATUS_OK){
        int result = reportStatus(vm, "Nested execution", status);
        cie_vm_destroy(vm);
        return result;
    }

    if(state.recordedValue != 42.0){
        fprintf(stderr, "Expected nested result 42, got %.14g.\n", state.recordedValue);
        cie_vm_destroy(vm);
        return 1;
    }

    printf("Nested host function result: %.14g\n", state.recordedValue);

    cie_vm_destroy(vm);
    return 0;
}