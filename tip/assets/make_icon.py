"""Generates completionist.ico: a pine-green rounded square with a white C and a caret bar.

    python make_icon.py

Uses only the standard library. The .ico holds PNG images at several sizes (supported since Vista).
"""

import math
import struct
import zlib
from pathlib import Path

SIZES = (16, 24, 32, 48, 64)
GREEN = (10, 90, 61)  # Evergreen --sign, #0A5A3D
WHITE = (255, 255, 255)
PINK = (255, 158, 210)  # Evergreen --vms-ink, #FF9ED2: the caret
SUPERSAMPLE = 4


def coverage(x: float, y: float) -> tuple[float, tuple[int, int, int]]:
    """Alpha and colour at a point of the unit square."""
    radius = 0.22
    cx = min(max(x, radius), 1 - radius)
    cy = min(max(y, radius), 1 - radius)
    if (x - cx) ** 2 + (y - cy) ** 2 > radius**2:
        return 0.0, GREEN
    dx, dy = x - 0.40, y - 0.50
    in_ring = 0.15 <= math.hypot(dx, dy) <= 0.27
    in_opening = dx > 0 and abs(dy) < dx  # the gap on the right, +-45 degrees
    c_shape = in_ring and not in_opening
    caret = 0.72 <= x <= 0.78 and 0.30 <= y <= 0.70
    return 1.0, PINK if caret else WHITE if c_shape else GREEN


def render(size: int) -> bytes:
    rows = bytearray()
    for py in range(size):
        rows.append(0)  # PNG filter: none
        for px in range(size):
            alpha = r = g = b = 0.0
            for sy in range(SUPERSAMPLE):
                for sx in range(SUPERSAMPLE):
                    a, colour = coverage((px + (sx + 0.5) / SUPERSAMPLE) / size, (py + (sy + 0.5) / SUPERSAMPLE) / size)
                    alpha += a
                    r += colour[0] * a
                    g += colour[1] * a
                    b += colour[2] * a
            samples = SUPERSAMPLE**2
            if alpha:
                rows += bytes((round(r / alpha), round(g / alpha), round(b / alpha), round(255 * alpha / samples)))
            else:
                rows += bytes(4)
    return rows


def png(size: int) -> bytes:
    def chunk(kind: bytes, data: bytes) -> bytes:
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

    header = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)  # 8-bit RGBA
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(render(size)), 9)) + chunk(b"IEND", b"")


def main() -> None:
    images = [png(size) for size in SIZES]
    out = bytearray(struct.pack("<HHH", 0, 1, len(images)))
    offset = 6 + 16 * len(images)
    for size, image in zip(SIZES, images):
        out += struct.pack("<BBBBHHII", size, size, 0, 0, 1, 32, len(image), offset)
        offset += len(image)
    for image in images:
        out += image
    target = Path(__file__).with_name("completionist.ico")
    target.write_bytes(out)
    print(f"wrote {target} ({len(out)} bytes, sizes {SIZES})")


if __name__ == "__main__":
    main()
