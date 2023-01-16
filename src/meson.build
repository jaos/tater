
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

libtatertot = library('libtatertot', sources, install: true, dependencies: [libm, libintl], version: meson.project_version(), soversion: 0, name_prefix: '')
libtatertota = static_library('libtatertot', sources, install: true, name_prefix: '')
tater = executable('tater', sources + ['main.c'], dependencies: [liblinenoise, libm, libintl], install: true)

pkgconfig = import('pkgconfig')
pkgconfig.generate(libtatertot, name: 'libtatertot', description: 'Library for tater, a simple scripting language because everyone loves tots.')
