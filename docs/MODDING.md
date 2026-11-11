# Modding

There are two ways to mod the port: **texture packs** that replace the game's textures, and a **mods folder** for loose game files. Mods never touch the game files; delete a mod folder to undo it.

## The mods folder

The `mods` folder sits next to the `textures` folder of the port (the folder that holds `gow2.exe`).

```
mods\<ModName>\mod.ini                 optional: name, author, version, description, url
mods\<ModName>\GearGame\...            files that replace or add game files
mods\<ModName>\<TITLEID>\replace\...   optional texture pack inside the mod
```

- Every subfolder is an active mod. A name starting with `_` or `.` disables it.
- Load order is the folder name order (case-insensitive); a later mod wins over an earlier one for the same file. Tip: prefix names with numbers (10-, 20-).
- The layout is the same as COGBox's, so loose-file mods made for it should work when they only contain cooked game files (not tested with every mod yet).
- Not loaded: `.xex`, `.exe` and `.dll` files; `Coalesced*.bin` files are not supported yet (the port edits Coalesced itself for PC Options).
- The log (`logs\gow2_NNN.log`) lists each mod: `mods: <name> <version> by <author> - <n> files, textures yes|no`, and each skipped file.
- Example `mod.ini` (key=value lines):

```
name=My Mod
author=Your Name
version=1.0
description=Replaces some textures and a sound file
url=https://example.com/my-mod
```

## Texture packs

See [docs/TEXTURE_PACKS.md](TEXTURE_PACKS.md) for the format and how to build one.

Texture packs, including the ones inside mods, load only with the Direct3D 12 graphics API; the experimental Vulkan API ignores them.

Additional details:

- Several packs can be active at once: every mod that contains a texture pack plus the pack chosen in PC Options. When two packs replace the same texture, the later mod wins, and mods win over the chosen pack.
- DDS files need a full mip chain, or the texture is refused (the log says `only N mips for M guest levels`). Example: `texconv -m 0 -f BC1_UNORM file.png` (use `BC3_UNORM` for textures with alpha).
- PC Options → Graphics → **Keep original menus and text** (`texture_replace_skip_ui = true` in `gow2.toml`; experimental, takes effect at once) keeps the original menus, HUD and text and replaces the rest. It treats textures without mip maps as UI, which fits most of this game's menus; report textures it gets wrong.

## Sharing mods

Sharing mods and packs is up to their authors. Ship only what you changed, not files copied unchanged from the game. This project does not publish or link mods or packs made from the game's art.

## For contributors

- Mod planning: `src/gow2_mods.h`, `src/gow2_mods.cpp` (tests in `tools/tests/mods_test.cpp`)
- Setup call: `src/gow2_app.h`
- Texture replacement: SDK patch `sdk/patches/0073-texture-packs-stack-skip-ui.patch` (`texture_replacement.cpp`)
- PC Options rows: `src/gow2_options_catalog.cpp`

See [CONTRIBUTING.md](../CONTRIBUTING.md) for contribution rules.