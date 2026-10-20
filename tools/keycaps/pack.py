"""Builds the keycap texture pack for the game's button font.

The prompts in the game are characters of the font Warfare_HUD.Xbox360_18pt (a pad button is one
glyph). This tool draws every keycap (keys.py, svgkit.py) into the two 256x256 texture pages of
that font, next to the glyphs the game still needs, and writes

  <out>/<hash>.dds            the two replacement pages (R8G8B8A8, 4x resolution, full mip chain),
                              named by the content hash of the game's page (the port's texture pack)
  src/gow2_keycap_layout.inc  which character shows which key, and the cell of each (a generated
                              C++ table the port patches into the font at run time)
  assets/keycaps/layout.json  the same, for people

Glyphs the game's own strings use, and the spinner on page 1, keep their cells and art (copied
from your texture dump of the game, so the pack holds game art: do not share it). All other
glyph cells, mostly unused placeholders, are reused.

    py tools/keycaps/pack.py --dump out/build/release/textures/4D53082D/dump \\
        --out out/build/release/textures/4D53082D/replace
"""
from __future__ import annotations

import argparse
import glob
import hashlib
import io
import json
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(HERE)), "tools"))

import keys  # noqa: E402
import svgkit  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(HERE))
PAGE = 256
LINE_HEIGHT = 26          # every cell of the retail font is 26 tall
FORBIDDEN = {ord(c) for c in "<>&\\"} | {160, 173}  # markup characters, no-break space, soft hyphen


def load_info() -> dict:
    with open(os.path.join(HERE, "font_info.json"), encoding="utf-8") as f:
        return json.load(f)


def protected_chars(info: dict) -> set[int]:
    prot = {int(c) for c in info["used_in_bundles"]} | {int(c) for c in info["used_in_scripts"]}
    prot |= set(info["art"]["1"])  # page 1 holds the progress spinner; it has no strings to scan
    return prot


def char_pool(info: dict, prot: set[int]) -> list[int]:
    return [c for c in list(range(33, 127)) + list(range(161, 256))
            if c not in prot and c not in FORBIDDEN]


class Occupancy:
    def __init__(self, info: dict, prot: set[int]):
        self.occ = [np.zeros((PAGE, PAGE), dtype=bool) for _ in range(2)]
        for c in prot:
            u, v, us, vs, tex, _ = info["cells"][c]
            if us and vs:
                self.occ[tex][v:v + vs, u:u + us] = True

    def find(self, w: int, h: int):
        for page in (1, 0):  # page 1 is mostly free
            occ = self.occ[page]
            integral = np.pad(occ.cumsum(0).cumsum(1), ((1, 0), (1, 0)))
            n = PAGE - h + 1
            m = PAGE - w + 1
            if n <= 0 or m <= 0:
                continue
            area = (integral[h:h + n, w:w + m] - integral[0:n, w:w + m]
                    - integral[h:h + n, 0:m] + integral[0:n, 0:m])
            ys, xs = np.nonzero(area == 0)
            if len(ys):
                i = np.lexsort((xs, ys))[0]  # topmost, then leftmost
                return page, int(xs[i]), int(ys[i])
        return None

    def take(self, page: int, u: int, v: int, w: int, h: int):
        self.occ[page][v:v + h, u:u + w] = True


def _try_layout(info, prot, order, catalog, cell_h, chars):
    art_h = cell_h - 1  # half a unit of clear space round the art; 1 unit = 4 texels at scale 4
    occ = Occupancy(info, prot)
    placed = []
    for k in order:
        art_w = max(1, round(svgkit.cluster_width(k, catalog) * art_h / 100))
        w, h = art_w + 1, cell_h
        spot = occ.find(w, h)
        if spot is None:
            return None
        page, u, v = spot
        occ.take(page, u, v, w, h)
        placed.append({"id": k.id, "char": chars[k.id], "page": page, "u": u, "v": v, "w": w,
                       "h": h, "vo": (LINE_HEIGHT - h) // 2, "art_w": art_w, "art_h": art_h})
    return placed


def layout(info: dict, entries: list, cell_h: int, attempts: int = 400):
    import random

    prot = protected_chars(info)
    pool = char_pool(info, prot)
    if len(pool) < len(entries):
        raise SystemExit(f"{len(entries)} glyphs but only {len(pool)} free characters")
    catalog = {k.id: k for k in entries}
    # Characters are handed out in catalog order whatever the packing order is.
    chars = {k.id: pool[i] for i, k in enumerate(entries)}
    width = lambda k: svgkit.cluster_width(k, catalog)
    orders = [sorted(entries, key=lambda k: (-width(k), k.id)), sorted(entries, key=lambda k: (width(k), k.id))]
    rng = random.Random(2008)
    for _ in range(attempts):
        o = sorted(entries, key=lambda k: -width(k))
        # Wide caps stay roughly first, but each cap's rank is jittered.
        o.sort(key=lambda k: -width(k) * (0.6 + 0.8 * rng.random()))
        orders.append(o)
    for i, order in enumerate(orders):
        placed = _try_layout(info, prot, order, catalog, cell_h, chars)
        if placed:
            placed.sort(key=lambda p: p["char"])
            used_area = sum(p["w"] * p["h"] for p in placed)
            print(f"{len(placed)} glyphs, cell height {cell_h}: {used_area} of {2 * PAGE * PAGE} px, "
                  f"{len(pool) - len(entries)} spare characters (packing attempt {i + 1})")
            return placed, prot
    raise SystemExit(f"no packing found at cell height {cell_h}; lower --cell-height")


# ---- rendering --------------------------------------------------------------------------------

def raster(key, catalog, px_w: int, px_h: int) -> Image.Image:
    import resvg_py

    png = bytes(resvg_py.svg_to_bytes(svg_string=svgkit.key_svg(key, catalog, px=(px_w, px_h))))
    im = Image.open(io.BytesIO(png)).convert("RGBA")
    return im if im.size == (px_w, px_h) else im.resize((px_w, px_h), Image.LANCZOS)


def _premul_resize(im: Image.Image, size: tuple[int, int]) -> Image.Image:
    a = np.asarray(im.convert("RGBA"), dtype=np.float32) / 255.0
    a[..., :3] *= a[..., 3:4]
    pm = Image.fromarray((a * 255).round().astype(np.uint8), "RGBA")
    out = np.asarray(pm.resize(size, Image.LANCZOS), dtype=np.float32) / 255.0
    alpha = out[..., 3:4]
    rgb = np.where(alpha > 1e-4, out[..., :3] / np.maximum(alpha, 1e-4), 0.0)
    res = np.concatenate([np.clip(rgb, 0, 1), alpha], axis=-1)
    return Image.fromarray((res * 255).round().astype(np.uint8), "RGBA")


def mip_chain(img: Image.Image) -> list[bytes]:
    chain = []
    w, h = img.size
    level = img
    while True:
        chain.append(level.tobytes())
        if w == 1 and h == 1:
            return chain
        w, h = max(w // 2, 1), max(h // 2, 1)
        level = _premul_resize(img, (w, h))


def find_dump_page(dump: str, h: str) -> Image.Image:
    files = glob.glob(os.path.join(dump, h + "_*.dds"))
    if not files:
        raise SystemExit(f"the texture dump has no page {h}: run the game once with texture_dump = true "
                         f"and open the main menu and the difficulty screen (see tools/keycaps/README.md)")
    return Image.open(files[0]).convert("RGBA")


def build_pages(info: dict, placed: list, prot: set[int], dump: str, scale: int, catalog: dict):
    pages = [Image.new("RGBA", (PAGE * scale, PAGE * scale), (0, 0, 0, 0)) for _ in range(2)]
    for p, h in enumerate(info["page_hashes"]):
        src = find_dump_page(dump, h)
        for c in sorted(prot):
            u, v, us, vs, tex, _ = info["cells"][c]
            if tex != p or not us or not vs:
                continue
            u2, v2 = min(u + us, PAGE), min(v + vs, PAGE)
            cell = src.crop((u, v, u2, v2))
            big = _premul_resize(cell, ((u2 - u) * scale, (v2 - v) * scale))
            pages[p].paste(big, (u * scale, v * scale))
    for pl in placed:
        key = catalog[pl["id"]]
        art = raster(key, catalog, pl["art_w"] * scale, pl["art_h"] * scale)
        pages[pl["page"]].alpha_composite(art, (pl["u"] * scale + scale // 2, pl["v"] * scale + scale // 2))
    return pages


def write_dds(path: str, img: Image.Image) -> None:
    import texpack

    texpack.write_dds(path, texpack.DXGI_R8G8B8A8_UNORM, img.width, img.height, mip_chain(img))


# ---- outputs ----------------------------------------------------------------------------------

def layout_id(placed: list, cell_h: int) -> str:
    """Names this exact layout, so the game can tell whether the installed pages match its table."""
    blob = json.dumps([cell_h, placed], sort_keys=True).encode()
    return hashlib.sha1(blob).hexdigest()[:16]


def write_include(info: dict, placed: list, path: str, cell_h: int) -> None:
    lines = [
        "// Generated by tools/keycaps/pack.py. Do not edit.",
        "// Which character of the button font shows which key, and where its cell is.",
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace rex::glue::keycap {",
        "",
        f"constexpr int kCellHeight = {cell_h};",
        f'constexpr const char* kLayoutId = "{layout_id(placed, cell_h)}";  // also in <pack>/keycaps.id',
        f'constexpr const char* kPageHash[2] = {{"{info["page_hashes"][0]}", "{info["page_hashes"][1]}"}};',
        "",
        "// One entry per character that shows a keycap: its cell in the font (StartU, StartV, USize,",
        "// VSize in 256x256 page units, TextureIndex, VerticalOffset).",
        "struct Cell {",
        "  char16_t ch;",
        "  uint8_t page;",
        "  int32_t u, v, w, h, vo;",
        "};",
        "constexpr Cell kCells[] = {",
    ]
    for p in placed:
        lines.append(f"    {{u'\\x{p['char']:04x}', {p['page']}, {p['u']}, {p['v']}, {p['w']}, {p['h']}, {p['vo']}}},"
                     f"  // {p['id']}")
    lines += [
        "};",
        "",
        "// Key name (as written in keybind_* settings) or cluster/icon id -> character.",
        "struct KeyChar {",
        "  const char* id;",
        "  char16_t ch;",
        "};",
        "constexpr KeyChar kKeyChars[] = {",
    ]
    for p in sorted(placed, key=lambda p: p["id"]):
        lines.append(f"    {{\"{p['id']}\", u'\\x{p['char']:04x}'}},")
    import atlas as atlas_tool

    ai = atlas_tool.load_info()
    lines += [
        "};",
        "",
        "// The tutorial pictograms' button sprites (atlas.py): the box of each, and its disc.",
        f'constexpr const char* kAtlasHash = "{ai["hash"]}";',
        "struct AtlasSprite {",
        "  const char* button;  // \"A\", \"B\", \"X\", \"Y\" or \"RS\"",
        "  int box[4];          // x0, y0, x1, y1 (inclusive)",
        "  int disc[4];",
        "};",
        "constexpr AtlasSprite kAtlasSprites[] = {",
    ]
    for sp in ai["sprites"]:
        lines.append(f'    {{"{sp["button"]}", {{{", ".join(map(str, sp["box"]))}}}, {{{", ".join(map(str, sp["disc"]))}}}}},')
    lines += ["};", "", "}  // namespace rex::glue::keycap", ""]
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dump", help="the port's texture dump folder (needed to write the pages)")
    ap.add_argument("--out", help="folder for the replacement DDS files")
    ap.add_argument("--cell-height", type=int, default=22)
    ap.add_argument("--scale", type=int, default=4, help="page resolution as a multiple of 256")
    ap.add_argument("--layout-only", action="store_true", help="only write the include and layout.json")
    ap.add_argument("--png", help="also write the two pages as PNG into this folder")
    args = ap.parse_args()

    info = load_info()
    entries = keys.everything()
    catalog = {k.id: k for k in entries}
    placed, prot = layout(info, entries, args.cell_height)

    write_include(info, placed, os.path.join(ROOT, "src", "gow2_keycap_layout.inc"), args.cell_height)
    os.makedirs(os.path.join(ROOT, "assets", "keycaps"), exist_ok=True)
    with open(os.path.join(ROOT, "assets", "keycaps", "layout.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump({"cell_height": args.cell_height, "page_hashes": info["page_hashes"], "glyphs": placed}, f, indent=1)
        f.write("\n")
    print("wrote src/gow2_keycap_layout.inc and assets/keycaps/layout.json")
    if args.layout_only:
        return 0
    if not args.dump or not args.out:
        raise SystemExit("--dump and --out are needed to write the pages")
    pages = build_pages(info, placed, prot, args.dump, args.scale, catalog)
    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "keycaps.id"), "w", encoding="ascii", newline="\n") as f:
        f.write(layout_id(placed, args.cell_height) + "\n")
    for h, im in zip(info["page_hashes"], pages):
        write_dds(os.path.join(args.out, h + ".dds"), im)
        print("wrote", os.path.join(args.out, h + ".dds"), im.size)
        if args.png:
            os.makedirs(args.png, exist_ok=True)
            im.save(os.path.join(args.png, h + ".png"))
    # The tutorial pictograms have buttons painted in: their keyboard variant has none.
    import atlas

    atlas.build(args.dump, os.path.join(os.path.dirname(os.path.abspath(args.out)), "replace_kbd_base"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
