# AGENTS.md

These instructions apply to the entire repository.

## Product Invariants

- The CLI is the primary product and owns all rendering behavior.
- The GUI is a separate frontend that invokes the CLI. It must not duplicate or link the
  renderer.
- Linux is the first supported platform, but avoid unnecessary barriers to Windows and macOS.
- Identical inputs must produce identical output regardless of thread count.
- `--range-mode=wrap` is the visual-compatibility default; clamping is opt-in.
- Invalid input must fail safely with a useful error and a nonzero exit status.
- Never pass user input through a shell.
- Final image files must not be left partially written after a failed render.
- Breaking CLI changes are allowed until the interface is explicitly marked stable.

## Build

Use an out-of-tree CMake build:

```sh
cmake -S . -B build
cmake --build build --parallel
```

Do not commit generated files from `build/` or rendered files from `outputs/`.

## Tests

Run the complete black-box CLI suite with:

```sh
ctest --test-dir build --output-on-failure
```

Rendering changes must test a tiny non-square image. Threading changes must verify byte-for-byte
equality between single-threaded and multithreaded output.

Use the sanitizer build for memory-safety and undefined-behavior changes:

```sh
cmake -S . -B build-sanitize -DMESCALINE_ENABLE_SANITIZERS=ON
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

Use the ThreadSanitizer build for threading changes:

```sh
cmake -S . -B build-tsan -DMESCALINE_ENABLE_THREAD_SANITIZER=ON
cmake --build build-tsan --parallel
ctest --test-dir build-tsan --output-on-failure
```

The local GCC/libgomp runtime is not ThreadSanitizer-aware and produces false races or runtime
mapping failures. Treat this build as opt-in coverage for environments with a TSan-aware OpenMP
runtime; normal and AddressSanitizer/UndefinedBehaviorSanitizer builds remain required locally.

Keep focused C unit tests for the rendering core and black-box Python tests for the executable.
Do not expose internal functions solely to test them.

## C Style

- Follow `.clang-format`.
- Prefer small functions with one clear responsibility over speculative abstractions.
- Check parsing, allocation, filesystem, write, close, and child-process failures.
- Check integer arithmetic before allocating or calculating image sizes.
- Keep rendering independent from argument parsing, output encoding, and UI concerns.
- Add dependencies only when the standard library or existing toolchain cannot reasonably solve
  the problem.

## Documentation

- `README.md` describes behavior that exists now.
- `ROADMAP.md` describes planned behavior and the draft product contract.
- `CHANGELOG.md` records user-visible changes under `Unreleased` until a release is tagged.
- Update documentation in the same change that alters its subject.
