#include <stdio.h>
#include <string.h>

#include <pico.h>

static void hostAdd(PicoCall* call, void* userData) {
    (void)userData;

    if(pico_call_arg_count(call) != 2){
        pico_call_error(call, "hostAdd() expects exactly two arguments.");
        return;
    }

    double left;
    double right;

    if(!pico_call_get_number(call, 0, &left) ||
       !pico_call_get_number(call, 1, &right)){
        pico_call_error(call, "hostAdd() expects two numbers.");
        return;
    }

    pico_call_return_number(call, left + right);
}

static int reportFailure(PicoVM* vm, const char* operation,
                         PicoStatus status) {
    const char* error = pico_vm_last_error(vm);

    fprintf(stderr, "%s failed: %s\n", operation,
            error != NULL ? error : pico_status_string(status));

    return 1;
}

int main(void) {
    PicoVM* vm = pico_vm_create();

    if(vm == NULL){
        fprintf(stderr, "Could not create PiCo VM.\n");
        return 1;
    }

    PicoStatus status =
        pico_vm_register_native(vm, "hostAdd", hostAdd, NULL);

    if(status != PICO_STATUS_OK){
        int exitCode = reportFailure(vm, "Registering hostAdd", status);
        pico_vm_destroy(vm);
        return exitCode;
    }

    const char* source =
        "func add(a, b) {\n"
        "    return a + b;\n"
        "}\n"
        "\n"
        "func calculate(a, b) {\n"
        "    return hostAdd(a, b) * 2;\n"
        "}\n"
        "\n"
        "func failCall() {\n"
        "    return hostAdd(\"invalid\", 1);\n"
        "}\n"
        "\n"
        "var notCallable = 42;\n";

    status = pico_vm_eval(vm, source, "embedding_call_script.pcs");

    if(status != PICO_STATUS_OK){
        int exitCode = reportFailure(vm, "Loading PiCo functions", status);
        pico_vm_destroy(vm);
        return exitCode;
    }

    PicoValue addArgs[] = {
        pico_value_number(20),
        pico_value_number(22)
    };

    PicoValue result;
    status = pico_vm_call(vm, "add", 2, addArgs, &result);

    if(status != PICO_STATUS_OK){
        int exitCode = reportFailure(vm, "Calling add", status);
        pico_vm_destroy(vm);
        return exitCode;
    }

    if(result.type != PICO_VALUE_NUMBER || result.as.number != 42.0){
        fprintf(stderr, "Expected add() to return 42.\n");
        pico_vm_destroy(vm);
        return 1;
    }

    printf("PiCo add result: %.14g\n", result.as.number);

    PicoValue calculateArgs[] = {
        pico_value_number(10),
        pico_value_number(11)
    };

    status = pico_vm_call(vm, "calculate", 2, calculateArgs, &result);

    if(status != PICO_STATUS_OK){
        int exitCode = reportFailure(vm, "Calling calculate", status);
        pico_vm_destroy(vm);
        return exitCode;
    }

    if(result.type != PICO_VALUE_NUMBER || result.as.number != 42.0){
        fprintf(stderr, "Expected calculate() to return 42.\n");
        pico_vm_destroy(vm);
        return 1;
    }

    printf("C -> PiCo -> C result: %.14g\n", result.as.number);

    status = pico_vm_call(vm, "missingFunction", 0, NULL, &result);

    if(status != PICO_STATUS_RUNTIME_ERROR){
        fprintf(stderr, "Expected missing function call to fail.\n");
        pico_vm_destroy(vm);
        return 1;
    }

    const char* error = pico_vm_last_error(vm);

    if(error == NULL || strstr(error, "not defined") == NULL){
        fprintf(stderr, "Unexpected missing-function error: %s\n",
                error != NULL ? error : "<none>");
        pico_vm_destroy(vm);
        return 1;
    }

    printf("Captured missing function error: %s\n", error);

    status = pico_vm_call(vm, "notCallable", 0, NULL, &result);

    if(status != PICO_STATUS_RUNTIME_ERROR){
        fprintf(stderr, "Expected non-callable global to fail.\n");
        pico_vm_destroy(vm);
        return 1;
    }

    error = pico_vm_last_error(vm);

    if(error == NULL || strstr(error, "Can only call") == NULL){
        fprintf(stderr, "Unexpected non-callable error: %s\n",
                error != NULL ? error : "<none>");
        pico_vm_destroy(vm);
        return 1;
    }

    printf("Captured non-callable error: %s\n", error);

    status = pico_vm_call(vm, "failCall", 0, NULL, &result);

    if(status != PICO_STATUS_RUNTIME_ERROR){
        fprintf(stderr, "Expected failCall() to produce a runtime error.\n");
        pico_vm_destroy(vm);
        return 1;
    }

    error = pico_vm_last_error(vm);

    if(error == NULL || strstr(error, "expects two numbers") == NULL){
        fprintf(stderr, "Unexpected nested callback error: %s\n",
                error != NULL ? error : "<none>");
        pico_vm_destroy(vm);
        return 1;
    }

    printf("Captured nested callback error: %s\n", error);

    /*
     * Verify that the VM remains usable after a failed host-triggered call.
    */
    status = pico_vm_call(vm, "add", 2, addArgs, &result);

    if(status != PICO_STATUS_OK ||
       result.type != PICO_VALUE_NUMBER ||
       result.as.number != 42.0){
        int exitCode = reportFailure(vm, "Calling add after recovery", status);
        pico_vm_destroy(vm);
        return exitCode;
    }

    printf("VM recovered after failed call: %.14g\n", result.as.number);

    pico_vm_destroy(vm);
    return 0;
}