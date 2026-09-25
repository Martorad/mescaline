#!/usr/bin/env python3

import argparse
from pathlib import Path
import statistics
import subprocess
import tempfile
import time


parser = argparse.ArgumentParser(description="Benchmark end-to-end rendering by thread count")
parser.add_argument("executable", type=Path)
parser.add_argument("--width", type=int, default=2000)
parser.add_argument("--height", type=int, default=2000)
parser.add_argument("--repeats", type=int, default=3)
parser.add_argument("--range-mode", choices=("wrap", "clamp"), default="wrap")
parser.add_argument("--threads", type=int, nargs="+", default=(1, 0))
args = parser.parse_args()

executable = args.executable.resolve()
pixels = args.width * args.height
with tempfile.TemporaryDirectory() as directory:
    for threads in args.threads:
        label = "auto" if threads == 0 else str(threads)
        for algorithm in ("checkerboard", "lasagna", "carreaux"):
            samples = []
            for repeat in range(args.repeats):
                output = Path(directory) / f"{algorithm}-{threads}-{repeat}.ppm"
                started = time.perf_counter()
                subprocess.run(
                    [
                        executable,
                        f"--algorithm={algorithm}",
                        f"--output={output}",
                        f"--width={args.width}",
                        f"--height={args.height}",
                        f"--range-mode={args.range_mode}",
                        f"--threads={threads}",
                        "--progress=none",
                    ],
                    check=True,
                )
                samples.append(time.perf_counter() - started)
            elapsed = statistics.median(samples)
            print(
                f"threads={label:>4}  {algorithm:12} {elapsed:8.3f}s  "
                f"{pixels / elapsed / 1_000_000:8.2f} MP/s"
            )
