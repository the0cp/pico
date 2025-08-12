#include "repl.h"
#include "vm.h"

void repl(VM* vm){
    char line[1024];
    while(true){
        printf(">>> ");
        if(!fgets(line, sizeof(line), stdin)){
            printf("\n");
            break;
        }
        line[strcspn(line, "\n")] = 0;
        if(line[0] == '\n') continue;
        interpret(vm, line);
    }
}