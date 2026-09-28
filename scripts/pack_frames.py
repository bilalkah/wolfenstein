#!/usr/bin/env python3
"""Stores each animated sprite's frames once. An owner (a weapon, an enemy,
a light) keeps one folder of its distinct frames, <owner>/frames/N.png, in
the order its clips first show them, and textures.json lists which of them
each clip plays: a frame several clips share, or a clip shows twice, is one
file. Images textures.json does not name are deleted.

    ./scripts/pack_frames.py   # rewrites assets/textures.json and the
                               # frames under assets/sprites; rerunning
                               # changes nothing
"""

import hashlib
import json
from pathlib import Path

ASSETS = Path(__file__).resolve().parent.parent / "assets"
MANIFEST = ASSETS / "textures.json"
SPRITES = "sprites/"


def owner_of(path):
    """The folder a clip frame belongs to: sprites/<kind>/<owner>."""
    parts = path.split("/")
    return "/".join(parts[:3]) if path.startswith(SPRITES) and len(parts) > 3 else None


def main():
    manifest = json.loads(MANIFEST.read_text())
    clips = manifest["clips"]
    # Every frame's bytes, read before anything moves
    content = {path: (ASSETS / path).read_bytes()
               for frames in clips.values() for path in frames
               if owner_of(path)}

    packed = {}  # owner -> {digest: packed path}
    for name, frames in clips.items():
        paths = []
        for path in frames:
            owner = owner_of(path)
            if owner is None:
                paths.append(path)
                continue
            known = packed.setdefault(owner, {})
            digest = hashlib.sha256(content[path]).hexdigest()
            if digest not in known:
                known[digest] = f"{owner}/frames/{len(known)}.png"
            paths.append(known[digest])
        clips[name] = paths

    # Every file under the owners' folders goes, then the distinct frames
    # come back under frames/
    for owner in packed:
        for old in (ASSETS / owner).rglob("*.png"):
            old.unlink()
        for folder in sorted((ASSETS / owner).rglob("*"), reverse=True):
            if folder.is_dir() and not any(folder.iterdir()):
                folder.rmdir()
    by_digest = {hashlib.sha256(data).hexdigest(): data
                 for data in content.values()}
    for known in packed.values():
        for digest, path in known.items():
            target = ASSETS / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(by_digest[digest])

    # Images nothing names: gone
    named = {*manifest["textures"].values(), *manifest["walls"],
             *(path for frames in clips.values() for path in frames)}
    for image in [*ASSETS.rglob("*.png"), *ASSETS.rglob("*.jpg")]:
        if image.relative_to(ASSETS).as_posix() not in named:
            image.unlink()
    for folder in sorted(ASSETS.rglob("*"), reverse=True):
        if folder.is_dir() and not any(folder.iterdir()):
            folder.rmdir()

    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n")
    frames = sum(len(known) for known in packed.values())
    print(f"{len(packed)} owners, {frames} distinct frames")


if __name__ == "__main__":
    main()
