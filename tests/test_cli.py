#!/usr/bin/env python3

import os
from pathlib import Path
import subprocess
import sys
import tempfile
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
        ffmpeg.write_text('#!/bin/sh\nexit "${FAKE_FFMPEG_STATUS:-0}"\n')
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

    def render(self, output, algorithm="checkerboard", width=4, height=3, env=None, **kwargs):
        args = [
            f"--output={output}",
            f"--algo={algorithm}",
            f"--horizontal={width}",
            f"--vertical={height}",
        ]
        args.extend(f"--{name}={value}" for name, value in kwargs.items())
        return self.run_cli(*args, env=env)

    def test_requires_output(self):
        result = self.run_cli("--algo=checkerboard")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("--output is required", result.stderr)

    def test_requires_algorithm(self):
        result = self.run_cli("--output=image.ppm")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("--algo is required", result.stderr)

    def test_rejects_bad_frame_counts(self):
        for frames in ("0", "1001", "invalid"):
            with self.subTest(frames=frames):
                result = self.run_cli(
                    "--output=image.ppm", "--algo=checkerboard", f"--frames={frames}"
                )
                self.assertNotEqual(result.returncode, 0)

    def test_renders_every_named_algorithm(self):
        for algorithm in ("checkerboard", "lasagna", "carreaux"):
            with self.subTest(algorithm=algorithm):
                output = self.root / f"{algorithm}.ppm"
                result = self.render(output, algorithm)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(self.read_ppm(output)[:2], (4, 3))

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

    def test_multiple_frames_are_numbered(self):
        result = self.render("frames/request.ppm", frames=3, width=2, height=2)
        self.assertEqual(result.returncode, 0, result.stderr)
        frames = self.root / "frames"
        self.assertEqual({path.name for path in frames.iterdir()}, {"0.ppm", "1.ppm", "2.ppm"})
        for frame in frames.iterdir():
            self.assertEqual(self.read_ppm(frame)[:2], (2, 2))

    def test_reports_output_open_failure(self):
        (self.root / "blocked").mkdir()
        result = self.render("blocked")
        self.assertNotEqual(result.returncode, 0)

    def test_help_succeeds(self):
        result = self.run_cli("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage:", result.stdout)
        self.assertIn("--output", result.stdout)
        self.assertIn("--algo", result.stdout)
        self.assertEqual(result.stderr, "")

    @unittest.expectedFailure
    def test_rejects_unknown_algorithm(self):
        result = self.render("unknown.ppm", "not-an-algorithm", width=2, height=2)
        self.assertNotEqual(result.returncode, 0)

    @unittest.expectedFailure
    def test_rejects_malformed_dimension(self):
        result = self.run_cli(
            "--output=image.ppm", "--algo=checkerboard", "--horizontal=invalid"
        )
        self.assertNotEqual(result.returncode, 0)

    @unittest.expectedFailure
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

    @unittest.expectedFailure
    def test_creates_absolute_output_parents(self):
        output = self.root / "nested" / "path" / "image.ppm"
        result = self.render(output, width=2, height=2)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(output.is_file())

    @unittest.expectedFailure
    def test_rejects_existing_output_without_force(self):
        output = self.root / "existing.ppm"
        output.write_bytes(b"keep me")
        result = self.render(output, width=2, height=2)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(output.read_bytes(), b"keep me")

    @unittest.expectedFailure
    def test_propagates_encoder_failure(self):
        result = self.render("image.ppm", env={"FAKE_FFMPEG_STATUS": "7"})
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]], verbosity=2)
