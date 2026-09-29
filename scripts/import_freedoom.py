#!/usr/bin/env python3
"""Imports the game's art and sounds from Freedoom (freedoom2.wad, from
https://github.com/freedoom/freedoom/releases), which may be used, changed
and sold under its BSD licence, kept in assets/licenses/freedoom:

- the enemies, with walking, shooting and pain seen from all 8 sides;
- the weapons in hand, their flashes drawn onto the firing frames;
- pickups, keys, torches, the HUD digits;
- walls, doors, the exit, the sky, the menu and result screens;
- sounds.

Needs Pillow (pip install pillow).

    ./scripts/import_freedoom.py freedoom2.wad BlackOpsOne-Regular.ttf
    ./scripts/pack_frames.py
    ./scripts/render_music.sh   # the music, from the MIDI it leaves

The font draws the result screens' titles. The script writes the images and
sounds under assets/, the clips into assets/textures.json and the art's sizes
into assets/levels/config.json; pack_frames.py then stores each owner's
frames once. Rerunning it rewrites the same files.
"""

import argparse
import json
import struct
import wave
from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageFont

ASSETS = Path(__file__).resolve().parent.parent / "assets"
MANIFEST = ASSETS / "textures.json"
CONFIG = ASSETS / "levels" / "config.json"

# World units a sprite pixel spans: the zombie soldier, 52 pixels from its
# feet to the top of its head, stands 0.66 tall, as the soldier always has
PIXEL = 0.66 / 52
# Rockets, bolts and their bursts are sized to the walls they burst on
# instead: Doom's are 128 pixels high, ours 1 (its figures stand larger here)
WALL_PIXEL = 1 / 128
# Doom's pixels are 1.2 times as tall as wide (320 x 200 on a 4:3 screen)
TALL = 1.2


class Wad:
    """A Doom-format WAD: its lumps, palette, pictures and wall textures."""

    def __init__(self, path):
        self.data = Path(path).read_bytes()
        magic, count, offset = struct.unpack("<4sII", self.data[:12])
        if magic not in (b"IWAD", b"PWAD"):
            raise SystemExit(f"{path} is not a WAD")
        self.lumps = {}
        self.sprites = []
        in_sprites = False
        for i in range(count):
            pos, size, raw = struct.unpack(
                "<II8s", self.data[offset + 16 * i: offset + 16 * i + 16])
            name = raw.rstrip(b"\0").decode("ascii").upper()
            self.lumps.setdefault(name, (pos, size))
            if name in ("S_START", "SS_START"):
                in_sprites = True
            elif name in ("S_END", "SS_END"):
                in_sprites = False
            elif in_sprites and size > 0:
                self.sprites.append(name)
        palette = self.lump("PLAYPAL")[:768]
        self.palette = [tuple(palette[3 * i: 3 * i + 3]) for i in range(256)]

    def lump(self, name):
        pos, size = self.lumps[name]
        return self.data[pos: pos + size]

    def picture(self, name):
        """A picture, and its offsets: from its left edge and top to the
        point it stands on (sprites), or its place on screen (weapons)."""
        data = self.lump(name)
        width, height, left, top = struct.unpack("<HHhh", data[:8])
        image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        pixels = image.load()
        for x in range(width):
            at = struct.unpack("<I", data[8 + 4 * x: 12 + 4 * x])[0]
            row = -1
            while data[at] != 0xFF:
                delta = data[at]
                # A tall picture's delta, not past the last, is relative
                row = row + delta if delta <= row else delta
                for y in range(data[at + 1]):
                    if 0 <= row + y < height:
                        pixels[x, row + y] = self.palette[data[at + 3 + y]] + (255,)
                at += data[at + 1] + 4
        return image, (left, top)

    def sprite(self, prefix, letter, view):
        """Frame `letter` seen from `view` (1 in front, 5 behind; 0 for a
        frame that looks the same from every side), mirrored if the WAD
        keeps it as the mirror image of another view."""
        for name in self.sprites:
            if not name.startswith(prefix):
                continue
            rest = name[4:]
            mirrored = len(rest) == 4 and rest[2] == letter and rest[3] == str(view)
            if (rest[0] == letter and rest[1] in (str(view), "0")) or mirrored:
                image, (left, top) = self.picture(name)
                if mirrored:
                    image = image.transpose(Image.FLIP_LEFT_RIGHT)
                    left = image.width - left
                return image, (left, top)
        raise SystemExit(f"{prefix}{letter}{view} missing")

    def rotates(self, prefix, letter):
        return not any(n.startswith(prefix + letter + "0") for n in self.sprites)

    def texture(self, name):
        """A wall texture, composed from its patches."""
        pnames = self.lump("PNAMES")
        patches = [pnames[4 + 8 * i: 12 + 8 * i].rstrip(b"\0").decode().upper()
                   for i in range(struct.unpack("<I", pnames[:4])[0])]
        for table in ("TEXTURE1", "TEXTURE2"):
            if table not in self.lumps:
                continue
            data = self.lump(table)
            count = struct.unpack("<I", data[:4])[0]
            for off in struct.unpack(f"<{count}I", data[4: 4 + 4 * count]):
                if data[off: off + 8].rstrip(b"\0").decode().upper() != name:
                    continue
                width, height = struct.unpack("<HH", data[off + 12: off + 16])
                image = Image.new("RGBA", (width, height), (0, 0, 0, 255))
                for p in range(struct.unpack("<H", data[off + 20: off + 22])[0]):
                    x, y, index = struct.unpack(
                        "<hhH", data[off + 22 + 10 * p: off + 28 + 10 * p])
                    patch, _ = self.picture(patches[index])
                    image.paste(patch, (x, y), patch)
                return image
        raise SystemExit(f"texture {name} missing")

    def samples(self, name):
        """A DMX sound lump's samples (8-bit unsigned) and their rate."""
        data = self.lump(name)
        _, rate, count = struct.unpack("<HHI", data[:8])
        # 16 bytes of padding either side of the samples
        return data[8 + 16: 8 + count - 16], rate

    def sound(self, name, path, *later):
        """A DMX sound lump as a 16-bit WAV; with `later`, (lump, seconds)
        pairs, those lumps start that many seconds in, over silence (a
        reload's click and clack, timed to its frames)."""
        parts = [(self.samples(lump), seconds)
                 for lump, seconds in ((name, 0.0), *later)]
        # At the finest of their rates, the others stretched to it
        rate = max(part_rate for (_, part_rate), _ in parts)
        mixed = []
        for (samples, part_rate), seconds in parts:
            start = round(seconds * rate)
            length = len(samples) * rate // part_rate
            mixed += [0] * max(start + length - len(mixed), 0)
            for i in range(length):
                s = samples[i * part_rate // rate] - 128
                mixed[start + i] = max(-128, min(127, mixed[start + i] + s))
        with wave.open(str(path), "wb") as out:
            out.setnchannels(1)
            out.setsampwidth(2)
            out.setframerate(rate)
            out.writeframes(b"".join(struct.pack("<h", s << 8) for s in mixed))


def place(canvas, image, x, y):
    """Draws `image` over `canvas` with its top left at (x, y), cut to the
    canvas where it hangs off an edge."""
    left, top = max(-x, 0), max(-y, 0)
    right = min(image.width, canvas.width - x)
    bottom = min(image.height, canvas.height - y)
    if right > left and bottom > top:
        canvas.alpha_composite(image.crop((left, top, right, bottom)),
                               (x + left, y + top))


# The menu's theme and each level's, from Freedoom's second phase (its
# intermission theme, and its first seven maps' tracks)
MUSIC = {"menu": "D_DM2INT", "level1": "D_RUNNIN", "level2": "D_STALKS",
         "level3": "D_COUNTD", "level4": "D_BETWEE", "level5": "D_DOOM",
         "level6": "D_THE_DA", "level7": "D_SHAWN"}


def save(image, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)
    return path.relative_to(ASSETS).as_posix()


# -- Things standing in the world: enemies, torches, pickups ---------------

def standing(wad, frames, lifts=None):
    """Frames [(image, (left, top))] placed on one canvas, their standing
    points together at its bottom centre, or `lifts[i]` pixels above it (a
    thing floating), so each is drawn in the same world rectangle. Returns
    the canvases and the canvas size in pixels."""
    lifts = lifts or [0] * len(frames)
    half = max(max(left, image.width - left) for image, (left, _) in frames)
    height = max(t + lift for (_, (_, t)), lift in zip(frames, lifts))
    width = 2 * half
    canvases = []
    for (image, (left, t)), lift in zip(frames, lifts):
        canvas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        # Below its standing point a sprite would sink into the floor
        place(canvas, image.crop((0, 0, image.width, t)), half - left,
              height - lift - t)
        canvases.append(canvas)
    return canvases, (width, height)


def enemy(wad, name, prefix, clips, pixel, hover=0):
    """An enemy's clips, {clip: letters}, each seen from 8 sides where its
    frames turn: "<name>_<clip>" from the front, "<name>_<clip>@<view>"
    from the others (2 front-left, round to 8 front-right). A flying one
    floats `hover` pixels up while it lives, and sinks to the floor as it
    dies. Returns the manifest's clips and the enemy's size in the world."""
    views = {clip: range(1, 9) if all(wad.rotates(prefix, l) for l in letters) else [0]
             for clip, letters in clips.items()}
    every = [(clip, view, i, wad.sprite(prefix, letter, view))
             for clip, letters in clips.items() for view in views[clip]
             for i, letter in enumerate(letters)]
    dying = len(clips.get("death", "")) - 1
    canvases, (width, height) = standing(
        wad, [f for *_, f in every],
        [hover * (dying - i) // max(dying, 1) if clip == "death" else hover
         for clip, _, i, _ in every])
    manifest = {}
    for (clip, view, i, _), canvas in zip(every, canvases):
        key = f"{name}_{clip}" + (f"@{view}" if view > 1 else "")
        path = save(canvas, ASSETS / "sprites" / "npc" / name / "import" /
                    f"{clip}_{view}_{i}.png")
        manifest.setdefault(key, []).append(path)
    return manifest, (round(width * pixel, 3), round(height * pixel, 3))


def projectile(wad, name, flight, burst):
    """A projectile's clips, each a sprite's prefix and letters:
    "<name>_flight", seen from 8 sides where its frames turn (a rocket), and
    "<name>_burst". Doom draws a missile about where it is, so each frame
    goes on its canvas with that point at the middle. Returns the manifest's
    clips and the sizes in the world, in flight and bursting."""
    manifest, sizes = {}, []
    for clip, (prefix, letters) in (("flight", flight), ("burst", burst)):
        views = range(1, 9) if all(wad.rotates(prefix, l) for l in letters) else [0]
        frames = [(view, i, wad.sprite(prefix, letter, view))
                  for view in views for i, letter in enumerate(letters)]
        half_w = max(max(left, image.width - left) for *_, (image, (left, _)) in frames)
        half_h = max(max(top, image.height - top) for *_, (image, (_, top)) in frames)
        for view, i, (image, (left, top)) in frames:
            canvas = Image.new("RGBA", (2 * half_w, 2 * half_h), (0, 0, 0, 0))
            place(canvas, image, half_w - left, half_h - top)
            key = f"{name}_{clip}" + (f"@{view}" if view > 1 else "")
            manifest.setdefault(key, []).append(save(
                canvas, ASSETS / "sprites" / "projectile" / name / "import" /
                f"{clip}_{view}_{i}.png"))
        sizes.append((round(2 * half_w * WALL_PIXEL, 3),
                      round(2 * half_h * WALL_PIXEL, 3)))
    return manifest, sizes


def still(wad, prefix, letters, path_for):
    """A thing seen the same from every side (a torch, a pickup): its frames
    on one canvas, and its size in the world."""
    canvases, (width, height) = standing(
        wad, [wad.sprite(prefix, letter, 0) for letter in letters])
    paths = [save(canvas, path_for(i)) for i, canvas in enumerate(canvases)]
    return paths, (round(width * PIXEL, 3), round(height * PIXEL, 3))


# -- The weapon in hand -----------------------------------------------------

def on_screen(parts, lowered=0):
    """Pictures placed as Doom places a weapon on its 320 x 200 screen, the
    gun `lowered` pixels below where it is held ready."""
    screen = Image.new("RGBA", (320, 200), (0, 0, 0, 0))
    for image, (left, top) in parts:
        place(screen, image, 1 - left, 32 - top + lowered)
    return screen


def weapon(wad, name, ready, firing, reload_frames=14, reloading=None):
    """A weapon's clips: `ready` the picture held, `firing` the frames of a
    shot, each a list of picture names (a gun and its flash). Reloading
    plays `reloading`, frames named the same way, for a gun drawn loading
    (the double-barrelled shotgun); else it lowers the gun out of sight and
    raises it again. Switching guns lowers and raises it."""
    def picture(names):
        return on_screen([wad.picture(n) for n in names])

    held = wad.picture(ready)
    shots = [picture(names) for names in firing]
    slide = [on_screen([held], lowered) for lowered in (0, 16, 32, 52, 76, 96)]
    down = reload_frames // 2 - 2
    reload = ([picture(names) for names in reloading] if reloading else
              [on_screen([held], 60 * i // down) for i in range(down)] +
              [on_screen([held], 60)] * 4 +
              [on_screen([held], 60 - 60 * i // down) for i in range(down)])
    clips = {"loaded": [on_screen([held])] + shots, "outofammo": [on_screen([held])],
             "reload": reload, "raise": slide[::-1], "lower": slide}
    # Every frame on one canvas: from the highest picture down to the bottom
    # of the screen, and as tall as Doom's pixels are
    top = min(frame.getbbox()[1] for frames in clips.values()
              for frame in frames if frame.getbbox())
    manifest = {}
    for clip, frames in clips.items():
        for i, frame in enumerate(frames):
            cut = frame.crop((0, top, 320, 200))
            cut = cut.resize((320, round((200 - top) * TALL)), Image.NEAREST)
            path = save(cut, ASSETS / "sprites" / "weapon" / name / "import" /
                        f"{clip}_{i}.png")
            manifest.setdefault(f"{name}_{clip}", []).append(path)
    return manifest


# -- Walls and screens --------------------------------------------------------

def silver(image):
    """Blue turned to silver: the silver key and its door."""
    grey = ImageEnhance.Color(image).enhance(0.0)
    return ImageEnhance.Brightness(grey).enhance(1.25)


def wall(image):
    """A wall texture as a square, as a map cell's face is."""
    side = max(image.size)
    return image.resize((side, side), Image.NEAREST)


def screen(wad, background, title, font_path, colour, darken):
    """A full screen: Freedoom's picture, darkened, a title across it."""
    picture, _ = wad.picture(background)
    image = picture.convert("RGB").resize((1200, 900), Image.NEAREST)
    image = ImageEnhance.Brightness(image).enhance(darken)
    if title:
        draw = ImageDraw.Draw(image)
        font = ImageFont.truetype(str(font_path), 150)
        box = draw.textbbox((0, 0), title, font=font)
        at = ((1200 - (box[2] - box[0])) // 2 - box[0], (900 - (box[3] - box[1])) // 2 - box[1])
        shadow = Image.new("RGBA", image.size, (0, 0, 0, 0))
        ImageDraw.Draw(shadow).text((at[0] + 8, at[1] + 10), title, font=font, fill=(0, 0, 0, 220))
        image = Image.alpha_composite(image.convert("RGBA"), shadow.filter(ImageFilter.GaussianBlur(6)))
        ImageDraw.Draw(image).text(at, title, font=font, fill=colour)
    return image.convert("RGB")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("wad", help="freedoom2.wad")
    parser.add_argument("font", help="the display font, for the result screens")
    parser.add_argument("--midi", default="build/music",
                        help="where the music goes as MIDI, for "
                             "scripts/render_music.sh to render")
    args = parser.parse_args()
    wad = Wad(args.wad)
    manifest = json.loads(MANIFEST.read_text())
    config = json.loads(CONFIG.read_text())
    clips = manifest["clips"]
    for key in [k for k in clips if k.split("_")[0] in ("soldier", "caco", "cyber", "pistol",
                                                         "mp5", "shotgun", "minigun", "demon",
                                                         "green", "red", "super",
                                                         "chainsaw", "rocket", "plasma")]:
        del clips[key]

    # Enemies: the zombie soldier, and stand-ins for the caco and cyber demons
    enemies = config["config_enemy"]
    soldier, size = enemy(wad, "soldier", "POSS", {
        "idle": "A", "walk": "ABCD", "attack": "EFE", "pain": "G",
        "death": "HIJKL"}, PIXEL)
    clips.update(soldier)
    enemies["soldier"].update(width=size[0], height=size[1], radius=0.165)
    # The flying one is larger than Doom made it, to fill the doorways it
    # did, and floats a little off the floor
    caco, size = enemy(wad, "caco_demon", "HEAD", {
        "idle": "A", "walk": "A", "attack": "BCD", "pain": "EF",
        "death": "GHIJKL"}, 0.62 / 61, hover=18)
    clips.update(caco)
    enemies["caco_demon"].update(width=size[0], height=size[1], radius=0.2)
    cyber, size = enemy(wad, "cyber_demon", "CYBR", {
        "idle": "A", "walk": "ABCD", "attack": "EFEF", "pain": "G",
        "death": "HIJKLMNOP"}, 1.0 / 101)
    clips.update(cyber)
    enemies["cyber_demon"].update(width=size[0], height=size[1], radius=0.25)

    # The tougher zombies and the demon, as the soldier is scaled
    for name, prefix, frames, radius in (
            ("shotgun_zombie", "SPOS", {"idle": "A", "walk": "ABCD", "attack": "EFE",
                                        "pain": "G", "death": "HIJKL"}, 0.17),
            ("minigun_zombie", "CPOS", {"idle": "A", "walk": "ABCD", "attack": "EF",
                                        "pain": "G", "death": "HIJKLMN"}, 0.22),
            ("demon", "SARG", {"idle": "A", "walk": "ABCD", "attack": "EFG",
                               "pain": "H", "death": "IJKLMN"}, 0.25)):
        made, size = enemy(wad, name, prefix, frames, PIXEL)
        clips.update(made)
        enemies[name].update(width=size[0], height=size[1], radius=radius)

    # Weapons in hand: the pistol, the minigun (the MP5's place) and shotgun
    clips.update(weapon(wad, "pistol", "PISGA0",
                        [["PISGB0", "PISFA0"], ["PISGC0"], ["PISGB0"]], 12))
    clips.update(weapon(wad, "mp5", "CHGGA0",
                        [["CHGGA0", "CHGFA0"], ["CHGGB0", "CHGFB0"], ["CHGGB0"]], 16))
    clips.update(weapon(wad, "shotgun", "SHTGA0",
                        [["SHTGA0", "SHTFA0"], ["SHTGA0", "SHTFB0"], ["SHTGB0"],
                         ["SHTGC0"], ["SHTGD0"], ["SHTGC0"], ["SHTGB0"]], 20))
    # Found later: the double-barrelled shotgun, broken open and loaded as
    # it reloads, and the saw
    clips.update(weapon(wad, "super_shotgun", "SHT2A0",
                        [["SHT2A0", "SHT2I0"], ["SHT2A0", "SHT2J0"], ["SHT2A0"]],
                        reloading=[[f"SHT2{c}0"] for c in "BCDEFGH"]))
    clips.update(weapon(wad, "chainsaw", "SAWGA0",
                        [["SAWGC0"], ["SAWGD0"], ["SAWGC0"], ["SAWGD0"]]))
    # And the two whose shots fly: the rocket launcher and the plasma rifle,
    # with their rockets and bolts, and the bursts of them
    clips.update(weapon(wad, "rocket_launcher", "MISGA0",
                        [["MISGB0", "MISFA0"], ["MISGB0", "MISFB0"], ["MISGB0", "MISFC0"],
                         ["MISGB0", "MISFD0"], ["MISGB0"]]))
    clips.update(weapon(wad, "plasma_rifle", "PLSGA0",
                        [["PLSGA0", "PLSFA0"], ["PLSGA0", "PLSFB0"]]))
    for name, flight, burst in (("rocket", ("MISL", "A"), ("MISL", "BCD")),
                                ("plasma", ("PLSS", "AB"), ("PLSE", "ABCDE"))):
        made, sizes = projectile(wad, name, flight, burst)
        clips.update(made)
        for w in config["weapons"]:
            if w.get("projectile", {}).get("name") == name:
                w["projectile"].update(width=sizes[0][0], height=sizes[0][1],
                                       burst_width=sizes[1][0], burst_height=sizes[1][1])
    for w in config["weapons"]:
        if w["name"] == "mp5":
            w["label"] = "MINIGUN"

    # Torches, the green 0.9 tall: Doom's walls are higher than ours, and a
    # torch as tall as Doom made it would reach over the wall behind it. The
    # two share a canvas, as they share a size in the config.
    torches = (("green_light", "TGRN"), ("red_light", "TRED"))
    canvases, (width, height) = standing(
        wad, [wad.sprite(prefix, letter, 0) for _, prefix in torches for letter in "ABCD"])
    for t, (name, _) in enumerate(torches):
        clips[name] = [save(canvases[4 * t + i], ASSETS / "sprites" / "animated_sprites" /
                            name / "import" / f"{i}.png") for i in range(4)]
    pixel = 0.9 / height
    config["config_dynamic"]["light"].update(width=round(width * pixel, 3),
                                             height=round(height * pixel, 3))

    # Pickups, keys and the weapons lying on the floor
    textures = dict(manifest["textures"])
    pickups = config["pickups"]
    for texture, prefix, recolour, entries in (
            ("medkit", "STIM", None, ["medkit"]),
            ("large_medkit", "MEDI", None, ["large_medkit"]),
            ("ammo_box", "AMMO", None, ["ammo_box"]),
            ("clip", "CLIP", None, ["clip"]),
            ("gold_key", "YKEY", None, ["gold_key"]),
            ("silver_key", "BKEY", silver, ["silver_key"]),
            ("mp5_pickup", "MGUN", None, ["mp5"]),
            ("shotgun_pickup", "SHOT", None, ["shotgun"]),
            ("super_shotgun_pickup", "SGN2", None, ["super_shotgun"]),
            ("chainsaw_pickup", "CSAW", None, ["chainsaw"]),
            ("rocket_launcher_pickup", "LAUN", None, ["rocket_launcher"]),
            ("plasma_rifle_pickup", "PLAS", None, ["plasma_rifle"])):
        [path], size = still(wad, prefix, "A", lambda i, t=texture:
                             ASSETS / "sprites" / "pickups" / f"{t}.png")
        if recolour:
            image = Image.open(ASSETS / path)
            recolour(image).save(ASSETS / path)
        textures[texture] = path
        for entry in entries:
            if entry in pickups:
                pickups[entry].update(texture=texture, width=size[0], height=size[1])
    # A page of intel pinned to a wall: the computer map, a tablet of green
    # lines, cut to its edges (it hangs on the wall, not stands on the floor)
    tablet, _ = wad.sprite("PMAP", "A", 0)
    textures["intel"] = save(tablet.crop(tablet.getbbox()),
                             ASSETS / "textures" / "intel.png")

    # Walls, doors and the exit: Freedoom's stand-ins for Wolfenstein's
    for i, name in enumerate(("ZZWOLF1", "ZZWOLF11", "ZZWOLF9", "STONGARG", "ZZWOLF12"), 1):
        save(wall(wad.texture(name)), ASSETS / "textures" / f"{i}.png")
    save(wall(wad.texture("ZDOORF1")), ASSETS / "textures" / "door.png")
    save(wall(wad.texture("M_YDOOR")), ASSETS / "textures" / "door_gold.png")
    save(silver(wall(wad.texture("M_BDOOR"))), ASSETS / "textures" / "door_silver.png")
    save(wall(wad.texture("PNK4EXIT")), ASSETS / "textures" / "exit.png")
    save(wad.texture("SKY1").convert("RGB"), ASSETS / "textures" / "sky.png")

    # HUD digits, 0 to 9 and the percent sign
    for i, name in enumerate([f"STTNUM{d}" for d in range(10)] + ["STTPRCNT"]):
        image, _ = wad.picture(name)
        save(image, ASSETS / "textures" / "digits" / f"{i}.png")

    # The menu, and the screens a game ends on
    font = Path(args.font)
    screen(wad, "BOSSBACK", None, font, None, 0.8).save(
        ASSETS / "textures" / "menu_background.png")
    screen(wad, "BOSSBACK", "GAME OVER", font, (200, 30, 20), 0.45).save(
        ASSETS / "textures" / "game_over.png")
    screen(wad, "INTERPIC", "VICTORY", font, (240, 200, 90), 0.55).save(
        ASSETS / "textures" / "win.png")

    # Sounds
    sounds = ASSETS / "sounds"
    for lump, files in (("DSPISTOL", ["pistol", "mp5", "npc_attack"]),
                        ("DSSHOTGN", ["shotgun"]), ("DSPOPAIN", ["npc_pain"]),
                        ("DSPODTH1", ["npc_death"]), ("DSPLPAIN", ["player_pain"]),
                        ("DSPOSIT1", ["enemy_alert"]), ("DSDOROPN", ["door"]),
                        ("DSITEMUP", ["pickup", "ammo_pickup", "key_pickup"]),
                        ("DSWPNUP", ["weapon_pickup"]),
                        ("DSSGTATK", ["demon_attack"]), ("DSSGTSIT", ["demon_alert"]),
                        ("DSDMPAIN", ["demon_pain"]), ("DSSGTDTH", ["demon_death"]),
                        ("DSCACSIT", ["caco_alert"]), ("DSCACDTH", ["caco_death"]),
                        ("DSCYBSIT", ["cyber_alert"]), ("DSCYBDTH", ["cyber_death"]),
                        ("DSPOSIT2", ["zombie_alert"]), ("DSPODTH2", ["zombie_death"]),
                        ("DSDSHTGN", ["super_shotgun"]), ("DSSAWUP", ["saw_up"]),
                        ("DSSAWFUL", ["saw"]), ("DSSAWHIT", ["saw_hit"]),
                        ("DSRLAUNC", ["rocket_launch"]), ("DSBAREXP", ["rocket_burst"]),
                        ("DSPLASMA", ["plasma"]), ("DSFIRXPL", ["plasma_burst"])):
        for file in files:
            wad.sound(lump, sounds / f"{file}.wav")
    # Broken open, the shells in, snapped shut: as its reload's frames show
    wad.sound("DSDBOPN", sounds / "super_shotgun_reload.wav",
              ("DSDBLOAD", 0.45), ("DSDBCLS", 1.0))

    # Music, as MIDI: rendering it to audio takes a synthesizer and a
    # soundfont (scripts/render_music.sh)
    midi = Path(args.midi)
    midi.mkdir(parents=True, exist_ok=True)
    for name, lump in MUSIC.items():
        track = wad.lump(lump)
        if track[:4] != b"MThd":
            raise SystemExit(f"{lump} is not a MIDI file")
        (midi / f"{name}.mid").write_bytes(track)

    manifest["textures"] = dict(sorted(textures.items()))
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n")
    CONFIG.write_text(json.dumps(config, indent=4) + "\n")
    print("imported; now run ./scripts/pack_frames.py and "
          "./scripts/render_music.sh")


if __name__ == "__main__":
    main()
