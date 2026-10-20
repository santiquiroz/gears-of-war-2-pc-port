"""Writes assets/keycaps/svg/<id>.svg for every keycap, cluster and icon, and a contact sheet.

    py tools/keycaps/gen_svgs.py [--sheet out.png]
"""
from __future__ import annotations

import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import keys  # noqa: E402
import svgkit  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(HERE))
SVG_DIR = os.path.join(ROOT, "assets", "keycaps", "svg")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--sheet", help="write a PNG contact sheet of every entry here")
    args = ap.parse_args()
    catalog = {k.id: k for k in keys.everything()}
    os.makedirs(SVG_DIR, exist_ok=True)
    for k in catalog.values():
        with open(os.path.join(SVG_DIR, k.id + ".svg"), "w", encoding="utf-8", newline="\n") as f:
            f.write(svgkit.key_svg(k, catalog))
    print(f"wrote {len(catalog)} SVGs to {SVG_DIR}")
    if args.sheet:
        import resvg_py
        from PIL import Image

        scale = 0.9
        pad = 10
        cells = []
        for k in catalog.values():
            png = bytes(resvg_py.svg_to_bytes(svg_string=svgkit.key_svg(k, catalog, scale)))
            import io
            cells.append((k.id, Image.open(io.BytesIO(png)).convert("RGBA")))
        sheet_w = 1500
        x = y = pad
        row_h = 0
        placed = []
        for name, im in cells:
            if x + im.width + pad > sheet_w:
                x = pad
                y += row_h + pad
                row_h = 0
            placed.append((x, y, im))
            x += im.width + pad
            row_h = max(row_h, im.height)
        sheet = Image.new("RGBA", (sheet_w, y + row_h + pad), (60, 64, 78, 255))
        for px, py, im in placed:
            sheet.alpha_composite(im, (px, py))
        sheet.save(args.sheet)
        print("sheet", sheet.size)
    return 0


if __name__ == "__main__":
    sys.exit(main())
