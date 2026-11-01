# Contributing

Bug reports and local patches are welcome. Keep changes focused and preserve
all three game modes and the headless-testable core.

## Before submitting a patch

```sh
make test
make strict-core
make analyze
make sanitize
```

If SDL2 development files are available, also run `make` and `make smoke`.
Record skipped checks honestly when a dependency is unavailable. Do not add
external art, audio, fonts, or other assets without documenting their source
and license. Keep screenshots as genuine captures of the running game.

The project is dedicated under CC0-1.0; see `LICENSE`.
