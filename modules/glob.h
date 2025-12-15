#ifndef PICO_MODULES_GLOB_H
#define PICO_MODULES_GLOB_H

#include "vm.h"

void registerGlobModule(VM* vm);
bool glob_match_string(const char* text, const char* pattern, bool ignoreCase);

#endif