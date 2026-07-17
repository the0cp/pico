#include <stdio.h>

#include <cieto.h>

static void hostAdd(CieCall* call, void* userData){
    (void)userData;

    double left;
    double right;

    if(!cie_call_get_number(call, 0, &left) || !cie_call_get_number(call, 1, &right)){
        cie_call_error(call, "hostAdd() expects two numbers.");
        return;
    }

    cie_call_return_number(call, left + right);
}

int main(void){
    CieVM* vm = cie_vm_create();

    if(vm == NULL){
        return 1;
    }

    cie_vm_register_native(vm, "hostAdd", hostAdd, NULL);

    const char* source =
        "func calculate(a, b){\n"
        "    return hostAdd(a, b) * 2;\n"
        "}\n";

    CieStatus status = cie_vm_eval(vm, source, "<external_cmake>");

    if(status != CIE_STATUS_OK){
        fprintf(stderr, "%s\n", cie_vm_last_error(vm));
        cie_vm_destroy(vm);
        return 1;
    }

    CieValue args[] = {
        cie_value_number(10),
        cie_value_number(11)
    };

    CieValue result;
    status = cie_vm_call(vm, "calculate", 2, args, &result);

    if(status != CIE_STATUS_OK){
        fprintf(stderr, "%s\n", cie_vm_last_error(vm));
        cie_vm_destroy(vm);
        return 1;
    }

    printf("Result: %.14g\n", result.as.number);

    cie_vm_destroy(vm);
    return 0;
}
