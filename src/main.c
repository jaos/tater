/*
 * Copyright (C) 2022-2023 Jason Woodward <woodwardj at jaos dot org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <readline/readline.h>
#include <readline/history.h>
# pragma GCC diagnostic pop

#include "common.h"
#include "debug.h"
#include "vm.h"
#include "vmopcodes.h"

#define TATER_PROMPT "tater> "
#define TATER_HISTORY_FILE ".tater_history"

static char *complete(const char *input, const int state)
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

static char **completer(const char *input, const int start __unused__, const int end __unused__)
{
    rl_attempted_completion_over = 1;
    return rl_completion_matches(input, complete);
}

static int repl(void)
{
    rl_attempted_completion_function = completer;

    char history_path[255] = {0};
    char *home = getenv("HOME");
    if (home != NULL) {
        if (snprintf(history_path, 255, "%s/%s", home, TATER_HISTORY_FILE) == -1) {
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
    bool is_a_tty = isatty(fileno(stdin));

    for (;;) {
        char *line = NULL;
        if (is_a_tty) {
            line = readline(TATER_PROMPT);
        } else {
            size_t getline_len = 0;
            ssize_t getline_size = getline(&line, &getline_len, stdin);
            if (getline_size == -1) {
                line = NULL;
            } else {
                line[getline_size - 1] = '\0'; // chop newline
            }
        }

        if (line != NULL) {
            if (line && *line && is_a_tty)
                add_history(line);

            const vm_t_interpret_result_t rv = vm_t_interpret(line);
            if (is_a_tty) printf("\n");
            free(line);

            if (rv == INTERPRET_EXIT || rv == INTERPRET_RUNTIME_ERROR) {
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
    return (status);
}

static char *read_file(const char *file_path)
{
    FILE *f = fopen(file_path, "rb");
    if (f == NULL) {
        fprintf(stderr, gettext("Could not read source file \"%s\".\n"), file_path);
        exit(EXIT_FAILURE);
    }
    if (fseek(f, 0L, SEEK_END) == -1) {
        perror(file_path);
        exit(EXIT_FAILURE);
    }
    const size_t fsize = ftell(f);
    char *buffer = malloc(sizeof *buffer * fsize);
    if (buffer == NULL) {
        fprintf(stderr, gettext("Could not allocate buffer to read \"%s\".\n"), file_path);
        exit(EXIT_FAILURE);
    }
    rewind(f);
    const size_t b_read = fread(buffer, sizeof(char), fsize, f);
    if (b_read < fsize) {
        fprintf(stderr, gettext("Could not read all of \"%s\" (expected %ld, got %ld).\n"), file_path, fsize, b_read);
        exit(EXIT_FAILURE);
    }
    buffer[fsize - 1] = '\0';
    fclose(f);
    return buffer;
}

static int run_file(const char *file_path)
{
    char *source = read_file(file_path);
    vm_t_interpret_result_t r = vm_t_interpret(source);
    free(source);
    switch (r) {
        case INTERPRET_OK:
        case INTERPRET_EXIT_OK:
            return EXIT_SUCCESS;
        default:
            return EXIT_FAILURE;
    }
}

static void version(const char *name)
{
    printf(gettext("%s version %s\n"), name, VERSION);
    printf("\n");
    printf("This program is free software; you can redistribute it and/or modify\n");
    printf("it under the terms of the GNU General Public License as published by\n");
    printf("the Free Software Foundation; either version 2 of the License, or\n");
    printf("any later version.\n");
    printf("This program is distributed in the hope that it will be useful,\n");
    printf("\n");
    printf("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
    printf("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
    printf("GNU Library General Public License for more details.\n");
    printf("\n");
    printf("You should have received a copy of the GNU General Public License\n");
    printf("along with this program; if not, write to the Free Software\n");
    printf("Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.\n");
}

static void help(const char *name)
{
    printf(gettext("Usage: %s [options] [path | -]\n"), name);
    printf("  -d, --debug     %s\n", gettext("Enable debugging"));
    printf("  -s, --gc-stress %s\n", gettext("Enable garbage collector stress testing"));
    printf("  -t, --gc-trace  %s\n", gettext("Enable garbage collector tracing"));
    printf("  -v, --version   %s\n", gettext("Show version"));
    printf("  -h, --help      %s\n", gettext("This help"));
}

#define HELP_OPT 'h'
#define VERSION_OPT 'v'
#define DEBUG_OPT 'd'
#define GC_STRESS_OPT 's'
#define GC_TRACE_OPT 't'

int main(const int argc, const char *argv[])
{
    bool debug = false;
    bool gc_trace = false;
    bool gc_stress = false;

    opterr = 0; // silence warnings
    int option = -1;
    while((option = getopt(argc, (char **)argv, "+dtsvh")) != -1) {
        switch (option) {
            case DEBUG_OPT: debug = true; break;
            case GC_TRACE_OPT: gc_trace = true; break;
            case GC_STRESS_OPT: gc_stress = true; break;
            case VERSION_OPT: version(argv[0]); return EXIT_SUCCESS;
            case HELP_OPT: help(argv[0]); return EXIT_SUCCESS;
            default: break; help(argv[0]); return EXIT_FAILURE;
        }
    }

    vm_t_init();
    if (debug) vm_toggle_stack_trace();
    if (gc_trace) vm_toggle_gc_trace();
    if (gc_stress) vm_toggle_gc_stress();

    int rv = 0;
    if (optind == argc) { // no args
        vm_set_argc_argv(argc, argv); // repl gets ours?
        vm_inherit_env();
        rv = repl();
    } else {
        vm_set_argc_argv(argc - optind, argv + optind);
        vm_inherit_env();
        rv = run_file(argv[optind]);
    }

    vm_t_free();
    return rv;
}
