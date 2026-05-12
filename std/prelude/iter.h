#ifndef PICO_MODULE_ITER_H
#define PICO_MODULE_ITER_H

#include "vm.h"

Value iterNative(VM* vm, int argCount, Value* args);
Value nextNative(VM* vm, int argCount, Value* args);

#endif