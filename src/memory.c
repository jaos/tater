#include <stdlib.h>

#include "memory.h"
#include "vm.h"

void *reallocate(void *pointer, const size_t old_size __unused__, const size_t new_size)
{
    if (new_size == 0) {
        free(pointer);
        return NULL;
    }

    void *result = realloc(pointer, new_size);
    if (result == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }
    return result;
}

static void free_object(obj_t *o)
{
    switch (o->type) {
        case OBJ_FUNCTION: {
            obj_function_t *function = (obj_function_t*)o;
            free_chunk(&function->chunk);
            FREE(obj_function_t, o);
            break;
        }
        case OBJ_NATIVE: {
            FREE(obj_native_t, o);
            break;
        }
        case OBJ_STRING: {
            obj_string_t *s = (obj_string_t*)o;
            reallocate(o, sizeof(obj_string_t) + s->length + 1, 0);
            break;
        }
        default: return; // unreachable
    }
}

void free_objects(void)
{
    obj_t *o = vm.objects;
    while (o != NULL) {
        obj_t *next = o->next;
        free_object(o);
        o = next;
    }
}
