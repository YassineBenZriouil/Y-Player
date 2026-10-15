"""Draw the Y-Player icon (ADR-018) with no third-party libraries.

    python scripts/make_icon.py   ->  app/resources/y-player.ico (16..256 px) and y-player.png (256 px)

A teal rounded square with a white "Y" monogram, rendered from signed distances with 4x4
supersampling. Regenerate after changing the colours or geometry below.
"""

import math
import struct
import zlib
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "app" / "resources"
SIZES = [16, 24, 32, 48, 64, 128, 256]
TOP, BOTTOM = (0x23, 0xD3, 0xC3), (0x12, 0xA9, 0x9C)  # brand teal gradient
INK = (0xFF, 0xFF, 0xFF)
RADIUS = 0.22   # corner radius, fraction of the size
STROKE = 0.115  # Y stroke width
STROKES = [((0.30, 0.25), (0.50, 0.50)), ((0.70, 0.25), (0.50, 0.50)), ((0.50, 0.50), (0.50, 0.78))]


def rounded_rect_inside(x, y):
    r = RADIUS
    cx = min(max(x, r), 1 - r)
    cy = min(max(y, r), 1 - r)
    return (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def segment_distance(px, py, a, b):
    ax, ay = a
    bx, by = b
    dx, dy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def render(size):
    ss = 4
    rows = []
    for j in range(size):
        row = bytearray([0])  # PNG filter type 0
        for i in range(size):
            bg = ink = 0
            for sj in range(ss):
                for si in range(ss):
                    x = (i + (si + 0.5) / ss) / size
                    y = (j + (sj + 0.5) / ss) / size
                    if rounded_rect_inside(x, y):
                        bg += 1
                        if min(segment_distance(x, y, a, b) for a, b in STROKES) <= STROKE / 2:
                            ink += 1
            n = ss * ss
            alpha = bg / n
            t = j / max(1, size - 1)
            base = [TOP[k] + (BOTTOM[k] - TOP[k]) * t for k in range(3)]
            mix = ink / bg if bg else 0.0
            rgb = [round(base[k] * (1 - mix) + INK[k] * mix) for k in range(3)]
            row += bytes(rgb + [round(alpha * 255)])
        rows.append(bytes(row))
    return b"".join(rows)


def png(size, pixels):
    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)  # 8-bit RGBA
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(pixels, 9)) + chunk(b"IEND", b"")


def ico(images):
    head = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries, blobs = b"", b""
    for size, data in images:
        dim = 0 if size >= 256 else size  # 0 means 256 in the ICO directory
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), offset + len(blobs))
        blobs += data
    return head + entries + blobs


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    images = [(s, png(s, render(s))) for s in SIZES]
    (OUT / "y-player.ico").write_bytes(ico(images))
    (OUT / "y-player.png").write_bytes(images[-1][1])
    print("wrote", OUT / "y-player.ico", "and y-player.png")


if __name__ == "__main__":
    main()
