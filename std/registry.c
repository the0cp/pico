#include <string.h>

#include "registry.h"
#include "object.h"
#include "value.h"
#include "vm.h"

void defineCFunc(VM* vm, HashTable* table, const char* name, CFunc func){
    push(vm, OBJECT_VAL(copyString(vm, name, (int)strlen(name))));
    push(vm, OBJECT_VAL(newCFunc(vm, func)));
    tableSet(vm, table, peek(vm, 1), peek(vm, 0));
    pop(vm);
    pop(vm);
}