#include "file.h"
#include "vm.h"

char* read(const char* path){
    FILE* file = fopen(path, "rb");
    if(!file){
        fprintf(stderr, "Could not open file %s\n", path);
        exit(EXIT_FAILURE);
    }
    fseek(file, 0L, SEEK_END);
    size_t size = ftell(file);
    rewind(file);

    char* content = malloc(size + 1);
    if(!content){
        fprintf(stderr, "Could not allocate memory for file content\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    content[fread(content, 1, size, file)] = '\0';
    fclose(file);
    return content;
}

void runScript(VM* vm, const char* path) {
    char* content = read(path);
    InterpreterStatus status = interpret(vm, content);
    free(content);

    if(status == VM_COMPILE_ERROR) exit(EXIT_FAILURE);
    if(status == VM_RUNTIME_ERROR) exit(EXIT_FAILURE);
}