#ifndef PICO_MODULE_LOADER_H
#define PICO_MODULE_LOADER_H

#include "vm.h"

typedef struct ImportResult{
    ObjectModule* module;
    ObjectClosure* closure;
}ImportResult;

InterpreterStatus importModule(
    VM* vm,
    ObjectString* spec,
    const char* requester,
    ImportResult* result
);

#endif // PICO_MODULE_LOADER_H