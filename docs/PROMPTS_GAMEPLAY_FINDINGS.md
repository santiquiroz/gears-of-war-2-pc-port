# Gameplay button prompts with `gow2_prompts = keyboard` (issue #21)

What issue #21 reports: with Controls → prompts set to keyboard, the menus show keyboard keys, but
gameplay prompts (pick up, reload, take cover, the context-action icons) still show Xbox buttons.

## How prompts are swapped today

- On Windows, `gow2_prompts` = `auto` **and** `keyboard` both use the live swap
  (`PromptsFollowController()` in `src/gow2_coalesced.cpp`: anything but `controller`). The bundles
  keep their pad glyphs; `CoalescedOptionsFromCvars()` leaves `prompt_labels` empty, so
  `AddPrompts()` (the `[ControllerFont]` bundle edit) is only used on Android and Linux.
- `src/gow2_prompt_swap.cpp` finds the loaded FStrings of `GearGameUI.int [ControllerFont]`
  (`Controller_A` = `<Fonts:Warfare_HUD.Xbox360_18pt>A<Fonts:/>` ...) in guest memory and rewrites
  them in place. With `keyboard` the reading is fixed at 1 (`PromptSwapTick`, `gow2_prompts ==
  "keyboard" || !PadConnected()`). The new text is keycap markup when the keycap pack is installed
  (`tools/keycaps`), or a bracketed label such as `[Space]` without it.
- These strings reach the screen in three ways, all by reference to `[ControllerFont]`:
  - `Xe-GearInput.ini [Engine.UIDataStore_InputAlias]` `ButtonFontMarkupString="<Strings:GearGameUI.ControllerFont.Controller_A>"`,
    and `GearGameUI.int [CalloutButtonText]` (`GenericAccept = <Strings:GearGameUI.ControllerFont.Controller_A> Accept`):
    the menu button bars.
  - `Xe-GearUI.ini [GearGame.GearUIDataStore_StringAliasMap]` `MenuInputMapArray` (`Set="360"`
    rows point at `ControllerFont`).
  - `Xe-GearUI.ini [GearGame.GearUIDataStore_StringAliasBindingsMap]` `ControllerMapArray`
    (`GMS_XBoxTypeS_A` → `Controller_A`): the tutorial text `<StringAliasBindings:GBA_Reload>` in
    `GearGame.int [TUTORIALS]` resolves the bound pad button, then `[ControllerFont]`. That is why the
    tutorial *text* already names your key.
- The tutorial / action pictogram atlas (512x512, content hash `ce11e464b5064714`,
  `tools/keycaps/atlas_info.json`) has Y, A, B, X and RS button sprites painted in. With the keycap
  pack and `replace_kbd_base`, `ComposeAtlas()` draws your keycaps onto it (`texture_replace_variant`
  `kbdA`/`kbdB`, SDK patch 0023).

## Why gameplay prompts are not covered

The button font `Warfare_HUD.Xbox360_18pt` draws a pad button as one character (`A` is the A
button, `` ` `` is RB, `]` LT, `^` RT, `_` LB, `X` X ...). Only the strings that go through
`[ControllerFont]` are rewritten. Gameplay prompts that write the character directly are not:

1. **Script literals in GearGame.u.** `tools/keycaps/font_info.json` `used_in_scripts` (made by
   `analyze_font.py` `used_in_packages()`, which looks for `<Fonts:Warfare_HUD.Xbox360_18pt>` in the
   decompressed packages) lists characters 62 `>`, 88 `X`, 92 `\`, 93 `]`, 94 `^`, 95 `_`, 96 `` ` ``,
   97 `a` and 102 `f` in `GearGame.upk`. That is script text, not Coalesced, so no bundle edit and
   no `[ControllerFont]` rewrite reaches it.

   **Not confirmed: which HUD prompt draws these characters.** `used_in_packages()` only records
   that the font tag followed by the character occurs somewhere in the package bytes; it does not
   record the function, class or default property holding it, nor whether that code runs during
   gameplay. So the presence of `X` (88) and `` ` `` (96, RB) in GearGame.upk does **not** show that
   the pick-up / swap-weapon prompt draws `X` or that the reload prompt draws RB from these literals.
   Those two prompts may instead come from a painted icon (list 3, e.g. the `CanvasIcon` of an
   action info) or from a `<StringAliasBindings:...>` string, which the existing swap already
   handles for text. The pairing "X = pick up, RB = reload" in this document is a hypothesis taken
   from the default Gears 2 pad layout, not a traced consumer.

   What would confirm it (needs the decompressed packages, not present here, see
   "Not confirmed on this machine"):
   - `python -I tools/ue3/scan_scripts.py out/ue3/pkg/GearGame.upk Xbox360_18pt` to list the
     functions / defaults containing each literal. A consumer is confirmed when the literal sits in
     the HUD path of the prompt (for example a `GearHUD_Base` / `GearHUDMP` draw function or the
     action-info defaults read by it), not just anywhere in the package.
   - `python -I tools/ue3/cdo.py out/ue3/pkg/GearGame.upk GearHUD_Base` (and `GearPC`,
     `GearPawn`, the weapon-pickup classes) to see whether the pick-up / reload action info
     carries a font string or only a `CanvasIcon` texture.
   - In game: step 3 of "How to verify in game" below. If the pick-up / reload prompt still shows
     the pad button while the log reports `N pad glyphs on keycaps` > 0, that prompt is not drawn
     with these font characters and belongs to list 3.
2. **Hardcoded glyphs in the .int text** that skip `[ControllerFont]`, counted in
   `Coalesced_int.bin` (114 values carry the font tag; 26 are `[ControllerFont]` itself):
   - `GearGame.int [GearHud_Base]`: `MPGotoSpectateString` (`^`), `StationaryCams` (`_` `w`),
     `CyclePlayerCam` (`^` `` ` ``)
   - `GearGame.int [UIScene_Spectator]`, `[GearUIScene_CoopSpectator]`, `[UIScene_InGameStats]`
     (`StatsButtonBar`), `[UIScene_GearScoreboard]`, `[TUTORIALS] DISABLE_PRESS` (`X`)
   - `GearGame.int [XboxButtonLabels]` (43 values, `Back = <Fonts:...>B<Fonts:/>Back` ...), the
     four `UIDataProvider_StickConfiguration` sections, `[GearTitleScreen]`, the lobby scenes.
3. **Painted art.** Button sprites inside textures (the pictogram atlas above; any other HUD
   texture with a button painted in). Without the keycap pack nothing replaces them; with it only
   the nine Y/A/B/X/RS sprites of `ce11e464b5064714` are redrawn.

The game does not pick these glyphs from an "active input device" flag: the cooked script and
text name the Xbox360 font characters directly. Forcing a PC flag would not change them: the
leftover PC rows (`Set="PC"` in `GearUIDataStore_StringAliasMap`, pointing at
`GearGameUI.int [KeycapMenuFont]` `Map_A = A`, `Map_LeftStick = LS` ... and
`[GameMappedStrings]`) only cover the menu aliases (`MENU_A` ..., `GAME_CHOOSELEFT/RIGHT`), and
their values are bare letters with no keycap art behind them.

## What was changed

All three sources above draw the same font characters. In keyboard mode with the keycap pack
installed, the `[ControllerFont]` strings no longer use the pad characters (they are rewritten to
keycap characters), so the pad characters are only drawn by the strings the swap does not reach.
The live swap now also points the font cell of each pad character at the keycap cell of the same
binding:

- `keycap::PadGlyphSwaps()` (`src/gow2_keycaps.cpp`): from the loaded `[ControllerFont]` values
  (one-glyph values only; `Controller_Start` `\>` and `Controller_Back` `\<` are two characters
  sharing `\` and are left alone) and the keycap of each entry's binding, returns pad character →
  keycap character. It never moves a keycap character's own cell, and leaves out a pad character
  two entries would send to different keys.
- `SwapPadGlyphs()` (`src/gow2_prompt_swap.cpp`): saves the original 24-byte cell of each pad
  character the first time it is moved, writes the keycap's cell in keyboard mode, and writes the
  saved cells back when a controller takes over (`auto`) or the bindings change. The log line now
  reads `PROMPTS live: keycap labels (N strings written, M pad glyphs on keycaps)`.
- Test: `tools/tests/keycaps_test.cpp` (`gow2_keycaps_test`, in `CMakeLists.txt` and
  `scripts/test-host.ps1`).

Effects to expect in keyboard mode with keycaps: every place that draws the pad characters of the
first two lists shows the bound key, including the menu text of `[XboxButtonLabels]` and the stick
pictures of the Controls screen (they show the move cluster). In `auto` mode the same happens
while no controller is connected and reverts when one is plugged in.

The cell swap works on the font itself, so it covers any consumer that draws these characters
with `Warfare_HUD.Xbox360_18pt`, whoever it is; it does not depend on knowing the consumer. What it
does depend on is the unconfirmed point of list 1: it fixes the pick-up and reload prompts only if
those prompts are drawn with these font characters. If they are painted icons, they stay as Xbox
buttons until step 4 below is done.

## Mouse buttons in the Choose Path prompt (issue #29)

`GearUIScene_PathChoice.HandleInputKey` takes `LeftMouseButton` as the left trigger's choice and
`RightMouseButton` as the right trigger's, while the default bindings put RMB on LT and LMB on RT,
so with native keys the mouse did the opposite of the prompt. When the triggers hold the mouse
buttons that way round (`keynames::MouseButtonsFollowTriggers`, decided once at start), the two
mouse buttons trade engine names at the engine boundary only: in the `Xe-GearInput.ini` bindings
the Coalesced layer writes and in the key names `src/gow2_native_keys.cpp` sends to `InputKey`;
everything else (routing, prompt labels, keycaps) keeps the true names.

## What still shows Xbox buttons, and the steps to finish it

1. **No keycap pack (text labels).** The font has no keyboard art, so the hardcoded glyphs and the
   painted sprites cannot become text: a `[Space]` label only works where a whole string is
   rewritten. Fix for players: build the pack (`Build keycap pack.cmd` in the release zip,
   `scripts/build-keycaps.ps1` from source) and keep `gow2_keycaps = true`, `texture_replace` on.
   A text fallback would need each script literal rewritten in GearGame.u bytecode (string
   literals are length-prefixed `StringConst` / `UnicodeStringConst` tokens; a longer label means
   re-laying the function, see `tools/script_patch*.py`), so it is not done.
2. **`a` (97) and `f` (102)** in GearGame.upk are not a `[ControllerFont]` value, so the port does
   not know which button they show and they keep their pad picture. Steps: decompress GearGame
   (`tools/ue3/README.md`), then
   `python -I tools/ue3/scan_scripts.py out/ue3/pkg/GearGame.upk Xbox360_18pt` lists the functions
   with the literals; look at the cells 97 and 102 in your dump of page `86bf4f7c0ec59721`; add the
   pair to `PadGlyphSwaps()` (for example as an extra key → character list) with a test.
3. **Start / Back** (`\>` and `\<`): one picture drawn as two characters. Moving `>` and `<` to
   the Start / Back keycaps and `\` to an empty cell (width 0) would do it; `\` must then be checked
   against every string that uses it (`used_in_bundles` 92) before it is blanked.
4. **Painted sprites other than the nine of `ce11e464b5064714`.** Steps: dump the HUD textures
   (`texture_dump`), find the ones with buttons painted in (`tools/keycaps/atlas.py` finds the
   sprites by connected components of the alpha), add each texture's hash and sprite boxes to
   `atlas_info.json` (`button` = Y/A/B/X/RS or a new name), extend `pack.py` to write their
   keyboard bases, and extend `ComposeAtlas()` in `src/gow2_prompt_swap.cpp` to compose every
   listed texture (today it composes only `keycap::kAtlasHash`). To find which texture the action
   icon of a prompt comes from: `python -I tools/ue3/cdo.py out/ue3/pkg/GearGame.upk GearPC` (and
   `GearHUD_Base`, `GearPawn`) lists the `CanvasIcon` defaults (`Texture`, `U`, `V`, `UL`, `VL`)
   of the action infos.

## Not confirmed on this machine

The decompressed packages (`out/ue3/pkg`) are not present here (only `game/default.xex` is), so
the function names behind the `used_in_scripts` characters were not listed and no HUD consumer of
them was identified. Confirmed from files in the repo: the `[ControllerFont]` swap path, the
hardcoded glyphs of list 2 (Coalesced dump), and the characters present in GearGame.upk
(`font_info.json`). Not confirmed: that the gameplay pick-up / reload / take-cover prompts draw
those GearGame.upk characters (see list 1). Nothing was run in game.

## How to verify in game

1. Install the keycap pack, set `gow2_prompts = "keyboard"` (or leave `auto` with no controller).
2. Start a campaign chapter. The log should show
   `PROMPTS live: keycap labels (... strings written, N pad glyphs on keycaps)` with N > 0
   (up to 24 with the English bundle: its 26 `[ControllerFont]` values minus Start and Back).
3. Walk to a weapon on the ground and empty a clip. This step is also the test of the unconfirmed
   point of list 1. If the pick-up prompt is drawn with the font character `X`, it now shows your
   `keybind_x` key (E by default); if the reload prompt is drawn with `` ` `` (RB), it shows your
   `keybind_right_shoulder` key (R). If either still shows the Xbox button, that prompt is a
   painted icon (or a string not covered here): write down which one and follow step 4 of the
   previous section.
4. In `auto`, plug a controller in: within a second the same prompts show the Xbox buttons again;
   unplug it and they return to keys. Rebind `keybind_x` in PC Options: the prompt follows.
5. Regressions to check: the menus' button bars, the tutorial panels, and the Controls screen
   still draw correctly; with `gow2_prompts = "controller"` nothing changes.
