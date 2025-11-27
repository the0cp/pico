#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "vm.h"
#include "object.h"
#include "value.h"
#include "modules.h"

static Value fs_readFile(VM* vm, int argCount, Value* args){
    if(argCount != 1 || !IS_STRING(args[0])){
        fprintf(stderr, "fs.read expects a single string argument.\n");
        return NULL_VAL;
    }

    char* path = AS_CSTRING(args[0]);
    FILE* file = fopen(path, "rb");
    if(!file){
        fprintf(stderr, "Could not open file %s\n", path);
        return NULL_VAL;
    }
    fseek(file, 0L, SEEK_END);
    size_t size = ftell(file);
    rewind(file);

    char* content = malloc(size + 1);
    if(!content){
        fprintf(stderr, "Could not allocate memory for file content\n");
        fclose(file);
        return NULL_VAL;
    }
    content[fread(content, 1, size, file)] = '\0';
    fclose(file);
    Value result = OBJECT_VAL(copyString(vm, content, size));
    free(content);
    return result;
}

static Value fs_writeFile(VM* vm, int argCount, Value* args){
    if(argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])){
        fprintf(stderr, "fs.write expects two string arguments.\n");
        return NULL_VAL;
    }

    char* path = AS_CSTRING(args[0]);
    char* content = AS_CSTRING(args[1]);

    FILE* file = fopen(path, "wb");
    if(!file){
        fprintf(stderr, "Could not open file %s for writing\n", path);
        return NULL_VAL;
    }
    size_t written = fwrite(content, sizeof(char), strlen(content), file);
    fclose(file);

    if(written < strlen(content)){
        fprintf(stderr, "Could not write all content to file %s\n", path);
        return NULL_VAL;
    }

    return BOOL_VAL(true);
}

static Value fs_exists(VM* vm, int argCount, Value* args){
    if(argCount != 1 || !IS_STRING(args[0])){
        fprintf(stderr, "fs.exists expects a single string argument.\n");
        return NULL_VAL;
    }

    char* path = AS_CSTRING(args[0]);
    FILE* file = fopen(path, "rb");
    if(file){
        fclose(file);
        return BOOL_VAL(true);
    }
    return BOOL_VAL(false);
}

static Value fs_remove(VM* vm, int argCount, Value* args){
    if(argCount != 1 || !IS_STRING(args[0])){
        fprintf(stderr, "fs.remove expects a single string argument.\n");
        return NULL_VAL;
    }

    char* path = AS_CSTRING(args[0]);
    if(remove(path) == 0){
        return BOOL_VAL(true);
    }else{
        return BOOL_VAL(false);
    }
}

static void defineNative(VM* vm, HashTable* table, const char* name, CFunc func){
    push(vm, OBJECT_VAL(copyString(vm, name, (int)strlen(name))));
    push(vm, OBJECT_VAL(newCFunc(vm, func)));
    tableSet(vm, table, AS_STRING(peek(vm, 1)), peek(vm, 0));
    pop(vm);
    pop(vm);
}

void registerFsModule(VM* vm){
    ObjectString* moduleName = copyString(vm, "fs", 2);
    push(vm, OBJECT_VAL(moduleName));

    ObjectModule* module = newModule(vm, moduleName);
    push(vm, OBJECT_VAL(module));

    defineNative(vm, &module->members, "read", fs_readFile);
    defineNative(vm, &module->members, "write", fs_writeFile);
    defineNative(vm, &module->members, "exists", fs_exists);
    defineNative(vm, &module->members, "remove", fs_remove);

    tableSet(vm, &vm->modules, moduleName, OBJECT_VAL(module));
    pop(vm);
    pop(vm);
}