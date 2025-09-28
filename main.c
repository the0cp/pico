#include "common.h"
#include "chunk.h"
#include  "debug.h"
#include "vm.h"
#include "repl.h"
#include "file.h"

int main(int argc, const char* argv[]){
    
    VM vm;

    initVM(&vm);

    /*
    for(int i = 0; i < 260; i++){
        writeConstant(&chunk, (Value)i, i + 1);
    }
    writeChunk(&chunk, OP_RETURN, 261);
    */

    /*
    writeConstant(&chunk, (Value)1.22333, 123);
    writeChunk(&chunk, OP_NEGATE, 123);
    writeConstant(&chunk, (Value)2.5, 123);
    writeChunk(&chunk, OP_ADD, 123);
    writeChunk(&chunk, OP_RETURN, 123);
    interpret(&chunk);
    freeVM();
    freeChunk(&chunk);
    */

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