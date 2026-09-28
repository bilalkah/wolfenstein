#!/usr/bin/env python3
"""Draws what Freedoom (scripts/import_freedoom.py) does not give the game:
the crosshair, the red of a hit closing in, the mark on secret walls, the
puffs where shots land and the marks they leave; and synthesises the dry
click of an empty gun, the thud of the player falling dead and footsteps.

Pixel art written as PNGs (on a transparent background) and sounds as WAVs;
standard library only, so rerunning gives the same files.

    ./scripts/make_art.py   # writes assets/textures/crosshair.png,
                            # damage_taken.png, secret_mark.png,
                            # bullet_mark.png, the puffs
                            # (assets/sprites/effects/*) and
                            # assets/sounds/{dry_fire,player_fall,step_*}.wav
"""

import math
import random
import struct
import wave
import zlib
from pathlib import Path

ASSETS = Path(__file__).resolve().parent.parent / "assets"

CLEAR = (0, 0, 0, 0)


class Canvas:
    def __init__(self, width, height):
        self.width, self.height = width, height
        self.pixels = [[CLEAR] * width for _ in range(height)]

    def rect(self, x0, y0, x1, y1, colour):
        """Fills columns x0..x1 and rows y0..y1, inclusive."""
        for y in range(max(y0, 0), min(y1, self.height - 1) + 1):
            for x in range(max(x0, 0), min(x1, self.width - 1) + 1):
                self.pixels[y][x] = colour

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


def crosshair():
    """A thin cross with a gap at its middle, outlined dark so it shows
    against bright walls as well as dark ones."""
    canvas = Canvas(32, 32)
    shade = (16, 16, 16, 200)
    white = (236, 236, 236, 235)
    for x0, y0, x1, y1 in ((3, 14, 12, 17), (19, 14, 28, 17),
                           (14, 3, 17, 12), (14, 19, 17, 28)):
        canvas.rect(x0, y0, x1, y1, shade)
        canvas.rect(x0 + 1, y0 + 1, x1 - 1, y1 - 1, white)
    return canvas


def damage_overlay():
    """Red closing in from the screen's edges, over the view after a hit:
    clear in the middle, deepening towards the rim."""
    width, height = 160, 120
    canvas = Canvas(width, height)
    for y in range(height):
        for x in range(width):
            dx = (x + 0.5) / width * 2 - 1
            dy = (y + 0.5) / height * 2 - 1
            edge = max(0.0, (math.hypot(dx, dy) - 0.55) / 0.85)
            alpha = min(1.0, edge) ** 1.4
            canvas.pixels[y][x] = (150, 8, 6, round(230 * alpha))
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
    crosshair().save(ASSETS / "textures" / "crosshair.png")
    damage_overlay().save(ASSETS / "textures" / "damage_taken.png")
    secret_mark().save(ASSETS / "textures" / "secret_mark.png")
    for frame in range(4):
        blood_puff(frame).save(ASSETS / "sprites" / "effects" / "blood" /
                               "frames" / f"{frame}.png")
        dust_puff(frame).save(ASSETS / "sprites" / "effects" / "dust" /
                              "frames" / f"{frame}.png")
    bullet_mark().save(ASSETS / "textures" / "bullet_mark.png")
    write_wave(ASSETS / "sounds" / "dry_fire.wav", dry_click(), loudness=0.1)
    write_wave(ASSETS / "sounds" / "player_fall.wav", body_fall(), loudness=0.5)
    write_wave(ASSETS / "sounds" / "step_left.wav", footstep(11), loudness=0.12)
    write_wave(ASSETS / "sounds" / "step_right.wav", footstep(12), loudness=0.12)


if __name__ == "__main__":
    main()
