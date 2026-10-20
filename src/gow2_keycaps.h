// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Keycap prompts: which characters of the game's button font show a keybind. The font and its
// two texture pages are patched by tools/keycaps/pack.py (cell table in gow2_keycap_layout.inc);
// this file only turns a keybind_* setting into characters.
#pragma once

#include <functional>
#include <string>
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

}  // namespace rex::glue::keycap
