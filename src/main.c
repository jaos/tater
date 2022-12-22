#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

static void repl(void)
{
    char line[1024];
    for (;;) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }
        interpret(line);
        printf("\n");
    }
}

static char *read_file(const char *file_path)
{
    FILE *f = fopen(file_path, "rb");
    if (f == NULL) {
        fprintf(stderr, "Could not read source file \"%s\".\n", file_path);
        exit(74);
    }
    if (fseek(f, 0L, SEEK_END) == -1) {
        perror(file_path);
    }
    const size_t fsize = ftell(f);
    char *buffer = malloc(sizeof *buffer * fsize);
    if (buffer == NULL) {
        fprintf(stderr, "Could not allocate buffer to read \"%s\".\n", file_path);
        exit(74);
    }
    const size_t b_read = fread(buffer, sizeof(char), fsize, f);
    if (b_read < fsize) {
        fprintf(stderr, "Could not read all of \"%s\".\n", file_path);
        exit(74);
    }
    buffer[fsize - 1] = '\0';
    fclose(f);
    return buffer;
}

static void run_file(const char *file_path)
{
    char *source = read_file(file_path);
    interpret_result_t r = interpret(source);
    free(source);
    if (r == INTERPRET_COMPILE_ERROR) exit(65);
    if (r == INTERPRET_RUNTRIME_ERROR) exit(70);
}

int main(const int argc, const char *argv[])
{
    init_vm();

    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        run_file(argv[1]);
    } else {
        fprintf(stderr, "Usage: %s [path]\n", argv[0]);
    }
    free_vm();
    return 0;
}
