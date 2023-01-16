project('linenoice', 'c',
    license: 'BSD',
    default_options: [
        'buildtype=debugoptimized',
        'b_lto=true',
        'sysconfdir=/etc',
        'warning_level=3',
        'werror=true',
        'c_std=gnu2x',
    ],
)

sources = [
    'linenoise.h',
    'linenoise.c',
    'utf8.h',
    'utf8.c',
]

liblinenoise_static = static_library('liblinenoise', sources, install: false, name_prefix: '')

liblinenoise_dep = declare_dependency(include_directories: include_directories('.'), link_with: liblinenoise_static)
