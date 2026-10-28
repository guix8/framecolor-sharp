#!/usr/bin/env python3
"""Generate a vkBasalt .cube using FrameColor's gain/gamma/contrast/saturation.

Usage: generate_vulkan_lut.py current.txt output.cube [size]
"""

import sys
import math
from pathlib import Path


def transform(value: float, contrast: float, gamma: float, gain: float = 1.0) -> float:
    contrast = min(1.0, max(0.0, contrast))
    gamma = min(2.50, max(0.50, gamma))
    gain = min(1.50, max(0.50, gain))
    color = min(1.0, max(0.0, value * gain)) ** (1.0 / gamma)
    smooth = color * color * (3.0 - 2.0 * color)
    return min(1.0, max(0.0, color * (1.0 - contrast) + smooth * contrast))


def apply_saturation(
    rgb: tuple[float, float, float], saturation: float | tuple[float, float, float]
) -> tuple[float, float, float]:
    levels = (saturation,) * 3 if isinstance(saturation, (int, float)) else saturation
    levels = tuple(min(2.0, max(0.0, level)) for level in levels)
    if levels == (1.0, 1.0, 1.0):
        return rgb
    luminance = rgb[0] * 0.2126 + rgb[1] * 0.7152 + rgb[2] * 0.0722
    return tuple(min(1.0, max(0.0, luminance + (channel - luminance) * levels[index]))
                 for index, channel in enumerate(rgb))


def main() -> None:
    if len(sys.argv) not in (3, 4):
        raise SystemExit(__doc__)
    values = [float(value) for value in Path(sys.argv[1]).read_text().split()]
    if len(values) < 6:
        raise SystemExit("current.txt must contain three contrast and three gamma values")
    if not all(math.isfinite(value) for value in values):
        raise SystemExit("current.txt contains a non-finite value")
    if any(not 0.0 <= value <= 1.0 for value in values[:3]):
        raise SystemExit("contrast must be in 0.0..1.0")
    if any(not 0.50 <= value <= 2.50 for value in values[3:6]):
        raise SystemExit("gamma must be in 0.50..2.50")
    if any(not 0.50 <= value <= 1.50 for value in values[6:9]):
        raise SystemExit("gain must be in 0.50..1.50")
    if len(values) == 11:
        raise SystemExit("saturation must contain one legacy value or three channel values")
    if any(not 0.0 <= value <= 2.0 for value in values[9:12]):
        raise SystemExit("saturation must be in 0.0..2.0")
    # The 13th value is the sharpen amount and the 14th is the compare bypass.
    # The Vulkan layer applies both in the shader, so they do not change the LUT.
    if any(not 0.0 <= value <= 1.0 for value in values[12:13]):
        raise SystemExit("sharpen must be in 0.0..1.0")
    if any(value not in (0.0, 1.0) for value in values[13:14]):
        raise SystemExit("bypass must be 0 or 1")
    size = int(sys.argv[3]) if len(sys.argv) == 4 else 33
    if not 2 <= size <= 65:
        raise SystemExit("LUT size must be between 2 and 65")
    contrast = values[:3]
    gamma = values[3:6]
    gain = values[6:9] if len(values) >= 9 else [1.0] * 3
    saturation = tuple(values[9:12]) if len(values) >= 12 else (values[9],) * 3 if len(values) == 10 else (1.0,) * 3
    lines = ["# FrameColor Vulkan test LUT", f"LUT_3D_SIZE {size}", "DOMAIN_MIN 0 0 0", "DOMAIN_MAX 1 1 1"]
    for blue in range(size):
        for green in range(size):
            for red in range(size):
                rgb = (red, green, blue)
                corrected = tuple(transform(rgb[i] / (size - 1), contrast[i], gamma[i], gain[i]) for i in range(3))
                transformed = apply_saturation(corrected, saturation)
                lines.append(" ".join(f"{channel:.7f}" for channel in transformed))
    Path(sys.argv[2]).write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
