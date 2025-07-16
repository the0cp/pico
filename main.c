#include "common.h"
#include "chunk.h"
#include  "debug.h"
#include "vm.h"

int main(int argc, const char* argv[]){
    initVM();
    Chunk chunk;
    initChunk(&chunk);

    /*
    for(int i = 0; i < 260; i++){
        writeConstant(&chunk, (Value)i, i + 1);
    }
    writeChunk(&chunk, OP_RETURN, 261);
    */

    writeConstant(&chunk, (Value)1.22333, 123);
    writeChunk(&chunk, OP_NEGATE, 123);
    writeChunk(&chunk, OP_RETURN, 123);
    interpret(&chunk);
    freeVM();
    freeChunk(&chunk);
    return 0;
}