#ifndef PICO_MEM_H
#define PICO_MEM_H

#include "common.h"
#include "vm.h"

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(vm, type, pointer, oldCount, newCount) \
    (type*)reallocate(vm, pointer, sizeof(type) * (oldCount), \
    sizeof(type) * (newCount))

#define FREE_ARRAY(vm, type, pointer, oldCount) \
    reallocate(vm, pointer, sizeof(type) * (oldCount), 0)

static void markRoots(VM* vm);
static void traceRef(VM* vm, Object* object);
void markObject(VM* vm, Object* object);
void markValue(VM* vm, Value value);
void markArray(VM* vm, ValueArray* array);
static void sweep(VM* vm);

void* reallocate(VM* vm, void* ptr, size_t oldSize, size_t newSize);

#endif