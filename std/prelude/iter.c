#include <stdio.h>
#include <string.h>

#include "iter.h"
#include "object.h"
#include "vm.h"
#include "value.h"

static Value iterMap(VM* vm, ObjectIterator* iter, ObjectMap* map){
    while(iter->index < map->table.capacity){
        Entry* entry = &map->table.entries[iter->index++];
        if(!IS_EMPTY(entry->key)){
            return entry->key;
        }
    }
    return NULL_VAL;
}

static Value iterFile(VM* vm, ObjectIterator* iter, ObjectFile* file){
    if(!file->isOpen || file->handle == NULL){
        return NULL_VAL;
    }

    char buffer[1024];
    if(fgets(buffer, sizeof(buffer), file->handle) != NULL){
        size_t len = strlen(buffer);
        if(len > 0 && buffer[len-1] == '\n'){
            buffer[len-1] = '\0';
            len--;
        }
        return OBJECT_VAL(copyString(vm, buffer, (int)len));
    }
    return NULL_VAL;
}

Value iterNative(VM* vm, int argCount, Value* args){
    if(argCount != 1){
        return NULL_VAL;
    }
    
    Value collection = args[0];
    if(IS_MAP(collection) || IS_LIST(collection) || IS_FILE(collection)){
        return OBJECT_VAL(newIterator(vm, collection));
    }
    
    runtimeError(vm, "Object is not iterable.");
    return NULL_VAL;
}

Value nextNative(VM* vm, int argCount, Value* args){
    if(argCount != 1 || !IS_ITERATOR(args[0])){
        return NULL_VAL;
    }

    ObjectIterator* iter = AS_ITERATOR(args[0]);
    Value receiver = iter->receiver;

    if(IS_LIST(receiver)){
        ObjectList* list = AS_LIST(receiver);
        if(iter->index < list->count){
            return list->items[iter->index++];
        }
    }else if(IS_MAP(receiver)){
        return iterMap(vm, iter, AS_MAP(receiver));
    }else if (IS_FILE(receiver)){
        return iterFile(vm, iter, AS_FILE(receiver));
    }

    return NULL_VAL;
}

void registerIterModule(VM* vm){
    push(vm, OBJECT_VAL(copyString(vm, "iter", 4)));
    push(vm, OBJECT_VAL(newCFunc(vm, iterNative)));
    tableSet(vm, &vm->globals, peek(vm, 1), peek(vm, 0));
    pop(vm); pop(vm);

    push(vm, OBJECT_VAL(copyString(vm, "next", 4)));
    push(vm, OBJECT_VAL(newCFunc(vm, nextNative)));
    tableSet(vm, &vm->globals, peek(vm, 1), peek(vm, 0));
    pop(vm); pop(vm);
}