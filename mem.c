#include <stdlib.h>

#include "mem.h"

void collectGarbage(VM* vm);

#define GC_HEAP_GROW_FACTOR 2

void* reallocate(VM* vm, void* ptr, size_t oldSize, size_t newSize){
    vm->bytesAllocated += newSize - oldSize;
    if(newSize > oldSize){
        #ifdef DEBUG_STRESS_GC
        collectGarbage(vm);
        #else
        if(vm->bytesAllocated > vm->nextGC){
            collectGarbage(vm);
        }
        #endif
    }

    if(newSize == 0){
        free(ptr);
        return NULL;
    }

    void* newPtr = realloc(ptr, newSize);
    if(newPtr == NULL){
        exit(EXIT_FAILURE);
    }
    return newPtr;
}