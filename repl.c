#include "repl.h"
#include "vm.h"

void repl(){
    char line[1024];
    while(true){
        printf(">>> ");
        if(!fgets(line, sizeof(line), stdin)){
            printf("\n");
            break;
        }
        if(line[0] == '\n') continue;
        interpret(line);
    }
}