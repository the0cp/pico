#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "mem.h"
#include "vm.h"
#include "object.h"
#include "value.h"
#include "registry.h"

#include "os.h"

#ifdef _WIN32
    #define popen _popen
    #define pclose _pclose
    #define WEXITSTATUS(code) (code)
#else
    #include <sys/wait.h>
#endif

static Value os_exec(VM* vm, int argCount, Value* args){
    if(argCount != 1 || !IS_STRING(args[0])){
        runtimeError(vm, "os.exec expects a single string argument.\n");
        return NULL_VAL;
    }

    char* command = AS_CSTRING(args[0]);
    FILE* pipe = popen(command, "r");
    if(!pipe){
        runtimeError(vm, "Failed to execute command: %s\n", strerror(errno));
        return NULL_VAL;
    }

    size_t capacity = 128;
    size_t length = 0;
    char* buffer = (char*)reallocate(vm, NULL, 0, capacity);
    if(!buffer){
        runtimeError(vm, "Memory allocation failed\n");
        pclose(pipe);
        return NULL_VAL;
    }

    char chunk[128];
    while(fgets(chunk, sizeof(chunk), pipe) != NULL){
        size_t chunkLen = strlen(chunk);
        if(length + chunkLen + 1 >= capacity){
            size_t old = capacity;
            while(length + chunkLen + 1 >= capacity){
                capacity *= 2;
            }
            buffer = (char*)reallocate(vm, buffer, old, capacity);
            if(!buffer){
                runtimeError(vm, "Memory allocation failed\n");
                pclose(pipe);
                return NULL_VAL;
            }
        }
        memcpy(buffer + length, chunk, chunkLen);
        length += chunkLen;
    }

    pclose(pipe);

    if(length == 0 && buffer != NULL){
        buffer[0] = '\0';
    }else{
        buffer[length] = '\0';
    }

    ObjectString* result = copyString(vm, buffer, (int)length);
    reallocate(vm, buffer, capacity, 0);
    return OBJECT_VAL(result);
}

static Value os_system(VM* vm, int argCount, Value* args){
    if(argCount != 1 || !IS_STRING(args[0])){
        runtimeError(vm, "os.run expects a single string argument.\n");
        return NULL_VAL;
    }

    char* command = AS_CSTRING(args[0]);
    int status = system(command);

#ifdef _WIN32
    return NUM_VAL((double)status);
#else
    if(WIFEXITED(status)){
        return NUM_VAL((double)WEXITSTATUS(status));
    }else{
        return NUM_VAL(-1);
    }
#endif
}

static Value os_getenv(VM* vm, int argCount, Value* args){
    if(argCount != 1 || !IS_STRING(args[0])){
        runtimeError(vm, "os.getenv expects a single string argument.\n");
        return NULL_VAL;
    }

    char* varName = AS_CSTRING(args[0]);
    char* value = getenv(varName);

    if(value == NULL){
        return NULL_VAL;
    }

    return OBJECT_VAL(copyString(vm, value, (int)strlen(value)));
}

static Value os_setenv(VM* vm, int argCount, Value* args){
    if(argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])){
        runtimeError(vm, "os.setenv expects two string arguments: key and value.\n");
        return NULL_VAL;
    }

    char* key = AS_CSTRING(args[0]);
    char* value = AS_CSTRING(args[1]);

    int result = 0;

#ifdef _WIN32
    result = _putenv_s(key, value);
#else
    result = setenv(key, value, 1);
#endif
    if(result == 0){
        return BOOL_VAL(true);
    }else{
        return BOOL_VAL(false);
    }
}

static Value os_exit(VM* vm, int argCount, Value* args){
    int exitCode = 0;
    if(argCount > 0 && IS_NUM(args[0])){
        exitCode = (int)AS_NUM(args[0]);
    }
    exit(exitCode);
    return NULL_VAL; // Unreachable
}

static void defineCFunc(VM* vm, HashTable* table, const char* name, CFunc func){
    push(vm, OBJECT_VAL(copyString(vm, name, (int)strlen(name))));
    push(vm, OBJECT_VAL(newCFunc(vm, func)));
    tableSet(vm, table, peek(vm, 1), peek(vm, 0));
    pop(vm);
    pop(vm);
}

void registerOsModule(VM* vm){
    ObjectString* moduleName = copyString(vm, "os", 2);
    push(vm, OBJECT_VAL(moduleName));

    ObjectModule* module = newModule(vm, moduleName);
    push(vm, OBJECT_VAL(module));

    defineCFunc(vm, &module->members, "exec", os_exec);
    defineCFunc(vm, &module->members, "run", os_system);
    defineCFunc(vm, &module->members, "getenv", os_getenv);
    defineCFunc(vm, &module->members, "setenv", os_setenv);
    defineCFunc(vm, &module->members, "exit", os_exit);

    tableSet(vm, &vm->modules, OBJECT_VAL(moduleName), OBJECT_VAL(module));
    pop(vm);
    pop(vm);
}