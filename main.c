#include "common.h"
#include "chunk.h"
#include  "debug.h"

int main(int argc, const char* argv[]){
    Chunk chunk;
    initChunk(&chunk);
    for(int i = 0; i < 260; i++){
        writeConstant(&chunk, (Value)i, i + 1);
    }
    writeChunk(&chunk, OP_RETURN, 261);
    dasmChunk(&chunk, "Test Chunk");
    freeChunk(&chunk);
    return 0;
}