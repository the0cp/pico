#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "mem.h"
#include "object.h"
#include "value.h"
#include "hashtable.h"
#include "vm.h"

#include "xxhash.h"

static uint64_t g_hash_seed = 0;

// FNV-1a Hash
/*
static uint64_t hashString(const char* key, int len){
    uint64_t hash = 2166136261u;
    for(int i = 0; i < len; i++){
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}
*/

static void initHashSeed(){
    srand((unsigned int)time(NULL));
    uint64_t p1 = (uint64_t)rand();
    uint64_t p2 = (uint64_t)rand();
    g_hash_seed = (p1 << 32) | p2;

    if(g_hash_seed == 0){
        g_hash_seed = 1;
    }
}

static uint64_t hashString(const char* key, int len){
    if(g_hash_seed == 0){
        initHashSeed();
    }
    return XXH3_64bits_withSeed(key, (size_t)len, g_hash_seed);
}

static ObjectString* allocString(VM* vm, int len, uint64_t hash){
    ObjectString* str = (ObjectString*)reallocate(vm, NULL, 0, sizeof(ObjectString) + len + 1);
    
    str->obj.type = OBJECT_STRING;
    str->obj.isMarked = false;
    str->length = len;
    str->hash = hash;
    str->obj.next = vm->objects;
    vm->objects = (Object*)str;

    return str;
}

ObjectString* copyString(VM* vm, const char* chars, int len){
    uint64_t hash = hashString(chars, len);

    ObjectString* interned = tableGetInternedString(&vm->strings, chars, len, hash);
    if(interned != NULL){
        return interned;
    }

    ObjectString* str = allocString(vm, len, hash);
    memcpy(str->chars, chars, len);
    str->chars[len] = '\0';

    push(vm, OBJECT_VAL(str));
    tableSet(vm, &vm->strings, str, NULL_VAL);
    pop(vm);

    return str;
}

ObjectString* takeString(VM* vm, char* chars, int length){
    uint64_t hash = hashString(chars, length);
    ObjectString* interned = tableGetInternedString(&vm->strings, chars, length, hash);
    if (interned != NULL) {
        reallocate(vm, chars, length + 1, 0); 
        return interned;
    }

    ObjectString* string = allocString(vm, length, hash);
    memcpy(string->chars, chars, length);
    string->chars[length] = '\0';

    push(vm, OBJECT_VAL(string));
    tableSet(vm, &vm->strings, string, NULL_VAL);
    pop(vm);

    return string;
}

ObjectList* newList(VM* vm){
    ObjectList* list = (ObjectList*)reallocate(vm, NULL, 0, sizeof(ObjectList));
    list->obj.type = OBJECT_LIST;
    list->obj.isMarked = false;
    list->count = 0;
    list->capacity = 0;
    list->items = NULL;

    list->obj.next = vm->objects;
    vm->objects = (Object*)list;

    return list;
}

void appendToList(VM* vm, ObjectList* list, Value value){
    if(list->count + 1 > list->capacity){
        size_t oldCapacity = list->capacity;
        list->capacity = oldCapacity < 8 ? 8 : oldCapacity * 2;
        list->items = (Value*)reallocate(
            vm,
            list->items,
            sizeof(Value) * oldCapacity,
            sizeof(Value) * list->capacity
        );
    }
    list->items[list->count++] = value;
}

ObjectFunc* newFunction(VM* vm){
    ObjectFunc* func = (ObjectFunc*)reallocate(vm, NULL, 0, sizeof(ObjectFunc));
    func->obj.type = OBJECT_FUNC;
    func->obj.isMarked = false;
    func->arity = 0;
    func->upvalueCnt = 0;
    func->name = NULL;
    func->srcName = NULL;
    func->type = TYPE_SCRIPT;
    func->fieldOwner = NULL;
    initChunk(&func->chunk);

    func->obj.next = vm->objects;
    vm->objects = (Object*)func;
    return func;
}

ObjectCFunc* newCFunc(VM* vm, CFunc func){
    ObjectCFunc* cfunc = (ObjectCFunc*)reallocate(vm, NULL, 0, sizeof(ObjectCFunc));
    cfunc->obj.type = OBJECT_CFUNC;
    cfunc->obj.isMarked = false;
    cfunc->func = func;
    cfunc->obj.isMarked = false;

    cfunc->obj.next = vm->objects;
    vm->objects = (Object*)cfunc;
    return cfunc;
}

ObjectModule* newModule(VM* vm, ObjectString* name){
    ObjectModule* module = (ObjectModule*)reallocate(vm, NULL, 0, sizeof(ObjectModule));
    module->obj.type = OBJECT_MODULE;
    module->obj.isMarked = false;
    module->name = name;
    initHashTable(&module->members);
    
    module->obj.next = vm->objects;
    vm->objects = (Object*)module;

    return module;
}

ObjectUpvalue* newUpvalue(VM* vm, Value* slot){
    ObjectUpvalue* upvalue = (ObjectUpvalue*)reallocate(vm, NULL, 0, sizeof(ObjectUpvalue));
    upvalue->obj.type = OBJECT_UPVALUE;
    upvalue->obj.isMarked = false;
    upvalue->location = slot;   // value on stack
    upvalue->closed = NULL_VAL;
    upvalue->next = NULL;

    upvalue->obj.next = vm->objects;
    vm->objects = (Object*)upvalue;
    return upvalue;
}

ObjectClosure* newClosure(VM* vm, ObjectFunc* func){
    size_t size = sizeof(ObjectClosure) + sizeof(ObjectUpvalue*) * func->upvalueCnt;
    ObjectClosure* closure = (ObjectClosure*)reallocate(vm, NULL, 0, size);

    closure->obj.type = OBJECT_CLOSURE;
    closure->obj.isMarked = false;
    closure->func = func;
    closure->upvalueCnt = func->upvalueCnt;

    for(int i = 0; i < func->upvalueCnt; i++){
        closure->upvalues[i] = NULL;
    }

    closure->obj.next = vm->objects;
    vm->objects = (Object*)closure;

    return closure;
}

ObjectClass* newClass(VM* vm, ObjectString* name){
    ObjectClass* klass = (ObjectClass*)reallocate(vm, NULL, 0, sizeof(ObjectClass));
    klass->obj.type = OBJECT_CLASS;
    klass->obj.isMarked = false;
    klass->obj.next = vm->objects;
    vm->objects = (Object*)klass;

    klass->name = name;
    initHashTable(&klass->methods);
    initHashTable(&klass->fields);

    return klass;
}

ObjectInstance* newInstance(VM* vm, ObjectClass* klass){
    ObjectInstance* instance = (ObjectInstance*)reallocate(vm, NULL, 0, sizeof(ObjectInstance));
    instance->obj.type = OBJECT_INSTANCE;
    instance->obj.isMarked = false;

    instance->obj.next = vm->objects;
    vm->objects = (Object*)instance;

    instance->klass = klass;
    initHashTable(&instance->fields);

    push(vm, OBJECT_VAL(instance));
    tableMerge(vm, &klass->fields, &instance->fields);
    pop(vm);

    return instance;
}

ObjectBoundMethod* newBoundMethod(VM* vm, Value receiver, ObjectClosure* method){
    ObjectBoundMethod* bound = (ObjectBoundMethod*)reallocate(vm, NULL, 0, sizeof(ObjectBoundMethod));
    bound->obj.type = OBJECT_BOUND_METHOD;
    bound->obj.isMarked = false;

    bound->obj.next = vm->objects;
    vm->objects = (Object*)bound;
    
    bound->receiver = receiver;
    bound->method = method;

    return bound;
}

void freeObject(VM* vm, Object* object){
    switch(object->type){
        case OBJECT_STRING:{
            ObjectString* string = (ObjectString*)object;
            reallocate(vm, object, sizeof(ObjectString) + string->length + 1, 0);
            break;
        }
        case OBJECT_LIST:{
            ObjectList* list = (ObjectList*)object;
            reallocate(vm, list->items, sizeof(Value) * list->capacity, 0);
            reallocate(vm, object, sizeof(ObjectList), 0);
            break;
        }
        case OBJECT_FUNC:{
            ObjectFunc* func = (ObjectFunc*)object;
            freeChunk(vm, &func->chunk);
            reallocate(vm, object, sizeof(ObjectFunc), 0);
            break;
        }
        case OBJECT_CFUNC:{
            reallocate(vm, object, sizeof(ObjectCFunc), 0);
            break;
        }
        case OBJECT_MODULE:{
            ObjectModule* module = (ObjectModule*)object;
            freeHashTable(vm, &module->members);
            reallocate(vm, module, sizeof(ObjectModule), 0);
            break;
        }
        case OBJECT_CLOSURE:{
            ObjectClosure* closure = (ObjectClosure*)object;
            size_t size = sizeof(ObjectClosure) + sizeof(ObjectUpvalue*) * closure->upvalueCnt;
            reallocate(vm, object, size, 0);
            break;
        }
        case OBJECT_UPVALUE:{
            reallocate(vm, object, sizeof(ObjectUpvalue), 0);
            break;
        }
        case OBJECT_CLASS:{
            ObjectClass* klass = (ObjectClass*)object;
            freeHashTable(vm, &klass->methods);
            freeHashTable(vm, &klass->fields);
            reallocate(vm, object, sizeof(ObjectClass), 0);
            break;
        }
        case OBJECT_INSTANCE:{
            ObjectInstance* instance = (ObjectInstance*)object;
            freeHashTable(vm, &instance->fields);
            reallocate(vm, object, sizeof(ObjectInstance), 0);
            break;
        }
        case OBJECT_BOUND_METHOD:{
            reallocate(vm, object, sizeof(ObjectBoundMethod), 0);
            break;
        }
    }
}

void freeObjects(VM* vm){
    Object* object = vm->objects;
    while(object != NULL){
        Object* next = object->next;
        freeObject(vm, object);
        object = next;
    }
}

void printObject(Value value){
    switch(OBJECT_TYPE(value)){
        case OBJECT_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJECT_LIST:
        {
            ObjectList* list = AS_LIST(value);
            printf("[");
            for(int i = 0; i < list->count; i++){
                printValue(list->items[i]);
                if(i < list->count - 1) printf(", ");
            }
            printf("]");
            break;
        }
        case OBJECT_FUNC:
            if(AS_FUNC(value)->name == NULL)
                printf("<script>");
            else
                printf("<fn %s>", AS_FUNC(value)->name->chars);
            break;
        case OBJECT_CFUNC:
            printf("<cfunc>");
            break;
        case OBJECT_MODULE:
            printf("<module '%s'>", AS_MODULE(value)->name->chars);
            break;
        case OBJECT_CLOSURE:
            if(AS_CLOSURE(value)->func->name == NULL)
                printf("<script>");
            else
                printf("<fn %s>", AS_CLOSURE(value)->func->name->chars);
            break;
        case OBJECT_UPVALUE:
            printf("<upvalue>");
            break;
        case OBJECT_CLASS:
            printf("<class %s>", AS_CLASS(value)->name->chars);
            break;
        case OBJECT_INSTANCE:
            printf("<instance of %s>", AS_INSTANCE(value)->klass->name->chars);
            break;
        case OBJECT_BOUND_METHOD:
            printf("<bound method %s>", AS_BOUND_METHOD(value)->method->func->name->chars);
            break;
    }
}