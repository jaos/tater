#include <stdio.h>
#include "debug.h"
#include "value.h"

void disassemble_chunk(chunk_t *chunk, const char* name)
{
    printf("== %s ==\n", name);
    for (int offset = 0; offset < chunk->count;) {
        offset = disassemble_instruction(chunk, offset);
    }
}

static int simple_instruction(const char *name, const int offset)
{
    printf("%s\n", name);
    return offset + 1;
}

static int constant_instruction(const char *name, chunk_t *chunk, const int offset)
{
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %d '", name, constant);
    print_value(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

static int long_constant_instruction(const char *name, chunk_t *chunk, const int offset)
{
    uint32_t constant = chunk->code[offset + 1] |
        (chunk->code[offset + 2] << 8) |
        (chunk->code[offset + 3] << 16);
    printf("%-16s %4d '", name, constant);
    print_value(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 4;
}

int disassemble_instruction(chunk_t *chunk, const int offset)
{
    printf("%04d ", offset);

    int line = get_line(chunk, offset);
    if (offset > 0 && line == get_line(chunk, offset - 1)) {
        printf("   | ");
    } else {
        printf("%4d ", line);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            return constant_instruction("OP_CONSTANT", chunk, offset);
        case OP_CONSTANT_LONG:
            return long_constant_instruction("OP_CONSTANT_LONG", chunk, offset);
        case OP_RETURN:
            return simple_instruction("OP_RETURN", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}
