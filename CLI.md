# Mescaline CLI Contract v1

This document describes Mescaline's current command-line interface for version 0.4.0.
It is authoritative for the current source tree, but it is not a backward-compatibility promise.

## Invocation

Mescaline accepts flat, case-sensitive long options:

```text
mescaline (
    --algorithm NAME |
    --expression EXPR |
    --expression-r EXPR --expression-g EXPR --expression-b EXPR
) --output PATH [OPTIONS]
```

Exactly one rendering mode is required. The three RGB expressions form one mode and must appear
together. Options accept `--name=value` and `--name value`, may appear in any order, and may appear
only once. There are no short options, abbreviated options, or positional arguments. Use the
equals form when a value begins with `--`.

`mescaline --help` prints usage and `mescaline --version` prints `mescaline 0.4.0`. Both write to
standard output and exit with status 0 without rendering.

## Options

| Option | Scope | Default | Contract |
| --- | --- | --- | --- |
| `--algorithm NAME` | Mode | None | `checkerboard`, `lasagna`, or `carreaux` |
| `--expression EXPR` | Mode | None | Scalar expression rendered through a palette |
| `--expression-r EXPR` | RGB mode | None | Red channel expression |
| `--expression-g EXPR` | RGB mode | None | Green channel expression |
| `--expression-b EXPR` | RGB mode | None | Blue channel expression |
| `--output PATH` | All | Required | PPM file, GIF file, or frame directory |
| `--width N` | All | `1000` | Integer from 1 through 100000 |
| `--height N` | All | `1000` | Integer from 1 through 100000 |
| `--scale N` | Algorithms | `1` | Finite number greater than 0 and at most 1000000 |
| `--color RRGGBB` | Algorithms, monochrome | `ffffff` | Exactly six hexadecimal digits |
| `--range-mode MODE` | All | `wrap` | `wrap` or `clamp` |
| `--palette NAME` | Scalar | `grayscale` | `grayscale`, `monochrome`, `viridis`, `plasma`, `magma`, `inferno`, or `turbo` |
| `--seed N` | Expressions | `0` | Unsigned 64-bit decimal integer |
| `--threads N` | All | `0` | Integer from 0 through 1024; zero selects available CPUs |
| `--frames N` | All | `1` | Integer from 1 through 1000 |
| `--fps N` | All | `30` | Integer from 1 through 1000; used for GIF encoding |
| `--progress MODE` | All | `text` | `text`, `json`, or `none` |
| `--force` | All | Off | Permit replacement of output files belonging to this job |
| `--help` | Metadata | | Print usage |
| `--version` | Metadata | | Print the program version |
| `--preview-stream` | Single-frame PPM | Off | Stream finished rows to stdout while rendering; requires `--progress=json` |

The canvas may contain at most 100 million pixels. Numeric input is strict: signs are not accepted
for integer options, and trailing characters, non-finite numbers, and out-of-range values fail.

`--color` is valid for built-in algorithms and for a scalar expression when
`--palette=monochrome` is explicit. `--palette` and `--color` are invalid in RGB mode.
`--palette` and `--seed` are invalid with built-in algorithms. `--scale` is valid only with a
built-in algorithm.

## Expression Language

Expressions contain decimal numbers; variables `px`, `py`, `x`, `y`, `frame`, `t`, `width`,
`height`, and `seed`; constants `pi` and `e`; operators `+`, `-`, `*`, `/`, `%`, and `^`; unary
signs; parentheses; and functions `sin`, `cos`, `tan`, `sqrt`, `log`, `abs`, `min`, `max`, `pow`,
and `random`. Two-argument functions accept either `,` or `;` between their arguments. The GUI
normalizes decimal commas to periods; when using decimal commas in two-argument functions, use
`;` between arguments (for example, `min(0,5; 1,5)`). The CLI expression language itself uses
periods for decimal fractions.

`px` and `py` are zero-based from the top-left. `x` and `y` include both edges of `[0, 1]`, with a
one-pixel dimension mapped to zero. `frame` is zero-based and `t` is `frame / frames`.
`random()` is deterministic from the seed, pixel, frame, channel, and expression occurrence.

An expression result is multiplied by 255 before range mapping. `wrap` truncates and applies
defined modulo-256 wrapping; `clamp` saturates to `0..255`. Non-finite results become zero and
produce one summary warning. Expressions are limited to 4096 source bytes, 1024 instructions, 64
nesting levels, and a 64-value evaluation stack.

## Output

The output path selects the output kind:

- A `.ppm` path requires one frame and produces a binary P6 image.
- A `.gif` path accepts one or more frames and invokes FFmpeg without a shell.
- An existing directory, a path ending in `/`, or an extensionless path receives numbered PPM
  files beginning with `frame-000000.ppm`.
- Any other extension is invalid.

Missing parent directories are created. Existing output files are rejected unless `--force` is
present. For sequences, `--force` replaces only requested `frame-NNNNNN.ppm` files and preserves
unrelated entries.

Single-file output is published atomically. Sequences are staged and published transactionally.
Successful GIF encoding removes staging data; encoder failure retains the frames and FFmpeg log
and reports their location. Signal cancellation removes unpublished temporary output when safe.

## Streams and Exit Status

Rendered data is written only to the selected output path, except for the optional preview row
stream described below. Standard output is otherwise reserved for `--help` and `--version`.
Human progress and diagnostics use standard error. `--progress=none`
suppresses routine progress and expression warnings; errors and cancellation diagnostics remain.

### Preview row stream

`--preview-stream` requires a `.ppm` output, exactly one frame, and `--progress=json`. It keeps the
normal atomic PPM output at the requested canvas resolution and emits an independent binary
display preview on standard output. For large canvases the display stream samples every Nth pixel
and row, with `N = ceil(max(canvas width, canvas height) / 1024)` (minimum 1). Its longest side is
therefore at most 1024 pixels; this does not reduce the render or saved PPM resolution.

The stream begins with ASCII `MPR1`, followed by unsigned 32-bit little-endian **display** width
and height. Each completed sampled row follows as an unsigned 32-bit little-endian zero-based
display row index plus exactly `display width * 3` RGB bytes. Rows can arrive out of order when
rendered on multiple threads; a cancelled or failed job can leave the stream incomplete. JSON
progress and errors remain on standard error. Consumers must check the process exit status before
treating the saved PPM as complete.

| Status | Meaning |
| --- | --- |
| `0` | Success |
| `1` | Internal or otherwise unclassified failure |
| `2` | Invalid command-line input |
| `3` | Filesystem or output failure |
| `4` | Rendering failure |
| `5` | Encoder failure |
| `128 + signal` | Interrupted by `SIGINT` or `SIGTERM` |

Callers should use the status class or JSON progress instead of parsing human-readable errors.

## JSON Progress Schema v1

`--progress=json` writes newline-delimited JSON objects to standard error. Every object contains
integer `version: 1` and a string `event`. Every line is valid UTF-8 and JSON. Valid UTF-8 in paths
and messages is preserved; invalid operating-system path bytes are represented by `U+FFFD`.

### `start`

Emitted after output preparation and before rendering:

```json
{"version":1,"event":"start","mode":"algorithm","algorithm":"lasagna","palette":null,"range_mode":"wrap","width":1000,"height":1000,"frames":1,"threads":8,"output_kind":"ppm"}
```

`mode` is `algorithm`, `scalar`, or `rgb`. `algorithm` is a name only in algorithm mode and is
otherwise `null`. `palette` is a name only in scalar mode and is otherwise `null`. `output_kind`
is `ppm`, `sequence`, or `gif`. `threads` is the effective worker count, not necessarily the
requested value.

### `frame`

Emitted after each frame is rendered and written to its file or staging area:

```json
{"version":1,"event":"frame","index":0,"completed":1,"total":60}
```

`index` is zero-based; `completed` is one-based and equals `index + 1`.

### `warning`

Emitted once after rendering only when an expression produced non-finite results:

```json
{"version":1,"event":"warning","kind":"nonfinite","count":42}
```

`count` is the number of channel evaluations mapped to zero.

### `encoding`

Emitted immediately before FFmpeg starts:

```json
{"version":1,"event":"encoding"}
```

### `complete`

The successful terminal event, emitted after final publication and cleanup:

```json
{"version":1,"event":"complete","output":"/absolute/path/image.ppm"}
```

`output` is absolute. It is informational when the original path contained invalid UTF-8 bytes.

### `cancelled`

The cancellation terminal event:

```json
{"version":1,"event":"cancelled","signal":15}
```

### `error`

The failure terminal event; it may be emitted before `start` when validation or output preparation
fails:

```json
{"version":1,"event":"error","status":3,"message":"Could not create directory","system":"Permission denied"}
```

`status` is one of 1 through 5. `message` is always present. `system` is present only when an
operating-system error is available.

Schema version 1 describes the fields above. Future releases may add, remove, rename, or retype
fields and events. Incompatible JSON changes must increment `version`, and consumers should reject
unsupported versions rather than infer their meaning.

## Change Policy

Mescaline's CLI and GUI are developed together for personal use. Backward compatibility is not a
requirement: options, defaults, behavior, exit statuses, and JSON events may change whenever doing
so improves the product.

Changes must update this document, tests, the changelog, and the GUI when it exists. Program
versions identify builds and development milestones; during 0.x they do not promise CLI
compatibility with earlier versions.
