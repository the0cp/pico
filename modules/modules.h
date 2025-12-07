#ifndef PICO_MODULES_H
#define PICO_MODULES_H

#include "vm.h"

void registerFsModule(VM* vm);
void registerTimeModule(VM* vm);
void registerOsModule(VM* vm);
void registerPathModule(VM* vm);
void registerGlobModule(VM* vm);

#endif // PICO_MODULES_H