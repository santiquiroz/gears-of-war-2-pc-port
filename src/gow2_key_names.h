// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Key names: the runtime's keybind_* names, Windows virtual keys, and the engine's own key names
// (the FNames of Engine.Keys: "SpaceBar", "LeftMouseButton", "One"...).
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace rex::glue::keynames {

// The engine key name of a Windows virtual key (VK_*), or nullptr when the game has none.
const char* EngineNameForVk(uint16_t vk);

// The engine key names a keybind_* key name stands for: "Space" -> {"SpaceBar"}, "Shift" ->
// {"LeftShift", "RightShift"}. Empty for names the game has no key for (PrintScreen, F13...).
std::vector<std::string> EngineNamesForBind(std::string_view sdk_name);

// One alternative of a keybind_* value with its modifier prefixes ("Shift+Up").
struct Chord {
  std::vector<std::string> engine_names;  // the key itself
  bool shift = false, control = false, alt = false;
};
// All alternatives of a keybind_* value ("F,V", "Semicolon,Space", "Shift+Up"), skipping the ones
// with no engine key.
std::vector<Chord> ParseChords(std::string_view value);

}  // namespace rex::glue::keynames
