# Tests for tools/keycaps/playstation.py. All inputs are synthetic; no game files needed.
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import numpy as np
from PIL import Image

import atlas
import pack
import playstation

SCALE = 2
# Cells (u, v, us, vs, tex, vo): 65 (A) and 66 (B) on page 0, 88 (X) on page 1.
CELLS = {65: [10, 20, 24, 26, 0, 0], 66: [40, 20, 24, 26, 0, 0], 88: [100, 100, 24, 26, 1, 0]}
GLYPHS = {65: "playstation_button_color_cross", 66: "playstation_button_color_circle",
          88: "playstation_button_color_square"}
BLOB = (14, 25, 30, 41)  # opaque pixels in the dump, inside the cell of 65 (x0, y0, x1, y1 exclusive)


def fake_info():
    cells = [[0, 0, 0, 0, 0, 0] for _ in range(256)]
    for c, cell in CELLS.items():
        cells[c] = list(cell)
    return {"cells": cells, "page_hashes": ["aaaa000000000000", "bbbb000000000000"]}


def fake_dump_page():
    px = np.zeros((256, 256, 4), dtype=np.uint8)
    x0, y0, x1, y1 = BLOB
    px[y0:y1, x0:x1] = (255, 255, 255, 255)
    return Image.fromarray(px, "RGBA")


def fake_page():
    """A built page: every pixel opaque grey, so cleared and untouched areas are easy to tell."""
    return Image.new("RGBA", (256 * SCALE, 256 * SCALE), (128, 128, 128, 255))


def test_paint_page_clears_cell_and_draws_inside_glyph_bbox():
    page = fake_page()
    out = np.asarray(playstation.paint_page(page, fake_dump_page(), fake_info(), 0, SCALE, GLYPHS))
    u, v, us, vs, _, _ = CELLS[65]
    cell = out[v * SCALE:(v + vs) * SCALE, u * SCALE:(u + us) * SCALE]
    x0, y0, x1, y1 = (c * SCALE for c in BLOB)
    inside = np.zeros(out.shape[:2], dtype=bool)
    inside[y0:y1, x0:x1] = True
    cell_mask = np.zeros(out.shape[:2], dtype=bool)
    cell_mask[v * SCALE:(v + vs) * SCALE, u * SCALE:(u + us) * SCALE] = True
    # Inside the cell, only the glyph bbox has visible pixels, and it has some.
    assert not out[cell_mask & ~inside][..., 3].any()
    assert out[inside][..., 3].any()
    assert cell[..., 3].any()
    # Outside the cell of 65 nothing changed (66's dump cell is empty).
    assert (out[~cell_mask] == (128, 128, 128, 255)).all()


def test_paint_page_skips_char_with_empty_dump_cell():
    page = fake_page()
    out = np.asarray(playstation.paint_page(page, fake_dump_page(), fake_info(), 0, SCALE, GLYPHS))
    u, v, us, vs, _, _ = CELLS[66]
    cell = out[v * SCALE:(v + vs) * SCALE, u * SCALE:(u + us) * SCALE]
    assert (cell == (128, 128, 128, 255)).all()


def test_paint_page_does_not_mutate_input():
    page = fake_page()
    before = page.tobytes()
    playstation.paint_page(page, fake_dump_page(), fake_info(), 0, SCALE, GLYPHS)
    assert page.tobytes() == before


def test_build_variants_writes_only_pages_with_mapped_chars(tmp_path, monkeypatch):
    info = fake_info()
    info["cells"][88] = [0, 0, 0, 0, 0, 0]  # nothing mapped lives on page 1 now
    written = []
    monkeypatch.setattr(pack, "find_dump_page", lambda dump, h: fake_dump_page())
    monkeypatch.setattr(pack, "write_dds", lambda path, img: written.append(path))
    pages = [fake_page(), fake_page()]
    playstation.build_variants(info, pages, "unused", str(tmp_path), SCALE)
    expected = {str(tmp_path / f"replace_{name}" / "aaaa000000000000.dds") for name in playstation.VARIANTS}
    assert set(written) == expected
    assert len(written) == len(playstation.VARIANTS)


def test_glyph_pngs_exist():
    names = set(playstation.ATLAS.values())
    for glyphs in playstation.VARIANTS.values():
        names |= set(glyphs.values())
    for name in sorted(names):
        path = os.path.join(playstation.glyph_dir(), name + ".png")
        assert os.path.isfile(path), path
        im = Image.open(path)
        assert im.size == (128, 128), name
        assert im.convert("RGBA").getchannel("A").getbbox() is not None, name


def test_erase_buttons_without_atlas(tmp_path):
    assert atlas.erase_buttons(str(tmp_path)) is None
