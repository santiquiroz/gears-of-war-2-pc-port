"""Reads the retail game's button font (Warfare_HUD.Xbox360_18pt) and writes font_info.json:

  cells        the 256 FFontCharacter records [StartU, StartV, USize, VSize, TextureIndex, VOffset]
  pages        the two texture pages' content hashes (the port's texture dump names) and size
  used         glyph characters that strings of the game put between <Fonts:Warfare_HUD.Xbox360_18pt>
               tags (Coalesced bundles of every locale, script packages)
  art          per page, the characters whose cell holds a drawing (not just a placeholder dot)

Only facts about the font are written (numbers and character codes), no game art. Inputs, all
from your own copy of the game:

  --upk       Startup_int.upk, made by tools/ue3/ue3x360_decompress from Startup_int.xxx
  --game      the game folder (GearGame\\CookedXenon\\Coalesced_*.bin)
  --pkgs      folder with the other decompressed .upk files (GearGame.upk ...), optional
  --dump      the port's texture dump folder (textures\\4D53082D\\dump) with both font pages

    py tools/keycaps/analyze_font.py --upk out/ue3/pkg/Startup_int.upk --game <folder> \\
        --pkgs out/ue3/pkg --dump out/build/release/textures/4D53082D/dump
"""
from __future__ import annotations

import argparse
import glob
import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "tools", "ue3"))

TAG = "<Fonts:Warfare_HUD.Xbox360_18pt>"
FONT_PATH = "Warfare_HUD.Xbox360_18pt"


def font_cells(upk_path: str):
    from upk import Package

    pk = Package(upk_path)
    exp = [e for e in pk.exports if pk.path(e["index"] + 1) == FONT_PATH][0]
    blob = pk.blob(exp)
    # Tagged properties: Characters (array of 21-byte records), Textures (two object refs).
    off = blob.find(struct.pack(">i", 256), 8)
    p = off + 4
    cells = []
    for _ in range(256):
        u, v, us, vs = struct.unpack_from(">4i", blob, p)
        cells.append([u, v, us, vs, blob[p + 16], struct.unpack_from(">i", blob, p + 17)[0]])
        p += 21
    return cells


def parse_bundle(path: str):
    d = open(path, "rb").read()
    p = 0

    def i32():
        nonlocal p
        v = struct.unpack_from(">i", d, p)[0]
        p += 4
        return v

    def fstr():
        nonlocal p
        n = i32()
        if n == 0:
            return ""
        if n < 0:
            n = -n
            s = d[p:p + 2 * (n - 1)].decode("utf-16-le", "replace")
            p += 2 * n
            return s
        s = d[p:p + n - 1].decode("latin-1")
        p += n
        return s

    for _ in range(i32()):
        fn = fstr()
        for _ in range(i32()):
            sec = fstr()
            for _ in range(i32()):
                yield fn, sec, fstr(), fstr()


def used_in_bundles(game: str):
    used: dict[int, set] = {}
    for f in sorted(glob.glob(os.path.join(game, "GearGame", "CookedXenon", "Coalesced_*.bin"))):
        for _fn, sec, key, val in parse_bundle(f):
            for m in re.finditer(re.escape(TAG) + r"(.*?)<Fonts:/>", val, re.S):
                for ch in m.group(1):
                    used.setdefault(ord(ch), set()).add(f"{sec}/{key}")
    return {c: sorted(v)[:3] for c, v in used.items()}


def used_in_packages(pkgs: str):
    used: dict[int, set] = {}
    tag16, tag8 = TAG.encode("utf-16-be"), TAG.encode("latin-1")
    for f in sorted(glob.glob(os.path.join(pkgs, "*.upk"))):
        d = open(f, "rb").read()
        for tag, close, step, dec in ((tag16, "<Fonts:/>".encode("utf-16-be"), 2, "utf-16-be"),
                                      (tag8, b"<Fonts:/>", 1, "latin-1")):
            i = 0
            while (i := d.find(tag, i)) >= 0:
                j = d.find(close, i + len(tag), i + len(tag) + 40 * step)
                if j > 0:
                    for ch in d[i + len(tag):j].decode(dec, "replace"):
                        used.setdefault(ord(ch), set()).add(os.path.basename(f))
                i += len(tag)
    return {c: sorted(v) for c, v in used.items()}


def art_chars(cells, dump: str, hashes):
    from PIL import Image

    art = {0: [], 1: []}
    for page, h in enumerate(hashes):
        files = glob.glob(os.path.join(dump, h + "_*.dds"))
        if not files:
            raise SystemExit(f"page {page}: no dump file {h}_*.dds in {dump}")
        alpha = Image.open(files[0]).convert("RGBA").getchannel("A")
        for ch, (u, v, us, vs, tex, _vo) in enumerate(cells):
            if tex != page or not us or not vs:
                continue
            bb = alpha.crop((u, v, min(256, u + us), min(256, v + vs))).point(lambda x: 255 if x > 8 else 0).getbbox()
            if bb and not (bb[2] - bb[0] <= 9 and bb[3] - bb[1] <= 9):  # a plain dot is a placeholder
                art[page].append(ch)
    return art


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--upk", required=True)
    ap.add_argument("--game", required=True)
    ap.add_argument("--pkgs")
    ap.add_argument("--dump", required=True)
    ap.add_argument("--out", default=os.path.join(HERE, "font_info.json"))
    ap.add_argument("--hashes", default="86bf4f7c0ec59721,fcd9499c23c5ebe5")
    args = ap.parse_args()
    cells = font_cells(args.upk)
    hashes = args.hashes.split(",")
    used = used_in_bundles(args.game)
    scripts = used_in_packages(args.pkgs) if args.pkgs else {}
    info = {
        "font": FONT_PATH,
        "page_hashes": hashes,
        "page_size": 256,
        "cells": cells,
        "used_in_bundles": {str(c): v for c, v in sorted(used.items())},
        "used_in_scripts": {str(c): v for c, v in sorted(scripts.items())},
        "art": {str(p): c for p, c in art_chars(cells, args.dump, hashes).items()},
    }
    with open(args.out, "w", encoding="utf-8", newline="\n") as f:
        json.dump(info, f, indent=1)
        f.write("\n")
    print(f"wrote {args.out}: {sum(1 for c in cells if c[2] and c[3])} cells, "
          f"{len(used)} chars used by bundles, {len(scripts)} by scripts, "
          f"{len(info['art']['0'])}+{len(info['art']['1'])} art cells")
    return 0


if __name__ == "__main__":
    sys.exit(main())
