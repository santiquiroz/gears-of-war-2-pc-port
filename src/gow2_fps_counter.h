// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
//
// Frame counter (gow2_show_fps): the game's frames per second and frame time from
// gow2_frame_stats.h, one line in a corner of the screen.
#pragma once

#include <memory>

namespace rex::ui {
class ImGuiDialog;
class ImGuiDrawer;
}  // namespace rex::ui

namespace rex::glue {

std::unique_ptr<rex::ui::ImGuiDialog> CreateFpsCounter(rex::ui::ImGuiDrawer* drawer);

}  // namespace rex::glue
