#!/usr/bin/env python3
"""Draws the pickup sprites (health, ammunition, keys, weapons), the knife and
pistol in the player's hand, the door textures, the exit switch and the mark
on secret walls, and synthesises the pickup sound.

Pixel art in the chunky style of the other sprites, written as PNGs (the
pickups on a transparent background), and a short rising chime as a WAV;
standard library only, so rerunning gives the same files.

    ./scripts/make_art.py   # writes assets/sprites/pickups/*.png,
                            # assets/sprites/weapon/{knife,pistol}/*/*.png,
                            # assets/textures/door*.png, exit.png,
                            # secret_mark.png, the puffs where shots land
                            # (assets/sprites/effects/*), bullet_mark.png
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


# 3x5 pixel letters for the exit sign
LETTERS = {
    "E": ["###", "#..", "##.", "#..", "###"],
    "X": ["#.#", "#.#", ".#.", "#.#", "#.#"],
    "I": ["###", ".#.", ".#.", ".#.", "###"],
    "T": ["###", ".#.", ".#.", ".#.", ".#."],
}


def exit_switch():
    """The level's way out: a riveted panel under a lit EXIT sign, with a
    big lever to throw."""
    canvas = Canvas(64, 64)
    steel, light, dark = (74, 80, 88, 255), (110, 118, 128, 255), \
        (44, 48, 54, 255)
    canvas.rect(0, 0, 63, 63, OUTLINE)
    canvas.box(1, 1, 62, 62, steel, light, dark)
    # The sign: green letters on a dark strip
    canvas.rect(12, 5, 51, 17, OUTLINE)
    canvas.rect(13, 6, 50, 16, (18, 40, 24, 255))
    x = 16
    for letter in "EXIT":
        for row, line in enumerate(LETTERS[letter]):
            for column, pixel in enumerate(line):
                if pixel == "#":
                    canvas.rect(x + column * 2, 7 + row * 2,
                                x + column * 2 + 1, 8 + row * 2,
                                (96, 240, 120, 255))
        x += 8
    # The lever in its slot
    canvas.rect(26, 24, 37, 57, OUTLINE)
    canvas.rect(27, 25, 36, 56, (30, 32, 36, 255))
    canvas.rect(30, 30, 33, 48, (150, 156, 164, 255))  # the arm
    canvas.rect(27, 26, 36, 31, (200, 40, 32, 255))	 # its red handle
    canvas.rect(27, 26, 36, 26, (240, 110, 96, 255))
    for rx, ry in ((6, 24), (55, 24), (6, 56), (55, 56)):
        canvas.rect(rx, ry, rx + 1, ry + 1, (170, 176, 184, 255))
    return canvas


def secret_mark():
    """What gives a secret wall away, to a careful eye: a jagged crack with
    a couple of branches and chipped spots, over a faintly worn patch, drawn
    over the wall's own texture."""
    canvas = Canvas(48, 32)
    crack = (18, 14, 12, 160)
    chip = (235, 228, 214, 70)
    wear = (255, 248, 230, 22)
    for x0, y0, x1, y1 in ((14, 9, 33, 22), (10, 12, 37, 19), (18, 6, 29, 25)):
        canvas.rect(x0, y0, x1, y1, wear)  # an uneven worn patch
    # The crack: runs of pixels stepping down and across
    path = [(6, 4), (9, 6), (11, 7), (13, 10), (14, 12), (17, 13), (19, 15),
            (20, 18), (23, 19), (25, 21), (26, 24), (29, 25), (31, 27),
            (34, 28), (36, 30)]
    for (x0, y0), (x1, y1) in zip(path, path[1:]):
        for t in range(max(abs(x1 - x0), abs(y1 - y0)) + 1):
            n = max(abs(x1 - x0), abs(y1 - y0)) or 1
            x = round(x0 + (x1 - x0) * t / n)
            y = round(y0 + (y1 - y0) * t / n)
            canvas.rect(x, y, x, y, crack)
    for x, y in ((18, 12), (19, 11), (21, 10), (22, 9)):  # a branch up
        canvas.rect(x, y, x, y, crack)
    for x, y in ((26, 22), (28, 21), (30, 21), (32, 20)):  # a branch across
        canvas.rect(x, y, x, y, crack)
    for x, y in ((15, 13), (24, 20), (33, 29)):  # chipped edges
        canvas.rect(x, y, x + 1, y, chip)
    return canvas


# The player's hand and sleeve, as in the other weapons' sprites
GLOVE = ((42, 42, 46, 255), (74, 74, 80, 255))
SLEEVE = ((70, 76, 58, 255), (96, 102, 78, 255))
FLASH = ((255, 244, 180, 255), (255, 196, 64, 255))
FIRST_PERSON = (180, 84)  # the MP5 sprite's proportions, at half its size


def rounded(canvas, x0, y0, x1, y1, colour, light=None, dark=None):
    """A filled box with its corners cut, outlined; optionally lit along
    its top and shaded along its bottom."""
    canvas.rect(x0 + 1, y0, x1 - 1, y1, OUTLINE)
    canvas.rect(x0, y0 + 1, x1, y1 - 1, OUTLINE)
    canvas.rect(x0 + 1, y0 + 1, x1 - 1, y1 - 1, colour)
    if light:
        canvas.rect(x0 + 2, y0 + 1, x1 - 2, y0 + 1, light)
    if dark:
        canvas.rect(x0 + 2, y1 - 1, x1 - 2, y1 - 1, dark)


def hand(canvas, x0, y0):
    """A gloved hand gripping from below, thumb over the grip, its sleeve
    running off the frame's lower right."""
    sleeve, sleeve_light = SLEEVE
    glove, glove_light = GLOVE
    glove_dark = (26, 26, 30, 255)
    # The sleeve, with a cuff
    rounded(canvas, x0 + 12, y0 + 16, x0 + 60, 90, sleeve, sleeve_light)
    canvas.rect(x0 + 13, y0 + 17, x0 + 59, y0 + 20, (58, 62, 48, 255))
    # The fist: four fingers stacked round the grip
    for finger in range(4):
        y = y0 + 2 + finger * 5
        rounded(canvas, x0 + 1, y, x0 + 25, y + 6, glove, glove_light,
                glove_dark)
    # The thumb, over the grip's top
    rounded(canvas, x0 - 3, y0 - 3, x0 + 12, y0 + 4, glove, glove_light,
            glove_dark)


def flash(canvas, cx, cy, size):
    """A muzzle flash: a bright star over a warm burst."""
    outer, inner = FLASH[1], FLASH[0]
    canvas.rect(cx - size, cy - 1, cx + size, cy + 1, outer)
    canvas.rect(cx - 1, cy - size, cx + 1, cy + size, outer)
    for d in range(1, size // 2 + 1):
        for sx, sy in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
            canvas.rect(cx + sx * d, cy + sy * d, cx + sx * d, cy + sy * d,
                        outer)
    canvas.rect(cx - size // 2, cy, cx + size // 2, cy, inner)
    canvas.rect(cx, cy - size // 2, cx, cy + size // 2, inner)


def pistol(dx=0, dy=0, muzzle=0):
    """A pistol held out, seen from behind and a little above: its slide
    narrowing to the sights, the grip going down into the hand."""
    canvas = Canvas(*FIRST_PERSON)
    x0, y0 = 98 + dx, 26 + dy
    if muzzle:
        flash(canvas, x0 + 9, y0 - 7, muzzle)
    steel, light, dark = (58, 60, 68, 255), (118, 122, 134, 255), \
        (36, 38, 44, 255)
    # The slide: wide at the back, narrower towards the muzzle
    for row in range(16):
        inset = max(0, 5 - row // 3)
        y = y0 + row
        canvas.rect(x0 + inset - 1, y, x0 + 19 - inset, y, OUTLINE)
        canvas.rect(x0 + inset, y, x0 + 18 - inset, y,
                    light if row == 0 else steel)
    canvas.rect(x0 + 8, y0 - 3, x0 + 10, y0, OUTLINE)  # front sight
    canvas.rect(x0 + 9, y0 - 2, x0 + 9, y0 - 1, light)
    canvas.rect(x0 + 1, y0 + 11, x0 + 5, y0 + 13, OUTLINE)  # rear sight
    canvas.rect(x0 + 13, y0 + 11, x0 + 17, y0 + 13, OUTLINE)
    canvas.rect(x0 + 2, y0 + 7, x0 + 16, y0 + 7, dark)  # ejection line
    # The frame and grip, into the hand
    canvas.rect(x0 + 3, y0 + 16, x0 + 15, y0 + 26, OUTLINE)
    canvas.rect(x0 + 4, y0 + 16, x0 + 14, y0 + 25, dark)
    hand(canvas, x0 - 2, y0 + 22)
    return canvas


def knife(dx=0, dy=0):
    """A knife held blade up, from the lower right."""
    canvas = Canvas(*FIRST_PERSON)
    x0, y0 = 104 + dx, 44 + dy
    blade, edge = (190, 194, 204, 255), (240, 242, 248, 255)
    # The blade narrows to its point, leaning left
    for row in range(31):
        y = y0 - row
        width = max(1, 7 - row // 5)
        left = x0 + 4 - row // 3
        canvas.rect(left - 1, y, left + width, y, OUTLINE)
        canvas.rect(left, y, left + width - 1, y, blade)
        canvas.rect(left, y, left, y, edge)
    canvas.rect(x0, y0, x0 + 16, y0 + 2, OUTLINE)  # the guard
    canvas.rect(x0 + 1, y0 + 1, x0 + 15, y0 + 1, (150, 154, 164, 255))
    canvas.rect(x0 + 4, y0 + 3, x0 + 12, y0 + 12, OUTLINE)  # the handle
    canvas.rect(x0 + 5, y0 + 3, x0 + 11, y0 + 11, (84, 56, 36, 255))
    hand(canvas, x0 - 2, y0 + 10)
    return canvas


def floor_weapon(kind):
    """A weapon lying on the floor, seen from the side."""
    canvas = Canvas(48, 20)
    dark, light = (34, 34, 38, 255), (78, 80, 90, 255)
    if kind == "mp5":
        canvas.rect(8, 6, 38, 12, OUTLINE)  # receiver
        canvas.rect(9, 7, 37, 11, dark)
        canvas.rect(9, 7, 37, 7, light)
        canvas.rect(38, 8, 46, 10, OUTLINE)  # barrel
        canvas.rect(2, 7, 8, 10, OUTLINE)  # stock
        canvas.rect(22, 12, 26, 19, OUTLINE)  # magazine
        canvas.rect(23, 12, 25, 18, dark)
        canvas.rect(14, 12, 17, 17, OUTLINE)  # grip
    else:
        wood, wood_light = (110, 70, 40, 255), (150, 100, 60, 255)
        canvas.rect(1, 9, 18, 15, OUTLINE)  # stock
        canvas.rect(2, 10, 17, 14, wood)
        canvas.rect(2, 10, 17, 10, wood_light)
        canvas.rect(18, 8, 30, 13, OUTLINE)  # receiver
        canvas.rect(19, 9, 29, 12, dark)
        canvas.rect(30, 7, 47, 9, OUTLINE)  # barrels
        canvas.rect(30, 10, 46, 12, OUTLINE)
        canvas.rect(31, 8, 46, 8, light)
        canvas.rect(31, 11, 45, 11, light)
    return canvas


def weapon_clips():
    """Every clip of the knife and pistol: frames by clip name."""
    idle_knife = knife()
    return {
        "knife": {
            # A stab: back, forward and up, back
            "loaded": [idle_knife, knife(8, 5), knife(-16, -12),
                       knife(-6, -4)],
            "outofammo": [idle_knife],
            "reload": [idle_knife],
        },
        "pistol": {
            "loaded": [pistol(), pistol(0, -3, 7), pistol(0, -2, 4),
                       pistol(0, -1)],
            "outofammo": [pistol(), pistol(0, 1)],  # a dry click
            # Lowered, the magazine swapped, raised again
            "reload": [pistol(6, 8), pistol(12, 20), pistol(14, 26),
                       pistol(12, 20), pistol(6, 8), pistol()],
        },
    }


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


# --------------------------------------------------------------- impacts

# A shot lands half a wall up; the puffs are drawn on a canvas the effect's
# size (0.4 of a wall by 0.8 of one), the puff where that height is
PUFF_CANVAS = (32, 64)
PUFF_CENTRE = (16, 24)


def splatter(canvas, cx, cy, radius, colour, seed, count, spread, fall=0.0):
    """A blob of `radius` and `count` droplets flung up to `spread` from it,
    dropping by `fall` px per px of distance; the same seed gives the same
    drops."""
    state = seed
    def rand():
        nonlocal state
        state = (state * 1103515245 + 12345) & 0x7FFFFFFF
        return state / 0x7FFFFFFF
    for y in range(canvas.height):
        for x in range(canvas.width):
            if (x - cx) ** 2 + (y - cy) ** 2 <= radius * radius:
                canvas.pixels[y][x] = colour
    for _ in range(count):
        angle = rand() * math.tau
        distance = radius + rand() * spread
        x = round(cx + math.cos(angle) * distance)
        y = round(cy + math.sin(angle) * distance + fall * distance)
        size = 1 if rand() < 0.6 else 2
        canvas.rect(x, y, x + size - 1, y + size - 1, colour)


def blood_puff(frame):
    canvas = Canvas(*PUFF_CANVAS)
    cx, cy = PUFF_CENTRE
    dark, red, bright = (96, 8, 8, 255), (168, 18, 16, 255), (224, 46, 36, 255)
    if frame == 0:
        splatter(canvas, cx, cy, 3, red, 7, 6, 3)
        splatter(canvas, cx, cy, 1, bright, 3, 0, 0)
    elif frame == 1:
        splatter(canvas, cx, cy, 5, dark, 11, 10, 6, 0.1)
        splatter(canvas, cx, cy, 3, red, 5, 6, 4)
        splatter(canvas, cx - 1, cy - 1, 1, bright, 3, 0, 0)
    elif frame == 2:
        splatter(canvas, cx, cy + 1, 5, (96, 8, 8, 200), 13, 12, 8, 0.3)
        splatter(canvas, cx, cy + 1, 3, (150, 16, 14, 220), 9, 4, 3, 0.3)
    else:
        splatter(canvas, cx, cy + 3, 3, (80, 6, 6, 140), 17, 10, 9, 0.6)
    return canvas


def dust_puff(frame):
    canvas = Canvas(*PUFF_CANVAS)
    cx, cy = PUFF_CENTRE
    if frame == 0:
        splatter(canvas, cx, cy, 2, (150, 142, 128, 255), 19, 5, 3)
        cross(canvas, cx, cy, 3, 1, (255, 236, 150, 255), (255, 196, 64, 255))
    elif frame == 1:
        splatter(canvas, cx, cy, 4, (132, 126, 114, 230), 23, 8, 5)
        splatter(canvas, cx - 1, cy - 1, 2, (170, 162, 148, 240), 29, 0, 0)
    elif frame == 2:
        splatter(canvas, cx, cy - 1, 6, (128, 122, 112, 150), 31, 8, 5, -0.1)
    else:
        splatter(canvas, cx, cy - 2, 7, (128, 122, 112, 70), 37, 6, 4, -0.2)
    return canvas


def bullet_mark():
    """A chipped hole: black in the middle, a dark bruise round it and a
    pale ring of stone knocked bare, so it reads on dark walls and light."""
    canvas = Canvas(16, 16)
    splatter(canvas, 7.5, 7.5, 6.2, (168, 160, 148, 110), 41, 8, 1.5)
    splatter(canvas, 7.5, 7.5, 4.4, (52, 48, 44, 210), 43, 5, 1.2)
    splatter(canvas, 7.5, 7.5, 2.4, (10, 8, 8, 255), 47, 0, 0)
    return canvas


def main():
    for name, draw in (("medkit", medkit), ("large_medkit", large_medkit),
                       ("ammo_box", ammo_box)):
        draw().save(ASSETS / "sprites" / "pickups" / f"{name}.png")
    key(GOLD).save(ASSETS / "sprites" / "pickups" / "gold_key.png")
    key(SILVER).save(ASSETS / "sprites" / "pickups" / "silver_key.png")
    door().save(ASSETS / "textures" / "door.png")
    door(GOLD).save(ASSETS / "textures" / "door_gold.png")
    door(SILVER).save(ASSETS / "textures" / "door_silver.png")
    exit_switch().save(ASSETS / "textures" / "exit.png")
    for kind in ("mp5", "shotgun"):
        floor_weapon(kind).save(ASSETS / "sprites" / "pickups" /
                                f"{kind}_pickup.png")
    for weapon, clips in weapon_clips().items():
        for clip, frames in clips.items():
            for index, frame in enumerate(frames):
                frame.save(ASSETS / "sprites" / "weapon" / weapon / clip /
                           f"{index}.png")
    secret_mark().save(ASSETS / "textures" / "secret_mark.png")
    for frame in range(4):
        blood_puff(frame).save(ASSETS / "sprites" / "effects" / "blood" /
                               f"{frame}.png")
        dust_puff(frame).save(ASSETS / "sprites" / "effects" / "dust" /
                              f"{frame}.png")
    bullet_mark().save(ASSETS / "textures" / "bullet_mark.png")
    chime(ASSETS / "sounds" / "pickup.wav")


if __name__ == "__main__":
    main()
