#ifndef PICO_MEM_H
#define PICO_MEM_H

#include "common.h"
#include "vm.h"

void* reallocate(VM* vm, void* ptr, size_t oldSize, size_t newSize);

#endif