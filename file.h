#ifndef FILE_H
#define FILE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"

char* read(const char* path);
void runScript(VM* vm, const char* path);

#endif // FILE_H