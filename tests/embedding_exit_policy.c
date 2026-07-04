#include <stdio.h>
#include <string.h>

#include <pico.h>

int main(void){
    PicoVM* vm = pico_vm_create();

    if(vm == NULL){
        fprintf(stderr, "Could not create PiCo VM.\n");
        return 1;
    }

    const char* source =
        "import \"os\";\n"
        "os.exit(7);\n"
        "print \"This line must not execute.\";\n";

    PicoStatus status = pico_vm_eval(
        vm,
        source,
        "embedding_exit_policy.pcs"
    );

    if(status != PICO_STATUS_RUNTIME_ERROR){
        fprintf(stderr, "Expected runtime error, got: %s\n", pico_status_string(status));

        pico_vm_destroy(vm);
        return 1;
    }

    const char* error = pico_vm_last_error(vm);

    if(error == NULL){
        fprintf(stderr, "Expected an error message.\n");
        pico_vm_destroy(vm);
        return 1;
    }

    if(strstr(error, "disabled") == NULL){
        fprintf(stderr, "Unexpected error message: %s\n", error);

        pico_vm_destroy(vm);
        return 1;
    }

    printf("Host survived os.exit().\n");
    printf("Captured error: %s\n", error);

    pico_vm_destroy(vm);
    return 0;
}