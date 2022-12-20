#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

int main(const int argc __unused__, const char *argv[] __unused__)
{
    init_vm();

    chunk_t chunk;
    init_chunk(&chunk);

    /*
    int constant = add_constant(&chunk, 1.2);
    write_chunk(&chunk, OP_CONSTANT, 123);
    write_chunk(&chunk, constant, 123);
    */
    write_constant(&chunk, 1.2, 123);
    write_constant(&chunk, 3.4, 123);
    write_chunk(&chunk, OP_ADD, 123);

    write_constant(&chunk, 5.6, 123);
    write_chunk(&chunk, OP_DIVIDE, 123);
    write_chunk(&chunk, OP_NEGATE, 123);


    write_chunk(&chunk, OP_RETURN, 123);
    //disassemble_chunk(&chunk, "test chunk");

    interpret_result_t r __unused__ = interpret(&chunk);

    free_vm();

    free_chunk(&chunk);

    return 0;
}
