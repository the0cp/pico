#include <stdlib.h>
#include <stdio.h>

#include "mem.h"
#include "compiler.h"

#define GC_HEAP_GROW_FACTOR 2

void collectGarbage(VM* vm){
    size_t before = vm->bytesAllocated;
    printf("[GC Begin] size: %zu, next: %zu\n", before, vm->nextGC);
    markRoots(vm);
    //tableRemoveWhite(vm, &vm->strings);
    sweep(vm);
    vm->nextGC = vm->bytesAllocated * GC_HEAP_GROW_FACTOR;
    printf("[GC End]   Freed: %zu, new size: %zu, next: %zu\n",
           before - vm->bytesAllocated, vm->bytesAllocated, vm->nextGC);
}

static void traceRef(VM* vm, Object* object){
    switch(object->type){
        case OBJECT_FUNC:{
            ObjectFunc* func = (ObjectFunc*)object;
            markObject(vm, (Object*)func->name);
            markObject(vm, (Object*)func->srcName);
            markArray(vm, &func->chunk.constants);
            break;
        }
        case OBJECT_CLOSURE:{
            ObjectClosure* closure = (ObjectClosure*)object;
            markObject(vm, (Object*)closure->func);
            for(int i = 0; i < closure->upvalueCnt; i++){
                markObject(vm, (Object*)closure->upvalues[i]);
            }
            break;
        }
        case OBJECT_UPVALUE:{
            markValue(vm, ((ObjectUpvalue*)object)->closed);
            break;
        }
        case OBJECT_MODULE:{
            ObjectModule* module = (ObjectModule*)object;
            markObject(vm, (Object*)module->name);
            markTable(vm, &module->members);
            break;
        }
        case OBJECT_STRING:
        case OBJECT_CFUNC:  
            break;
    }
}

void markObject(VM* vm, Object* object){
    if(object == NULL)  return;
    if(object->isMarked)    return;

    object->isMarked = true;
    traceRef(vm, object);
}

void markValue(VM* vm, Value value){
    if(IS_OBJECT(value)){
        markObject(vm, AS_OBJECT(value));
    }
}

void markArray(VM* vm, ValueArray* array){
    for(int i = 0; i < array->count; i++){
        markValue(vm, array->values[i]);
    }
}

static void markRoots(VM* vm){
    for(Value* slot = vm->stack; slot < vm->stackTop; slot++){
        markValue(vm, *slot);
    }

    for(int i = 0; i < vm->frameCount; i++){
        markObject(vm, (Object*)vm->frames[i].closure);
    }

    for(ObjectUpvalue* upvalue = vm->openUpvalues; upvalue != NULL; upvalue = upvalue->next){
        markObject(vm, (Object*)upvalue);
    }

    markTable(vm, vm->curGlobal);

    for(int i = 0; i < vm->globalCnt; i++){
        markTable(vm, vm->globalStack[i]);
    }

    markTable(vm, &vm->modules);

    markTable(vm, &vm->strings);

    if(vm->compiler != NULL){
        markCompilerRoots(vm);
    }
}

static void sweep(VM* vm){
    Object* prev = NULL;
    Object* object = vm->objects;
    while(object != NULL){
        if(object->isMarked){
            object->isMarked = false;
            prev = object;
            object = object->next;
        }else{
            Object* unreached = object;
            object = object->next;
            if(prev != NULL){
                prev->next = object;
            }else{  // prev is head
                vm->objects = object;
            }
            freeObject(vm, unreached);
        }
    }
}

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