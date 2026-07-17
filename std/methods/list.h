#ifndef CIETO_METHODS_LIST_H
#define CIETO_METHODS_LIST_H

#include "vm.h"

Value list_push(VM* vm, int argCount, Value* args);
Value list_pop(VM* vm, int argCount, Value* args);
Value list_size(VM* vm, int argCount, Value* args);

#endif // CIETO_METHODS_LIST_H