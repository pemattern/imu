#include <string.h>
#include <stdlib.h>

#include "append_buffer.h"

void append(struct append_buffer *ab, const char *string, int length)
{
    char *new = realloc(ab->buffer, ab->length + length);

    if (new == NULL) return;
    memcpy(&new[ab->length], string, length);
    ab->buffer = new;
    ab->length += length;
}

void free_append_buffer(struct append_buffer *ab)
{
    free(ab->buffer);
}