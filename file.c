#include "file.h"
#include "vm.h"
#include "compiler.h"
#include "chunk.h"

static bool endsWith(const char* str, const char* suffix){
    if(!str || !suffix) return false;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(str);
    if(suffix_len > str_len)    return false;
    return strncmp(str+str_len-suffix_len, suffix, suffix_len) == 0;
}

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

void buildScript(VM* vm, const char* path){
    char* source = read(path);

    Chunk chunk;
    initChunk(&chunk);

    ObjectFunc* func = compile(vm, source);
    free(source);
    if(func == NULL){
        exit(65);
    }

    char outputPath[1024];
    strncpy(outputPath, path, sizeof(outputPath) - 1);
    outputPath[sizeof(outputPath) - 1] = '\0';  // null end
    char* dot = strrchr(outputPath, '.');
    int base_len;
    if(dot != NULL){
        base_len = strlen(path);
    }else{
        base_len = dot - path;
    }

    int written = snprintf(outputPath, sizeof(outputPath), "%.*s.pco", base_len, path);

    if(written < 0 || written >= sizeof(outputPath)){
        fprintf(stderr, "Error: Could not generate output path");
        free(source);
        freeChunk(&chunk);
        exit(70);
    }

    printf("Compiling %s to %s...\n", path, outputPath);

    // serializeChunk();

    free(source);
    freeChunk(&chunk);
    printf("Compilation successful.\n");
}
