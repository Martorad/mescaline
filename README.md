# Mescaline

Mescaline is a command-line generative graphics renderer written in C. It produces binary PPM
images, frame sequences, and GIF animations from built-in algorithms or custom expressions.

The CLI is the primary product. A separate Qt/QML desktop frontend is planned after the CLI
interface is safe, tested, and stable. See [ROADMAP.md](ROADMAP.md) for the draft product
contract and implementation order.

## Status

Mescaline is pre-alpha. Built-in and custom-expression rendering are implemented and tested, but
the stable interface declaration and desktop GUI remain in development. Breaking changes are
still allowed until the CLI stabilization milestone.

## Requirements

- Linux
- A C compiler with C23 and OpenMP support
- CMake 3.21 or newer
- FFmpeg for GIF output
- Python 3.8 or newer for tests

## Build

```sh
cmake -S . -B build
cmake --build build --parallel
```

The executable is written to `build/mescaline`.

## Test

```sh
ctest --test-dir build --output-on-failure
```

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
- Functions: `sin`, `cos`, `tan`, `sqrt`, `log`, `abs`, `min`, `max`, `pow`, `random`

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

Standard output remains unused except for `--help`.

## Benchmark

Run the repeatable end-to-end scaling benchmark with:

```sh
python3 benchmarks/render.py build/mescaline
```

It compares one thread with automatic CPU selection and reports median megapixels per second for
every built-in algorithm. Rendering, atomic PPM writing, and synchronization are included in the
measurement. Supply explicit counts with `--threads 1 2 4 8`.

## License

GPL-3.0 has been selected for the project. The license file will be added before the first
release.
