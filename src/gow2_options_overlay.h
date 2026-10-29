// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// The persistent PC Options dialog and its entry points.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include "gow2_options_values.h"

struct ImFontAtlas;
namespace rex::ui { class ImGuiDialog; class ImGuiDrawer; }

namespace rex::glue::options {
enum class OpenSource : uint8_t { kShortcut, kChord, kGear, kGameMenu };
void AddOptionsFonts(ImFontAtlas* atlas);
std::unique_ptr<rex::ui::ImGuiDialog> CreateOptionsOverlay(
    rex::ui::ImGuiDrawer* drawer, const GamePaths& paths, bool touch_controls);
void TogglePcOptions(OpenSource source);
// Before the runtime exists: full-screen PC Options whose Back is Play. False when there is no overlay.
bool OpenPcOptionsLauncher(std::function<void()> play, std::function<void()> quit);
void UpdatePcOptionsPaths(const GamePaths& paths);
void ClosePcOptionsForShutdown();
}
