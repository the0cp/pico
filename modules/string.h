#ifndef PICO_MODULES_STRING_H
#define PICO_MODULES_STRING_H

#include "vm.h"

void registerStringModule(VM* vm);

Value string_len(VM* vm, int argCount, Value* args);
Value string_sub(VM* vm, int argCount, Value* args);
Value string_trim(VM* vm, int argCount, Value* args);
Value string_upper(VM* vm, int argCount, Value* args);
Value string_lower(VM* vm, int argCount, Value* args);
Value string_split(VM* vm, int argCount, Value* args);
Value string_replace(VM* vm, int argCount, Value* args);
Value string_find(VM* vm, int argCount, Value* args);

#endif  // PICO_MODULES_STRING_H