#!/usr/bin/env python3

import os
from pathlib import Path
import json
import selectors
import signal
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

    def test_requires_output(self):
        result = self.run_cli("--algorithm=checkerboard")
        self.assertEqual(result.returncode, 2)
        self.assertIn("--output is required", result.stderr)

    def test_requires_algorithm(self):
        result = self.run_cli("--output=image.ppm")
        self.assertEqual(result.returncode, 2)
        self.assertIn("--algorithm is required", result.stderr)

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
        self.assertIn("--range-mode", result.stdout)
        self.assertIn("--threads", result.stdout)
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
        events = [record["event"] for record in records]
        self.assertEqual(events, ["start", "frame", "frame", "complete"])
        self.assertEqual(records[0]["range_mode"], "wrap")
        self.assertEqual(records[0]["threads"], 2)
        self.assertEqual(result.stdout, "")

    def test_json_errors_do_not_depend_on_option_order(self):
        for arguments in (
            ("--progress=json", "--width=bad"),
            ("--width=bad", "--progress=json"),
        ):
            with self.subTest(arguments=arguments):
                result = self.run_cli(*arguments)
                self.assertEqual(result.returncode, 2)
                error = json.loads(result.stderr)
                self.assertEqual(error["event"], "error")

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
