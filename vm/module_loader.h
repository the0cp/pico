#ifndef CIETO_MODULE_LOADER_H
#define CIETO_MODULE_LOADER_H

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

#endif // CIETO_MODULE_LOADER_H