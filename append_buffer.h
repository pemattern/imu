#ifndef APPEND_BUFFER_H_
#define APPEND_BUFFER_H_

#define APPEND_BUFFER_INIT {NULL, 0}

struct append_buffer
{
    char *buffer;
    int length;
};

void append(struct append_buffer *ab, const char *string, int length);
void free_append_buffer(struct append_buffer *ab);

#endif