#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "value.h"

void init_chunk(chunk_t *chunk)
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    init_value_array_t(&chunk->constants);

    chunk->line_count = 0;
    chunk->line_capacity = 0;
    chunk->lines = NULL;
}

void free_chunk(chunk_t *chunk)
{
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(line_info_t, chunk->lines, chunk->line_capacity);
    free_value_array_t(&chunk->constants);
    init_chunk(chunk);
}

void write_chunk(chunk_t *chunk, const uint8_t byte, const int line)
{
    if (chunk->capacity < chunk->count + 1) {
        int old_capacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(old_capacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;

    if (chunk->line_count > 0 && chunk->lines[chunk->line_count - 1].line == line) {
        return;
    }
    if (chunk->line_capacity < chunk->line_count + 1) {
        int old_capacity = chunk->line_capacity;
        chunk->line_capacity = GROW_CAPACITY(old_capacity);
        chunk->lines = GROW_ARRAY(line_info_t, chunk->lines, old_capacity, chunk->line_capacity);
    }
    line_info_t *start = &chunk->lines[chunk->line_count++];
    start->offset = chunk->count - 1;
    start->line = line;
}

uint32_t add_constant(chunk_t *chunk, const value_t value)
{
    write_value_array_t(&chunk->constants, value);
    return chunk->constants.count - 1;
}

int get_line(const chunk_t *chunk, const int instruction)
{
    int start = 0;
    int end = chunk->line_count - 1;
    for (;;) {
        int mid = (start + end) / 2;
        const line_info_t *line = &chunk->lines[mid];
        if (instruction < line->offset) {
            end = mid - 1;
        } else if (mid == chunk->line_count - 1 || instruction < chunk->lines[mid + 1].offset) {
            return line->line;
        } else {
            start = mid + 1;
        }
    }
}

void write_constant(chunk_t *chunk, const value_t value, const int line)
{
    int index = add_constant(chunk, value);
    if (index < 256) {
        write_chunk(chunk, OP_CONSTANT, line);
        write_chunk(chunk, (uint8_t)index, line);
    } else {
        write_chunk(chunk, OP_CONSTANT_LONG, line);
        write_chunk(chunk, (uint8_t)(index & 0xff), line);
        write_chunk(chunk, (uint8_t)((index >> 8) & 0xff), line);
        write_chunk(chunk, (uint8_t)((index >> 16) & 0xff), line);
    }
}
