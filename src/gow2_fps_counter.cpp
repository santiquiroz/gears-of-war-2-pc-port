// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors

#include "gow2_fps_counter.h"
#include "gow2_frame_stats.h"

#include <cfloat>
#include <cstdio>

#include <imgui.h>

#include <rex/cvar.h>
#include <rex/platform.h>
#include <rex/runtime.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>

REXCVAR_DEFINE_BOOL(gow2_show_fps, false, "UI", "Show the game's frame rate in a corner");

namespace rex::glue {

namespace {

constexpr float kInset = 0.015f;
constexpr float kTextHeight = 0.022f;
constexpr ImU32 kTextColour = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kShadowColour = IM_COL32(0, 0, 0, 255);

float TextLeft(const ImGuiIO& io, float text_width) {
#if REX_PLATFORM_ANDROID
  // The touch controls' LB button sits in the top-left corner.
  return (io.DisplaySize.x - text_width) * 0.5f;
#else
  (void)text_width;
  return io.DisplaySize.x * kInset;
#endif
}

void DrawReading(const ImGuiIO& io, const FrameReading& reading) {
  char text[48];
  std::snprintf(text, sizeof(text), "%.0f fps  %.1f ms", reading.fps, reading.frame_ms);
  ImFont* font = ImGui::GetFont();
  const float size = io.DisplaySize.y * kTextHeight;
  const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
  const ImVec2 pos(TextLeft(io, extent.x), io.DisplaySize.y * kInset);
  ImDrawList* list = ImGui::GetForegroundDrawList();
  list->AddText(font, size, ImVec2(pos.x + 1.0f, pos.y + 1.0f), kShadowColour, text);
  list->AddText(font, size, pos, kTextColour, text);
}

class FpsCounter final : public rex::ui::ImGuiDialog {
 public:
  explicit FpsCounter(rex::ui::ImGuiDrawer* drawer) : ImGuiDialog(drawer) {}

#if REX_PLATFORM_ANDROID
  // Redrawn with each guest frame, like the touch controls.
  bool WantsContinuousPaint() const override { return false; }
#endif

 protected:
  void OnDraw(ImGuiIO& io) override {
    // The launcher draws before the runtime exists.
    if (!rex::Runtime::instance() || !REXCVAR_GET(gow2_show_fps)) return;
    if (io.DisplaySize.y <= 0) return;
    const auto reading = LatestFrameReading();
    if (reading) DrawReading(io, *reading);
  }
};

}  // namespace

std::unique_ptr<rex::ui::ImGuiDialog> CreateFpsCounter(rex::ui::ImGuiDrawer* drawer) {
  if (!drawer) return nullptr;
  return std::make_unique<FpsCounter>(drawer);
}

}  // namespace rex::glue
