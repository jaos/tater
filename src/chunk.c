#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "value.h"
#include "vm.h"

void chunk_t_init(chunk_t *chunk)
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    value_array_t_init(&chunk->constants);
    chunk->line_count = 0;
    chunk->line_capacity = 0;
    chunk->lines = NULL;
}

void chunk_t_free(chunk_t *chunk)
{
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(line_info_t, chunk->lines, chunk->line_capacity);
    value_array_t_free(&chunk->constants);
    chunk_t_init(chunk);
}

void chunk_t_write(chunk_t *chunk, const uint8_t byte, const int line)
{
    if (chunk->capacity < chunk->count + 1) {
        const int old_capacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(old_capacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
        if (chunk->code == NULL) {
            fprintf(stderr, gettext("Could not allocate chunk code storage."));
            exit(EXIT_FAILURE);
        }
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;

    if (chunk->line_count > 0 && chunk->lines[chunk->line_count - 1].line == line) {
        return;
    }

    if (chunk->line_capacity < chunk->line_count + 1) {
        const int old_capacity = chunk->line_capacity;
        chunk->line_capacity = GROW_CAPACITY(old_capacity);
        chunk->lines = GROW_ARRAY(line_info_t, chunk->lines, old_capacity, chunk->line_capacity);
        if (chunk->lines == NULL) {
            fprintf(stderr, gettext("Could not allocate chunk line storage."));
            exit(EXIT_FAILURE);
        }
    }
    line_info_t *line_info = &chunk->lines[chunk->line_count++];
    line_info->offset = chunk->count - 1;
    line_info->line = line;
}

int chunk_t_get_line(const chunk_t *chunk, const int instruction)
{
    int start = 0;
    int end = chunk->line_count - 1;
    for (;;) {
        const int mid = (start + end) / 2;
        line_info_t *line = &chunk->lines[mid];
        if (instruction < line->offset) {
            end = mid - 1;
        } else if (mid == chunk->line_count - 1 || instruction < chunk->lines[mid + 1].offset) {
            return line->line;
        } else {
            start = mid + 1;
        }
    }
}

int chunk_t_add_constant(chunk_t *chunk, const value_t value)
{
    vm_push(value); // make GC happy
    value_array_t_add(&chunk->constants, value);
    vm_pop(); // make GC happy
    return chunk->constants.count - 1;
}

/* Used by OP_CONSTANT_LONG
void write_constant(chunk_t *chunk, const value_t value, const int line)
{
    int index = chunk_t_add_constant(chunk, value);
    if (index < 256) {
        chunk_t_write(chunk, OP_CONSTANT, line);
        chunk_t_write(chunk, (uint8_t)index, line);
    } else {
        chunk_t_write(chunk, OP_CONSTANT_LONG, line);
        chunk_t_write(chunk, (uint8_t)(index & 0xff), line);
        chunk_t_write(chunk, (uint8_t)((index >> 8) & 0xff), line);
        chunk_t_write(chunk, (uint8_t)((index >> 16) & 0xff), line);
    }
}
*/
