project('lox', 'c',
    version: '0.1',
    license: 'GPLv2',
    meson_version: '>= 0.56',
    default_options: [
        # 'buildtype=debugoptimized',
        'buildtype=debug',
        'b_lto=true',
        'sysconfdir=/etc',
        'warning_level=3',
        'werror=true',
        'c_std=gnu2x',
    ],
)

cc = meson.get_compiler('c')
cflags = ['-Wshadow', '-Wstrict-overflow', '-fno-strict-aliasing', '-fstack-protector', '-ggdb', '-fno-omit-frame-pointer']
add_project_arguments(cc.get_supported_arguments(cflags), language: 'c')
add_project_link_arguments(cc.get_supported_link_arguments(cflags), language: 'c')

subdir('src')
