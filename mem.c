#include <stdlib.h>

#include "mem.h"

void* resize(void* ptr, size_t oldSize, size_t newSize){
    if(newSize == 0){
        free(ptr);
        return NULL;
    }

    void* newPtr = realloc(ptr, newSize);
    if(newPtr == NULL){
        // Handle memory allocation failure
        exit(EXIT_FAILURE);
    }
    return newPtr;
}