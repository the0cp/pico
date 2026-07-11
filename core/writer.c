#include "writer.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void writerW(Writer* writer, const char* text, size_t length){
    if(writer == NULL || writer->write == NULL || text == NULL || length == 0){
        return;
    }

    writer->write(text, length, writer->userData);
}

void writerWCString(Writer* writer, const char* text){
    if(text == NULL){
        return;
    }

    writerW(writer, text, strlen(text));
}

void writerWFormat(Writer* writer, const char* format, ...){
    if(writer == NULL || writer->write == NULL || format == NULL){
        return;
    }

    char buffer[256];

    va_list args;
    va_start(args, format);
    int length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if(length < 0){
        return;
    }

    if((size_t)length >= sizeof(buffer)){
        length = (int)sizeof(buffer) - 1;
    }

    writerW(writer, buffer, (size_t)length);
}