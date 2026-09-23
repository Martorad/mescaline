# Mescaline

Mescaline is an experimental command-line generative graphics renderer written in C. It
currently produces binary PPM images from three built-in mathematical algorithms and can
invoke FFmpeg to combine numbered frames into a GIF.

The CLI is the primary product. A separate Qt/QML desktop frontend is planned after the CLI
interface is safe, tested, and stable. See [ROADMAP.md](ROADMAP.md) for the draft product
contract and implementation order.

## Status

Mescaline is pre-alpha. The current executable predates the draft interface in the roadmap and
has known correctness, safety, and output-handling problems. Do not rely on its current options
or behavior remaining compatible.

## Requirements

- Linux
- A C compiler with C23 support
- CMake 3.21 or newer
- FFmpeg for the current GIF encoding step
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

The current executable accepts GNU-style long options:

```sh
./build/mescaline \
  --output=outputs/image.ppm \
  --algo=lasagna \
  --horizontal=1000 \
  --vertical=1000 \
  --tile=2 \
  --color=ff8000
```

| Option | Required | Default | Description |
| --- | --- | --- | --- |
| `--output` | Yes | None | Output path |
| `--algo` | Yes | None | `checkerboard`, `lasagna`, or `carreaux` |
| `--horizontal` | No | `5000` | Horizontal pixel count |
| `--vertical` | No | `5000` | Vertical pixel count |
| `--tile` | No | `1` | Algorithm scale |
| `--color` | No | `ffffff` | RGB tint in hexadecimal |
| `--frames` | No | `1` | Frame count from 1 to 1000 |
| `--help` | No | | Print usage and exit |

When multiple frames are requested, the current implementation writes `0.ppm`, `1.ppm`, and
so on in the output path's directory. It then always runs this hard-coded command, regardless of
the supplied output path:

```sh
ffmpeg -framerate 30 -i outputs/%d.ppm outputs/output.gif
```

This behavior will be replaced by the output contract in [ROADMAP.md](ROADMAP.md).

## License

GPL-3.0 has been selected for the project. The license file will be added before the first
release.
