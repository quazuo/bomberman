#ifndef ROBOTS_BUFFER
#define ROBOTS_BUFFER

#include <stdlib.h>

#define BASE_CAPACITY 256

typedef struct buffer {
    char *buf;
    size_t capacity;
    size_t size;
} buffer_t;

buffer_t *buffer_new();

void buffer_free(buffer_t *buffer);

void buffer_push(buffer_t *buffer, void *data, size_t size);

#endif // ROBOTS_BUFFER