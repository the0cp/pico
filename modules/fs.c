#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "mem.h"
#include "vm.h"
#include "object.h"
#include "value.h"
#include "modules.h"
#include "fs.h"

#define GET_FILE(val) \
    if(!IS_FILE(val)){ \
        fprintf(stderr, "Expected a file object.\n"); \
        return NULL_VAL; \
    } \
    ObjectFile* fileObj = AS_FILE(val); \
    if(!fileObj->isOpen || fileObj->handle == NULL){ \
        fprintf(stderr, "File is not open.\n"); \
        return NULL_VAL; \
    }

Value file_read(VM* vm, int argCount, Value* args){
    GET_FILE(args[-1]);
    long curPos = ftell(fileObj->handle);
    fseek(fileObj->handle, 0L, SEEK_END);
    long endPos = ftell(fileObj->handle);
    fseek(fileObj->handle, curPos, SEEK_SET);

    size_t size = endPos - curPos;
    char* content = (char*)malloc(size + 1);
    if(!content){
        fprintf(stderr, "Could not allocate memory for file content\n");
        return NULL_VAL;
    }

    size_t readBytes = fread(content, 1, size, fileObj->handle);
    content[readBytes] = '\0';
    return OBJECT_VAL(copyString(vm, content, (int)readBytes));
}

Value file_close(VM* vm, int argCount, Value* args){
    GET_FILE(args[-1]);
    fclose(fileObj->handle);
    fileObj->isOpen = false;
    fileObj->handle = NULL;
    return NULL_VAL;
}

Value file_write(VM* vm, int argCount, Value* args){
    GET_FILE(args[-1]);
    if(argCount != 1 || !IS_STRING(args[0])){
        fprintf(stderr, "file.write expects a single string argument.\n");
        return NULL_VAL;
    }
    char* content = AS_CSTRING(args[0]);
    fprintf(fileObj->handle, "%s", content);
    return NULL_VAL;
}

Value file_readLine(VM* vm, int argCount, Value* args){
    GET_FILE(args[-1]);
    size_t capacity = 128;
    size_t length = 0;
    char* buffer = (char*)reallocate(vm, NULL, 0, capacity);
    int c;
    while(true){
        c = fgetc(fileObj->handle);
        if(c == EOF || c == '\n'){
            break;
        }
        if(c == '\r')   continue;

        if(length + 1 >= capacity){
            size_t old = capacity;
            capacity *= 2;
            buffer = (char*)reallocate(vm, buffer, old, capacity);
        }
        buffer[length++] = (char)c;
    }

    if(length == 0 && c == EOF){
        reallocate(vm, buffer, capacity, 0);
        return NULL_VAL;
    }

    ObjectString* lineStr = copyString(vm, buffer, (int)length);
    reallocate(vm, buffer, capacity, 0);
    return OBJECT_VAL(lineStr);
}

static Value fs_open(VM* vm, int argCount, Value* args){
    if(argCount < 1 || !IS_STRING(args[0])){
        fprintf(stderr, "fs.open expects a file path string as the first argument.\n");
        return NULL_VAL;
    }

    char* path = AS_CSTRING(args[0]);
    char* mode = "r";
    if(argCount > 1 && IS_STRING(args[1])){
        mode = AS_CSTRING(args[1]);
    }

    FILE* file = fopen(path, mode);
    if(!file){
        fprintf(stderr, "Could not open file %s with mode %s\n", path, mode);
        return NULL_VAL;
    }
    return OBJECT_VAL(newFile(vm, file));
}


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

static Value fs_readLines(VM* vm, int argCount, Value* args){
    // read all lines
    if(argCount != 1 || !IS_STRING(args[0])){
        fprintf(stderr, "fs.rline expects a single string argument.\n");
        return NULL_VAL;
    }

    char* path = AS_CSTRING(args[0]);
    FILE* file = fopen(path, "r");
    if(!file){
        fprintf(stderr, "Could not open file %s\n", path);
        return NULL_VAL;
    }

    ObjectList* list = newList(vm);
    push(vm, OBJECT_VAL(list));

    char line[1024];
    while(fgets(line, sizeof(line), file)){
        size_t len = strlen(line);
        if(len > 0 && line[len - 1] == '\n'){
            line[len - 1] = '\0';
            len--;
        }

        ObjectString* lineStr = copyString(vm, line, (int)len);
        push(vm, OBJECT_VAL(lineStr));
        appendToList(vm, list, OBJECT_VAL(lineStr));
        pop(vm);
    }

    fclose(file);
    pop(vm);
    return OBJECT_VAL(list);
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

static Value fs_appendFile(VM* vm, int argCount, Value* args){
    if(argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])){
        fprintf(stderr, "fs.append expects path and content strings.\n");
        return NULL_VAL;
    }

    char* path = AS_CSTRING(args[0]);
    char* content = AS_CSTRING(args[1]);

    FILE* file = fopen(path, "ab");
    if(!file){
        fprintf(stderr, "Could not open file %s\n", path);
        return NULL_VAL;
    }

    fwrite(content, sizeof(char), strlen(content), file);
    fclose(file);
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

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

static Value fs_listDir(VM* vm, int argCount, Value* args){
    if(argCount != 1 || !IS_STRING(args[0])){
        fprintf(stderr, "fs.list expects a single dir string argument.\n");
        return NULL_VAL;
    }

    char* path = AS_CSTRING(args[0]);

    ObjectList* list = newList(vm);
    push(vm, OBJECT_VAL(list));

#ifdef _WIN32
    char listPath[2048];
    snprintf(listPath, 2048, "%s\\*.*", path);
    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile(searchPath, &fd);
    if(hFind != INVALID_HANDLE_VALUE){
        do{
            if(strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0){
                ObjectString* name = copyString(vm, fd.cFileName, strlen(fd.cFileName));
                push(vm, OBJECT_VAL(name));
                appendToList(vm, list, OBJECT_VAL(name));
                pop(vm);
            }
        }while(FindNextFile(hFind, &fd));
        FindClose(hFind);
    }
#else
    DIR* dir;
    struct dirent* ent;
    if((dir = opendir(path)) != NULL){
        while((ent = readdir(dir)) != NULL){
            if(strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0){
                ObjectString* name = copyString(vm, ent->d_name, strlen(ent->d_name));
                push(vm, OBJECT_VAL(name));
                appendToList(vm, list, OBJECT_VAL(name));
                pop(vm);
            }
        }
        closedir(dir);
    }
#endif

    pop(vm);
    return OBJECT_VAL(list);
}

static void defineCFunc(VM* vm, HashTable* table, const char* name, CFunc func){
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

    defineCFunc(vm, &module->members, "read", fs_readFile);
    defineCFunc(vm, &module->members, "write", fs_writeFile);
    defineCFunc(vm, &module->members, "exists", fs_exists);
    defineCFunc(vm, &module->members, "remove", fs_remove);
    defineCFunc(vm, &module->members, "list", fs_listDir);
    defineCFunc(vm, &module->members, "rlines", fs_readLines);
    defineCFunc(vm, &module->members, "append", fs_appendFile);

    defineCFunc(vm, &module->members, "open", fs_open);

    tableSet(vm, &vm->modules, moduleName, OBJECT_VAL(module));
    pop(vm);
    pop(vm);
}