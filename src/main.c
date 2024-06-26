/*
 * Copyright (C) 2022-2024 Jason Woodward <woodwardj at jaos dot org>
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
#include <limits.h>

#include "common.h"
#include "debug.h"
#include "vm.h"
#include "vmopcodes.h"

#include "linenoise.h"

#define TATER_PROMPT "tater> "
#define TATER_HISTORY_FILE ".tater_history"

static void completion(const char *input, linenoiseCompletions *completions)
{
    const size_t input_len = strlen(input);
    // only complete the last word
    size_t len = input_len;
    size_t prefix_len = 0;
    char *word = rindex(input, ' ');
    if (word != NULL) {
        word++;
        len = strlen(word);
        prefix_len = input_len - len;
    } else {
        word = (char *)input;
    }

    for (int i = 0; i < vm.globals.capacity; i++) {
        table_entry_t e = vm.globals.entries[i];
        if (IS_EMPTY(e.key))
            continue;

        if (IS_STRING(e.key)) {
            const char *keyword = AS_STRING(e.key)->chars;
            const size_t keyword_len = strlen(keyword);
            if ((keyword_len >= len) && memcmp(keyword, word, len) == 0) {
                size_t str_size = prefix_len + keyword_len;
                char *full = malloc(sizeof *full *str_size + 1);
                snprintf(full, str_size + 1, "%.*s%s", (int)prefix_len, input, keyword);
                linenoiseAddCompletion(completions, full);
                free(full);
            }
        }
    }

    int keyword_counter = 0;
    const char *keyword = NULL;
    while ((keyword = token_keyword_names[keyword_counter++]) != NULL) {
        if ((strlen(keyword) >= len) && memcmp(keyword, word, len) == 0) {
            const size_t keyword_len = strlen(keyword);
            size_t str_size = prefix_len + keyword_len;
            char *full = malloc(sizeof *full *str_size + 1);
            snprintf(full, str_size + 1, "%.*s%s", (int)prefix_len, input, keyword);
            linenoiseAddCompletion(completions, full);
            free(full);
        }
    }
}

static int repl(void)
{
    int status = EXIT_SUCCESS;
    bool is_a_tty = isatty(fileno(stdin));

    char history_path[255] = {0};
    if (is_a_tty) {
        linenoiseSetMultiLine(1);
        linenoiseSetCompletionCallback(completion);

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

        linenoiseHistorySetMaxLen(10000);
        if (*history_path) {
            if (linenoiseHistoryLoad(history_path)) {
                fprintf(stderr, gettext("Failed to ready history file\n"));
                exit(EXIT_FAILURE);
            }
        }
    }

    for (;;) {
        char *line = linenoise(TATER_PROMPT);

        if (line != NULL) {
            if (is_a_tty) {
                linenoiseHistoryAdd(line);
            }

            const vm_t_interpret_result_t rv = vm_t_interpret(line);
            if (is_a_tty) {
                printf("\n");
            }
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

    if (*history_path && is_a_tty) {
        if (linenoiseHistorySave(history_path)) {
            fprintf(stderr, gettext("Failed to write history file\n"));
            exit(EXIT_FAILURE);
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
    printf("  -d, %s\n", gettext("Enable debugging"));
    printf("  -s, %s\n", gettext("Enable garbage collector stress testing"));
    printf("  -t, %s\n", gettext("Enable garbage collector tracing"));
    printf("  -v, %s\n", gettext("Show version"));
    printf("  -h, %s\n", gettext("This help"));
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
            default: help(argv[0]); return EXIT_FAILURE;
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
