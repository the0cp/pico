#ifndef PICO_REPL_H
#define PICO_REPL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct VM VM;

void repl(VM* vm);

#endif // PICO_REPL_H