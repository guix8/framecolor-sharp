import math
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from generate_vulkan_lut import apply_saturation, transform  # noqa: E402


class LutTests(unittest.TestCase):
    def test_neutral_curve(self):
        for x in (0.0, 0.1, 0.5, 0.9, 1.0):
            self.assertAlmostEqual(transform(x, 0.0, 1.0), x)

    def test_gamma_changes_midtones(self):
        self.assertGreater(transform(0.5, 0.0, 2.0), 0.5)
        self.assertLess(transform(0.5, 0.0, 0.5), 0.5)

    def test_gain_changes_brightness_before_gamma(self):
        self.assertAlmostEqual(transform(0.5, 0.0, 1.0, 1.5), 0.75)
        self.assertAlmostEqual(transform(0.5, 0.0, 1.0, 0.5), 0.25)
        self.assertAlmostEqual(transform(0.5, 0.0, 2.0, 0.5), math.sqrt(0.25))
        self.assertAlmostEqual(transform(1.0, 0.0, 1.0, 1.5), 1.0)

    def test_curve_is_bounded(self):
        for contrast in (0.0, 0.5, 1.0):
            for gamma in (0.5, 1.0, 2.5):
                for point in range(101):
                    value = transform(point / 100, contrast, gamma)
                    self.assertTrue(math.isfinite(value))
                    self.assertGreaterEqual(value, 0.0)
                    self.assertLessEqual(value, 1.0)

    def test_cube_dimensions(self):
        with tempfile.TemporaryDirectory() as directory:
            values = Path(directory) / "current.txt"
            output = Path(directory) / "framecolor.cube"
            values.write_text("0 0 0 1 1 1 1 1 1\n")
            subprocess.run(
                [sys.executable, str(ROOT / "tools/generate_vulkan_lut.py"), str(values), str(output), "3"],
                check=True,
            )
            rows = [line for line in output.read_text().splitlines() if line and line[0].isdigit()]
            self.assertEqual(len(rows), 27)
            self.assertEqual(rows[0], "0.0000000 0.0000000 0.0000000")
            self.assertEqual(rows[-1], "1.0000000 1.0000000 1.0000000")

    def test_cube_uses_independent_channel_gains(self):
        with tempfile.TemporaryDirectory() as directory:
            values = Path(directory) / "current.txt"
            output = Path(directory) / "framecolor.cube"
            values.write_text("0 0 0 1 1 1 1.5 1 0.5\n")
            subprocess.run(
                [sys.executable, str(ROOT / "tools/generate_vulkan_lut.py"), str(values), str(output), "3"],
                check=True,
            )
            rows = [line for line in output.read_text().splitlines() if line and line[0].isdigit()]
            self.assertEqual(rows[13], "0.7500000 0.5000000 0.2500000")

    def test_saturation_neutral_and_grayscale(self):
        rgb = (1.0, 0.2, 0.1)
        self.assertEqual(apply_saturation(rgb, 1.0), rgb)
        gray = apply_saturation(rgb, 0.0)
        self.assertAlmostEqual(gray[0], gray[1])
        self.assertAlmostEqual(gray[1], gray[2])
        self.assertGreater(apply_saturation(rgb, 2.0)[0] - apply_saturation(rgb, 2.0)[1], rgb[0] - rgb[1])

    def test_cube_saturation_is_cross_channel(self):
        with tempfile.TemporaryDirectory() as directory:
            values = Path(directory) / "current.txt"
            output = Path(directory) / "framecolor.cube"
            values.write_text("0 0 0 1 1 1 1 1 1 0\n")
            subprocess.run(
                [sys.executable, str(ROOT / "tools/generate_vulkan_lut.py"), str(values), str(output), "3"],
                check=True,
            )
            rows = [line for line in output.read_text().splitlines() if line and line[0].isdigit()]
            self.assertEqual(rows[1], "0.1063000 0.1063000 0.1063000")

    def test_channel_saturation_changes_only_selected_output(self):
        rgb = (0.5, 0.0, 0.0)
        self.assertEqual(apply_saturation(rgb, (0.0, 1.0, 1.0)), (0.1063, 0.0, 0.0))
        with tempfile.TemporaryDirectory() as directory:
            values = Path(directory) / "current.txt"
            output = Path(directory) / "framecolor.cube"
            values.write_text("0 0 0 1 1 1 1 1 1 0 1 1\n")
            subprocess.run(
                [sys.executable, str(ROOT / "tools/generate_vulkan_lut.py"), str(values), str(output), "3"],
                check=True,
            )
            rows = [line for line in output.read_text().splitlines() if line and line[0].isdigit()]
            self.assertEqual(rows[1], "0.1063000 0.0000000 0.0000000")

    def test_sharpen_value_does_not_change_cube(self):
        with tempfile.TemporaryDirectory() as directory:
            values = Path(directory) / "current.txt"
            plain = Path(directory) / "plain.cube"
            sharpened = Path(directory) / "sharpened.cube"
            values.write_text("0 0 0 1 1 1 1 1 1 1 1 1\n")
            subprocess.run(
                [sys.executable, str(ROOT / "tools/generate_vulkan_lut.py"), str(values), str(plain), "3"],
                check=True,
            )
            values.write_text("0 0 0 1 1 1 1 1 1 1 1 1 0.6\n")
            subprocess.run(
                [sys.executable, str(ROOT / "tools/generate_vulkan_lut.py"), str(values), str(sharpened), "3"],
                check=True,
            )
            self.assertEqual(plain.read_text(), sharpened.read_text())

    def test_sharpen_out_of_range_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            values = Path(directory) / "current.txt"
            values.write_text("0 0 0 1 1 1 1 1 1 1 1 1 1.5\n")
            result = subprocess.run(
                [sys.executable, str(ROOT / "tools/generate_vulkan_lut.py"), str(values),
                 str(Path(directory) / "out.cube"), "3"],
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("sharpen", result.stderr)


if __name__ == "__main__":
    unittest.main()
