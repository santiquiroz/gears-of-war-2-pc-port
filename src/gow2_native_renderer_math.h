// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Whether the SDK's native renderer (gpu_native_surfaces and gpu_native_resolves) is on: header only so a
// host test can check the decision (src/gow2_gpu_pick.cpp reads the cvars and the GPU).
#pragma once

#include <string>

#include "gow2_widescreen_math.h"

namespace rex::glue::native_renderer {

// "on" and "off" name it; anything else is "auto", which turns it on for AMD unless the player chose a render
// target path.
inline bool UseNativeRenderer(const std::string& mode, bool path_set_by_player, const std::string& gpu_name) {
  if (mode == "on") return true;
  if (mode == "off") return false;
  return !path_set_by_player &&
         (widescreen::GpuNameHas(gpu_name, "AMD") || widescreen::GpuNameHas(gpu_name, "RADEON"));
}

}  // namespace rex::glue::native_renderer
