#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "vm.h"

void *reallocate(void *pointer, const size_t old_size __unused__, const size_t new_size)
{
    vm.bytes_allocated += new_size - old_size;
    if (new_size > old_size) {
        if (vm.flags & VM_FLAG_GC_STRESS || vm.bytes_allocated > vm.next_garbage_collect) {
            vm_collect_garbage();
        }
    }

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
