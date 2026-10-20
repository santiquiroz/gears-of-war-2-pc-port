# Keycap prompts

The game draws its button prompts ("A Accept", the hint "press RT to fire") with one font,
`Warfare_HUD.Xbox360_18pt`: a pad button is a single character whose picture sits in one of the
font's two 256x256 texture pages. This folder draws a keycap for every key a `keybind_*` setting
can name and puts them into those pages, so the prompts show the keys you actually use and the
pad's buttons while a controller is connected.

```text
keys.py        the catalog: every key name the runtime accepts (A-Z, 0-9, F1-F24, punctuation,
               Space/Enter/Tab..., arrows, modifiers, numeric keypad, LMB/RMB/MMB), plus a few
               clusters (WASD, the arrows, 1-4, W/S, Up/Down) and a move and a mouse icon
svgkit.py      draws one cap as an SVG: label outlines come from DejaVu Sans Bold (fonts/)
gen_svgs.py    writes assets/keycaps/svg/<key>.svg, and a contact sheet with --sheet
analyze_font.py  reads the font out of your game (cell table, which characters strings use)
                 into font_info.json; run once, the result is committed
pack.py        lays the caps out in the free space of the two pages, renders them, copies the
               glyphs the game still needs from your texture dump, writes the pack and
               src/gow2_keycap_layout.inc
```

## Using it

`scripts/build-keycaps.ps1` does everything: it installs the Python packages, makes the game dump
its two font pages (play to the Select Difficulty screen once), runs `pack.py` and puts the result
in `textures\4D53082D\replace` next to `gow2.exe`. The pack holds game art: do not share it.

The SVGs in `assets/keycaps/svg` are original artwork and can be used on their own.

## How it works in the game

- The texture pack (`texture_replace`, turned on by itself while the pack is installed) swaps the
  two pages for 1024x1024 versions. Characters the game's own strings use, and the spinner's, keep
  their cells; every other character is free and gets a keycap.
- `src/gow2_prompt_swap.cpp` finds the font's cell table in guest memory once (24-byte records, one
  per character), points the keycap characters at their cells, and rewrites the strings of
  `[ControllerFont]` (`Controller_A` ...) in place: keycap markup while no controller is connected,
  the original glyph while one is. It reads the `keybind_*` settings twice a second, so a rebind
  shows up at once on the next screen drawn. `src/gow2_keycaps.cpp` maps a binding to characters.
- `keycaps.id` in the pack names the layout; the game ignores a pack made for another layout.
- The tutorial pictograms (the "look at something" hint and the other tutorial panels) are one
  texture with Xbox buttons painted in. `atlas.py` makes the same picture without those nine
  sprites (`replace_kbd_base`). While no controller is connected the game draws the keycaps of your
  bindings onto it (`src/gow2_atlas_swap` code in `gow2_prompt_swap.cpp`, from the keycap pages of
  the pack), writes the result to `replace_kbdA` or `replace_kbdB` (alternating) and sets
  `texture_replace_variant` to it; SDK patch 0023 reloads the texture at once. A rebind composes it
  again, a controller gets the original picture back.

## Limits

- 22-unit keycaps (the Xbox glyphs are about 24): the two pages cannot hold more at this count of
  keys without moving the glyphs the game still uses.
- A string with room for one character shows the main key of a chord (`Shift+Up` shows Up); only
  the stick and D-pad clusters listed in `keys.py` have a single glyph.
- Screens already open keep the labels they were built with until they are opened again.
- A wide key (Space, Enter) is drawn small on a pictogram: the sprite is as wide as the Xbox button
  was. The press-animation dots round the buttons are gone with a keyboard.
- Windows only; Android keeps its text labels.
