#include "common.h"
#include "chunk.h"
#include  "debug.h"
#include "vm.h"
#include "repl.h"
#include "file.h"

int main(int argc, const char* argv[]){
    
    VM vm;

    if(argc == 1){
        initVM(&vm, 0, NULL);
        repl(&vm);
    }else{
        int scriptArgsSt = 1;
        if(strcmp(argv[1], "run") == 0){
            scriptArgsSt = 2;
        }
        if(scriptArgsSt >= argc){
             fprintf(stderr, "Usage: %s [run] [script] [args...]\n", argv[0]);
             return 64;
        }
        initVM(&vm, argc - scriptArgsSt, argv + scriptArgsSt);
        runScript(&vm, argv[scriptArgsSt]);
    }
    
    freeVM(&vm);
    return 0;
}