// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Host menu state and access to the runtime's input and settings.

#include "gow2_host_ui.h"
#include "gow2_mouse_aim.h"
#include "gow2_test_input.h"
#include "gow2_xam_input_guard.h"

#include <algorithm>
#include <atomic>
#include <string>

#include <rex/cvar.h>
#include <rex/input/input.h>
#include <rex/input/input_system.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/ui_event.h>
#include <rex/ui/virtual_key.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace rex::glue {
namespace {

std::atomic<bool> g_host_ui_open{false};

float NormalizeStick(int16_t value) {
  return static_cast<float>(value) / (value < 0 ? 32768.0f : 32767.0f);
}

HostPad PadFromState(uint16_t buttons, int16_t lx, int16_t ly, int16_t rx, int16_t ry, uint8_t lt,
                     uint8_t rt) {
  return {true, buttons, NormalizeStick(lx), NormalizeStick(ly), NormalizeStick(rx), NormalizeStick(ry), lt, rt};
}

HostPad ReadRuntimePad(rex::Runtime* runtime) {
  rex::input::X_INPUT_STATE state{};
  X_RESULT result = X_ERROR_DEVICE_NOT_CONNECTED;
  auto* input = static_cast<rex::input::InputSystem*>(runtime->input_system());
  const auto read = [&] { result = input->GetState(0, &state); };
#ifdef _WIN32
  WithXamInputLock(read);
#else
  read();
#endif
  if (result != X_ERROR_SUCCESS) return {};
  return PadFromState(static_cast<uint16_t>(state.gamepad.buttons),
                      static_cast<int16_t>(state.gamepad.thumb_lx), static_cast<int16_t>(state.gamepad.thumb_ly),
                      static_cast<int16_t>(state.gamepad.thumb_rx), static_cast<int16_t>(state.gamepad.thumb_ry),
                      state.gamepad.left_trigger, state.gamepad.right_trigger);
}

#ifdef _WIN32
// XINPUT_STATE's layout; <Xinput.h> would #define XINPUT_DEVTYPE_GAMEPAD over the SDK's enum of that name.
struct XInputState {
  DWORD packet;
  WORD buttons;
  BYTE left_trigger, right_trigger;
  SHORT thumb_lx, thumb_ly, thumb_rx, thumb_ry;
};
static_assert(sizeof(XInputState) == 16);
using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XInputState*);

XInputGetStateFn LoadXInputGetState() {
  HMODULE module = LoadLibraryW(L"xinput1_4.dll");
  if (!module) module = LoadLibraryW(L"xinput9_1_0.dll");
  if (!module) return nullptr;
  return reinterpret_cast<XInputGetStateFn>(GetProcAddress(module, "XInputGetState"));
}

// The launcher runs before the runtime and its input system exist (Steam Deck players have only the pad).
HostPad ReadXInputPad() {
  static const XInputGetStateFn get_state = LoadXInputGetState();
  XInputState state{};
  if (!get_state || get_state(0, &state) != ERROR_SUCCESS) return {};
  return PadFromState(state.buttons, state.thumb_lx, state.thumb_ly, state.thumb_rx, state.thumb_ry,
                      state.left_trigger, state.right_trigger);
}
#endif

HostPad ReadDevicePad() {
  rex::Runtime* runtime = rex::Runtime::instance();
  if (runtime && runtime->input_system()) return ReadRuntimePad(runtime);
#ifdef _WIN32
  return ReadXInputPad();
#else
  return {};
#endif
}

}  // namespace

bool HostUiOpen() {
  return g_host_ui_open.load(std::memory_order_acquire);
}

void SetHostUiOpen(bool open) {
  g_host_ui_open.store(open, std::memory_order_release);
  if (open) DiscardNativeMouseInput();
}

HostPad ReadHostPad() {
  HostPad pad = ReadDevicePad();
  test_input::PadState scripted;
  if (test_input::CurrentPad(&scripted)) {
    pad.buttons |= scripted.buttons;
    if (scripted.lx != 0 || scripted.ly != 0) {
      pad.lx = NormalizeStick(scripted.lx);
      pad.ly = NormalizeStick(scripted.ly);
    }
    if (scripted.rx != 0 || scripted.ry != 0) {
      pad.rx = NormalizeStick(scripted.rx);
      pad.ry = NormalizeStick(scripted.ry);
    }
    // The template argument keeps windows.h's max macro out.
    pad.lt = std::max<uint8_t>(pad.lt, scripted.left_trigger);
    pad.rt = std::max<uint8_t>(pad.rt, scripted.right_trigger);
    pad.connected = true;
  }
  return pad;
}

void ToggleRuntimeSettings(rex::ui::Window* window) {
  if (!window) return;
  const std::string key = rex::cvar::Query<std::string>("bind_settings");
  const rex::ui::VirtualKey virtual_key = rex::ui::ParseVirtualKey(key);
  if (virtual_key == rex::ui::VirtualKey::kNone) {
    REXLOG_WARN("PCOPTIONS runtime settings: bind_settings '{}' is not a key", key);
    return;
  }
  rex::ui::KeyEvent event(window, virtual_key, 1, false, false, false, false, false);
  if (!rex::ui::ProcessKeyEvent(event)) {
    REXLOG_WARN("PCOPTIONS runtime settings: no bind answered {}", key);
  }
}

}  // namespace rex::glue
