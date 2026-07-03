#include <stdio.h>

#include <pico.h>

int main(void) {
    PicoVM* vm = pico_vm_create();

    if (vm == NULL) {
        fprintf(stderr, "Could not create PiCo VM.\n");
        return 1;
    }

    const char* source =
        "var project = \"PiCo\";\n"
        "print \"Hello from embedded ${project}!\";\n";

    PicoStatus status = pico_vm_eval(
        vm,
        source,
        "embed_basic.pcs"
    );

    if (status != PICO_STATUS_OK) {
        fprintf(
            stderr,
            "PiCo execution failed: %s\n",
            pico_status_string(status)
        );
    }

    pico_vm_destroy(vm);

    return status == PICO_STATUS_OK ? 0 : 1;
}