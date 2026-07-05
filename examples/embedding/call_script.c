/*
 * This example demonstrates how to embed PiCo into a host application and call a PiCo function from the host.
 * It defines a native host function hostAdd() that adds two numbers, and registers it as a global PiCo function.
 * The PiCo script defines a function calculate() that calls hostAdd() and multiplies the result by 2. 
 * The host application calls calculate() and prints the result.
*/

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
    pico_vm_register_native(vm, "hostAdd", hostAdd, NULL);

    const char* source =
        "func calculate(a, b){\n"
        "    return hostAdd(a, b) * 2;\n"
        "}\n";

    if(pico_vm_eval(vm, source, "<call_script>") != PICO_STATUS_OK){
        fprintf(stderr, "%s\n", pico_vm_last_error(vm));
        pico_vm_destroy(vm);
        return 1;
    }

    PicoValue args[] = {
        pico_value_number(10),
        pico_value_number(11)
    };

    PicoValue result;

    if(pico_vm_call(vm, "calculate", 2, args, &result) != PICO_STATUS_OK){
        fprintf(stderr, "%s\n", pico_vm_last_error(vm));
        pico_vm_destroy(vm);
        return 1;
    }

    printf("Result: %.14g\n", result.as.number);

    pico_vm_destroy(vm);
    return 0;
}