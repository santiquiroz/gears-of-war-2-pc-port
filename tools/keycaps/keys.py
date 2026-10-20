"""Catalog of the keycaps: one entry per key name the runtime's key parser accepts.

The names are the ones of the SDK's `ParseVirtualKey` (src/ui/keybinds.cpp): what a keybind_*
setting in gow2.toml can contain. `width` is in key units (1 = a square key). `kind` picks the
cap colour: standard, mod (modifiers and other wide control keys), num (numeric keypad), fn
(function row), mouse (an icon, not a key).
"""
from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class Key:
    id: str          # SDK key name, e.g. "Space"; clusters and icons use their own ids
    label: str       # text drawn on the cap ("" when an icon draws it)
    width: float = 1.0
    kind: str = "standard"
    icon: str = ""   # name of a drawing in svgkit.ICONS
    group: str = ""  # keycap | cluster | icon


def _letters():
    return [Key(c, c) for c in "ABCDEFGHIJKLMNOPQRSTUVWXYZ"]


def _digits():
    return [Key(str(d), str(d)) for d in range(10)]


def _fkeys():
    return [Key(f"F{n}", f"F{n}", kind="fn") for n in range(1, 25)]


def _numpad():
    keys = [Key(f"Numpad{d}", str(d), kind="num") for d in range(10)]
    keys += [
        Key("NumpadEnter", "Enter", 1.5, "num"),
        Key("NumpadPlus", "+", kind="num"),
        Key("NumpadMinus", "-", kind="num"),
        Key("NumpadStar", "*", kind="num"),
        Key("NumpadSlash", "/", kind="num"),
    ]
    return keys


def _punct():
    return [
        Key("Backtick", "`"), Key("Minus", "-"), Key("Plus", "="), Key("Comma", ","),
        Key("Period", "."), Key("Semicolon", ";"), Key("Slash", "/"), Key("Backslash", "\\"),
        Key("LBracket", "["), Key("RBracket", "]"), Key("Quote", "'"),
    ]


def _control():
    return [
        Key("Escape", "Esc", 1.2, "mod"),
        Key("Return", "Enter", 1.6, "mod", icon="enter"),
        Key("Space", "Space", 2.4, "standard"),
        Key("Tab", "Tab", 1.4, "mod", icon="tab"),
        Key("Backspace", "Back", 1.6, "mod", icon="backspace"),
        Key("Delete", "Del", 1.2, "mod"),
        Key("Insert", "Ins", 1.2, "mod"),
        Key("Home", "Home", 1.4, "mod"),
        Key("End", "End", 1.2, "mod"),
        Key("PageUp", "PgUp", 1.4, "mod"),
        Key("PageDown", "PgDn", 1.4, "mod"),
        Key("Left", "", 1.0, "mod", icon="left"),
        Key("Right", "", 1.0, "mod", icon="right"),
        Key("Up", "", 1.0, "mod", icon="up"),
        Key("Down", "", 1.0, "mod", icon="down"),
        Key("Shift", "Shift", 1.6, "mod", icon="shift"),
        Key("Control", "Ctrl", 1.4, "mod"),
        Key("Alt", "Alt", 1.2, "mod"),
        Key("PrintScreen", "PrtSc", 1.4, "mod"),
        Key("Pause", "Pause", 1.5, "mod"),
        Key("CapsLock", "Caps", 1.4, "mod"),
        Key("NumLock", "Num", 1.3, "mod"),
        Key("ScrollLock", "ScrLk", 1.4, "mod"),
    ]


def _mouse():
    return [
        Key("LMB", "", 1.0, "mouse", icon="mouse_l"),
        Key("RMB", "", 1.0, "mouse", icon="mouse_r"),
        Key("MMB", "", 1.0, "mouse", icon="mouse_m"),
    ]


def keycaps() -> list[Key]:
    keys = (_letters() + _digits() + _fkeys() + _punct() + _control() + _numpad() + _mouse())
    return [Key(k.id, k.label, k.width, k.kind, k.icon, "keycap") for k in keys]


def clusters() -> list[Key]:
    """Several caps in one glyph: the game's labels for a whole stick or the D-pad are one string
    with room for a single character, so the common sets are pre-built."""
    return [
        Key("CL_WASD", "WASD", 4.2, "cluster", group="cluster"),
        Key("CL_ARROWS", "", 4.2, "cluster", icon="cl_arrows", group="cluster"),
        Key("CL_1234", "1234", 4.2, "cluster", group="cluster"),
        Key("PR_WS", "W/S", 2.2, "cluster", group="cluster"),
        Key("PR_UPDOWN", "", 2.2, "cluster", icon="pr_updown", group="cluster"),
        Key("IC_MOVE", "", 1.0, "icon", icon="move", group="icon"),
        Key("IC_MOUSE", "", 1.0, "icon", icon="mouse_move", group="icon"),
        Key("IC_UNKNOWN", "?", 1.0, "standard", group="icon"),
    ]


def everything() -> list[Key]:
    return keycaps() + clusters()


# Clusters keyed by the four (up, left, down, right) key names of a stick, and by the D-pad's.
STICK_CLUSTERS = {
    ("W", "A", "S", "D"): "CL_WASD",
    ("Up", "Left", "Down", "Right"): "CL_ARROWS",
}
DPAD_CLUSTERS = {
    ("1", "2", "3", "4"): "CL_1234",  # up, right, down, left as the game lists them
}
UPDOWN_PAIRS = {
    ("W", "S"): "PR_WS",
    ("Up", "Down"): "PR_UPDOWN",
}

if __name__ == "__main__":
    ks = everything()
    print(len(ks), "entries;", sum(k.width for k in ks), "key units")
