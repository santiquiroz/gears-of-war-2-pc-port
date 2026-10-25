// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Which device the button prompts show: header only so a host test can check the decision
// (src/gow2_prompt_swap.cpp reads the cvar and the input times).
#pragma once

#include <cstdint>
#include <string>

namespace rex::glue::prompt_device {

enum class Device { kKeyboard, kXbox, kPlayStation4, kPlayStation5 };

// sdl_type is the controller's SDL_GamepadType: PS3 = 4, PS4 = 5, PS5 = 6. Unknown pads show Xbox buttons.
inline Device PadDevice(int sdl_type) {
  if (sdl_type == 6) return Device::kPlayStation5;
  if (sdl_type == 4 || sdl_type == 5) return Device::kPlayStation4;
  return Device::kXbox;
}

struct Inputs {
  std::string mode;  // gow2_prompts
  uint64_t pad_ms = 0;  // host uptime of the last controller input, 0 = none yet
  int pad_type = 0;
  uint64_t kbm_ms = 0;  // host uptime of the last keyboard or mouse input, 0 = none yet
  bool pad_connected = false;
};

// keyboard, controller and playstation are fixed; anything else follows the device used last.
inline Device Choose(const Inputs& in) {
  if (in.mode == "keyboard") return Device::kKeyboard;
  if (in.mode == "controller") return Device::kXbox;
  if (in.mode == "playstation")
    return in.pad_type == 4 || in.pad_type == 5 ? Device::kPlayStation4 : Device::kPlayStation5;
  if (in.pad_ms == 0 && in.kbm_ms == 0) return in.pad_connected ? PadDevice(in.pad_type) : Device::kKeyboard;
  if (in.pad_ms > 0 && in.pad_ms >= in.kbm_ms) return PadDevice(in.pad_type);
  return Device::kKeyboard;
}

// The texture pack variant (replace_<name>) holding the font page with these buttons; "" = none.
inline const char* VariantFor(Device d) {
  if (d == Device::kPlayStation4) return "ps4";
  if (d == Device::kPlayStation5) return "ps5";
  return "";
}

inline bool IsPlayStation(Device d) { return d == Device::kPlayStation4 || d == Device::kPlayStation5; }

}  // namespace rex::glue::prompt_device
