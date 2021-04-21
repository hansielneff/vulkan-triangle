#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

uint8_t *readBinaryFile(const char *filename, uint32_t *filesize) {
    FILE *file = fopen(filename, "rb");
    uint32_t size = 4096;
    uint32_t sizeIncrement = size;
    uint8_t *buffer = (uint8_t *) malloc(size);

    if (!buffer || !file) return NULL;

    // Read file one byte at the time
    uint32_t c, bytesRead = 0;
    while ((c = getc(file)) != EOF) {
        // Allocate more memory if we run out
        if (bytesRead == size) {
            size += sizeIncrement;
            buffer = (uint8_t *) realloc(buffer, size);
            if (!buffer) return NULL;
        }

        buffer[bytesRead++] = (uint8_t) c;
    }
    
    // Shrink allocation to fit file size exactly
    buffer = (uint8_t *) realloc(buffer, bytesRead);
    if (!buffer) return NULL;

    fclose(file);

    if (filesize) *filesize = bytesRead;

    return buffer;
}

#endif