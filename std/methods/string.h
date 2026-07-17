#ifndef CIETO_METHODS_STRING_H
#define CIETO_METHODS_STRING_H

#include "vm.h"

Value string_len(VM* vm, int argCount, Value* args);
Value string_sub(VM* vm, int argCount, Value* args);
Value string_trim(VM* vm, int argCount, Value* args);
Value string_upper(VM* vm, int argCount, Value* args);
Value string_lower(VM* vm, int argCount, Value* args);
Value string_split(VM* vm, int argCount, Value* args);
Value string_replace(VM* vm, int argCount, Value* args);
Value string_find(VM* vm, int argCount, Value* args);

#endif  // CIETO_METHODS_STRING_H