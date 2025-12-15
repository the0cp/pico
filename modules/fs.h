#ifndef PICO_MODULES_FS_H
#define PICO_MODULES_FS_H

#include "vm.h"

typedef struct GlobConfig{
    const char* pattern;
    bool ignoreCase;
    Value excludeVal;
    bool recursive;
}GlobConfig;

void registerFsModule(VM* vm);

static bool is_excluded(VM* vm, const char* filename, Value excludeVal, bool ignoreCase);
static void scan_dir(VM* vm, const char* baseDir, const char* relDir, ObjectList* list, GlobConfig* config);
Value file_read(VM* vm, int argCount, Value* args);
Value file_readLine(VM* vm, int argCount, Value* args);
Value file_write(VM* vm, int argCount, Value* args);
Value file_close(VM* vm, int argCount, Value* args);

#endif // PICO_MODULES_FS_H