// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
//
// The game's own frame rate: the player's input step (once per game frame in gameplay) is counted
// for the frame counter (gow2_show_fps), and with gow2_frame_log on statistics are logged every five
// seconds as a FRAMESTATS line. Presented frames are no measure on a phone: while an ImGui overlay is
// up the presenter repaints at the display's refresh rate.
#pragma once

#include <optional>

#include "gow2_frame_stats_core.h"

namespace rex::glue {

void InstallFrameStats();
std::optional<FrameReading> LatestFrameReading();

}  // namespace rex::glue
