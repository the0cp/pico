#ifndef PICO_LIST_OPS_H
#define PICO_LIST_OPS_H

#include "vm.h"

Value list_push(VM* vm, int argCount, Value* args);
Value list_pop(VM* vm, int argCount, Value* args);
Value list_size(VM* vm, int argCount, Value* args);

#endif