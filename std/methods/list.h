#ifndef PICO_METHODS_LIST_H
#define PICO_METHODS_LIST_H

#include "vm.h"

Value list_push(VM* vm, int argCount, Value* args);
Value list_pop(VM* vm, int argCount, Value* args);
Value list_size(VM* vm, int argCount, Value* args);

#endif // PICO_METHODS_LIST_H