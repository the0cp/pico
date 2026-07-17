#ifndef CIETO_MODULES_GLOB_H
#define CIETO_MODULES_GLOB_H

#include "vm.h"

void initGlobModule(VM* vm, ObjectModule* module);
bool glob_match_string(const char* text, const char* pattern, bool ignoreCase);

#endif