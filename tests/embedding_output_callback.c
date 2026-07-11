#include <stdio.h>
#include <string.h>

#include <pico.h>

typedef struct Buffer{
    char data[1024];
    size_t length;
}Buffer;

static void bufferWrite(const char* text, size_t length, void* userData){
    Buffer* buffer = (Buffer*)userData;

    if(buffer->length + length >= sizeof(buffer->data)){
        length = sizeof(buffer->data) - buffer->length - 1;
    }

    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
}

int main(void){
    PicoVM* vm = pico_vm_create();

    if(vm == NULL){
        fprintf(stderr, "Could not create PiCo VM.\n");
        return 1;
    }

    Buffer out = {0};
    Buffer err = {0};

    pico_vm_set_output(vm, bufferWrite, &out);
    pico_vm_set_error_output(vm, bufferWrite, &err);

    PicoStatus status = pico_vm_eval(vm,
        "print \"hello\";\n"
        "print 42;\n"
        "print [1, true, null];\n",
        "<output_test>"
    );

    if(status != PICO_STATUS_OK){
        fprintf(stderr, "Unexpected eval failure: %s\n", pico_status_string(status));
        pico_vm_destroy(vm);
        return 1;
    }

    if(strcmp(out.data, "hello\n42\n[1, true, null]\n") != 0){
        fprintf(stderr, "Unexpected captured output: [%s]\n", out.data);
        pico_vm_destroy(vm);
        return 1;
    }

    status = pico_vm_eval(vm, "print 1 / 0;\n", "<error_test>");

    if(status != PICO_STATUS_RUNTIME_ERROR){
        fprintf(stderr, "Expected runtime error, got %s.\n", pico_status_string(status));
        pico_vm_destroy(vm);
        return 1;
    }

    if(strstr(err.data, "Division by zero") == NULL){
        fprintf(stderr, "Unexpected captured error: [%s]\n", err.data);
        pico_vm_destroy(vm);
        return 1;
    }

    printf("Captured output: %s", out.data);
    printf("Captured error: %s", err.data);

    pico_vm_destroy(vm);
    return 0;
}