"""The tutorial pictogram atlas has Xbox buttons painted into it (Y, A, B, X, RS circles with their
press-animation dots). This makes the keyboard variant of that texture: the same picture without
those eight sprites, so that with a keyboard only the hint text next to it (which shows your keys)
names a button. pack.py puts the result in <pack>\\4D53082D\\replace_kbd; the game turns it on while
no controller is connected (texture_replace_variant, SDK patch 0023).

The atlas is found by its content hash, so it needs your texture dump of the game. It is the
512x512 texture the tutorial panels draw (the move pictograms, an eye, ladders and the buttons).
"""
from __future__ import annotations

import glob
import json
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE)))
import texpack  # noqa: E402

INFO = os.path.join(HERE, "atlas_info.json")


def load_info() -> dict:
    with open(INFO, encoding="utf-8") as f:
        return json.load(f)


def _components(alpha: np.ndarray):
    """8-connected components of the visible pixels: (label image, bounding boxes)."""
    h, w = alpha.shape
    lab = np.zeros((h, w), dtype=np.int32)
    boxes = {}
    n = 0
    for y in range(h):
        for x in range(w):
            if not alpha[y, x] or lab[y, x]:
                continue
            n += 1
            lab[y, x] = n
            stack = [(y, x)]
            x0 = x1 = x
            y0 = y1 = y
            while stack:
                cy, cx = stack.pop()
                x0, x1, y0, y1 = min(x0, cx), max(x1, cx), min(y0, cy), max(y1, cy)
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        ny, nx = cy + dy, cx + dx
                        if 0 <= ny < h and 0 <= nx < w and alpha[ny, nx] and not lab[ny, nx]:
                            lab[ny, nx] = n
                            stack.append((ny, nx))
            boxes[n] = (x0, y0, x1, y1)
    return lab, boxes


def build(dump: str, out_dir: str) -> bool:
    """Writes <out_dir>/<hash>.dds; False (with a message) when the dump lacks the atlas."""
    info = load_info()
    h = info["hash"]
    files = glob.glob(os.path.join(dump, h + "_*.dds"))
    if not files:
        print(f"atlas: no dump of {h} (the tutorial pictograms): the keyboard variant is skipped")
        return False
    size = info["size"]
    img = Image.open(files[0]).convert("RGBA")
    if img.size != (size, size):
        print(f"atlas: unexpected size {img.size}: skipped")
        return False
    px = np.array(img)
    lab, boxes = _components(px[..., 3] > 24)
    erased = 0
    for sprite in info["sprites"]:
        want = sprite["box"]  # [x0, y0, x1, y1] as found in the retail art
        hits = [n for n, b in boxes.items() if tuple(b) == tuple(want)]
        if len(hits) != 1:
            print(f"atlas: sprite {want} not found (the art differs from the retail texture): skipped")
            return False
        x0, y0, x1, y1 = want
        # The sprite, plus the faint anti-aliasing halo round it that belongs to no other sprite.
        window = np.zeros(lab.shape, dtype=bool)
        window[max(y0 - 3, 0):y1 + 4, max(x0 - 3, 0):x1 + 4] = True
        px[(lab == hits[0]) | (window & (lab == 0))] = (0, 0, 0, 0)
        erased += 1
    os.makedirs(out_dir, exist_ok=True)
    out = Image.fromarray(px, "RGBA")
    texpack.write_dds(os.path.join(out_dir, h + ".dds"), texpack.DXGI_R8G8B8A8_UNORM, size, size,
                      texpack.rgba_mip_chain(out))
    print(f"atlas: wrote {os.path.join(out_dir, h + '.dds')} ({erased} button sprites erased)")
    return True


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: atlas.py <texture dump folder> <replace_kbd folder>")
    sys.exit(0 if build(sys.argv[1], sys.argv[2]) else 1)
