#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repl.h"
#include "vm.h"

#include "linenoise.h"

static const char* keywords[] = {
    "and", "break", "class", "continue", "default", "else", "false",
    "for", "func", "if", "import", "method", "null", "or", "print",
    "return", "switch", "this", "true", "var", "while", "system",
    NULL
};

void completion(const char *buf, linenoiseCompletions *lc){
    size_t len = strlen(buf);

    for(int i = 0; keywords[i] != NULL; i++){
        if(strncmp(buf, keywords[i], len) == 0){
            linenoiseAddCompletion(lc, keywords[i]);
        }
    }
}

void repl(VM* vm){
    char* line;
    const char* his = ".pico_history";

    linenoiseSetCompletionCallback(completion);
    linenoiseHistorySetMaxLen(100);
    linenoiseHistoryLoad(his);

    printf("PiCo REPL. Press Ctrl+C to exit.\n");

    while((line = linenoise(">>> ")) != NULL){
        if(line[0] != '\0'){
            linenoiseHistoryAdd(line);
            linenoiseHistorySave(his);
            interpret(vm, line, "<stdin>");
        }
        free(line);
    }
}