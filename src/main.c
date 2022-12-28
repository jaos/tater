#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

#define CLOX_PROMPT "clox> "
#define CLOX_HISTORY_FILE ".clox_history"

static char *keywords(const char *input, const int state __unused__)
{
    static int list_index, len;
    if (!state) {
        list_index = 0;
        len = strlen(input);
    }
    char *name;
    while ((name = (char *)token_keyword_names[list_index++]) != NULL) {
        if (strncmp(name, input, len) == 0) {
            return strdup(name);
        }
    }
    return NULL;
}

static char **completer(const char *input __unused__, const int start __unused__, const int end __unused__)
{
    rl_attempted_completion_over = 1;
    return rl_completion_matches(input, keywords);
}

static void repl(void)
{
    rl_attempted_completion_function = completer;

    char history_path[255] = {0};
    char *home = getenv("HOME");
    if (home != NULL) {
        if (snprintf(history_path, 255, "%s/%s", home, CLOX_HISTORY_FILE) == -1) {
            exit(EXIT_FAILURE);
        }
        FILE *h = fopen(history_path, "a");
        if (h == NULL) {
            history_path[0] = '\0';
        } else {
            fclose(h);
        }
    }

    if (*history_path) {
        if (read_history(history_path)) {
            perror(gettext("Failed to ready history file"));
        }
    }

    int status = EXIT_SUCCESS;

    for (;;) {
        char *line = readline(CLOX_PROMPT);
        if (line != NULL) {
            if (line && *line)
                add_history(line);
            const interpret_result_t rv = interpret(line);
            printf("\n");
            free(line);
            if (rv == INTERPRET_EXIT) {
                status = vm.exit_status;
                break;
            } else if (rv == INTERPRET_EXIT_OK) {
                break;
            }
        } else {
            break;
        }
    }
    if (*history_path) {
        if (write_history(history_path)) {
            perror(gettext("Failed to write history file"));
        }
    }
    exit(status);
}

static char *read_file(const char *file_path)
{
    FILE *f = fopen(file_path, "rb");
    if (f == NULL) {
        fprintf(stderr, "Could not read source file \"%s\".\n", file_path);
        exit(EXIT_FAILURE);
    }
    if (fseek(f, 0L, SEEK_END) == -1) {
        perror(file_path);
        exit(EXIT_FAILURE);
    }
    const size_t fsize = ftell(f);
    char *buffer = malloc(sizeof *buffer * fsize);
    if (buffer == NULL) {
        fprintf(stderr, "Could not allocate buffer to read \"%s\".\n", file_path);
        exit(EXIT_FAILURE);
    }
    rewind(f);
    const size_t b_read = fread(buffer, sizeof(char), fsize, f);
    if (b_read < fsize) {
        fprintf(stderr, "Could not read all of \"%s\" (expected %ld, got %ld).\n", file_path, fsize, b_read);
        exit(EXIT_FAILURE);
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
    switch (r) {
        case INTERPRET_OK:
        case INTERPRET_EXIT_OK:
            exit(EXIT_SUCCESS);
            break;
        default:
            exit(EXIT_FAILURE);
    }
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
