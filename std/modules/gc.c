#include <string.h>

#include "registry.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "mem.h"
#include "gc_policy.h"
#include "gc.h"

static Value gc_mode(VM* vm, int argCount, Value* args){
    if(argCount == 0){
        const char* modeStr = gcModeName(vm->gcMode);
        return OBJECT_VAL(copyString(vm, modeStr, (int)strlen(modeStr)));
    }

    if(argCount != 1 || !IS_STRING(args[0])){
        runtimeError(vm, "gc.mode expects a single string argument.\n");
        return NULL_VAL;
    }

    GCMode mode;
    const char* modeStr = AS_CSTRING(args[0]);

    if(!gcModeFromString(modeStr, &mode)){
        runtimeError(vm, "Invalid GC mode: %s. Valid modes are 'auto', 'manual', 'off'.\n", modeStr);
        return NULL_VAL;
    }

    gcSetMode(vm, mode);
    return args[0];
}

static Value gc_collect(VM* vm, int argCount, Value* args){
    if(argCount != 0){
        runtimeError(vm, "gc.collect does not take any arguments.\n");
        return NULL_VAL;
    }

    return BOOL_VAL(gcCollect(vm, GC_REASON_MANUAL));
}

static Value gc_threshold(VM* vm, int argCount, Value* args){
    if(argCount == 0){
        return NUM_VAL((double)vm->gcThreshold);
    }

    if(argCount != 1 || !IS_NUM(args[0])){
        runtimeError(vm, "gc.threshold expects a single numeric argument.\n");
        return NULL_VAL;
    }

    double threshold = AS_NUM(args[0]);

    if(threshold < 0){
        runtimeError(vm, "gc.threshold expects a non-negative number.\n");
        return NULL_VAL;
    }

    vm->gcThreshold = (size_t)threshold;
    vm->nextGC = vm->gcThreshold;

    return NUM_VAL((double)vm->gcThreshold);
}

static int countObjects(VM* vm){
    int count = 0;
    Object* obj = vm->objects;
    while(obj){
        count++;
        obj = obj->next;
    }
    return count;
}

static void mapCString(VM* vm, ObjectMap* map, const char* key, Value value){
    push(vm, value);

    ObjectString* keyStr = copyString(vm, key, (int)strlen(key));
    push(vm, OBJECT_VAL(keyStr));

    tableSet(vm, &map->table, OBJECT_VAL(keyStr), value);

    pop(vm);    // keyStr
    pop(vm);    // value
}

static Value gc_stats(VM* vm, int argCount, Value* args){
    if(argCount != 0){
        runtimeError(vm, "gc.stats does not take any arguments.\n");
        return NULL_VAL;
    }

    ObjectMap* statsMap = newMap(vm);
    push(vm, OBJECT_VAL(statsMap));

    const char* modeName = gcModeName(vm->gcMode);
    ObjectString* modeStr = copyString(vm, modeName, (int)strlen(modeName));

    mapCString(vm, statsMap, "mode", OBJECT_VAL(modeStr));
    mapCString(vm, statsMap, "bytes", NUM_VAL((double)vm->bytesAllocated));
    mapCString(vm, statsMap, "next", NUM_VAL((double)vm->nextGC));
    mapCString(vm, statsMap, "threshold", NUM_VAL((double)vm->gcThreshold));
    mapCString(vm, statsMap, "objects", NUM_VAL((double)countObjects(vm)));
    mapCString(vm, statsMap, "running", BOOL_VAL(vm->gcRunning));
    mapCString(vm, statsMap, "policy",
        OBJECT_VAL(copyString(vm,
            vm->gcPolicy != NULL ? vm->gcPolicy->name : "none",
            (int)strlen(vm->gcPolicy != NULL ? vm->gcPolicy->name : "none")
        ))
    );

    pop(vm);
    return OBJECT_VAL(statsMap);
}

void initGcModule(VM* vm, ObjectModule* module){
    defineCFunc(vm, &module->members, "mode", gc_mode);
    defineCFunc(vm, &module->members, "collect", gc_collect);
    defineCFunc(vm, &module->members, "threshold", gc_threshold);
    defineCFunc(vm, &module->members, "stats", gc_stats);
}