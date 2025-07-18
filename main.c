#include "common.h"
#include "chunk.h"
#include  "debug.h"
#include "vm.h"
#include "repl.h"
#include "file.h"

int main(int argc, const char* argv[]){
    initVM();

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
        repl();
    }else if(argc == 2){
        runScript(argv[1]);
    }else{
        fprintf(stderr, "Usage: %s [script]\n", argv[0]);
        return 1;
    }
    freeVM();
    return 0;
}