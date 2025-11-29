#define _POSIX_C_SOURCE 199309L
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/time.h>
#endif

#include "modules.h"
#include "object.h"
#include "value.h"
#include "vm.h"

static double getSteady(){
#ifdef _WIN32
    static LARGE_INTEGER frequency;
    static int initialized = 0;
    if(!initialized){
        QueryPerformanceFrequency(&frequency);
        initialized = 1;
    }
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec ts;
    if(clock_gettime(CLOCK_MONOTONIC, &ts) == 0){
        return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
    }
    return (double)clock() / (double)CLOCKS_PER_SEC;
#endif
}

static double getSystemHighRes(){
#ifdef _WIN32
    struct timeespec ts;
    if(timespec_get(&ts, TIME_UTC) == 0){
        return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
    }
#else
    struct timespec ts;
    if(clock_gettime(CLOCK_REALTIME, &ts) == 0){
        return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
    }
#endif
    return (double)time(NULL);
}

static Value time_steady(VM* vm, int argCount, Value* args){
    return NUM_VAL(getSteady());
}

static Value time_now(VM* vm, int argCount, Value* args){
    return NUM_VAL(getSystemHighRes());
}

static Value time_system(VM* vm, int argCount, Value* args){
    return NUM_VAL((double)time(NULL));
}

static Value time_sleep(VM* vm, int argCount, Value* args){
    if(argCount != 1 || !IS_NUM(args[0])){
        fprintf(stderr, "time.sleep expects a single numeric argument.\n");
        return NULL_VAL;
    }

    double seconds = AS_NUM(args[0]);
    if(seconds < 0){
        fprintf(stderr, "time.sleep expects a non-negative number.\n");
        return NULL_VAL;
    }

#ifdef _WIN32
    Sleep((DWORD)(seconds * 1000));
#else
    usleep((useconds_t)(seconds * 1e6));
#endif
    return NULL_VAL;
}

static Value time_fmt(VM* vm, int argCount, Value* args){
    if(argCount < 1)    return NULL_VAL;
    if(!IS_NUM(args[0])){
        fprintf(stderr, "time.fmt expects a time number.\n");
        return NULL_VAL;
    }

    time_t rawtime = (time_t)AS_NUM(args[0]);

    const char* format = (argCount >= 2 && IS_STRING(args[1]))
                         ? AS_CSTRING(args[1])
                         : "%Y-%m-%d %H:%M:%S";
    
    struct tm* timeinfo = localtime(&rawtime);
    if(timeinfo == NULL){
        fprintf(stderr, "time.fmt: invalid time value.\n");
        return NULL_VAL;
    }

    char buffer[128];
    size_t len = strftime(buffer, sizeof(buffer), format, timeinfo);
    if(len == 0){
        fprintf(stderr, "time.fmt: formatting error.\n");
        return NULL_VAL;
    }

    return OBJECT_VAL(copyString(vm, buffer, (int)len));
}

static void defineCFunc(VM* vm, HashTable* table, const char* name, CFunc function){
    push(vm, OBJECT_VAL(copyString(vm, name, (int)strlen(name))));
    push(vm, OBJECT_VAL(newCFunc(vm, function)));
    tableSet(vm, table, AS_STRING(peek(vm, 1)), peek(vm, 0));
    pop(vm);
    pop(vm);
}

void registerTimeModule(VM* vm){
    ObjectString* moduleName = copyString(vm, "time", 4);
    push(vm, OBJECT_VAL(moduleName));

    ObjectModule* module = newModule(vm, moduleName);
    push(vm, OBJECT_VAL(module));

    defineCFunc(vm, &module->members, "steady", time_steady);
    defineCFunc(vm, &module->members, "now", time_now);
    defineCFunc(vm, &module->members, "clock", time_system);
    defineCFunc(vm, &module->members, "sleep", time_sleep);
    defineCFunc(vm, &module->members, "fmt", time_fmt);

    tableSet(vm, &vm->modules, moduleName, OBJECT_VAL(module));
    pop(vm);
    pop(vm);
}