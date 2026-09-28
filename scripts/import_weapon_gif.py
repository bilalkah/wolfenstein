#!/usr/bin/env python3
"""Cuts an animated GIF of a weapon in the player's hand into the game's
clips: the frames the clips play, each stored once as a PNG with the GIF's
background colour (its top-left pixel) made transparent, and textures.json
listing which of them each clip plays.

Needs Pillow (pip install pillow).

    ./scripts/import_weapon_gif.py art/pistol.gif pistol \\
        --loaded 0-4 --outofammo 0 --reload 6-18

A clip's frames are given as ranges and single frames: 6-18 or 0,2,4.
"""

import argparse
import json
from pathlib import Path

from PIL import Image, ImageSequence

ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / "assets"


def frames_of(spec):
    frames = []
    for part in spec.split(","):
        first, _, last = part.partition("-")
        frames += range(int(first), int(last or first) + 1)
    return frames


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("gif", type=Path)
    parser.add_argument("weapon", help="its name in config.json")
    for clip in ("loaded", "outofammo", "reload"):
        parser.add_argument(f"--{clip}", required=True, metavar="FRAMES")
    args = parser.parse_args()

    source = Image.open(args.gif)
    images = [frame.convert("RGBA") for frame in ImageSequence.Iterator(source)]
    background = images[0].getpixel((0, 0))
    manifest_path = ASSETS / "textures.json"
    manifest = json.loads(manifest_path.read_text())
    # Each GIF frame a clip plays is stored once, in the order the clips
    # first show them (see scripts/pack_frames.py)
    folder = ASSETS / "sprites" / "weapon" / args.weapon
    for old in folder.rglob("*.png"):
        old.unlink()
    (folder / "frames").mkdir(parents=True, exist_ok=True)
    stored = {}  # a frame's pixels -> its file: frames alike are one file
    for clip in ("loaded", "outofammo", "reload"):
        paths = []
        for frame in frames_of(getattr(args, clip)):
            image = images[frame].copy()
            pixels = image.load()
            for y in range(image.height):
                for x in range(image.width):
                    if pixels[x, y] == background:
                        pixels[x, y] = (0, 0, 0, 0)
            key = image.tobytes()
            if key not in stored:
                name = f"sprites/weapon/{args.weapon}/frames/{len(stored)}.png"
                image.save(ASSETS / name, optimize=True)
                stored[key] = name
            paths.append(stored[key])
        manifest["clips"][f"{args.weapon}_{clip}"] = paths
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
