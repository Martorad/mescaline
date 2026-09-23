# Changelog

All notable user-visible changes to Mescaline will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The project will use
[Semantic Versioning](https://semver.org/) once its first version is assigned.

## Unreleased

### Added

- Initial project README with current build and usage instructions.
- Repository-wide contributor and agent guidance.
- Draft CLI, output, expression, GUI, and release contracts.
- Ordered implementation roadmap.
- CTest-driven CLI regression suite covering all current algorithms, parsing, output, paths,
  deterministic rendering, frame generation, and known defects.
- Optional AddressSanitizer and UndefinedBehaviorSanitizer build configuration.
- `--help` output for the current CLI.
- Strict built-in rendering interface with validated dimensions, scale, color, animation, output,
  overwrite, and progress options.
- Atomic PPM output, transactional frame-sequence publication, and isolated GIF staging.
- Text, silent, and newline-delimited JSON progress modes.
- Signal-aware cancellation and documented exit statuses.
- Single-thread rendering benchmark.
- Focused rendering unit tests and expanded end-to-end CLI coverage.

### Changed

- Selected GPL-3.0 as the project license for the first release.
- Raised the declared CMake minimum to 3.21 and made C23 support mandatory.
- Separated parsing, algorithms, rendering, output, encoding, and progress into dedicated modules.
- Replaced shell-based FFmpeg execution with fixed-argument `posix_spawnp` execution.
- Replaced `--algo`, `--horizontal`, `--vertical`, and `--tile` with `--algorithm`, `--width`,
  `--height`, and `--scale`.
