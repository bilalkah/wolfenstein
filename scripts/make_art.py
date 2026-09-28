#!/usr/bin/env python3
"""Draws the pickup sprites (health, ammunition, keys, weapons), the door
textures, the exit switch and the mark on secret walls, and synthesises the
pickup sound.

Pixel art in the chunky style of the other sprites, written as PNGs (the
pickups on a transparent background), and as WAVs a short rising chime,
the pistol's and the MP5's shots, a dry click for an empty gun, the thud
of the player falling dead, doors, an enemy's alert shout, footsteps and
the ammunition, key and weapon pickups;
standard library only, so rerunning gives the same files.

    ./scripts/make_art.py   # writes assets/sprites/pickups/*.png,
                            # assets/textures/door*.png, exit.png,
                            # secret_mark.png, the puffs where shots land
                            # (assets/sprites/effects/*), bullet_mark.png
                            # and assets/sounds/*.wav (all but the Doom ones)
"""

import math
import random
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


# --------------------------------------------------------------- sounds

SOUND_RATE = 44100


def write_wave(path, samples, loudness=0.9):
    """Mono 16-bit samples, scaled so the loudest is `loudness` of full."""
    peak = max(abs(sample) for sample in samples) or 1.0
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(SOUND_RATE)
        out.writeframes(b"".join(
            struct.pack("<h", int(sample / peak * loudness * 32767))
            for sample in samples))


def lowpass(samples, cutoff):
    """One-pole low-pass at `cutoff` Hz."""
    alpha = 1 - math.exp(-2 * math.pi * cutoff / SOUND_RATE)
    out, level = [], 0.0
    for sample in samples:
        level += alpha * (sample - level)
        out.append(level)
    return out


def gunshot(seed, seconds, crack, body, body_cutoff, thump_from, thump_to,
            thump_decay, tail, tail_level):
    """A shot: a sharp crack of noise, a duller body of it, a falling low
    thump and a room's tail. Decays in seconds; the same seed gives the same
    shot."""
    rng = random.Random(seed)
    count = int(seconds * SOUND_RATE)
    noise = [rng.uniform(-1.0, 1.0) for _ in range(count)]
    dull = lowpass(noise, body_cutoff)
    room = lowpass(noise, 500.0)
    samples = []
    phase = 0.0
    for i in range(count):
        t = i / SOUND_RATE
        # The crack: bright noise, the low part taken out
        bright = noise[i] - dull[i]
        sample = 2.2 * bright * math.exp(-t / crack)
        sample += 1.2 * dull[i] * math.exp(-t / body)
        frequency = thump_to + (thump_from - thump_to) * math.exp(-t / 0.015)
        phase += 2 * math.pi * frequency / SOUND_RATE
        sample += 0.35 * math.sin(phase) * math.exp(-t / thump_decay)
        sample += tail_level * room[i] * math.exp(-t / tail)
        # A fast attack, so it does not click in
        sample *= min(1.0, t / 0.0005)
        samples.append(math.tanh(1.2 * sample))
    return samples


def pistol_shot():
    return gunshot(seed=101, seconds=0.45, crack=0.012, body=0.04,
                   body_cutoff=1800.0, thump_from=220.0, thump_to=95.0,
                   thump_decay=0.035, tail=0.15, tail_level=0.9)


def smg_shot():
    """Lighter and shorter than the pistol's, so bursts do not smear."""
    return gunshot(seed=202, seconds=0.26, crack=0.008, body=0.022,
                   body_cutoff=2600.0, thump_from=260.0, thump_to=120.0,
                   thump_decay=0.02, tail=0.07, tail_level=0.6)


def body_fall():
    """A body hitting the floor: a dull, low thud and the short scuff of it
    settling."""
    rng = random.Random(404)
    count = int(0.45 * SOUND_RATE)
    noise = [rng.uniform(-1.0, 1.0) for _ in range(count)]
    dull = lowpass(lowpass(noise, 300.0), 300.0)
    scuff = lowpass(noise, 1200.0)
    samples, phase = [], 0.0
    for i in range(count):
        t = i / SOUND_RATE
        frequency = 55.0 + 40.0 * math.exp(-t / 0.03)
        phase += 2 * math.pi * frequency / SOUND_RATE
        sample = 1.0 * math.sin(phase) * math.exp(-t / 0.07)
        sample += 3.0 * dull[i] * math.exp(-t / 0.05)
        if t > 0.12:
            sample += 0.25 * scuff[i] * math.exp(-(t - 0.12) / 0.06)
        samples.append(math.tanh(sample * min(1.0, t / 0.002)))
    return samples


def resonate(samples, frequency, bandwidth):
    """A two-pole resonator: rings at `frequency`, `bandwidth` Hz wide."""
    r = math.exp(-math.pi * bandwidth / SOUND_RATE)
    c = 2 * r * math.cos(2 * math.pi * frequency / SOUND_RATE)
    out, y1, y2 = [], 0.0, 0.0
    for sample in samples:
        y = (1 - r) * sample + c * y1 - r * r * y2
        out.append(y)
        y1, y2 = y, y1
    return out


def door_slide():
    """A heavy door sliding in its frame: a low rumble that swells and
    fades, and a clunk as it stops."""
    rng = random.Random(505)
    count = int(0.7 * SOUND_RATE)
    noise = [rng.uniform(-1.0, 1.0) for _ in range(count)]
    rumble = lowpass(lowpass(noise, 220.0), 220.0)
    grind = resonate(noise, 520.0, 90.0)
    samples, phase = [], 0.0
    for i in range(count):
        t = i / SOUND_RATE
        swell = math.sin(math.pi * min(t / 0.5, 1.0)) if t < 0.5 else 0.0
        sample = 5.0 * rumble[i] * swell + 0.6 * grind[i] * swell
        if t >= 0.48:  # the stop
            since = t - 0.48
            phase += 2 * math.pi * 70.0 / SOUND_RATE
            sample += 0.8 * math.sin(phase) * math.exp(-since / 0.05)
            sample += 2.0 * rumble[i] * math.exp(-since / 0.03)
        samples.append(math.tanh(sample))
    return samples


def alert_shout(seed, pitch):
    """A short gruff shout ("Hah!"): a buzzing voice through the resonances
    of an open vowel, its pitch falling, with breath in it."""
    rng = random.Random(seed)
    count = int(0.32 * SOUND_RATE)
    source, phase = [], 0.0
    for i in range(count):
        t = i / SOUND_RATE
        frequency = pitch * (1.15 - 0.35 * min(t / 0.3, 1.0))
        phase = (phase + frequency / SOUND_RATE) % 1.0
        buzz = 2 * phase - 1  # a sawtooth: the vocal folds
        breath = rng.uniform(-1.0, 1.0) * 0.35
        source.append(buzz + breath)
    voice = [a + 0.7 * b + 0.3 * c for a, b, c in zip(
        resonate(source, 700.0, 110.0), resonate(source, 1150.0, 130.0),
        resonate(source, 2500.0, 200.0))]
    peak = max(abs(sample) for sample in voice) or 1.0
    samples = []
    for i, sample in enumerate(lowpass(voice, 3200.0)):
        t = i / SOUND_RATE
        envelope = min(1.0, t / 0.02) * math.exp(-max(t - 0.12, 0.0) / 0.07)
        samples.append(math.tanh(1.3 * sample / peak * envelope))
    return samples


def footstep(seed):
    """A boot on stone: a soft low thud and a scuff."""
    rng = random.Random(seed)
    count = int(0.16 * SOUND_RATE)
    noise = [rng.uniform(-1.0, 1.0) for _ in range(count)]
    thud = lowpass(lowpass(noise, 350.0), 350.0)
    scuff = resonate(noise, 1800.0 + 300.0 * (seed % 3), 900.0)
    samples = []
    for i in range(count):
        t = i / SOUND_RATE
        sample = 6.0 * thud[i] * math.exp(-t / 0.025)
        sample += 0.25 * scuff[i] * math.exp(-t / 0.018)
        samples.append(math.tanh(sample * min(1.0, t / 0.002)))
    return samples


def metal_clicks(seed, clicks):
    """Metal parts meeting: each (start, pitch, level) a short bright ring."""
    rng = random.Random(seed)
    count = int((max(start for start, _, _ in clicks) + 0.08) * SOUND_RATE)
    samples = [0.0] * count
    for start, pitch, level in clicks:
        offset = int(start * SOUND_RATE)
        for i in range(int(0.06 * SOUND_RATE)):
            t = i / SOUND_RATE
            ring = (math.sin(2 * math.pi * pitch * t) +
                    0.5 * math.sin(2 * math.pi * pitch * 2.76 * t))
            tick = rng.uniform(-1.0, 1.0) * math.exp(-t / 0.0015)
            if offset + i < count:
                samples[offset + i] += level * (
                    0.6 * ring * math.exp(-t / 0.012) + 0.6 * tick)
    return samples


def ammo_pickup():
    """A box of rounds taken: a magazine's two clicks."""
    return metal_clicks(606, ((0.0, 2400.0, 1.0), (0.07, 1900.0, 0.8)))


def key_pickup():
    """A key taken: keys on a ring, jingling."""
    return metal_clicks(707, ((0.0, 3300.0, 0.8), (0.05, 4100.0, 0.6),
                              (0.09, 2900.0, 0.7), (0.15, 3700.0, 0.4)))


def gun_cock():
    """A gun taken: its action worked, back and forth, and a clack."""
    rng = random.Random(808)
    slide = [s * 0.35 for s in resonate([rng.uniform(-1.0, 1.0)
                                          for _ in range(int(0.3 * SOUND_RATE))],
                                         1500.0, 700.0)]
    clicks = metal_clicks(809, ((0.0, 1700.0, 1.0), (0.2, 1300.0, 1.2)))
    samples = [0.0] * max(len(slide), len(clicks))
    for i, sample in enumerate(clicks):
        samples[i] += sample
    for i, sample in enumerate(slide):
        t = i / SOUND_RATE
        # The slide sounds between the two clicks
        if 0.02 <= t <= 0.19:
            samples[i] += sample * math.sin(math.pi * (t - 0.02) / 0.17)
    return samples


def dry_click():
    """The trigger pulled on an empty gun: two small metal clicks."""
    rng = random.Random(303)
    count = int(0.12 * SOUND_RATE)
    samples = [0.0] * count
    for start, pitch, level in ((0.0, 3100.0, 1.0), (0.045, 2300.0, 0.6)):
        offset = int(start * SOUND_RATE)
        for i in range(int(0.02 * SOUND_RATE)):
            t = i / SOUND_RATE
            ring = math.sin(2 * math.pi * pitch * t) * math.exp(-t / 0.003)
            tick = rng.uniform(-1.0, 1.0) * math.exp(-t / 0.0008)
            samples[offset + i] += level * (0.7 * ring + 0.5 * tick)
    return samples


# --------------------------------------------------------------- impacts

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


# The puffs where shots land: square, the puff at the middle, which the game
# puts where the shot struck. Drawn smooth (each pixel covered as much as a
# shape covers it), so they stay soft up close.
PUFF_SIZE = 96


class SoftCanvas:
    """Colour laid on in layers, each pixel as much as a shape covers it."""

    def __init__(self, size):
        self.size = size
        self.rgba = [[0.0, 0.0, 0.0, 0.0] for _ in range(size * size)]

    def disc(self, cx, cy, radius, colour, alpha, mist=False):
        """A disc; a mist fades from its middle to nothing at its edge."""
        r, g, b = colour
        for y in range(max(0, int(cy - radius - 1)),
                       min(self.size, int(cy + radius + 2))):
            for x in range(max(0, int(cx - radius - 1)),
                           min(self.size, int(cx + radius + 2))):
                distance = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
                if mist:
                    cover = max(0.0, 1.0 - (distance / radius) ** 2)
                else:
                    cover = min(1.0, max(0.0, radius + 0.5 - distance))
                a = alpha * cover
                if a <= 0.0:
                    continue
                pixel = self.rgba[y * self.size + x]
                keep = 1.0 - a
                pixel[0] = r * a + pixel[0] * keep
                pixel[1] = g * a + pixel[1] * keep
                pixel[2] = b * a + pixel[2] * keep
                pixel[3] = a + pixel[3] * keep

    def droplets(self, cx, cy, seed, count, reach, size, colour, alpha,
                 fall=0.0, near=0.0):
        """`count` drops flung between `near` and `reach` from the middle,
        dropping `fall` px per px out; the same seed gives the same drops."""
        rng = random.Random(seed)
        for _ in range(count):
            angle = rng.uniform(0.0, math.tau)
            out = rng.uniform(near, reach)
            radius = size * rng.uniform(0.5, 1.0)
            self.disc(cx + math.cos(angle) * out,
                      cy + math.sin(angle) * out + fall * out, radius, colour,
                      alpha)

    def canvas(self):
        out = Canvas(self.size, self.size)
        for y in range(self.size):
            for x in range(self.size):
                r, g, b, a = self.rgba[y * self.size + x]
                if a > 0.0:
                    out.pixels[y][x] = (round(r / a), round(g / a), round(b / a),
                                        round(a * 255))
        return out


BLOOD_DARK, BLOOD, BLOOD_BRIGHT = (100, 8, 8), (160, 18, 16), (210, 38, 30)


def blood_puff(frame):
    """A spray of blood: a burst, spreading into a fine mist and drops that
    fall and fade."""
    soft = SoftCanvas(PUFF_SIZE)
    c = PUFF_SIZE / 2
    if frame == 0:
        soft.disc(c, c, 7, BLOOD_DARK, 0.5, mist=True)
        soft.droplets(c, c, 1, 18, 9, 1.6, BLOOD, 1.0)
        soft.disc(c, c, 3.5, BLOOD_BRIGHT, 1.0)
    elif frame == 1:
        soft.disc(c, c, 14, BLOOD_DARK, 0.45, mist=True)
        soft.droplets(c, c, 2, 26, 18, 1.8, BLOOD, 1.0, fall=0.1, near=4)
        soft.disc(c, c, 5, BLOOD, 0.9, mist=True)
    elif frame == 2:
        soft.disc(c, c + 3, 20, BLOOD_DARK, 0.28, mist=True)
        soft.droplets(c, c, 3, 26, 28, 1.6, BLOOD_DARK, 0.9, fall=0.3, near=8)
    else:
        soft.disc(c, c + 6, 24, BLOOD_DARK, 0.12, mist=True)
        soft.droplets(c, c, 4, 18, 34, 1.3, BLOOD_DARK, 0.55, fall=0.55,
                      near=14)
    return soft.canvas()


DUST, DUST_LIGHT, SPARK = (128, 122, 112), (170, 162, 148), (255, 226, 140)


def dust_puff(frame):
    """Stone struck: a spark and chips, then a cloud of dust that thins."""
    soft = SoftCanvas(PUFF_SIZE)
    c = PUFF_SIZE / 2
    if frame == 0:
        soft.disc(c, c, 6, DUST_LIGHT, 0.6, mist=True)
        soft.droplets(c, c, 5, 10, 12, 1.4, DUST, 1.0)
        soft.disc(c, c, 3, SPARK, 1.0, mist=True)
    elif frame == 1:
        soft.disc(c, c, 11, DUST_LIGHT, 0.6, mist=True)
        soft.droplets(c, c, 6, 12, 18, 1.4, DUST, 0.9, fall=0.2, near=4)
    elif frame == 2:
        soft.disc(c, c - 2, 16, DUST, 0.4, mist=True)
    else:
        soft.disc(c, c - 4, 20, DUST, 0.18, mist=True)
    return soft.canvas()


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
    secret_mark().save(ASSETS / "textures" / "secret_mark.png")
    for frame in range(4):
        blood_puff(frame).save(ASSETS / "sprites" / "effects" / "blood" /
                               "frames" / f"{frame}.png")
        dust_puff(frame).save(ASSETS / "sprites" / "effects" / "dust" /
                              "frames" / f"{frame}.png")
    bullet_mark().save(ASSETS / "textures" / "bullet_mark.png")
    chime(ASSETS / "sounds" / "pickup.wav")
    # A little under the shotgun, whose shot is compressed loud
    write_wave(ASSETS / "sounds" / "pistol.wav", pistol_shot(), loudness=0.3)
    write_wave(ASSETS / "sounds" / "mp5.wav", smg_shot(), loudness=0.24)
    write_wave(ASSETS / "sounds" / "dry_fire.wav", dry_click(), loudness=0.1)
    write_wave(ASSETS / "sounds" / "player_fall.wav", body_fall(), loudness=0.5)
    write_wave(ASSETS / "sounds" / "door.wav", door_slide(), loudness=0.18)
    write_wave(ASSETS / "sounds" / "enemy_alert.wav", alert_shout(901, 115.0),
               loudness=0.22)
    write_wave(ASSETS / "sounds" / "step_left.wav", footstep(11), loudness=0.12)
    write_wave(ASSETS / "sounds" / "step_right.wav", footstep(12), loudness=0.12)
    write_wave(ASSETS / "sounds" / "ammo_pickup.wav", ammo_pickup(),
               loudness=0.3)
    write_wave(ASSETS / "sounds" / "key_pickup.wav", key_pickup(), loudness=0.3)
    write_wave(ASSETS / "sounds" / "weapon_pickup.wav", gun_cock(),
               loudness=0.35)


if __name__ == "__main__":
    main()
