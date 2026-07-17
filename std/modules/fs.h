#ifndef CIETO_MODULES_FS_H
#define CIETO_MODULES_FS_H

#include "vm.h"

typedef struct GlobConfig{
    const char* pattern;
    bool ignoreCase;
    Value excludeVal;
    bool recursive;
}GlobConfig;

void initFsModule(VM* vm, ObjectModule* module);

Value file_read(VM* vm, int argCount, Value* args);
Value file_readLine(VM* vm, int argCount, Value* args);
Value file_write(VM* vm, int argCount, Value* args);
Value file_close(VM* vm, int argCount, Value* args);

#endif // CIETO_MODULES_FS_H