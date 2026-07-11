#include <stdio.h>

#include <pico.h>

int main(void){
    PicoVM* vm = pico_vm_create();

    if(vm == NULL){
        fprintf(stderr, "Could not create PiCo VM.\n");
        return 1;
    }

    PicoStatus status = pico_vm_eval(vm,
        "var name = \"PiCo\";\n"
        "print \"Hello from ${name}!\";\n",
        "<basic>"
    );

    if(status != PICO_STATUS_OK){
        const char* error = pico_vm_last_error(vm);
        fprintf(stderr, "%s\n", error != NULL ? error : pico_status_string(status));
        pico_vm_destroy(vm);
        return 1;
    }

    pico_vm_destroy(vm);
    return 0;
}
