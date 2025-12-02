#ifndef PICO_MODULES_FS_H
#define PICO_MODULES_FS_H

#include "vm.h"

void registerFsModule(VM* vm);

Value file_read(VM* vm, int argCount, Value* args);
Value file_readLine(VM* vm, int argCount, Value* args);
Value file_write(VM* vm, int argCount, Value* args);
Value file_close(VM* vm, int argCount, Value* args);

#endif // PICO_MODULES_FS_H