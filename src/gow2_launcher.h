// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Whether PC Options opens before the game loads (Windows; Android has its own launch screen).
#pragma once

#include <cstdint>

namespace rex::glue::launcher {

// A relaunch by Restart now or by the launcher's own Play carries gow2_wait_pid: straight into the game.
constexpr bool ShouldShowLauncher(bool launcher_cvar, int32_t wait_pid) {
  return launcher_cvar && wait_pid <= 0;
}

}  // namespace rex::glue::launcher
