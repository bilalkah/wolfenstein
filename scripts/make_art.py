#!/usr/bin/env python3
"""Draws the pickup sprites (health, ammunition, keys) and the door textures,
and synthesises the pickup sound.

Pixel art in the chunky style of the other sprites, written as PNGs (the
pickups on a transparent background), and a short rising chime as a WAV;
standard library only, so rerunning gives the same files.

    ./scripts/make_art.py   # writes assets/sprites/pickups/*.png,
                            # assets/textures/door*.png and
                            # assets/sounds/pickup.wav
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


# Metal colours for keys and locks: face, highlight, shadow
GOLD = ((214, 170, 56, 255), (250, 222, 120, 255), (140, 100, 24, 255))
SILVER = ((176, 184, 196, 255), (232, 236, 244, 255), (104, 110, 124, 255))


def key(metal):
    """A large old key lying flat: a ring bow, a shaft and a toothed bit."""
    face, light, dark = metal
    canvas = Canvas(40, 40)
    # Bow: a square ring
    canvas.rect(4, 12, 17, 27, OUTLINE)
    canvas.rect(5, 13, 16, 26, face)
    canvas.rect(5, 13, 16, 14, light)
    canvas.rect(9, 17, 12, 22, OUTLINE)
    # Shaft
    canvas.rect(17, 17, 36, 22, OUTLINE)
    canvas.rect(17, 18, 36, 21, face)
    canvas.rect(17, 18, 36, 18, light)
    canvas.rect(17, 21, 36, 21, dark)
    # Bit: two teeth below the shaft's end
    for x in (27, 32):
        canvas.rect(x, 22, x + 3, 29, OUTLINE)
        canvas.rect(x + 1, 22, x + 2, 28, dark)
    return canvas


def door(lock=None):
    """A steel door: riveted plates, a bracing band, a viewing slit. A locked
    door's band and handle are its key's metal, with a keyhole."""
    canvas = Canvas(64, 64)
    steel, light, dark = (86, 100, 122, 255), (122, 138, 160, 255), \
        (52, 62, 78, 255)
    canvas.rect(0, 0, 63, 63, OUTLINE)
    canvas.box(1, 1, 62, 62, steel, light, dark)
    for y0, y1 in ((4, 28), (35, 59)):  # two plates
        canvas.box(4, y0, 59, y1, steel, light, dark)
    canvas.rect(2, 30, 61, 33, (70, 82, 100, 255))  # band between them
    canvas.rect(2, 30, 61, 30, light)
    canvas.rect(2, 33, 61, 33, dark)
    for x in (7, 18, 29, 40, 51, 56):  # rivets along the band
        canvas.rect(x, 31, x + 1, 32, (180, 190, 205, 255))
    for x, y in ((7, 7), (55, 7), (7, 55), (55, 55), (7, 25), (55, 25),
                 (7, 38), (55, 38)):  # plate corners
        canvas.rect(x, y, x + 1, y + 1, (180, 190, 205, 255))
        canvas.rect(x + 1, y + 1, x + 1, y + 1, dark)
    canvas.rect(22, 12, 41, 16, OUTLINE)  # viewing slit
    canvas.rect(23, 13, 40, 15, (16, 16, 20, 255))
    canvas.rect(50, 44, 53, 51, OUTLINE)  # handle
    canvas.rect(51, 45, 52, 50, (200, 170, 90, 255))
    if lock is not None:
        face, light, dark = lock
        canvas.rect(2, 30, 61, 33, face)  # the band in the key's metal
        canvas.rect(2, 30, 61, 30, light)
        canvas.rect(2, 33, 61, 33, dark)
        canvas.rect(46, 40, 57, 55, OUTLINE)  # lock plate with a keyhole
        canvas.rect(47, 41, 56, 54, face)
        canvas.rect(47, 41, 56, 41, light)
        canvas.rect(50, 44, 53, 47, OUTLINE)
        canvas.rect(51, 48, 52, 51, OUTLINE)
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
    key(GOLD).save(ASSETS / "sprites" / "pickups" / "gold_key.png")
    key(SILVER).save(ASSETS / "sprites" / "pickups" / "silver_key.png")
    door().save(ASSETS / "textures" / "door.png")
    door(GOLD).save(ASSETS / "textures" / "door_gold.png")
    door(SILVER).save(ASSETS / "textures" / "door_silver.png")
    chime(ASSETS / "sounds" / "pickup.wav")


if __name__ == "__main__":
    main()
