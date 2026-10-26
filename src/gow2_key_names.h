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

// GearUIScene_PathChoice reads LeftMouseButton as the left trigger's choice, so the engine gets the
// two mouse buttons swapped when the triggers hold them the other way round (RMB on LT, LMB on RT).
bool MouseButtonsFollowTriggers(std::string_view left_trigger_value,
                                std::string_view right_trigger_value);
// The engine name to use for a true key name: the two mouse buttons trade names when `swapped`.
std::string_view EngineMouseName(std::string_view engine_name, bool swapped);

}  // namespace rex::glue::keynames
