#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "vm.h"
#include "mem.h"
#include "object.h"
#include "value.h"
#include "list.h"
#include "modules.h"
#include "glob.h"

#ifdef _WIN32
    #include <windows.h>
    #define PATH_SEP '\\'
#else
    #include <glob.h>
    #define PATH_SEP '/'
#endif

static Value glob_match(VM* vm, int argCount, Value* args){
    if(argCount != 1 || !IS_STRING(args[0])){
        fprintf(stderr, "glob.match expects a single string argument as pattern.\n");
        return NULL_VAL;
    }

    char* pattern = AS_CSTRING(args[0]);
    ObjectList* list = newList(vm);
    push(vm, OBJECT_VAL(list));

    #ifdef _WIN32
    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile(pattern, &fd);

    if(hFind != INVALID_HANDLE_VALUE){
        char* lastSep = strrchr(pattern, '/');
        char* lastSepWin = strrchr(pattern, '\\');
        char* sep = (lastSep > lastSepWin) ? lastSep : lastSepWin;
        
        size_t prefixLen = 0;
        char* prefix = NULL;

        if(sep != NULL){
            prefixLen = sep - pattern + 1;
            prefix = (char*)malloc(prefixLen + 1);
            memcpy(prefix, pattern, prefixLen);
            prefix[prefixLen] = '\0';
        }

        do{
            if(strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0){
                ObjectString* pathObj;
                if(prefix != NULL){
                    size_t nameLen = strlen(fd.cFileName);
                    size_t totalLen = prefixLen + nameLen;
                    char* fullPath = (char*)malloc(totalLen + 1);
                    memcpy(fullPath, prefix, prefixLen);
                    memcpy(fullPath + prefixLen, fd.cFileName, nameLen);
                    fullPath[totalLen] = '\0';
                    
                    pathObj = copyString(vm, fullPath, (int)totalLen);
                    free(fullPath);
                }else{
                    pathObj = copyString(vm, fd.cFileName, (int)strlen(fd.cFileName));
                }
                
                push(vm, OBJECT_VAL(pathObj));
                appendToList(vm, list, OBJECT_VAL(pathObj));
                pop(vm);
            }
        }while(FindNextFile(hFind, &fd));

        if(prefix != NULL) free(prefix);
        FindClose(hFind);
    }
    #else
    glob_t glob_res;
    int ret = glob(pattern, GLOB_TILDE, NULL, &glob_res);
    
    if(ret == 0){
        for(size_t i = 0; i < glob_res.gl_pathc; i++){
            char* matchPath = glob_res.gl_pathv[i];
            ObjectString* path = copyString(vm, matchPath, (int)strlen(matchPath));
            push(vm, OBJECT_VAL(path));
            appendToList(vm, list, OBJECT_VAL(path));
            pop(vm);
        }
    }
    globfree(&glob_res);
    #endif

    pop(vm);    // pop the list
    return OBJECT_VAL(list);
}

static void defineCFunc(VM* vm, HashTable* table, const char* name, CFunc func){
    push(vm, OBJECT_VAL(copyString(vm, name, (int)strlen(name))));
    push(vm, OBJECT_VAL(newCFunc(vm, func)));
    tableSet(vm, table, AS_STRING(peek(vm, 1)), peek(vm, 0));
    pop(vm);
    pop(vm);
}

void registerGlobModule(VM* vm){
    ObjectString* moduleName = copyString(vm, "glob", 4);
    push(vm, OBJECT_VAL(moduleName));

    ObjectModule* module = newModule(vm, moduleName);
    push(vm, OBJECT_VAL(module));

    defineCFunc(vm, &module->members, "match", glob_match);

    tableSet(vm, &vm->modules, moduleName, OBJECT_VAL(module));
    pop(vm);
    pop(vm);
}