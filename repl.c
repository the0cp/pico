#include "repl.h"
#include "vm.h"

void repl(){
    char line[1024];
    while(true){
        printf(">>> ");
        if(!fgets(line, sizeof(line), stdin)){
            print("\n");
            break;
        }
        interpret(line);
    }
}