"""Converts the harness's .raw captures (width, height, BGRA pixels) to PNG files. Standard library only."""

import struct
import sys
import zlib
from pathlib import Path


def png(width: int, height: int, bgra: bytes) -> bytes:
    def chunk(kind: bytes, data: bytes) -> bytes:
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

    rows = bytearray()
    for y in range(height):
        rows.append(0)
        line = bgra[y * width * 4 : (y + 1) * width * 4]
        for x in range(width):
            b, g, r = line[x * 4], line[x * 4 + 1], line[x * 4 + 2]
            rows += bytes((r, g, b))
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(rows), 6)) + chunk(b"IEND", b"")


for raw in sorted(Path(sys.argv[1]).glob("*.raw")):
    data = raw.read_bytes()
    width, height = struct.unpack_from("<ii", data)
    raw.with_suffix(".png").write_bytes(png(width, height, data[8:]))
    raw.unlink()
    print("wrote", raw.with_suffix(".png"))
