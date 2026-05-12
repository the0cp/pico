#ifndef PICO_MODULES_FS_H
#define PICO_MODULES_FS_H

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

#endif // PICO_MODULES_FS_H