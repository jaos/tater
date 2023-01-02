# tater

Simple scripting language based upon the great work of (Robert Nystrom)[https://craftinginterpreters.com/]

## Building

```sh
meson build -Ddebuglog=enabled -Db_coverage=true
meson compile -C build
```

## Testing

```sh
meson test -C build && ninja coverage-html -C build
```

Run the REPL

```sh
meson devenv -C build ./src/tater
```

Run the benchmark

```sh
meson devenv -C build ./src/tater $PWD/t/bench.tot
```

## Translations

```sh
# edit po/LINGUAS etc
meson compile tater-pot -C build
meson compile tater-update-po -C build
```
