# Mescaline

Mescaline is a command-line generative graphics renderer written in C. It produces binary PPM
images, frame sequences, and GIF animations from built-in mathematical algorithms.

The CLI is the primary product. A separate Qt/QML desktop frontend is planned after the CLI
interface is safe, tested, and stable. See [ROADMAP.md](ROADMAP.md) for the draft product
contract and implementation order.

## Status

Mescaline is pre-alpha. Its built-in rendering CLI is implemented and tested, but threading,
custom expressions, the stable interface declaration, and the desktop GUI remain in development.
Breaking changes are still allowed until the CLI stabilization milestone.

## Requirements

- Linux
- A C compiler with C23 support
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
repository. Run it directly for individual test names and known expected failures:

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

Exactly one built-in algorithm and an output path are required:

```sh
./build/mescaline \
  --output=outputs/image.ppm \
  --algorithm=lasagna \
  --width=1000 \
  --height=1000 \
  --scale=2 \
  --color=ff8000
```

| Option | Required | Default | Description |
| --- | --- | --- | --- |
| `--output` | Yes | None | Output path |
| `--algorithm` | Yes | None | `checkerboard`, `lasagna`, or `carreaux` |
| `--width` | No | `1000` | Canvas width from 1 to 100000 |
| `--height` | No | `1000` | Canvas height from 1 to 100000 |
| `--scale` | No | `1` | Positive algorithm scale up to 1000000 |
| `--color` | No | `ffffff` | RGB tint in hexadecimal |
| `--frames` | No | `1` | Frame count from 1 to 1000 |
| `--fps` | No | `30` | GIF frame rate from 1 to 1000 |
| `--progress` | No | `text` | `text`, `json`, or `none` |
| `--force` | No | Off | Replace existing target files |
| `--help` | No | | Print usage and exit |

The canvas is limited to 100 million pixels so invalid jobs fail before attempting an excessive
allocation.

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

Run the repeatable end-to-end single-thread benchmark with:

```sh
python3 benchmarks/render.py build/mescaline
```

It reports median megapixels per second for every built-in algorithm. Rendering, atomic PPM
writing, and synchronization are included in the measurement.

## License

GPL-3.0 has been selected for the project. The license file will be added before the first
release.
