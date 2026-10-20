// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_native_bindings.h"

#include <algorithm>

#include "gow2_key_names.h"

namespace rex::glue {
namespace {

struct Action {
  const char* setting;
  const char* fallback;  // the value the port ships (config/gow2.toml, SDK defaults)
  const char* command;
};

// Pad button or stick direction -> the game's command (Engine.PlayerInput.Bindings of the pad keys).
constexpr Action kActions[] = {
    {"keybind_a", "Space", "GBA_MoveAction"},
    {"keybind_b", "F,V", "GBA_Melee"},
    {"keybind_x", "E", "GBA_Pickup"},
    {"keybind_y", "Q", "GBA_POI"},
    {"keybind_left_shoulder", "G", "GBA_Taccom"},
    {"keybind_right_shoulder", "R", "GBA_Reload"},
    {"keybind_left_trigger", "RMB", "GBA_Target"},
    {"keybind_right_trigger", "LMB", "GBA_Shoot"},
    {"keybind_lstick_press", "C", "GBA_Crouch"},
    {"keybind_rstick_press", "MMB,Z", "GBA_Zoom"},
    {"keybind_dpad_up", "1", "GBA_Grenades"},
    {"keybind_dpad_right", "2", "GBA_Weap1"},
    {"keybind_dpad_down", "3", "GBA_Pistols"},
    {"keybind_dpad_left", "4", "GBA_Weap2"},
    {"keybind_start", "Return,Escape", "GBA_Pause"},
    {"keybind_back", "Tab", "GBA_Scoreboard"},
    // The stick directions: the game's digital PC commands, which have no dead zone.
    {"keybind_lstick_up", "W", "GBA_MoveForward"},
    {"keybind_lstick_down", "S", "GBA_MoveBackward"},
    {"keybind_lstick_left", "A", "GBA_StrafeLeft"},
    {"keybind_lstick_right", "D", "GBA_StrafeRight"},
    {"keybind_rstick_left", "Left", "GBA_TurnLeft"},
    {"keybind_rstick_right", "Right", "GBA_TurnRight"},
    {"keybind_rstick_up", "Up", "Axis aLookup Speed=0.75"},
    {"keybind_rstick_down", "Down", "Axis aLookup Speed=-0.75"},
};

}  // namespace

std::vector<NativeBinding> NativeKeyBindings(const std::function<std::string(const char*)>& get) {
  std::vector<NativeBinding> out;
  for (const Action& action : kActions) {
    std::string value = get(action.setting);
    if (value.empty()) value = action.fallback;
    for (const keynames::Chord& chord : keynames::ParseChords(value))
      for (const std::string& name : chord.engine_names)
        out.push_back({name, action.command, chord.shift, chord.control, chord.alt});
  }
  out.push_back({"MouseScrollUp", "GBA_PrevWeapon", false, false, false});
  out.push_back({"MouseScrollDown", "GBA_NextWeapon", false, false, false});
  return out;
}

std::string BindingIniValue(const NativeBinding& b) {
  std::string v = "(Name=\"" + b.key + "\",Command=\"" + b.command + "\"";
  if (b.shift || b.control || b.alt) {
    v += std::string(",Control=") + (b.control ? "True" : "False") + ",Shift=" +
         (b.shift ? "True" : "False") + ",Alt=" + (b.alt ? "True" : "False");
  }
  return v + ")";
}

std::vector<std::string> RouteKey(const std::vector<NativeBinding>& baked,
                                  const std::vector<NativeBinding>& current, const std::string& key,
                                  bool shift, bool control, bool alt) {
  if (baked.empty()) return {key};
  const auto same = [](const NativeBinding& a, const NativeBinding& b) {
    return a.key == b.key && a.command == b.command && a.shift == b.shift &&
           a.control == b.control && a.alt == b.alt;
  };
  std::vector<std::string> out;
  const auto add = [&](const std::string& name) {
    if (std::find(out.begin(), out.end(), name) == out.end()) out.push_back(name);
  };
  bool hit = false;
  for (const NativeBinding& now : current) {
    if (now.key != key || (now.shift && !shift) || (now.control && !control) || (now.alt && !alt))
      continue;
    hit = true;
    if (std::any_of(baked.begin(), baked.end(), [&](const NativeBinding& b) { return same(b, now); })) {
      add(key);
      continue;
    }
    const auto carrier = std::find_if(baked.begin(), baked.end(), [&](const NativeBinding& b) {
      return b.command == now.command && !b.shift && !b.control && !b.alt;
    });
    if (carrier != baked.end()) add(carrier->key);
  }
  if (!hit && std::none_of(baked.begin(), baked.end(), [&](const NativeBinding& b) { return b.key == key; }))
    add(key);
  return out;
}

std::vector<std::string> NativeBindingKeys(const std::vector<NativeBinding>& bindings) {
  std::vector<std::string> keys;
  for (const NativeBinding& b : bindings)
    if (std::find(keys.begin(), keys.end(), b.key) == keys.end()) keys.push_back(b.key);
  return keys;
}

}  // namespace rex::glue
