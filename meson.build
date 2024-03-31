project('tater', 'c',
    version: '0.1',
    license: 'GPLv2',
    meson_version: '>= 0.56',
    default_options: [
        'buildtype=debugoptimized',
        'b_lto=true',
        'sysconfdir=/etc',
        'warning_level=3',
        'werror=true',
        'c_std=gnu2x',
    ],
)

cc = meson.get_compiler('c')
i18n = import('i18n')
libm = cc.find_library('m')
libintl = cc.find_library('intl', required: false)

cflags = [
  '-U_FORTIFY_SOURCE',
  '-D_FORTIFY_SOURCE=3',
  '-fcf-protection=full',
  '-fstack-clash-protection',
  '-fstack-protector-all',
  '-ftrivial-auto-var-init=zero',
  '-ftrapv',
  '-ggdb3',
  '-mshstk',

  '-ffat-lto-objects',
  '-fno-extended-identifiers',
  '-fno-omit-frame-pointer',
  '-fno-strict-aliasing',
  '-Warith-conversion',
  '-Warray-bounds=2',
  '-Wbidi-chars=any',
  '-Wcast-align=strict',
  '-Wduplicated-branches',
  '-Wduplicated-cond',
  '-Wformat-nonliteral',
  '-Wformat-overflow=2',
  '-Wformat-security',
  '-Wformat-truncation',
  '-Wformat=2',
  '-Wimplicit-fallthrough=3',
  '-Wimplicit-function-declaration',
  '-Wincompatible-pointer-types',
  '-Winit-self',
  '-Wint-conversion',
  '-Wlogical-op',
  '-Wmaybe-uninitialized',
  '-Wmissing-declarations',
  '-Wmissing-field-initializers',
  '-Wnull-dereference',
  '-Wold-style-definition',
  '-Wparentheses',
  '-Wredundant-decls',
  '-Wshadow',
  '-Wshift-overflow=2',
  '-Wstrict-overflow',
  '-Wstrict-prototypes',
  '-Wstringop-overflow=4',
  '-Wswitch-default',
  '-Wtrampolines',
  '-Wundef',
  '-Wwrite-strings',
]
add_project_arguments(cc.get_supported_arguments(cflags), language: 'c')
add_project_link_arguments(cc.get_supported_link_arguments(cflags), language: 'c')

lflags = ['-Wl,-z,defs',
  '-Wl,-z,noexecstack',
  '-Wl,-z,now',
  '-Wl,-z,relro',
  '-Wl,-z,separate-code',
]
add_project_link_arguments(cc.get_supported_link_arguments(lflags), language: 'c')

if get_option('debugging').enabled()
  add_global_arguments('-DDEBUG', language : 'c')
endif
add_project_arguments('-DVERSION="' + meson.project_version() + '"', language: 'c')

linenoise = subproject('linenoise')
liblinenoise = linenoise.get_variable('liblinenoise_dep')

subdir('po')
subdir('src')
subdir('t')

datadocs = [
  'COPYING',
  'README.md',
]

install_data_dir = join_paths(get_option('prefix'), get_option('datadir'), meson.project_name() + '-' + meson.project_version())
install_data(datadocs, install_dir: install_data_dir)
install_subdir('data/vim', install_dir: install_data_dir)
install_man(['data/man/tater.1'])
