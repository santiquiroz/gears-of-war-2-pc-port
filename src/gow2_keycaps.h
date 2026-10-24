// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Keycap prompts: which characters of the game's button font show a keybind. The font and its
// two texture pages are patched by tools/keycaps/pack.py (cell table in gow2_keycap_layout.inc);
// this file only turns a keybind_* setting into characters.
#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace rex::glue::keycap {

// The first alternative of a keybind list ("Semicolon,Space" -> Space), with its modifier
// prefixes ("Shift+Up" -> {Shift}, Up). Names are the runtime's key names; "Ctrl" is "Control".
struct Binding {
  std::vector<std::string> modifiers;
  std::string key;
};
Binding ParseBinding(const std::string& list);

// The character that shows a key or glyph id ("W", "Space", "CL_WASD"), or 0 when it has no cap.
char16_t CharFor(const std::string& id);

// `<Fonts:Warfare_HUD.Xbox360_18pt>` + characters + `<Fonts:/>`.
std::u16string Markup(const std::u16string& chars);

// What one [ControllerFont] entry shows with keycaps. `full` has the modifiers too; `brief` is
// the main key alone for strings with no room for more. Both empty: no cap, use a text label.
struct Prompt {
  std::u16string full, brief;
};
// `get` reads a setting by name (keybind_a, mnk_mouse ...), "" when it has no value.
Prompt PromptFor(const std::string& controller_key, const std::function<std::string(const char*)>& get);

// The names [ControllerFont] entries the port gives a cap to.
const std::vector<std::string>& ControllerKeys();

// The one character of a one-glyph markup ("<Fonts:...>X<Fonts:/>" -> 'X'), 0 for anything else.
char16_t SingleGlyph(const std::u16string& markup);

// Pad glyphs that strings draw without going through [ControllerFont]: the HUD text of
// GearGame.int (GearHud_Base, UIScene_Spectator, TUTORIALS ...) and string literals in the
// script of GearGame.u. `pad` is the character of a one-glyph [ControllerFont] value, `cap` the
// keycap character of the same entry's binding; pointing pad's font cell at cap's makes every
// such string show the key. A pad character two entries would send to different caps is left out.
struct GlyphSwap {
  char16_t pad, cap;
};
std::vector<GlyphSwap> PadGlyphSwaps(const std::vector<std::pair<std::string, std::u16string>>& originals,
                                     const std::function<Prompt(const std::string&)>& prompt_for);

}  // namespace rex::glue::keycap
