// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Whether the SDK's native renderer (gpu_native_surfaces and gpu_native_resolves) is on: header only so a
// host test can check the decision (src/gow2_gpu_pick.cpp reads the cvars and the GPU).
#pragma once

#include <string>

#include "gow2_widescreen_math.h"

namespace rex::glue::native_renderer {

// "on" and "off" name it; anything else is "auto", which turns it on only on Android, unless the player chose a
// render target path. On Windows auto stays emulated: native smears on an RX 7900 XT, an RX 9070 XT and an
// RTX 3070 Ti (issue #26).
inline bool UseNativeRenderer(const std::string& mode, bool path_set_by_player,
                              [[maybe_unused]] const std::string& gpu_name, bool android) {
  if (mode == "on") return true;
  if (mode == "off") return false;
  return android && !path_set_by_player;
}

}  // namespace rex::glue::native_renderer
