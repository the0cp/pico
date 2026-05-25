#ifndef PICO_MODULES_H
#define PICO_MODULES_H

#include "vm.h"

typedef void (*NativeModuleInit)(VM* vm, ObjectModule* module);

typedef struct NativeModuleDef{
    const char* name;
    NativeModuleInit initFunc;
}NativeModuleDef;

void defineCFunc(VM* vm, GlobalEnv* env, const char* name, CFunc func);

const NativeModuleDef* findNativeModule(const char* name);

void registerPrelude(VM* vm);

#endif // PICO_MODULES_H