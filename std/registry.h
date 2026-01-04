#ifndef PICO_MODULES_H
#define PICO_MODULES_H

#include "vm.h"

void registerFsModule(VM* vm);
void registerTimeModule(VM* vm);
void registerOsModule(VM* vm);
void registerPathModule(VM* vm);
void registerGlobModule(VM* vm);
void registerListModule(VM* vm);
void registerIterModule(VM* vm);

// String Functions
Value string_len(VM* vm, int argCount, Value* args);
Value string_sub(VM* vm, int argCount, Value* args);
Value string_trim(VM* vm, int argCount, Value* args);
Value string_upper(VM* vm, int argCount, Value* args);
Value string_lower(VM* vm, int argCount, Value* args);
Value string_find(VM* vm, int argCount, Value* args);
Value string_split(VM* vm, int argCount, Value* args);
Value string_replace(VM* vm, int argCount, Value* args);

// List Functions
Value list_push(VM* vm, int argCount, Value* args);
Value list_pop(VM* vm, int argCount, Value* args);
Value list_size(VM* vm, int argCount, Value* args);

// File Functions
Value file_read(VM* vm, int argCount, Value* args);
Value file_write(VM* vm, int argCount, Value* args);
Value file_close(VM* vm, int argCount, Value* args);
Value file_readLine(VM* vm, int argCount, Value* args);

#endif // PICO_MODULES_H