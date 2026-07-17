#ifndef CIETO_REPL_H
#define CIETO_REPL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct VM VM;

void repl(VM* vm);

#endif // CIETO_REPL_H