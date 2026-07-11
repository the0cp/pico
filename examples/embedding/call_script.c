#include <stdio.h>

#include <pico.h>

static void hostAdd(PicoCall* call, void* userData){
    (void)userData;

    double left;
    double right;

    if(!pico_call_get_number(call, 0, &left) || !pico_call_get_number(call, 1, &right)){
        pico_call_error(call, "hostAdd() expects two numbers.");
        return;
    }

    pico_call_return_number(call, left + right);
}

int main(void){
    PicoVM* vm = pico_vm_create();

    if(vm == NULL){
        fprintf(stderr, "Could not create PiCo VM.\n");
        return 1;
    }

    pico_vm_register_native(vm, "hostAdd", hostAdd, NULL);

    const char* source =
        "func calculate(a, b){\n"
        "    return hostAdd(a, b) * 2;\n"
        "}\n";

    PicoStatus status = pico_vm_eval(vm, source, "<call_script>");

    if(status != PICO_STATUS_OK){
        const char* error = pico_vm_last_error(vm);
        fprintf(stderr, "%s\n", error != NULL ? error : pico_status_string(status));
        pico_vm_destroy(vm);
        return 1;
    }

    PicoValue args[] = {
        pico_value_number(10),
        pico_value_number(11)
    };

    PicoValue result;
    status = pico_vm_call(vm, "calculate", 2, args, &result);

    if(status != PICO_STATUS_OK){
        const char* error = pico_vm_last_error(vm);
        fprintf(stderr, "%s\n", error != NULL ? error : pico_status_string(status));
        pico_vm_destroy(vm);
        return 1;
    }

    printf("Result: %.14g\n", result.as.number);

    pico_vm_destroy(vm);
    return 0;
}
