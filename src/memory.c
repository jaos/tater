#include <stdlib.h>

#include "memory.h"

void *reallocate(void *pointer, size_t old_size, size_t new_size)
{
    if (new_size == 0) {
        free(pointer);
        return NULL;
    }

    (void)old_size;

    void *result = realloc(pointer, new_size);
    if (result == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }
    return result;
}
