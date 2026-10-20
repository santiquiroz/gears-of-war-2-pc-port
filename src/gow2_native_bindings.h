// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// The engine's own key bindings for the keyboard (Engine.PlayerInput.Bindings in Xe-GearInput.ini),
// made from the keybind_* settings: every key of a keybind_* value runs the game's command for the
// pad button it stands for, the way the game's PC commands (GBA_MoveForward ...) were meant to be
// used. Pure functions, tested on the host.
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace rex::glue {

struct NativeBinding {
  std::string key;      // engine key name ("SpaceBar")
  std::string command;  // "GBA_MoveAction" or an inline command
  bool shift = false, control = false, alt = false;
};

// `get` reads a setting by name (keybind_a ...), "" when it has no value. `defaults` fills a
// missing value with the setting's default. Mouse wheel bindings are always added.
std::vector<NativeBinding> NativeKeyBindings(const std::function<std::string(const char*)>& get);

// The line for the bundle: (Name="SpaceBar",Command="GBA_MoveAction",Shift=False,...).
std::string BindingIniValue(const NativeBinding& binding);

// The engine key names the bindings use, for removing the game's own debug bindings of them.
std::vector<std::string> NativeBindingKeys(const std::vector<NativeBinding>& bindings);

// Live rebinding. The bundle's bindings (`baked`) are fixed when the game starts, so a key changed
// later in the options is routed instead: the engine key to send for `key` pressed with the
// modifiers held is a key the bundle binds to the same command as `current` (the settings now).
// A key the bundle binds but `current` no longer does sends nothing; a key neither knows passes
// through. With no `baked` the key passes through.
// shortcut: the carrier must be a bare key of the bundle; a command the bundle binds only with a
// modifier cannot be moved to another key until the next start.
std::vector<std::string> RouteKey(const std::vector<NativeBinding>& baked,
                                  const std::vector<NativeBinding>& current, const std::string& key,
                                  bool shift, bool control, bool alt);

}  // namespace rex::glue
