#include "buffer.h"

#include <string.h>

#include "err.h"

buffer_t *buffer_new() {
    buffer_t *buffer = malloc(sizeof *buffer);
    ENSURE(buffer != NULL);

    buffer->buf = malloc(BASE_CAPACITY * sizeof *buffer);
    ENSURE(buffer->buf != NULL);
    buffer->capacity = BASE_CAPACITY;
    buffer->size = 0;

    return buffer;
}

void buffer_free(buffer_t *buffer) {
    if (buffer && buffer->buf)
        free(buffer->buf);
    free(buffer);
}

void buffer_push(buffer_t *buffer, void *data, size_t size) {
    if (buffer->size + size >= buffer->capacity) {
        buffer->capacity *= 2;
        buffer->buf = realloc(buffer->buf, buffer->capacity * sizeof *buffer->buf);
        ENSURE(buffer->buf != NULL);
    }

    memcpy(buffer->buf + buffer->size, data, size);
    buffer->size += size;
}

void buffer_clear(buffer_t *buffer) {
    buffer->size = 0;
}