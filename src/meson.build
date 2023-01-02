
sources = [
    'common.h',
    'compiler.c',
    'compiler.h',
    'debug.c',
    'debug.h',
    'memory.c',
    'memory.h',
    'scanner.c',
    'scanner.h',
    'type.c',
    'type.h',
    'vm.h',
    'vm.c',
    'vmopcodes.h',
]

libtater = library('libtater', sources, install: false, version: meson.project_version(), soversion: 0, name_prefix: '')
tater = executable('tater', sources + ['main.c'], dependencies: [readline])
