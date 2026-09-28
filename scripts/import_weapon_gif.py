#!/usr/bin/env python3
"""Cuts an animated GIF of a weapon in the player's hand into the game's
clips: the frames each clip plays, as PNGs, with the GIF's background colour
(its top-left pixel) made transparent, and textures.json pointed at them.

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
    for clip in ("loaded", "outofammo", "reload"):
        folder = ASSETS / "sprites" / "weapon" / args.weapon / clip
        folder.mkdir(parents=True, exist_ok=True)
        for old in folder.glob("*.png"):
            old.unlink()
        paths = []
        for index, frame in enumerate(frames_of(getattr(args, clip))):
            image = images[frame].copy()
            pixels = image.load()
            for y in range(image.height):
                for x in range(image.width):
                    if pixels[x, y] == background:
                        pixels[x, y] = (0, 0, 0, 0)
            image.save(folder / f"{index}.png", optimize=True)
            paths.append(f"sprites/weapon/{args.weapon}/{clip}/{index}.png")
        manifest["clips"][f"{args.weapon}_{clip}"] = paths
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
