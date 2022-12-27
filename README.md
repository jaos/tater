# clox

C based Lox VM working through https://craftinginterpreters.com/

## Building

```sh
meson build -Ddebuglog=enabled -Db_coverage=true
meson compile -C build
```

## Testing

```sh
meson test -C build && ninja coverage-html -C build
```

## Translations

```sh
# edit po/LINGUAS etc
meson compile clox-pot -C build
meson compile clox-update-po -C build
```
