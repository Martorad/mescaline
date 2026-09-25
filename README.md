# Mescaline

Mescaline is a command-line generative graphics renderer written in C. It produces binary PPM
images, frame sequences, and GIF animations from built-in algorithms or custom expressions.

The CLI is the primary product. A separate Qt/QML desktop frontend is available. See
[CLI.md](CLI.md) for the current CLI v1 contract and [ROADMAP.md](ROADMAP.md) for implementation
order.

## Status

Mescaline is at version 0.4.0, an early release; 1.0 remains future work. Built-in and
custom-expression rendering are implemented and tested, and the optional Qt desktop frontend is
available.

## Requirements

- Linux
- A C compiler with C23 and OpenMP support
- CMake 3.21 or newer
- FFmpeg for GIF output
- Python 3.8 or newer for tests
- Qt 6.4 Quick, Quick Controls 2, Qt Quick Templates, and Qt Test for the optional GUI

## Build

```sh
cmake -S . -B build
cmake --build build --parallel
```

The executable is written to `build/mescaline`.

To build the separate Qt Quick / Material frontend:

```sh
sudo apt install qt6-base-dev qt6-declarative-dev qml6-module-qtqml \
  qml6-module-qtqml-workerscript qml6-module-qtquick qml6-module-qtquick-controls \
  qml6-module-qtquick-templates qml6-module-qtquick-layouts qml6-module-qtquick-window
cmake -S . -B build-gui -DMESCALINE_BUILD_GUI=ON
cmake --build build-gui --parallel
ctest --test-dir build-gui --output-on-failure
./build-gui/gui/mescaline-gui
```

The GUI locates the CLI next to itself or one directory above; use
`mescaline-gui --cli /path/to/mescaline` if needed. The Settings button opens a centered dialog
with categories on the right. Appearance offers System (default), Light, and Dark themes; System
tracks palette changes. Performance controls worker threads, reduced-resolution previews, and a
scale from 1–100%. The Expressions button beside Settings opens a scalar-expression cheat sheet
covering coordinates, functions, animation, randomness, and examples.
The default window is 1700 pixels wide, with render controls on the left and an export preview on
the right. Preview renders the first frame at full canvas resolution by default. The Live checkbox
automatically rerenders after edits and shows completed rows as they arrive from multiple CLI
workers; the regular Preview button also works without Live. Render uses the chosen output path
and full settings. Both launch the CLI as a separate process and report its JSON progress; Cancel
sends termination to that process.

Preview shows only the first animation frame. To see an animation, render to a `.gif` and use
Open result with an external viewer, or render a PPM frame sequence. GIF encoding temporarily
stores PPM frames; they are removed on success but retained with the FFmpeg log if encoding fails.

Live mode still renders and saves the preview at the chosen canvas resolution; its on-screen
stream samples large images to at most 1024 pixels on the longest side so display updates do not
stall the render. Use Performance → Reduce preview resolution to lower the actual render workload.

Numeric fractions entered with commas are immediately displayed with periods, regardless of the
desktop locale. This includes expression inputs: `0,5*x` becomes `0.5*x`. Use `;` between arguments
when entering decimal commas in two-argument functions, for example `min(0,5; 1,5)` becomes
`min(0.5; 1.5)`. Output paths and hexadecimal colors are not numeric fields and remain unchanged.

## Test

```sh
ctest --test-dir build --output-on-failure
```

Building with `MESCALINE_BUILD_GUI=ON` adds GUI process and offscreen QML smoke tests. A desktop
session is needed to inspect the visual layout and system theme integration interactively.

The suite uses a temporary fake FFmpeg executable and does not leave rendered files in the
repository. Run it directly for individual test names and detailed output:

```sh
python3 tests/test_cli.py build/mescaline
```

Build and test with AddressSanitizer and UndefinedBehaviorSanitizer using:

```sh
cmake -S . -B build-sanitize -DMESCALINE_ENABLE_SANITIZERS=ON
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

## Current Usage

Exactly one rendering mode and an output path are required:

```sh
./build/mescaline \
  --output=outputs/image.ppm \
  --algorithm=lasagna \
  --width=1000 \
  --height=1000 \
  --scale=2 \
  --range-mode=wrap \
  --color=ff8000
```

| Option | Required | Default | Description |
| --- | --- | --- | --- |
| `--output` | Yes | None | Output path |
| `--algorithm` | Mode | None | `checkerboard`, `lasagna`, or `carreaux` |
| `--expression` | Mode | None | Scalar expression rendered through a palette |
| `--expression-r/g/b` | Mode | None | Three expressions rendered directly as RGB |
| `--width` | No | `1000` | Canvas width from 1 to 100000 |
| `--height` | No | `1000` | Canvas height from 1 to 100000 |
| `--scale` | No | `1` | Positive built-in algorithm scale up to 1000000 |
| `--color` | No | `ffffff` | Algorithm tint or monochrome palette color |
| `--range-mode` | No | `wrap` | Map out-of-range values with `wrap` or `clamp` |
| `--palette` | No | `grayscale` | Palette for scalar expressions |
| `--seed` | No | `0` | Deterministic expression seed |
| `--threads` | No | `0` | Worker count; zero selects available CPUs |
| `--frames` | No | `1` | Frame count from 1 to 1000 |
| `--fps` | No | `30` | GIF frame rate from 1 to 1000 |
| `--progress` | No | `text` | `text`, `json`, or `none` |
| `--force` | No | Off | Replace existing target files |
| `--help` | No | | Print usage and exit |
| `--version` | No | | Print `mescaline 0.4.0` and exit |

The canvas is limited to 100 million pixels so invalid jobs fail before attempting an excessive
allocation.

`wrap` safely reproduces the original Lasagna and Carreaux look by truncating and wrapping values
modulo 256. `clamp` saturates values to the `0..255` channel range instead.

Rendering uses static OpenMP row partitioning. `--threads=0` selects the available processor
count, capped by the canvas height and the 1024-worker safety limit. Explicit thread counts produce
byte-identical output; use `--threads=1` when measuring the single-thread baseline.

## Custom Expressions

Scalar expressions map one normalized result through `grayscale`, `monochrome`, `viridis`,
`plasma`, `magma`, `inferno`, or `turbo`:

```sh
./build/mescaline \
  --expression='sin(x * pi * 8 + t * pi * 2) * 0.5 + 0.5' \
  --palette=viridis \
  --output=outputs/waves.gif \
  --frames=60
```

Independent channel expressions produce RGB directly:

```sh
./build/mescaline \
  --expression-r='x' \
  --expression-g='y' \
  --expression-b='random()' \
  --seed=42 \
  --output=outputs/rgb.ppm
```

The language supports:

- Variables: `px`, `py`, `x`, `y`, `frame`, `t`, `width`, `height`, `seed`
- Constants: `pi`, `e`
- Operators: `+`, `-`, `*`, `/`, `%`, `^`, parentheses, and unary signs
- Functions: `sin`, `cos`, `tan`, `sqrt`, `log`, `abs`, `min`, `max`, `pow`, `random` (`min`, `max`,
  and `pow` accept either `,` or `;` between arguments)

`x` and `y` span `0..1` across the image; `t` is `frame / frames`. Expression results are
multiplied by 255 and then processed by `--range-mode`. `random()` is deterministic for the seed,
pixel, frame, channel, and occurrence, so thread count never changes the result.

Expressions are compiled once into bounded bytecode. Input is limited to 4096 bytes, 1024
instructions, 64 nesting levels, and a 64-value evaluation stack. Non-finite results map to zero
and produce one summary warning. Mescaline never evaluates expressions as C, TeX, or shell code.

## Output

- A `.ppm` path accepts one frame and produces a binary P6 image.
- A `.gif` path accepts one or more frames and invokes FFmpeg without a shell.
- An existing directory, a path ending in `/`, or an extensionless path receives numbered PPM
  frames such as `frame-000000.ppm`.
- Existing targets are rejected unless `--force` is supplied.

PPM files are written to temporary siblings and published atomically. Frame sequences are fully
rendered in a private staging directory before publication. GIF staging data is removed on
success and retained with its FFmpeg log when encoding fails.

JSON progress is emitted as one object per line on standard error:

```sh
./build/mescaline \
  --algorithm=carreaux \
  --output=outputs/animation.gif \
  --frames=64 \
  --progress=json
```

Standard output remains unused except for `--help`, `--version`, and the binary
`--preview-stream` option. The complete stream format, versioned JSON events, and change policy
are documented in [CLI.md](CLI.md).

## Benchmark

Run the repeatable end-to-end scaling benchmark with:

```sh
python3 benchmarks/render.py build/mescaline
```

It compares one thread with automatic CPU selection and reports median megapixels per second for
every built-in algorithm. Rendering, atomic PPM writing, and synchronization are included in the
measurement. Supply explicit counts with `--threads 1 2 4 8`.

## License

Licensed under GPL-3.0. See [LICENSE](LICENSE).
