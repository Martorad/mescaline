#!/usr/bin/env python3

import os
from pathlib import Path
import json
import selectors
import signal
import struct
import subprocess
import sys
import tempfile
import time
import unittest


class MescalineCliTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.executable = Path(sys.argv[1]).resolve()

    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.root = Path(self.temp_dir.name)
        bin_dir = self.root / "bin"
        bin_dir.mkdir()
        ffmpeg = bin_dir / "ffmpeg"
        ffmpeg.write_text(
            """#!/usr/bin/env python3
import json
import os
from pathlib import Path
import sys
import time

if os.environ.get("FAKE_FFMPEG_ARGS"):
    Path(os.environ["FAKE_FFMPEG_ARGS"]).write_text(json.dumps(sys.argv[1:]))
if os.environ.get("FAKE_FFMPEG_STARTED"):
    Path(os.environ["FAKE_FFMPEG_STARTED"]).touch()
time.sleep(float(os.environ.get("FAKE_FFMPEG_SLEEP", "0")))
status = int(os.environ.get("FAKE_FFMPEG_STATUS", "0"))
if status == 0:
    Path(sys.argv[-1]).write_bytes(b"GIF89a")
raise SystemExit(status)
"""
        )
        ffmpeg.chmod(0o755)
        self.env = os.environ.copy()
        self.env["PATH"] = f"{bin_dir}:{os.environ['PATH']}"

    def tearDown(self):
        self.temp_dir.cleanup()

    def run_cli(self, *args, env=None):
        process_env = self.env.copy()
        process_env.update(env or {})
        return subprocess.run(
            [self.executable, *args],
            cwd=self.root,
            env=process_env,
            capture_output=True,
            text=True,
            timeout=10,
        )

    def read_ppm(self, path):
        with path.open("rb") as image:
            self.assertEqual(image.readline(), b"P6\n")
            width, height = map(int, image.readline().split())
            self.assertEqual(image.readline(), b"255\n")
            pixels = image.read()
        self.assertEqual(len(pixels), width * height * 3)
        return width, height, pixels

    def wait_for_event(self, process, event, timeout=5):
        selector = selectors.DefaultSelector()
        selector.register(process.stderr, selectors.EVENT_READ)
        deadline = time.monotonic() + timeout
        try:
            while time.monotonic() < deadline:
                ready = selector.select(deadline - time.monotonic())
                if not ready:
                    break
                line = process.stderr.readline()
                if line and json.loads(line)["event"] == event:
                    return
                if process.poll() is not None:
                    break
        finally:
            selector.close()
        self.fail(f"process did not emit {event!r}")

    def render(self, output, algorithm="checkerboard", width=4, height=3, env=None, **options):
        args = [
            f"--output={output}",
            f"--algorithm={algorithm}",
            f"--width={width}",
            f"--height={height}",
            "--progress=none",
        ]
        for name, value in options.items():
            name = name.replace("_", "-")
            args.append(f"--{name}" if value is True else f"--{name}={value}")
        return self.run_cli(*args, env=env)

    def render_expression(
        self, output, expression=None, rgb=None, width=4, height=3, env=None, **options
    ):
        args = [f"--output={output}", f"--width={width}", f"--height={height}", "--progress=none"]
        if expression is not None:
            args.append(f"--expression={expression}")
        if rgb is not None:
            args.extend(
                (f"--expression-r={rgb[0]}", f"--expression-g={rgb[1]}", f"--expression-b={rgb[2]}")
            )
        for name, value in options.items():
            name = name.replace("_", "-")
            args.append(f"--{name}" if value is True else f"--{name}={value}")
        return self.run_cli(*args, env=env)

    def test_requires_output(self):
        result = self.run_cli("--algorithm=checkerboard")
        self.assertEqual(result.returncode, 2)
        self.assertIn("--output is required", result.stderr)

    def test_requires_rendering_mode(self):
        result = self.run_cli("--output=image.ppm")
        self.assertEqual(result.returncode, 2)
        self.assertIn("Choose exactly one rendering mode", result.stderr)

    def test_rejects_invalid_options_and_arguments(self):
        for argument in ("--algo=checkerboard", "--unknown=value", "positional"):
            with self.subTest(argument=argument):
                result = self.run_cli(argument)
                self.assertEqual(result.returncode, 2)

    def test_rejects_bad_numeric_values(self):
        cases = (
            "--width=0",
            "--width=-1",
            "--width=+1",
            "--width=invalid",
            "--width=100001",
            "--height=0",
            "--frames=0",
            "--frames=1001",
            "--fps=0",
            "--fps=1001",
            "--scale=0",
            "--scale=nan",
            "--scale=inf",
            "--scale=1x",
            "--threads=-1",
            "--threads=invalid",
            "--threads=1025",
            "--seed=-1",
            "--seed=18446744073709551616",
        )
        for argument in cases:
            with self.subTest(argument=argument):
                result = self.run_cli(
                    "--algorithm=checkerboard", "--output=image.ppm", argument
                )
                self.assertEqual(result.returncode, 2)

        result = self.run_cli(
            "--algorithm=checkerboard", "--output=image.ppm", "--width=100000", "--height=1001"
        )
        self.assertEqual(result.returncode, 2)

    def test_rejects_duplicate_options(self):
        result = self.run_cli(
            "--algorithm=checkerboard",
            "--algorithm=lasagna",
            "--output=image.ppm",
        )
        self.assertEqual(result.returncode, 2)

    def test_rejects_bad_colors(self):
        for color in ("fff", "gggggg", "1234567"):
            with self.subTest(color=color):
                result = self.run_cli(
                    "--algorithm=checkerboard", "--output=image.ppm", f"--color={color}"
                )
                self.assertEqual(result.returncode, 2)

    def test_rejects_bad_range_mode(self):
        result = self.run_cli(
            "--algorithm=carreaux", "--output=image.ppm", "--range-mode=invalid"
        )
        self.assertEqual(result.returncode, 2)

    def test_renders_every_named_algorithm(self):
        for algorithm in ("checkerboard", "lasagna", "carreaux"):
            with self.subTest(algorithm=algorithm):
                output = self.root / f"{algorithm}.ppm"
                result = self.render(output, algorithm)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(self.read_ppm(output)[:2], (4, 3))
                self.assertEqual(result.stdout, "")

    def test_checkerboard_applies_color(self):
        output = self.root / "color.ppm"
        result = self.render(output, width=1, height=1, color="123456")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.read_ppm(output)[2], bytes.fromhex("123456"))

    def test_scalar_expression_coordinates(self):
        output = self.root / "scalar.ppm"
        result = self.render_expression(output, "x", width=3, height=1, threads=2)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(
            self.read_ppm(output)[2],
            b"\x00\x00\x00\x7f\x7f\x7f\xff\xff\xff",
        )

    def test_preview_stream_matches_ppm_across_thread_counts(self):
        for threads in (1, 4):
            with self.subTest(threads=threads):
                output = self.root / f"preview-{threads}.ppm"
                result = subprocess.run(
                    [self.executable, "--expression=x+y", f"--output={output}",
                     "--width=31", "--height=17", f"--threads={threads}",
                     "--progress=json", "--preview-stream"],
                    cwd=self.root, env=self.env, capture_output=True, timeout=10,
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout[:4], b"MPR1")
                self.assertEqual(struct.unpack_from("<II", result.stdout, 4), (31, 17))
                rows = {}
                row_bytes = 31 * 3
                self.assertEqual(len(result.stdout), 12 + 17 * (4 + row_bytes))
                for offset in range(12, len(result.stdout), row_bytes + 4):
                    y, = struct.unpack_from("<I", result.stdout, offset)
                    self.assertNotIn(y, rows)
                    rows[y] = result.stdout[offset + 4:offset + 4 + row_bytes]
                self.assertEqual(set(rows), set(range(17)))
                self.assertEqual(b"".join(rows[y] for y in range(17)), self.read_ppm(output)[2])
                events = [json.loads(line)["event"] for line in result.stderr.splitlines()]
                self.assertEqual(events, ["start", "frame", "complete"])

    def test_large_preview_stream_samples_display_not_saved_image(self):
        output = self.root / "large-preview.ppm"
        result = subprocess.run(
            [self.executable, "--expression=x+y", f"--output={output}",
             "--width=2051", "--height=11", "--threads=4",
             "--progress=json", "--preview-stream"],
            cwd=self.root, env=self.env, capture_output=True, timeout=10,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout[:4], b"MPR1")
        self.assertEqual(struct.unpack_from("<II", result.stdout, 4), (684, 4))
        self.assertEqual(self.read_ppm(output)[:2], (2051, 11))
        pixels = self.read_ppm(output)[2]
        sampled_rows = {}
        row_bytes = 684 * 3
        for offset in range(12, len(result.stdout), row_bytes + 4):
            y, = struct.unpack_from("<I", result.stdout, offset)
            sampled_rows[y] = result.stdout[offset + 4:offset + 4 + row_bytes]
        self.assertEqual(set(sampled_rows), set(range(4)))
        for y in range(4):
            expected = b"".join(
                pixels[((y * 3) * 2051 + x * 3) * 3:((y * 3) * 2051 + x * 3) * 3 + 3]
                for x in range(684)
            )
            self.assertEqual(sampled_rows[y], expected)

    def test_preview_stream_requires_single_ppm_and_json(self):
        for extra in (("--progress=json", "--output=bad.gif"),
                      ("--progress=json", "--frames=2", "--output=bad.ppm"),
                      ("--output=bad.ppm",)):
            with self.subTest(extra=extra):
                result = self.run_cli("--algorithm=checkerboard", "--preview-stream", *extra)
                self.assertEqual(result.returncode, 2)
                self.assertFalse((self.root / "bad.ppm").exists())

    def test_broken_preview_pipe_does_not_publish_ppm(self):
        output = self.root / "interrupted-preview.ppm"
        process = subprocess.Popen(
            [self.executable, "--algorithm=lasagna", f"--output={output}",
             "--width=3000", "--height=3000", "--threads=1", "--preview-stream",
             "--progress=json"],
            cwd=self.root, env=self.env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        try:
            self.assertEqual(process.stdout.read(12)[:4], b"MPR1")
            process.stdout.close()
            _, stderr = process.communicate(timeout=10)
            self.assertEqual(process.returncode, 3, stderr)
            self.assertFalse(output.exists())
        finally:
            if process.poll() is None:
                process.kill()
                process.communicate()

    def test_rgb_expression_coordinates(self):
        output = self.root / "rgb.ppm"
        result = self.render_expression(output, rgb=("x", "y", "t"), width=2, height=2)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(
            self.read_ppm(output)[2],
            bytes((0, 0, 0, 255, 0, 0, 0, 255, 0, 255, 255, 0)),
        )

    def test_scalar_palettes_and_monochrome_color(self):
        for palette in ("grayscale", "monochrome", "viridis", "plasma", "magma", "inferno", "turbo"):
            with self.subTest(palette=palette):
                output = self.root / f"{palette}.ppm"
                options = {"palette": palette}
                if palette == "monochrome":
                    options["color"] = "123456"
                result = self.render_expression(output, "x", width=2, height=1, **options)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(self.read_ppm(output)[:2], (2, 1))
        self.assertEqual(self.read_ppm(self.root / "monochrome.ppm")[2][-3:], bytes.fromhex("123456"))
        self.assertEqual(self.read_ppm(self.root / "viridis.ppm")[2][:3], bytes((68, 1, 84)))
        self.assertEqual(self.read_ppm(self.root / "viridis.ppm")[2][-3:], bytes((253, 231, 37)))

    def test_expression_random_is_seeded_and_thread_independent(self):
        first = self.root / "random-first.ppm"
        second = self.root / "random-second.ppm"
        different = self.root / "random-different.ppm"
        self.assertEqual(
            self.render_expression(first, "random()", width=31, height=17, seed=42, threads=1).returncode,
            0,
        )
        self.assertEqual(
            self.render_expression(second, "random()", width=31, height=17, seed=42, threads=4).returncode,
            0,
        )
        self.assertEqual(
            self.render_expression(different, "random()", width=31, height=17, seed=43).returncode,
            0,
        )
        self.assertEqual(first.read_bytes(), second.read_bytes())
        self.assertNotEqual(first.read_bytes(), different.read_bytes())

        rgb = self.root / "random-rgb.ppm"
        result = self.render_expression(
            rgb, rgb=("random()", "random()", "random()"), width=2, height=1, seed=42
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        pixels = self.read_ppm(rgb)[2]
        self.assertTrue(any(len(set(pixels[index : index + 3])) > 1 for index in range(0, len(pixels), 3)))

    def test_expression_animation_variables(self):
        frames = self.root / "expression-frames"
        result = self.render_expression(frames, "t", width=1, height=1, frames=2)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.read_ppm(frames / "frame-000000.ppm")[2], b"\0\0\0")
        self.assertEqual(self.read_ppm(frames / "frame-000001.ppm")[2], b"\x7f\x7f\x7f")

    def test_expression_range_modes(self):
        wrapped = self.root / "expression-wrapped.ppm"
        clamped = self.root / "expression-clamped.ppm"
        self.assertEqual(self.render_expression(wrapped, "2", width=1, height=1).returncode, 0)
        self.assertEqual(
            self.render_expression(clamped, "2", width=1, height=1, range_mode="clamp").returncode,
            0,
        )
        self.assertEqual(self.read_ppm(wrapped)[2], b"\xfe\xfe\xfe")
        self.assertEqual(self.read_ppm(clamped)[2], b"\xff\xff\xff")

    def test_rejects_invalid_expressions(self):
        for expression in ("1 +", "unknown", "sin()", "min(1)", "random(1)", "(1"):
            with self.subTest(expression=expression):
                result = self.render_expression("invalid.ppm", expression)
                self.assertEqual(result.returncode, 2)
                self.assertIn("Expression error", result.stderr)
                self.assertFalse((self.root / "invalid.ppm").exists())

    def test_rejects_expression_mode_conflicts(self):
        cases = (
            ("--algorithm=carreaux", "--expression=x"),
            ("--expression=x", "--expression-r=x", "--expression-g=y", "--expression-b=t"),
            ("--expression-r=x", "--expression-g=y"),
            ("--expression=x", "--scale=2"),
            ("--algorithm=carreaux", "--palette=viridis"),
            ("--algorithm=carreaux", "--seed=1"),
            ("--expression=x", "--color=ffffff"),
            ("--expression-r=x", "--expression-g=y", "--expression-b=t", "--palette=viridis"),
        )
        for options in cases:
            with self.subTest(options=options):
                result = self.run_cli("--output=invalid.ppm", *options)
                self.assertEqual(result.returncode, 2)

        result = self.run_cli("--expression=x", "--output=invalid.ppm", "--palette=unknown")
        self.assertEqual(result.returncode, 2)

    def test_repeated_render_is_deterministic(self):
        first = self.root / "first.ppm"
        second = self.root / "second.ppm"
        self.assertEqual(self.render(first, "lasagna").returncode, 0)
        self.assertEqual(self.render(second, "lasagna").returncode, 0)
        self.assertEqual(first.read_bytes(), second.read_bytes())

    def test_thread_counts_produce_identical_output(self):
        for algorithm in ("checkerboard", "lasagna", "carreaux"):
            for range_mode in ("wrap", "clamp"):
                with self.subTest(algorithm=algorithm, range_mode=range_mode):
                    single = self.root / f"{algorithm}-{range_mode}-single.ppm"
                    multiple = self.root / f"{algorithm}-{range_mode}-multiple.ppm"
                    automatic = self.root / f"{algorithm}-{range_mode}-automatic.ppm"
                    self.assertEqual(
                        self.render(
                            single,
                            algorithm,
                            width=31,
                            height=17,
                            range_mode=range_mode,
                            threads=1,
                        ).returncode,
                        0,
                    )
                    self.assertEqual(
                        self.render(
                            multiple,
                            algorithm,
                            width=31,
                            height=17,
                            range_mode=range_mode,
                            threads=4,
                        ).returncode,
                        0,
                    )
                    self.assertEqual(
                        self.render(
                            automatic,
                            algorithm,
                            width=31,
                            height=17,
                            range_mode=range_mode,
                            threads=0,
                        ).returncode,
                        0,
                    )
                    self.assertEqual(single.read_bytes(), multiple.read_bytes())
                    self.assertEqual(single.read_bytes(), automatic.read_bytes())

    def test_legacy_wrapping_is_default_and_clamping_is_optional(self):
        for algorithm in ("lasagna", "carreaux"):
            with self.subTest(algorithm=algorithm):
                default = self.root / f"{algorithm}-default.ppm"
                wrapped = self.root / f"{algorithm}-wrapped.ppm"
                clamped = self.root / f"{algorithm}-clamped.ppm"
                self.assertEqual(self.render(default, algorithm, width=16, height=12).returncode, 0)
                self.assertEqual(
                    self.render(wrapped, algorithm, width=16, height=12, range_mode="wrap").returncode,
                    0,
                )
                self.assertEqual(
                    self.render(clamped, algorithm, width=16, height=12, range_mode="clamp").returncode,
                    0,
                )
                self.assertEqual(default.read_bytes(), wrapped.read_bytes())
                self.assertNotEqual(default.read_bytes(), clamped.read_bytes())

    def test_multiple_frames_are_numbered(self):
        frames = self.root / "frames"
        frames.mkdir()
        (frames / "unrelated.txt").write_text("keep")
        result = self.render(frames, frames=3, width=2, height=2)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(
            {path.name for path in frames.iterdir()},
            {"frame-000000.ppm", "frame-000001.ppm", "frame-000002.ppm", "unrelated.txt"},
        )
        for frame in frames.glob("*.ppm"):
            self.assertEqual(self.read_ppm(frame)[:2], (2, 2))

    def test_reports_output_open_failure(self):
        (self.root / "blocked").write_text("not a directory")
        result = self.render("blocked/image.ppm")
        self.assertEqual(result.returncode, 3)

    def test_help_succeeds(self):
        result = self.run_cli("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage:", result.stdout)
        self.assertIn("--output", result.stdout)
        self.assertIn("--algorithm", result.stdout)
        self.assertIn("--expression", result.stdout)
        self.assertIn("--palette", result.stdout)
        self.assertIn("--seed", result.stdout)
        self.assertIn("--range-mode", result.stdout)
        self.assertIn("--threads", result.stdout)
        self.assertIn("--version", result.stdout)
        self.assertEqual(result.stderr, "")

    def test_version_succeeds(self):
        result = self.run_cli("--version")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "mescaline 0.4.0\n")
        self.assertEqual(result.stderr, "")

    def test_rejects_unknown_algorithm(self):
        result = self.render("unknown.ppm", "not-an-algorithm", width=2, height=2)
        self.assertEqual(result.returncode, 2)

    def test_rejects_malformed_dimension(self):
        result = self.run_cli(
            "--output=image.ppm", "--algorithm=checkerboard", "--width=invalid"
        )
        self.assertEqual(result.returncode, 2)

    def test_non_square_checkerboard_geometry(self):
        output = self.root / "non-square.ppm"
        result = self.render(output, width=12, height=11, color="204060")
        self.assertEqual(result.returncode, 0, result.stderr)
        width, height, pixels = self.read_ppm(output)
        on = bytes.fromhex("204060")
        off = b"\0\0\0"
        expected = b"".join(
            on if ((x // 10) + ((y // 10) % 2)) % 2 == 0 else off
            for y in range(height)
            for x in range(width)
        )
        self.assertEqual(pixels, expected)

    def test_creates_absolute_output_parents(self):
        output = self.root / "nested" / "path" / "image.ppm"
        result = self.render(output, width=2, height=2)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(output.is_file())

    def test_rejects_existing_output_without_force(self):
        output = self.root / "existing.ppm"
        output.write_bytes(b"keep me")
        result = self.render(output, width=2, height=2)
        self.assertEqual(result.returncode, 3)
        self.assertEqual(output.read_bytes(), b"keep me")

        result = self.render(output, width=2, height=2, force=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.read_ppm(output)[:2], (2, 2))

    def test_sequence_preflight_prevents_partial_output(self):
        frames = self.root / "frames"
        frames.mkdir()
        existing = frames / "frame-000001.ppm"
        existing.write_bytes(b"keep me")
        result = self.render(frames, frames=3, width=2, height=2)
        self.assertEqual(result.returncode, 3)
        self.assertFalse((frames / "frame-000000.ppm").exists())
        self.assertEqual(existing.read_bytes(), b"keep me")

    def test_forced_sequence_rolls_back_if_publication_fails(self):
        frames = self.root / "frames"
        frames.mkdir()
        original = frames / "frame-000000.ppm"
        original.write_bytes(b"original")
        (frames / "frame-000001.ppm").mkdir()
        result = self.render(frames, frames=2, width=2, height=2, force=True)
        self.assertEqual(result.returncode, 3)
        self.assertEqual(original.read_bytes(), b"original")
        self.assertTrue((frames / "frame-000001.ppm").is_dir())
        self.assertFalse(list(frames.glob(".mescaline-frames-*")))

    def test_rejects_ppm_animation_and_unknown_extension(self):
        ppm = self.render("animation.ppm", frames=2)
        self.assertEqual(ppm.returncode, 2)
        unknown = self.render("image.png")
        self.assertEqual(unknown.returncode, 2)

    def test_propagates_encoder_failure(self):
        result = self.render("image.gif", env={"FAKE_FFMPEG_STATUS": "7"})
        self.assertEqual(result.returncode, 5)
        self.assertFalse((self.root / "image.gif").exists())
        staging = list(self.root.glob("image.gif.frames.*"))
        self.assertEqual(len(staging), 1)
        self.assertTrue((staging[0] / "ffmpeg.log").exists())
        self.assertIn("frames retained", result.stderr)

    def test_encodes_gif_with_explicit_arguments(self):
        arguments = self.root / "ffmpeg-arguments.json"
        output = self.root / "animation.gif"
        result = self.render(
            output,
            "carreaux",
            frames=3,
            fps=24,
            env={"FAKE_FFMPEG_ARGS": str(arguments)},
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(output.read_bytes(), b"GIF89a")
        ffmpeg_arguments = json.loads(arguments.read_text())
        self.assertEqual(ffmpeg_arguments[ffmpeg_arguments.index("-framerate") + 1], "24")
        self.assertEqual(ffmpeg_arguments[ffmpeg_arguments.index("-frames:v") + 1], "3")
        self.assertFalse(list(self.root.glob("animation.gif.frames.*")))
        self.assertFalse(list(self.root.glob("animation.gif.tmp.*")))

    def test_output_path_is_not_interpreted_by_a_shell(self):
        arguments = self.root / "arguments.json"
        output = self.root / "danger;$(touch owned)% name.gif"
        result = self.render(output, env={"FAKE_FFMPEG_ARGS": str(arguments)})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(output.is_file())
        self.assertFalse((self.root / "owned").exists())
        ffmpeg_arguments = json.loads(arguments.read_text())
        pattern = ffmpeg_arguments[ffmpeg_arguments.index("-i") + 1]
        self.assertIn("%%", pattern)

    def test_ffmpeg_paths_cannot_be_options_or_protocols(self):
        for output in ("--not-an-option.gif", "file:not-a-protocol.gif"):
            with self.subTest(output=output):
                result = self.render(output)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertTrue((self.root / output).is_file())

    def test_json_progress_is_machine_readable(self):
        result = self.run_cli(
            "--algorithm=checkerboard",
            "--output=frames",
            "--width=2",
            "--height=2",
            "--frames=2",
            "--progress=json",
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        records = [json.loads(line) for line in result.stderr.splitlines()]
        self.assertEqual(
            records,
            [
                {
                    "version": 1,
                    "event": "start",
                    "mode": "algorithm",
                    "algorithm": "checkerboard",
                    "palette": None,
                    "range_mode": "wrap",
                    "width": 2,
                    "height": 2,
                    "frames": 2,
                    "threads": 2,
                    "output_kind": "sequence",
                },
                {"version": 1, "event": "frame", "index": 0, "completed": 1, "total": 2},
                {"version": 1, "event": "frame", "index": 1, "completed": 2, "total": 2},
                {
                    "version": 1,
                    "event": "complete",
                    "output": str((self.root / "frames").resolve()),
                },
            ],
        )
        self.assertEqual(result.stdout, "")

    def test_json_expression_start_events(self):
        cases = (
            (
                ("--expression=x", "--palette=viridis"),
                {
                    "version": 1,
                    "event": "start",
                    "mode": "scalar",
                    "algorithm": None,
                    "palette": "viridis",
                    "range_mode": "wrap",
                    "width": 1,
                    "height": 1,
                    "frames": 1,
                    "threads": 1,
                    "output_kind": "ppm",
                },
            ),
            (
                ("--expression-r=x", "--expression-g=y", "--expression-b=t"),
                {
                    "version": 1,
                    "event": "start",
                    "mode": "rgb",
                    "algorithm": None,
                    "palette": None,
                    "range_mode": "wrap",
                    "width": 1,
                    "height": 1,
                    "frames": 1,
                    "threads": 1,
                    "output_kind": "ppm",
                },
            ),
        )
        for index, (mode_arguments, expected) in enumerate(cases):
            with self.subTest(mode=expected["mode"]):
                result = self.run_cli(
                    *mode_arguments,
                    f"--output=expression-{index}.ppm",
                    "--width=1",
                    "--height=1",
                    "--progress=json",
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                start = json.loads(result.stderr.splitlines()[0])
                self.assertEqual(start, expected)

    def test_json_encoding_event(self):
        result = self.run_cli(
            "--algorithm=checkerboard",
            "--output=animation.gif",
            "--width=1",
            "--height=1",
            "--progress=json",
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        records = [json.loads(line) for line in result.stderr.splitlines()]
        self.assertEqual(
            [record["event"] for record in records],
            ["start", "frame", "encoding", "complete"],
        )
        self.assertEqual(records[2], {"version": 1, "event": "encoding"})

    def test_json_escapes_paths_and_replaces_invalid_bytes(self):
        filename = b"caf\xc3\xa9-\"\n\\-\xff.ppm"
        result = subprocess.run(
            [
                os.fsencode(self.executable),
                b"--algorithm=checkerboard",
                b"--output=" + filename,
                b"--width=1",
                b"--height=1",
                b"--progress=json",
            ],
            cwd=os.fsencode(self.root),
            env=self.env,
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        records = [json.loads(line) for line in result.stderr.decode("utf-8").splitlines()]
        self.assertTrue(records[-1]["output"].endswith('/café-"\n\\-�.ppm'))
        self.assertTrue(os.path.isfile(os.path.join(os.fsencode(self.root), filename)))

    def test_nonfinite_expression_results_warn_and_map_to_zero(self):
        result = self.run_cli(
            "--expression=1/0",
            "--output=nonfinite.ppm",
            "--width=2",
            "--height=2",
            "--progress=json",
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        records = [json.loads(line) for line in result.stderr.splitlines()]
        warning = next(record for record in records if record["event"] == "warning")
        self.assertEqual(warning, {"version": 1, "event": "warning", "kind": "nonfinite", "count": 4})
        self.assertEqual(self.read_ppm(self.root / "nonfinite.ppm")[2], b"\0" * 12)

    def test_json_errors_do_not_depend_on_option_order(self):
        for arguments in (
            ("--progress=json", "--width=bad"),
            ("--width=bad", "--progress=json"),
        ):
            with self.subTest(arguments=arguments):
                result = self.run_cli(*arguments)
                self.assertEqual(result.returncode, 2)
                error = json.loads(result.stderr)
                self.assertEqual(
                    error,
                    {
                        "version": 1,
                        "event": "error",
                        "status": 2,
                        "message": "Invalid width 'bad'",
                    },
                )

    def test_none_progress_is_silent(self):
        result = self.render("image.ppm")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "")
        self.assertEqual(result.stderr, "")

    def test_sigterm_cancels_encoder_and_removes_temporary_output(self):
        command = [
            self.executable,
            "--algorithm=checkerboard",
            "--output=animation.gif",
            "--width=2",
            "--height=2",
            "--progress=json",
        ]
        env = self.env.copy()
        env["FAKE_FFMPEG_SLEEP"] = "10"
        started = self.root / "ffmpeg-started"
        env["FAKE_FFMPEG_STARTED"] = str(started)
        process = subprocess.Popen(
            command,
            cwd=self.root,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and not started.exists() and process.poll() is None:
            time.sleep(0.01)
        self.assertTrue(started.exists(), "FFmpeg did not start")
        process.send_signal(signal.SIGTERM)
        stdout, stderr = process.communicate(timeout=5)
        self.assertEqual(process.returncode, 128 + signal.SIGTERM, stderr)
        self.assertEqual(stdout, "")
        self.assertEqual(
            json.loads(stderr.splitlines()[-1]),
            {"version": 1, "event": "cancelled", "signal": signal.SIGTERM},
        )
        self.assertFalse((self.root / "animation.gif").exists())
        self.assertFalse(list(self.root.glob("animation.gif.frames.*")))

    def test_sigterm_cancels_render_without_publishing_sequence(self):
        command = [
            self.executable,
            "--algorithm=lasagna",
            "--output=frames",
            "--width=5000",
            "--height=5000",
            "--frames=2",
            "--progress=json",
        ]
        process = subprocess.Popen(
            command,
            cwd=self.root,
            env=self.env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        self.wait_for_event(process, "start")
        process.send_signal(signal.SIGTERM)
        stdout, stderr = process.communicate(timeout=5)
        self.assertEqual(process.returncode, 128 + signal.SIGTERM, stderr)
        self.assertEqual(stdout, "")
        frames = self.root / "frames"
        self.assertFalse(list(frames.glob("frame-*.ppm")))
        self.assertFalse(list(frames.glob(".mescaline-frames-*")))


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]], verbosity=2)
