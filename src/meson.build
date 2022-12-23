
sources = [
    'chunk.c',
    'chunk.h',
    'common.h',
    'compiler.c',
    'compiler.h',
    'debug.c',
    'debug.h',
    'memory.c',
    'memory.h',
    'object.c',
    'object.h',
    'scanner.c',
    'scanner.h',
    'table.c',
    'table.h',
    'value.c',
    'value.h',
    'vm.h',
    'vm.c',
]

libclox = library('libclox', sources, install: false, version: meson.project_version(), soversion: 0, name_prefix: '')
executable('clox', sources + ['main.c'])
