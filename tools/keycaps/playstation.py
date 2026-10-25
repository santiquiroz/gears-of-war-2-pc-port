"""PlayStation button variants of the keycap pack.

The game draws pad buttons as characters of its button font (and paints five of them into the
tutorial pictogram atlas). This writes the same pages with PlayStation art in those cells:

  <pack>/replace_ps4/<hash>.dds   font pages and atlas with PS4 buttons (Share, Options)
  <pack>/replace_ps5/<hash>.dds   the same with PS5 buttons (Create, Options)

Only the pages that hold a swapped character are written; the rest fall back to replace/. The
game turns a variant on while a PlayStation controller is the device in use. The button art is
Kenney's Input Prompts (CC0), in assets/glyphs/playstation.
"""
from __future__ import annotations

import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.dirname(HERE))

import pack  # noqa: E402

COMMON = {
    65: "playstation_button_color_cross",     # A
    66: "playstation_button_color_circle",    # B
    88: "playstation_button_color_square",    # X
    89: "playstation_button_color_triangle",  # Y
    67: "playstation_stick_r",                # right stick
    76: "playstation_stick_l",                # left stick
    77: "playstation_stick_l",                # left stick, second cell
    93: "playstation_trigger_l2",             # LT
    119: "playstation_trigger_l2",            # LT, second set
    94: "playstation_trigger_r2",             # RT
    120: "playstation_trigger_r2",            # RT, second set
    95: "playstation_trigger_l1",             # LB
    121: "playstation_trigger_l1",            # LB, second set
    96: "playstation_trigger_r1",             # RB
    122: "playstation_trigger_r1",            # RB, second set
    104: "playstation_button_r3",             # right stick click
    105: "playstation_button_r3",             # right stick click, stick drawn
    106: "playstation_button_l3",             # left stick click
    107: "playstation_button_l3",             # left stick click, stick drawn
}
# Back and Start: 60 and 62, and the second pair 58 and 59.
PS4 = {**COMMON, 58: "playstation4_button_share", 60: "playstation4_button_share",
       59: "playstation4_button_options", 62: "playstation4_button_options"}
PS5 = {**COMMON, 58: "playstation5_button_create", 60: "playstation5_button_create",
       59: "playstation5_button_options", 62: "playstation5_button_options"}
VARIANTS = {"ps4": PS4, "ps5": PS5}

# Button sprites of the tutorial pictogram atlas (atlas_info.json "sprites").
ATLAS = {
    "A": "playstation_button_color_cross",
    "B": "playstation_button_color_circle",
    "X": "playstation_button_color_square",
    "Y": "playstation_button_color_triangle",
    "RS": "playstation_stick_r",
}


def glyph_dir() -> str:
    if getattr(sys, "frozen", False):
        return os.path.join(sys._MEIPASS, "glyphs", "playstation")
    return os.path.join(os.path.dirname(os.path.dirname(HERE)), "assets", "glyphs", "playstation")


def load_glyph(name: str) -> Image.Image:
    """The PNG cropped to its visible pixels."""
    im = Image.open(os.path.join(glyph_dir(), name + ".png")).convert("RGBA")
    box = im.getchannel("A").getbbox()
    return im.crop(box) if box else im


def _alpha_bbox(im: Image.Image, threshold: int):
    """(x0, y0, x1, y1), exclusive, of the pixels with alpha > threshold; None when there are none."""
    ys, xs = np.nonzero(np.asarray(im.convert("RGBA"))[..., 3] > threshold)
    if not len(ys):
        return None
    return int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1


def _fit(dst: Image.Image, glyph: Image.Image, box: tuple[int, int, int, int]) -> None:
    """Composites glyph into dst, as large as fits in box (exclusive) with its aspect kept, centered."""
    x0, y0, x1, y1 = box
    bw, bh = x1 - x0, y1 - y0
    if bw <= 0 or bh <= 0:
        return
    s = min(bw / glyph.width, bh / glyph.height)
    w = min(bw, max(1, round(glyph.width * s)))
    h = min(bh, max(1, round(glyph.height * s)))
    art = pack._premul_resize(glyph, (w, h))
    dst.alpha_composite(art, (x0 + (bw - w) // 2, y0 + (bh - h) // 2))


def chars_on_page(info: dict, glyphs: dict, page_index: int) -> list[int]:
    out = []
    for c in sorted(glyphs):
        u, v, us, vs, tex, _ = info["cells"][c]
        if tex == page_index and us > 0 and vs > 0:
            out.append(c)
    return out


def paint_page(page: Image.Image, dump_page: Image.Image, info: dict, page_index: int, scale: int,
               glyphs: dict) -> Image.Image:
    """A copy of page with the PlayStation art in the cells of the pad characters it holds."""
    out = page.copy()
    cache = {}
    for c in chars_on_page(info, glyphs, page_index):
        u, v, us, vs, tex, _ = info["cells"][c]
        u2, v2 = min(u + us, pack.PAGE), min(v + vs, pack.PAGE)
        bb = _alpha_bbox(dump_page.crop((u, v, u2, v2)), 24)
        if bb is None:
            continue
        out.paste((0, 0, 0, 0), (u * scale, v * scale, u2 * scale, v2 * scale))
        if glyphs[c] not in cache:
            cache[glyphs[c]] = load_glyph(glyphs[c])
        bx0, by0, bx1, by1 = bb
        _fit(out, cache[glyphs[c]], ((u + bx0) * scale, (v + by0) * scale, (u + bx1) * scale, (v + by1) * scale))
    return out


def build_variants(info: dict, pages: list, dump: str, out_root: str, scale: int,
                   png: str | None = None) -> None:
    """Writes <out_root>/replace_<name>/<hash>.dds for the font pages that hold a pad character."""
    dumps = {}
    for name, glyphs in VARIANTS.items():
        out_dir = os.path.join(out_root, "replace_" + name)
        for p, h in enumerate(info["page_hashes"]):
            if not chars_on_page(info, glyphs, p):
                continue
            if h not in dumps:
                dumps[h] = pack.find_dump_page(dump, h)
            im = paint_page(pages[p], dumps[h], info, p, scale, glyphs)
            os.makedirs(out_dir, exist_ok=True)
            path = os.path.join(out_dir, h + ".dds")
            pack.write_dds(path, im)
            print("wrote", path, im.size)
            if png:
                os.makedirs(png, exist_ok=True)
                im.save(os.path.join(png, f"replace_{name}_{h}.png"))


def build_atlas_variants(dump: str, out_root: str) -> bool:
    """Writes the tutorial pictogram atlas with PlayStation buttons into each variant folder."""
    import atlas
    import texpack

    base = atlas.erase_buttons(dump)
    if base is None:
        print("playstation: no atlas, the pictograms keep their Xbox buttons")
        return False
    ai = atlas.load_info()
    h, size = ai["hash"], ai["size"]
    cache = {}
    for name in VARIANTS:
        img = base.copy()
        for sp in ai["sprites"]:
            art = ATLAS.get(sp["button"])
            if art is None:
                continue
            if art not in cache:
                cache[art] = load_glyph(art)
            x0, y0, x1, y1 = sp["disc"]  # inclusive
            _fit(img, cache[art], (x0, y0, x1 + 1, y1 + 1))
        out_dir = os.path.join(out_root, "replace_" + name)
        os.makedirs(out_dir, exist_ok=True)
        path = os.path.join(out_dir, h + ".dds")
        texpack.write_dds(path, texpack.DXGI_R8G8B8A8_UNORM, size, size, texpack.rgba_mip_chain(img))
        print(f"atlas: wrote {path} (PlayStation buttons)")
    return True
