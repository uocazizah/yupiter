# Contributing

Contributions, bug reports and suggestions are welcome.

## Before you start

- The public interface is stable within 1.x: changes to exported
  symbols, signatures, C linkage, structure layouts or documented
  behavior need a new major version and a strong reason.
- Portability fixes, bug fixes and new tests are always welcome.

## Build and test

```sh
./configure
make
make check
```

Tests run on Linux for aarch64 and x86_64 via GitHub Actions with GCC
and Clang. State explicitly if a change cannot be tested on a supported
target.

## Submitting changes

- Keep changes focused and reviewable.
- Add tests for new behavior.
- Update documentation when public behavior changes.
- Sign off commits with `git commit -s`.

## License

By contributing, you agree that your contributions are licensed under
the same multi-license as the project (GPLv2+, LGPLv2.1+ or
Apache-2.0). See `LICENSE` and `LICENSES/`.

## Code of Conduct

See `CODE_OF_CONDUCT.md`.