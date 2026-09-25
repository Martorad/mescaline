# Roadmap

This document defines the planned product and implementation order. The contract is a draft
until the CLI stabilization milestone; breaking changes are currently allowed.

## Product Principles

- Mescaline is a command-line generative graphics renderer first.
- Every rendering feature is available from the CLI.
- The desktop GUI is a separate process frontend, not a second renderer.
- Rendering is deterministic, including seeded randomness and multithreaded execution.
- Safe failure is more important than preserving incomplete output.
- Linux is supported first while implementation choices remain portable where practical.

## Draft CLI Contract

Mescaline uses flat long options. Exactly one rendering mode is required:

```text
mescaline (
    --algorithm NAME |
    --expression EXPR |
    --expression-r EXPR --expression-g EXPR --expression-b EXPR
) --output PATH [OPTIONS]
```

The three RGB expression options form one mode and must be supplied together. Rendering modes
are mutually exclusive.

### Core Options

| Option | Default | Description |
| --- | --- | --- |
| `--algorithm NAME` | None | Select a built-in algorithm |
| `--expression EXPR` | None | Render one scalar expression through a palette |
| `--expression-r EXPR` | None | Red channel expression |
| `--expression-g EXPR` | None | Green channel expression |
| `--expression-b EXPR` | None | Blue channel expression |
| `--output PATH` | Required | PPM file, GIF file, or frame-sequence directory |
| `--width N` | `1000` | Canvas width in pixels |
| `--height N` | `1000` | Canvas height in pixels |
| `--scale N` | `1` | Spatial scale used by built-in algorithms |
| `--frames N` | `1` | Number of frames to render |
| `--fps N` | `30` | GIF playback frame rate |
| `--palette NAME` | `grayscale` | Palette for scalar output |
| `--color RRGGBB` | `ffffff` | Target color for the `monochrome` palette |
| `--range-mode MODE` | `wrap` | Map out-of-range values with `wrap` or `clamp` |
| `--seed N` | `0` | Seed for deterministic randomness |
| `--threads N` | `0` | Worker count from 0 to 1024; zero selects available CPUs |
| `--progress MODE` | `text` | `text`, `json`, or `none` |
| `--force` | Off | Permit replacing output files created by this job |
| `--help` | | Print usage and exit |
| `--version` | | Print version and exit |

Initial built-in algorithms remain `checkerboard`, `lasagna`, and `carreaux`. Initial palettes
are `grayscale`, `monochrome`, `viridis`, `plasma`, `magma`, `inferno`, and `turbo`.

All numeric arguments are parsed strictly. Values with trailing characters, values outside the
supported range, impossible image sizes, and conflicting options are errors. Width and height are
limited to 100000 each, and a canvas may contain at most 100 million pixels.

### Output Behavior

The output path determines the result type:

- A `.ppm` path accepts exactly one frame and produces a binary P6 image.
- A `.gif` path accepts one or more frames and uses FFmpeg for encoding.
- An existing directory, a path ending in a directory separator, or an extensionless path
  receives `frame-000000.ppm`, `frame-000001.ppm`, and so on.
- Other file extensions are rejected.

Missing parent directories are created. Existing output is rejected unless `--force` is given.
For a frame directory, `--force` only replaces frame names belonging to the requested job; it
does not delete unrelated files.

Single-file results are written to a temporary sibling and renamed only after successful
completion. Temporary GIF frames are removed after successful encoding. If encoding fails, the
frame directory is retained and reported for diagnosis. Interruptions remove temporary files
created by the current process where this can be done safely.

FFmpeg is a runtime dependency only for GIF output. PPM files and sequences do not require it.
Encoder failures produce a nonzero exit status.

### Progress And Errors

Human-readable progress and diagnostics are written to standard error. Standard output remains
unused so a future streaming mode can use it without ambiguity.

`--progress=json` emits one JSON object per line for the GUI. Events include startup, frame
completion, encoding, successful completion, cancellation, and errors. The event schema will be
versioned and frozen before GUI implementation.

Planned exit statuses are:

| Status | Meaning |
| --- | --- |
| `0` | Success |
| `1` | Internal or otherwise unclassified failure |
| `2` | Invalid command-line input |
| `3` | Filesystem or output failure |
| `4` | Rendering failure |
| `5` | Encoder failure |
| `128 + signal` | Interrupted by a signal |

## Expression Contract

Custom expressions use a safe calculator-style language. Mescaline will not execute TeX,
arbitrary C, shell commands, or dynamically generated native code.

### Variables

| Variable | Meaning |
| --- | --- |
| `px`, `py` | Zero-based pixel coordinates; origin is the top-left |
| `x`, `y` | Coordinates normalized to `[0, 1]`, including both image edges |
| `frame` | Zero-based frame index |
| `t` | Animation position: `frame / frames`, in `[0, 1)` |
| `width`, `height` | Canvas dimensions |
| `seed` | User-provided deterministic seed |

For a one-pixel dimension, its normalized coordinate is zero. For a single-frame render, `t` is
zero.

The initial language includes numeric literals, parentheses, `+`, `-`, `*`, `/`, `%`, and `^`;
constants `pi` and `e`; and functions `sin`, `cos`, `tan`, `sqrt`, `log`, `abs`, `min`, `max`,
`pow`, and `random`.

Each `random()` occurrence produces a deterministic value in `[0, 1]` based on the seed, pixel,
frame, and expression location. It does not depend on evaluation or thread order.

Expression channels are scaled to byte values and processed by the selected range mode before
palette or RGB output. `wrap` is the default and applies defined modulo-256 wrapping; `clamp`
saturates to the byte range. Non-finite results map to zero and produce a summary warning rather
than undefined behavior.

## GUI Contract

The Linux desktop GUI will use Qt 6 Quick/QML and Qt Quick Controls Material. It lives under
`gui/`, has its own build definition, and launches the CLI with `QProcess`.

The GUI must expose every stable CLI function, consume versioned JSON progress, support
cancellation, and show complete errors. Preview renders are reduced-size CLI jobs; rendering
logic is never reimplemented in QML or linked into the GUI.

The first GUI release includes built-in and expression modes, output settings, palettes, color,
dimensions, scale, range mode, animation settings, seed, thread count, preview, progress,
cancellation, and opening the completed output. Keyboard access, labels, scalable layout, and
light/dark themes are release requirements.

## Milestones

### 1. Product Foundation

- Define the draft CLI, output, expression, and GUI contracts.
- Add `README.md`, `AGENTS.md`, `ROADMAP.md`, and `CHANGELOG.md`.
- Select GPL-3.0 and add its license text before the first release.

### 2. Regression Suite

- [x] Add CTest-based end-to-end CLI tests.
- [x] Cover parsing, invalid input, paths, non-square images, output failures, and all algorithms.
- [x] Add an AddressSanitizer and UndefinedBehaviorSanitizer development configuration.
- [x] Establish tiny deterministic checks without relying on platform-sensitive floating-point
  equality where it is inappropriate.
- [x] Add focused unit tests as the rendering core is separated into testable modules.

### 3. Safe Rendering Core

- [x] Separate argument parsing, rendering, algorithms, image writing, encoding, and progress.
- [x] Correct width and height traversal.
- [x] Replace unchecked conversions, fixed path buffers, per-byte output, and `system()`.
- [x] Check allocation arithmetic and output operations.
- [x] Add atomic output, sequence staging, signal handling, stable errors, and progress modes.
- [x] Establish a repeatable single-thread performance benchmark.

### 4. Multithreaded Rendering

- [x] Render contiguous row partitions with static OpenMP scheduling.
- [x] Add automatic and explicit worker counts.
- [x] Preserve byte-for-byte output across thread counts.
- [x] Measure scaling by algorithm, thread count, and image size.
- [x] Add a ThreadSanitizer build configuration. Runtime verification requires a TSan-aware
  OpenMP runtime; the local GCC/libgomp combination reports false races.

### 5. Custom Expressions

- Parse and validate the expression language into an AST or compact bytecode once per job.
- Implement scalar palettes and independent RGB expressions.
- Add deterministic seeded randomness.
- Apply resource limits to untrusted expressions and fuzz the parser.
- Evaluate expressions through the same multithreaded renderer as built-in algorithms.

### 6. Stable CLI Contract

- Finalize option names, exit statuses, and the versioned JSON progress schema.
- Document compatibility policy and mark the CLI contract stable.
- Add `--version` and semantic versioning.

### 7. Desktop GUI

- Add the independent Qt/QML Material frontend.
- Implement configuration, reduced-resolution preview, execution, progress, cancellation, and
  result handling.
- Add GUI smoke tests and accessibility checks.

### 8. Release

- Add the full GPL-3.0 license text.
- Add continuous integration for builds, tests, sanitizers, formatting, and GUI compilation.
- Finish user documentation and package the CLI beside the GUI for Linux.
- Consider AppImage or Flatpak after the application workflow is stable.

## Deferred

GPU rendering, distributed rendering, third-party algorithm plugins, full LaTeX parsing, and a
general plugin API are deliberately deferred until measured needs justify them.
