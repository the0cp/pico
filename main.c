#include "common.h"
#include "chunk.h"
#include  "debug.h"
#include "vm.h"
#include "repl.h"
#include "file.h"

int main(int argc, const char* argv[]){
    
    VM vm;

    initVM(&vm);

    if(argc == 1){
        repl(&vm);
    }else if(argc == 2){
        runScript(&vm, argv[1]);
    }else if(argc == 3 && strcmp(argv[1], "run") == 0){
        runScript(&vm, argv[2]);
    }else{
        fprintf(stderr, "Usage: %s [script]\n", argv[0]);
        return 64;
    }
    freeVM(&vm);
    return 0;
}