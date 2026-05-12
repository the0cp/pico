#include <string.h>

#include "registry.h"

#include "prelude/iter.h"

#include "modules/fs.h"
#include "modules/path.h"
#include "modules/glob.h"
#include "modules/os.h"
#include "modules/time.h"
#include "modules/gc.h"

void defineCFunc(VM* vm, HashTable* table, const char* name, CFunc func){
    push(vm, OBJECT_VAL(copyString(vm, name, (int)strlen(name))));
    push(vm, OBJECT_VAL(newCFunc(vm, func)));
    tableSet(vm, table, peek(vm, 1), peek(vm, 0));
    pop(vm);
    pop(vm);
}

static NativeModuleDef nativeModules[] = {
    {"fs", initFsModule},
    {"time", initTimeModule},
    {"os", initOsModule},
    {"path", initPathModule},
    {"glob", initGlobModule},
    {"gc", initGcModule},
    {NULL, NULL}
};

const NativeModuleDef* findNativeModule(const char* name){
    for(int i = 0; nativeModules[i].name != NULL; i++){
        if(strcmp(nativeModules[i].name, name) == 0){
            return &nativeModules[i];
        }
    }
    return NULL;
}

void registerPrelude(VM* vm){
    defineCFunc(vm, &vm->globals, "iter", iterNative);
    defineCFunc(vm, &vm->globals, "next", nextNative);
}