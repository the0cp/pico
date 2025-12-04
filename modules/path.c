#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "vm.h"
#include "mem.h"
#include "object.h"
#include "value.h"
#include "modules.h"

#ifdef _WIN32
    #include <dirent.h>
    #define getcwd _getcwd
    #define PATH_SEP '\\'
    #define PATH_SEP_STR "\\"
#else
    #include <unistd.h>
    #include <linux/limits.h>
    #define PATH_SEP '/'
    #define PATH_SEP_STR "/"
#endif


static bool isSep(char c){
#ifdef _WIN32
    return c == '\\' || c == '/';
#else
    return c == '/';
#endif
}

static Value path_join(VM* vm, int argCount, Value* args){
    if(argCount < 1){
        fprintf(stderr, "path.join expects at least one argument.\n");
        return NULL_VAL;
    }

    if(!IS_STRING(args[0])){
        fprintf(stderr, "path.join expects string arguments.\n");
        return NULL_VAL;
    }

    ObjectString* result = AS_STRING(args[0]);

    for(int i = 1; i < argCount; i++){
        if(!IS_STRING(args[i])){
            fprintf(stderr, "path.join expects string arguments.\n");
            continue;
        }

        ObjectString* next = AS_STRING(args[i]);
        push(vm, OBJECT_VAL(result));

        bool hasSepCur = (result->length > 0 && isSep(result->chars[result->length - 1]));
        bool hasSepNext = (next->length > 0 && isSep(next->chars[0]));

        size_t len = result->length + next->length - hasSepCur - hasSepNext + 1; // add 1 for sep
        char* chars = (char*)reallocate(vm, NULL, 0, len + 1); // add 1 for '\0'

        if(chars == NULL){
            fprintf(stderr, "Memory allocation failed for path join.\n");
            pop(vm);
            return NULL_VAL;
        }

        memcpy(chars, result->chars, result->length);

        if(!hasSepCur && !hasSepNext){
            chars[result->length] = PATH_SEP;
            memcpy(chars + result->length + 1, next->chars, next->length);
        }else if(hasSepCur && hasSepNext){
            memcpy(chars + result->length, next->chars + 1, next->length - 1);
        }else{
            memcpy(chars + result->length, next->chars, next->length);
        }

        chars[len] = '\0';

        ObjectString* newStr = copyString(vm, chars, (int)len);
        reallocate(vm, chars, len + 1, 0);
        pop(vm); // pop pushed old result
        result = newStr;
    }
    return OBJECT_VAL(result);
}

static Value path_base(VM* vm, int argCount, Value* args){
    if(argCount != 1 || !IS_STRING(args[0])){
        fprintf(stderr, "path.base expects a single string argument.\n");
        return NULL_VAL;
    }

    ObjectString* pathStr = AS_STRING(args[0]);
    
    if(pathStr->length == 0){
        return OBJECT_VAL(copyString(vm, "", 0));
    }

    int end = (int)pathStr->length - 1;

    while(end >= 0 && isSep(pathStr->chars[end])){
        end--;
    }

    int endPos = end + 1;

    while(end >= 0){
        if(isSep(pathStr->chars[end])){
            break;
        }
        end--;
    }

    int startPos = end + 1;
    int len = endPos - startPos;

    return OBJECT_VAL(copyString(vm, pathStr->chars + startPos, len));
}

static Value path_dirname(VM* vm, int argCount, Value* args){
    if(argCount != 1 || !IS_STRING(args[0])){
        fprintf(stderr, "path.dir expects a single string argument.\n");
        return NULL_VAL;
    }
    
    ObjectString* pathStr = AS_STRING(args[0]);
    if(pathStr->length == 0){
        return OBJECT_VAL(copyString(vm, ".", 1));
    }

    int index = (int)pathStr->length - 1;
    while(index >= 0 && isSep(pathStr->chars[index])){
        index--;
    }   // strip trailing seps

    while(index >= 0 && !isSep(pathStr->chars[index])){
        index--;
    }   // skip basename

    if(index < 0){
        return OBJECT_VAL(copyString(vm, ".", 1));
    }   // only basename

    while(index > 0 && isSep(pathStr->chars[index])){
        index--;
    }   // trim trailing seps of dir

    int len = index + 1;
    if(len == 0 && isSep(pathStr->chars[0])){
        len = 1;
    }   // root dir

    return OBJECT_VAL(copyString(vm, pathStr->chars, len));
}

static Value path_ext(VM* vm, int argCount, Value* args){
    if(argCount != 1 || !IS_STRING(args[0])){
        fprintf(stderr, "path.ext expects a single string argument.\n");
        return NULL_VAL;
    }

    ObjectString* pathStr = AS_STRING(args[0]);
    int index = (int)pathStr->length - 1;

    while(index >= 0){
        if(pathStr->chars[index] == '.'){
            return OBJECT_VAL(copyString(vm, pathStr->chars + index, (int)(pathStr->length - index)));
        }
        if(isSep(pathStr->chars[index])){
            break;
        }
        index--;
    }
    return OBJECT_VAL(copyString(vm, "", 0));
}

static Value path_isAbs(VM* vm, int argCount, Value* args){
    if(argCount != 1 || !IS_STRING(args[0])){
        fprintf(stderr, "path.isAbs expects a single string argument.\n");
        return NULL_VAL;
    }

    ObjectString* pathStr = AS_STRING(args[0]);
    if(pathStr->length == 0){
        return BOOL_VAL(false);
    }

#ifdef _WIN32
    if(pathStr->length >= 2 && ((pathStr->chars[0] >= 'A' && pathStr->chars[0] <= 'Z') ||
                               (pathStr->chars[0] >= 'a' && pathStr->chars[0] <= 'z')) &&
                               pathStr->chars[1] == ':'){
        return BOOL_VAL(true);
    }
#else
    if(pathStr->chars[0] == '/'){
        return BOOL_VAL(true);
    }
#endif
    return BOOL_VAL(false);
}

static Value path_abs(VM* vm, int argCount, Value* args){
    if(argCount != 1 || !IS_STRING(args[0])){
        fprintf(stderr, "path.abs expects a single string argument.\n");
        return NULL_VAL;
    }

    ObjectString* pathStr = AS_STRING(args[0]);
    // PATH_MAX 4096 from limits.h
    
}

static Value path_sep(VM* vm, int argCount, Value* args){
    if(argCount != 0){
        fprintf(stderr, "path.sep expects no arguments.\n");
        return NULL_VAL;
    }

    return OBJECT_VAL(copyString(vm, PATH_SEP_STR, 1));
}

static void defineCFunc(VM* vm, HashTable* table, const char* name, CFunc func){
    push(vm, OBJECT_VAL(copyString(vm, name, (int)strlen(name))));
    push(vm, OBJECT_VAL(newCFunc(vm, func)));
    tableSet(vm, table, AS_STRING(peek(vm, 1)), peek(vm, 0));
    pop(vm);
    pop(vm);
}

void registerPathModule(VM* vm){
    ObjectString* moduleName = copyString(vm, "path", 4);
    push(vm, OBJECT_VAL(moduleName));
    ObjectModule* module = newModule(vm, moduleName);
    push(vm, OBJECT_VAL(module));

    defineCFunc(vm, &module->members, "join", path_join);
    defineCFunc(vm, &module->members, "base", path_base);
    defineCFunc(vm, &module->members, "dirname", path_dirname);
    defineCFunc(vm, &module->members, "ext", path_ext);
    defineCFunc(vm, &module->members, "isAbs", path_isAbs);
    defineCFunc(vm, &module->members, "abs", path_abs);
    defineCFunc(vm, &module->members, "sep", path_sep);
    tableSet(vm, &vm->modules, moduleName, OBJECT_VAL(module));
    pop(vm);
    pop(vm);
}