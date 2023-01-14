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
readline = dependency('readline')
libm = cc.find_library('m')
libintl = cc.find_library('intl', required: false)

cflags = [
  '-Wshadow',
  '-Wstrict-overflow',
  '-fno-strict-aliasing',
  '-fstack-protector',
  '-fstack-protector-strong',
  '-ggdb',
  '-D_FORTIFY_SOURCE=2',
  '-fno-omit-frame-pointer',
  '-ffat-lto-objects',
  '-Warith-conversion',
  '-Warray-bounds=2',
  '-Wcast-align=strict',
  '-Wduplicated-cond',
  '-Wduplicated-branches',
  '-Wformat-nonliteral',
  '-Wformat-overflow=2',
  '-Wformat-security',
  '-Wformat-truncation',
  '-Wformat=2',
  '-Wimplicit-function-declaration',
  '-Wimplicit-fallthrough=3',
  '-Wincompatible-pointer-types',
  '-Winit-self',
  '-Wint-conversion',
  '-Wlogical-op',
  '-Wmaybe-uninitialized',
  '-Wmissing-field-initializers',
  '-Wmissing-declarations',
  '-Wnull-dereference',
  '-Wold-style-definition',
  '-Wparentheses',
  '-Wredundant-decls',
  '-Wshift-overflow=2',
  '-Wstrict-prototypes',
  '-Wstringop-overflow=4',
  '-Wswitch-default',
  '-Wtrampolines',
  '-Wundef',
  '-Wshadow',
  '-Wwrite-strings',
  '-Wduplicated-branches',
  '-Wduplicated-cond',
  '-Wno-common',
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
