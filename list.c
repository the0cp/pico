#include "list.h"
#include "object.h"

Value list_push(VM* vm, int argCount, Value* args){
    Value receiver = args[-1];
    if(!IS_LIST(receiver)){
        runtimeError(vm, "list.push: receiver is not a list.");
        return NULL_VAL;
    }

    ObjectList* list = AS_LIST(receiver);
    for(int i = 0; i < argCount; i++){
        appendToList(vm, list, args[i]);
    }
    return NUM_VAL(list->count);
}

Value list_pop(VM* vm, int argCount, Value* args){
    Value receiver = args[-1];
    if(!IS_LIST(receiver)){
        runtimeError(vm, "list.pop: receiver is not a list.");
        return NULL_VAL;
    }

    ObjectList* list = AS_LIST(receiver);
    if(list->count == 0){
        runtimeError(vm, "list.pop: cannot pop from empty list.");
        return NULL_VAL;
    }

    list->count--;
    return list->items[list->count];
}

Value list_size(VM* vm, int argCount, Value* args){
    Value receiver = args[-1];
    if(!IS_LIST(receiver)){
        runtimeError(vm, "list.size: receiver is not a list.");
        return NULL_VAL;
    }

    return NUM_VAL(AS_LIST(receiver)->count);
}