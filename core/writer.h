#ifndef PICO_WRITER_H
#define PICO_WRITER_H

#include <stddef.h>

typedef void (*WriterFunc)(const char* text, size_t length, void* userData);

typedef struct Writer{
    WriterFunc write;
    void* userData;
} Writer;

void writerW(Writer* writer, const char* text, size_t length);
void writerWCString(Writer* writer, const char* text);
void writerWFormat(Writer* writer, const char* format, ...);

#endif // PICO_WRITER_H