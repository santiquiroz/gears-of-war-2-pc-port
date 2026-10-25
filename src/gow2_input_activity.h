// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// When the player last used the keyboard or mouse and the controller, in host uptime milliseconds
// (rex::chrono::Clock::QueryHostUptimeMillis), so the button prompts can follow the device used last.
#pragma once

#include <cstdint>

namespace rex::ui {
class Window;
}

namespace rex::glue {

void NoteKeyboardMouseActivity();
uint64_t LastKeyboardMouseActivityMs();  // 0 = none yet
// The last physical controller button or stick past its dead zone (0 = none yet), from the runtime;
// *sdl_type gets that controller's SDL_GamepadType (0 unknown). 0 and 0 with an older rexruntime.dll.
uint64_t LastPadActivityMs(int* sdl_type);
// Notes key presses, mouse buttons, the wheel and real mouse movement on the window. Null: nothing.
void InstallInputActivityWatch(rex::ui::Window* window);

}  // namespace rex::glue
