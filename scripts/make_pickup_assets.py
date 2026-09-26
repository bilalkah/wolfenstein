#!/usr/bin/env python3
"""Draws the pickup sprites and synthesises the pickup sound.

Pixel art in the chunky style of the other sprites, written as PNGs with a
transparent background, and a short rising chime as a WAV; standard library
only, so rerunning gives the same files.

    ./scripts/make_pickup_assets.py   # writes assets/sprites/pickups/*.png
                                      # and assets/sounds/pickup.wav
"""

import math
import struct
import wave
import zlib
from pathlib import Path

ASSETS = Path(__file__).resolve().parent.parent / "assets"

CLEAR = (0, 0, 0, 0)
OUTLINE = (24, 20, 18, 255)


class Canvas:
    def __init__(self, width, height):
        self.width, self.height = width, height
        self.pixels = [[CLEAR] * width for _ in range(height)]

    def rect(self, x0, y0, x1, y1, colour):
        """Fills columns x0..x1 and rows y0..y1, inclusive."""
        for y in range(max(y0, 0), min(y1, self.height - 1) + 1):
            for x in range(max(x0, 0), min(x1, self.width - 1) + 1):
                self.pixels[y][x] = colour

    def box(self, x0, y0, x1, y1, face, light, dark):
        """A box with an outline, a lit top-left edge and a shaded one."""
        self.rect(x0, y0, x1, y1, OUTLINE)
        self.rect(x0 + 1, y0 + 1, x1 - 1, y1 - 1, face)
        self.rect(x0 + 1, y0 + 1, x1 - 1, y0 + 1, light)
        self.rect(x0 + 1, y0 + 1, x0 + 1, y1 - 1, light)
        self.rect(x0 + 1, y1 - 1, x1 - 1, y1 - 1, dark)
        self.rect(x1 - 1, y0 + 1, x1 - 1, y1 - 1, dark)

    def save(self, path):
        raw = b"".join(
            b"\x00" + bytes(channel for pixel in row for channel in pixel)
            for row in self.pixels)

        def chunk(kind, data):
            return (struct.pack(">I", len(data)) + kind + data +
                    struct.pack(">I", zlib.crc32(kind + data)))

        header = struct.pack(">IIBBBBB", self.width, self.height, 8, 6, 0, 0, 0)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) +
                         chunk(b"IDAT", zlib.compress(raw, 9)) +
                         chunk(b"IEND", b""))


def cross(canvas, cx, cy, arm, thickness, colour, shade):
    half = thickness // 2
    canvas.rect(cx - arm, cy - half, cx + arm, cy - half + thickness - 1, colour)
    canvas.rect(cx - half, cy - arm, cx - half + thickness - 1, cy + arm, colour)
    canvas.rect(cx - arm, cy - half + thickness - 1, cx + arm,
                cy - half + thickness - 1, shade)


def medkit():
    """A white first-aid box with a red cross."""
    canvas = Canvas(48, 36)
    canvas.rect(19, 6, 28, 8, OUTLINE)  # handle
    canvas.rect(21, 7, 26, 8, CLEAR)
    canvas.box(4, 9, 43, 35, (222, 222, 214, 255), (250, 250, 244, 255),
               (160, 160, 150, 255))
    cross(canvas, 24, 22, 8, 6, (200, 24, 24, 255), (140, 12, 12, 255))
    return canvas


def large_medkit():
    """A red field case with a white cross and straps: more health."""
    canvas = Canvas(48, 36)
    canvas.rect(17, 3, 30, 5, OUTLINE)
    canvas.rect(19, 4, 28, 5, CLEAR)
    canvas.box(1, 6, 46, 35, (176, 30, 26, 255), (214, 64, 56, 255),
               (112, 16, 14, 255))
    for x in (9, 38):  # straps
        canvas.rect(x, 7, x + 1, 34, (96, 70, 44, 255))
    cross(canvas, 24, 20, 9, 6, (240, 240, 232, 255), (180, 180, 170, 255))
    return canvas


def ammo_box():
    """An olive ammunition crate, open, with brass rounds showing."""
    canvas = Canvas(48, 36)
    for i in range(7):  # rounds standing in the crate
        x = 8 + i * 5
        canvas.rect(x, 4, x + 3, 13, OUTLINE)
        canvas.rect(x + 1, 5, x + 2, 12, (212, 168, 60, 255))
        canvas.rect(x + 1, 5, x + 2, 7, (150, 110, 60, 255))  # bullet tip
    canvas.box(4, 12, 43, 35, (92, 104, 58, 255), (126, 140, 80, 255),
               (60, 68, 36, 255))
    canvas.rect(15, 20, 32, 26, (212, 196, 120, 255))  # stencilled label
    canvas.rect(16, 21, 31, 25, (92, 104, 58, 255))
    canvas.rect(18, 22, 29, 24, (212, 196, 120, 255))
    return canvas


def chime(path):
    """Two quick rising notes with a soft attack and decay."""
    rate = 22050
    frames = []
    for start, frequency, length in ((0.0, 880.0, 0.09), (0.07, 1318.5, 0.16)):
        offset = int(start * rate)
        for i in range(int(length * rate)):
            t = i / rate
            envelope = min(1.0, t / 0.005) * math.exp(-t * 18.0)
            sample = envelope * (math.sin(2 * math.pi * frequency * t) +
                                 0.3 * math.sin(4 * math.pi * frequency * t))
            index = offset + i
            while len(frames) <= index:
                frames.append(0.0)
            frames[index] += sample
    peak = max(abs(sample) for sample in frames)
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(rate)
        out.writeframes(b"".join(
            struct.pack("<h", int(sample / peak * 0.6 * 32767))
            for sample in frames))


def main():
    for name, draw in (("medkit", medkit), ("large_medkit", large_medkit),
                       ("ammo_box", ammo_box)):
        draw().save(ASSETS / "sprites" / "pickups" / f"{name}.png")
    chime(ASSETS / "sounds" / "pickup.wav")


if __name__ == "__main__":
    main()
