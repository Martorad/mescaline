# Changelog

All notable user-visible changes to Mescaline will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The project uses
[Semantic Versioning](https://semver.org/) beginning with version 0.3.0.

## 0.4.0 - 2026-09-25

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
- Configurable `wrap` and `clamp` range modes, with legacy wrapping restored as the default.
- OpenMP row-parallel rendering with automatic or explicit worker counts.
- Thread-count determinism tests, scaling benchmarks, and a ThreadSanitizer build configuration.
- Bounded custom-expression bytecode with arithmetic, variables, functions, and animation inputs.
- Scalar expression palettes and independent RGB channel expressions.
- Deterministic seeded `random()` independent of evaluation and thread order.
- Summary warnings for non-finite expression results.
- `--version` output and semantic program version 0.3.0.
- Documented CLI contract v1 with a versioned newline-delimited JSON progress schema.
- Optional separate Qt Quick / Material desktop frontend with system-following, light, and dark
  themes, previews, CLI progress, cancellation, and opening rendered output.
- Moved theme selection into a Settings menu and added a full-height export preview beside the
  render controls in a window twice as wide.
- Full-resolution preview by default, optional percentage scaling in Settings, and live preview
  with multithreaded row streaming and debounced rerendering after edits.
- Centered Settings dialog with Appearance and Performance categories; numeric GUI inputs accept
  decimal commas and display periods, including expression inputs with semicolon-separated
  function arguments where needed.
- Moved the GUI worker thread setting from the canvas form into Performance settings.
- Kept large live previews responsive by sampling only the display stream (at most 1024 pixels
  on the longest side) while retaining full-resolution rendering and atomic PPM output.
- Added a scalar-expression cheat sheet dialog beside Settings in the GUI.
- Added the GPL-3.0 license text for the first tagged release.

### Known limitations

- GCC ThreadSanitizer does not understand libgomp synchronization; the TSan build requires a
  compatible OpenMP runtime for meaningful results.

### Changed

- Selected GPL-3.0 as the project license for the first release.
- Raised the declared CMake minimum to 3.21 and made C23 support mandatory.
- Separated parsing, algorithms, rendering, output, encoding, and progress into dedicated modules.
- Replaced shell-based FFmpeg execution with fixed-argument `posix_spawnp` execution.
- Replaced `--algo`, `--horizontal`, `--vertical`, and `--tile` with `--algorithm`, `--width`,
  `--height`, and `--scale`.
- Adopted a flexible 0.x policy that permits coordinated CLI and GUI improvements without
  backward compatibility.
- JSON progress now guarantees valid UTF-8, replacing invalid operating-system path bytes.

### Fixed

- Preview pane now starts at its full size instead of growing when Live is enabled.
- Expression cheat sheet now displays each item and meaning in aligned columns instead of
  space-padded prose.
