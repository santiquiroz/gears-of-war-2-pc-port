"""SVG drawings of the keycaps: glossy, bevelled, light-grey keys in the spirit of the Xbox 360
button glyphs the game ships. Everything is plain SVG paths (the label text is turned into
outlines with DejaVu Sans Bold, see fonts/LICENSE_DEJAVU), so the files look the same anywhere.

One key is 100 units tall; its width is `Key.width * 100`.
"""
from __future__ import annotations

import os
from functools import lru_cache

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

from keys import Key

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_FILE = os.path.join(HERE, "fonts", "DejaVuSans-Bold.ttf")
CAP_HEIGHT = 0.729  # DejaVu Sans Bold capital height in em

INK = "#1c1c1c"

# (outline, base top, base bottom, face top, face mid, face bottom, face border)
PALETTE = {
    "standard": ("#101010", "#f4f4f4", "#a9a9a9", "#ffffff", "#ececec", "#d2d2d2", "#8c8c8c"),
    "mod": ("#101010", "#e2e2e2", "#8f8f8f", "#f1f1f1", "#d8d8d8", "#bcbcbc", "#7a7a7a"),
    "fn": ("#101010", "#eceaf0", "#9d9aa8", "#fbfaff", "#e3e1ea", "#c9c6d3", "#85828f"),
    "num": ("#101010", "#e4ebf3", "#9aa6b6", "#f6faff", "#dbe4ef", "#c1cddc", "#7b8797"),
    "mouse": ("#101010", "#f0f0f0", "#a2a2a2", "#fbfbfb", "#e4e4e4", "#cbcbcb", "#858585"),
}


@lru_cache(maxsize=1)
def _font():
    f = TTFont(FONT_FILE)
    return f, f.getGlyphSet(), f.getBestCmap(), f["hmtx"], f["head"].unitsPerEm


def _n(v: float) -> str:
    s = f"{v:.2f}".rstrip("0").rstrip(".")
    return s if s not in ("-0", "") else "0"


def text_width(text: str, size: float) -> float:
    _, _, cmap, hmtx, upem = _font()
    return sum(hmtx[cmap[ord(c)]][0] for c in text) * size / upem


def text_path(text: str, size: float, cx: float, cy: float, max_w: float | None = None) -> str:
    """Outlines of `text`, horizontally centred on cx, capitals vertically centred on cy."""
    _, gs, cmap, hmtx, upem = _font()
    if max_w is not None:
        w = text_width(text, size)
        if w > max_w:
            size *= max_w / w
    scale = size / upem
    width = sum(hmtx[cmap[ord(c)]][0] for c in text) * scale
    x = cx - width / 2
    base = cy + CAP_HEIGHT * size / 2
    parts = []
    for c in text:
        name = cmap[ord(c)]
        pen = SVGPathPen(gs, ntos=_n)
        gs[name].draw(TransformPen(pen, (scale, 0, 0, -scale, x, base)))
        parts.append(pen.getCommands())
        x += hmtx[name][0] * scale
    return "".join(parts)


def _ink(d: str, extra: str = "") -> str:
    # A pale offset copy under the dark one reads as the legend being printed into the cap.
    return (f'<path d="{d}" transform="translate(0 2.2)" fill="#ffffff" fill-opacity=".75"/>'
            f'<path d="{d}" fill="{INK}"{extra}/>')


# ---- icons (drawn in a 100 x 100 box, centre 50,45) --------------------------------------------

def _arrow(direction: str, cx: float, cy: float, size: float) -> str:
    h = size / 2
    head = size * 0.52
    shaft = size * 0.2
    # An upward arrow, rotated for the other directions.
    pts = [(0, -h), (head / 1.1, -h + head), (shaft / 2, -h + head), (shaft / 2, h),
           (-shaft / 2, h), (-shaft / 2, -h + head), (-head / 1.1, -h + head)]
    rot = {"up": 0, "right": 90, "down": 180, "left": 270}[direction]
    d = "M" + "L".join(f"{_n(x)} {_n(y)}" for x, y in pts) + "Z"
    return _ink_shape(d, f"translate({_n(cx)} {_n(cy)}) rotate({rot})")


def _ink_shape(d: str, transform: str) -> str:
    return (f'<path d="{d}" transform="{transform} translate(0 2.2)" fill="#ffffff" fill-opacity=".75"/>'
            f'<path d="{d}" transform="{transform}" fill="{INK}"/>')


def _bent_arrow(cx: float, cy: float, s: float) -> str:
    """The return-key arrow: down, then left with a head."""
    w = s * 0.14
    d = (f"M{_n(cx + s / 2)} {_n(cy - s / 2)} V{_n(cy + s * 0.12)} H{_n(cx - s * 0.18)} "
         f"V{_n(cy + s * 0.34)} L{_n(cx - s / 2)} {_n(cy + s * 0.12 - w / 2 + w / 2)} "
         f"L{_n(cx - s * 0.18)} {_n(cy - s * 0.1)} V{_n(cy + s * 0.12 - w)} "
         f"H{_n(cx + s / 2 - w)} V{_n(cy - s / 2)}Z")
    return _ink(d)


def _tab_arrow(cx: float, cy: float, s: float) -> str:
    w = s * 0.16
    d = (f"M{_n(cx - s / 2)} {_n(cy - w / 2)} H{_n(cx + s * 0.18)} V{_n(cy - s * 0.26)} "
         f"L{_n(cx + s * 0.5)} {_n(cy)} L{_n(cx + s * 0.18)} {_n(cy + s * 0.26)} "
         f"V{_n(cy + w / 2)} H{_n(cx - s / 2)}Z")
    bar = f"M{_n(cx + s / 2 + w * 0.2)} {_n(cy - s * 0.3)} h{_n(w)} v{_n(s * 0.6)} h{_n(-w)}Z"
    return _ink(d + bar)


def _shift_arrow(cx: float, cy: float, s: float) -> str:
    # Hollow up arrow.
    h = s / 2
    d = (f"M{_n(cx)} {_n(cy - h)} L{_n(cx + h)} {_n(cy)} H{_n(cx + h * 0.45)} V{_n(cy + h)} "
         f"H{_n(cx - h * 0.45)} V{_n(cy)} H{_n(cx - h)}Z "
         f"M{_n(cx)} {_n(cy - h * 0.55)} L{_n(cx - h * 0.45)} {_n(cy - h * 0.1)} H{_n(cx - h * 0.2)} "
         f"V{_n(cy + h * 0.72)} H{_n(cx + h * 0.2)} V{_n(cy - h * 0.1)} H{_n(cx + h * 0.45)}Z")
    return _ink(d, ' fill-rule="evenodd"')


def _mouse(cx: float, cy: float, s: float, hot: str) -> str:
    """A mouse seen from above; `hot` is the pressed part: l, r, m or '' (none)."""
    w, h = s * 0.62, s
    x0, y0 = cx - w / 2, cy - h / 2
    split = y0 + h * 0.42
    r = w / 2
    body = (f"M{_n(x0)} {_n(y0 + r)} A{_n(r)} {_n(r)} 0 0 1 {_n(x0 + w)} {_n(y0 + r)} "
            f"V{_n(y0 + h - r)} A{_n(r)} {_n(r)} 0 0 1 {_n(x0)} {_n(y0 + h - r)}Z")
    out = [f'<path d="{body}" fill="#f7f7f7" stroke="{INK}" stroke-width="5" stroke-linejoin="round"/>']
    red = "#d8352a"
    if hot == "l":
        out.append(f'<path d="M{_n(x0)} {_n(y0 + r)} A{_n(r)} {_n(r)} 0 0 1 {_n(cx)} {_n(y0)} V{_n(split)} H{_n(x0)}Z" '
                   f'fill="{red}" stroke="{INK}" stroke-width="3"/>')
    elif hot == "r":
        out.append(f'<path d="M{_n(cx)} {_n(y0)} A{_n(r)} {_n(r)} 0 0 1 {_n(x0 + w)} {_n(y0 + r)} V{_n(split)} H{_n(cx)}Z" '
                   f'fill="{red}" stroke="{INK}" stroke-width="3"/>')
    out.append(f'<path d="M{_n(x0)} {_n(split)} H{_n(x0 + w)} M{_n(cx)} {_n(y0)} V{_n(split)}" '
               f'fill="none" stroke="{INK}" stroke-width="4"/>')
    wheel_fill = red if hot == "m" else "#9a9a9a"
    out.append(f'<rect x="{_n(cx - w * 0.09)}" y="{_n(y0 + h * 0.1)}" width="{_n(w * 0.18)}" '
               f'height="{_n(h * 0.24)}" rx="{_n(w * 0.09)}" fill="{wheel_fill}" stroke="{INK}" stroke-width="3"/>')
    return "".join(out)


def _move_icon(cx: float, cy: float, s: float) -> str:
    return "".join(_arrow(d, cx + dx * s * 0.3, cy + dy * s * 0.3, s * 0.38)
                   for d, dx, dy in (("up", 0, -1), ("down", 0, 1), ("left", -1, 0), ("right", 1, 0)))


# ---- one cap ---------------------------------------------------------------------------------

def _defs(uid: str, kind: str) -> str:
    ol, bt, bb, ft, fm, fb, _ = PALETTE[kind]
    return (
        f'<linearGradient id="b{uid}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="{bt}"/>'
        f'<stop offset="1" stop-color="{bb}"/></linearGradient>'
        f'<linearGradient id="f{uid}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="{ft}"/>'
        f'<stop offset=".55" stop-color="{fm}"/><stop offset="1" stop-color="{fb}"/></linearGradient>'
        f'<linearGradient id="g{uid}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff" stop-opacity=".9"/>'
        f'<stop offset="1" stop-color="#fff" stop-opacity="0"/></linearGradient>')


def cap_group(key: Key, uid: str, x: float = 0.0) -> tuple[str, str]:
    """(defs, drawing) of one cap whose left edge is at x."""
    w = key.width * 100
    kind = key.kind if key.kind in PALETTE else "standard"
    ol, _, _, _, _, _, border = PALETTE[kind]
    fx, fy, fw, fh = 11, 9, w - 22, 72
    cx, cy = w / 2, fy + fh / 2
    g = [
        f'<g transform="translate({_n(x)} 0)">',
        f'<rect x="1.5" y="1.5" width="{_n(w - 3)}" height="97" rx="18" fill="{ol}"/>',
        f'<rect x="5" y="5" width="{_n(w - 10)}" height="90" rx="14" fill="url(#b{uid})"/>',
        f'<rect x="{fx}" y="{fy}" width="{_n(fw)}" height="{fh}" rx="10" fill="url(#f{uid})" '
        f'stroke="{border}" stroke-opacity=".6" stroke-width="1.6"/>',
        f'<clipPath id="c{uid}"><rect x="{fx}" y="{fy}" width="{_n(fw)}" height="{fh}" rx="10"/></clipPath>',
        f'<ellipse cx="{_n(cx)}" cy="{fy + 4}" rx="{_n(fw * 0.62)}" ry="{_n(fh * 0.46)}" '
        f'fill="url(#g{uid})" opacity=".55" clip-path="url(#c{uid})"/>',
    ]
    g.append(_label(key, cx, cy, fw))
    g.append("</g>")
    return _defs(uid, kind), "".join(g)


def _label(key: Key, cx: float, cy: float, fw: float) -> str:
    icon = key.icon
    if icon in ("up", "down", "left", "right"):
        return _arrow(icon, cx, cy, 44)
    if icon.startswith("mouse_") and icon != "mouse_move":
        return _mouse(cx, cy + 1, 62, icon[-1])
    if icon == "mouse_move":
        return _mouse(cx - 6, cy + 1, 56, "") + _arrow("left", cx + 20, cy - 8, 17) + _arrow("right", cx + 20, cy + 14, 17)
    if icon == "move":
        return _move_icon(cx, cy, 52)
    label = key.label
    size = 52 if len(label) == 1 else 36
    if len(label) == 1 and label in "`'.,;-=/\\[]*+":
        size = 70 if label in "`'.,;" else 60
    if icon in ("enter", "backspace", "tab", "shift"):
        text_w = text_width(label, 32)
        gap, isz = 10, 34
        total = isz + gap + text_w
        x0 = cx - total / 2
        ix = x0 + isz / 2
        tx = x0 + isz + gap + text_w / 2
        if icon == "enter":
            ic = _bent_arrow(ix, cy, isz)
        elif icon == "backspace":
            ic = _arrow("left", ix, cy, isz)
        elif icon == "shift":
            ic = _shift_arrow(ix, cy, isz)
        else:  # tab: arrow after the word
            tx = x0 + text_w / 2
            ix = x0 + text_w + gap + isz / 2
            ic = _tab_arrow(ix, cy, isz)
        return ic + _ink(text_path(label, 32, tx, cy))
    # Text keys: one size per length, shrunk to fit the cap.
    if len(label) > 1:
        size = {2: 44, 3: 38, 4: 34, 5: 32}.get(len(label), 28)
        if label == "Space":
            size = 44
    return _ink(text_path(label, size, cx, cy, fw * 0.86))


CLUSTER_MEMBERS = {
    "CL_WASD": ("W", "A", "S", "D"),
    "CL_ARROWS": ("Up", "Left", "Down", "Right"),
    "CL_1234": ("1", "2", "3", "4"),
    "PR_WS": ("W", "S"),
    "PR_UPDOWN": ("Up", "Down"),
}


def key_svg(key: Key, catalog: dict[str, Key], scale: float = 1.0, px: tuple[int, int] | None = None) -> str:
    """A standalone SVG of one entry (cap, cluster of caps or icon). `px` forces the pixel size."""
    members = CLUSTER_MEMBERS.get(key.id)
    if members:
        defs, body, x = [], [], 0.0
        gap = 4.0
        for i, name in enumerate(members):
            d, g = cap_group(catalog[name], f"{key.id}{i}", x)
            defs.append(d)
            body.append(g)
            x += catalog[name].width * 100 + gap
        total = x - gap
    else:
        d, g = cap_group(key, key.id)
        defs, body, total = [d], [g], key.width * 100
    w = str(px[0]) if px else _n(total * scale)
    h = str(px[1]) if px else _n(100 * scale)
    fit = ' preserveAspectRatio="none"' if px else ""
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}"{fit} '
            f'viewBox="0 0 {_n(total)} 100"><title>{key.id}</title><defs>{"".join(defs)}</defs>'
            f'{"".join(body)}</svg>\n')


def cluster_width(key: Key, catalog: dict[str, Key]) -> float:
    members = CLUSTER_MEMBERS.get(key.id)
    if not members:
        return key.width * 100
    return sum(catalog[m].width * 100 for m in members) + 4.0 * (len(members) - 1)
