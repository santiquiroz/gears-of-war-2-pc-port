// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// The game's own "new item unlocked" / progress toast (UI scene GearUIScene_Toast) stays on screen for ever when
// the picture is wider than 16:9, and even at 16:9 it lingers for about seven seconds. Its animation slides the
// panel in, holds it and slides it out, with every position in pixels of the 1280-wide design: at 1680 wide the
// slide-out ends at x = 1280, still on screen, and the panel stays there until the next toast replaces it.
//
// This keeps the toast for gow2_toast_seconds in total (default 3, with a short fade at the end) and then hides
// the whole scene through its opacity until the next toast starts. 0 leaves the game's own timing alone.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

#include <rex/cvar.h>
#include <rex/logging.h>

#include "gow2_dev_guest_calls.h"
#include "gow2_engine_tick.h"
#include "gow2_guest.h"
#include "gow2_widescreen.h"

REXCVAR_DEFINE_DOUBLE(gow2_toast_seconds, 3.0, "Display",
                      "How long the game's own unlock / progress toast stays on screen in total, in seconds "
                      "(0.5 .. 30), fade included. 0 keeps the game's own timing, which leaves the toast on "
                      "screen for ever in ultrawide.");

namespace rex::glue {
namespace {

// Offsets inside GearUIScene_Toast of this exact build of the game (found by diffing the scene's memory while a
// toast played): the three slot-free bytes (1 = free), the number of active slots, and UIScreenObject::Opacity.
constexpr uint32_t kSlotFree = 1568;
constexpr uint32_t kActiveSlots = 1572;
constexpr uint32_t kOpacity = 192;
constexpr float kFadeSeconds = 0.4f;
constexpr float kFadeInSeconds = 0.25f;  // hides the slide-in starting at x = 1280 in the middle of an ultrawide picture

struct ToastState {
  uint32_t scene = 0;
  uint32_t counter = 0;
  int last_busy = 0;
  std::chrono::steady_clock::time_point started;
  float written = -1.0f;
};

void Apply(uint8_t* base, ToastState& st, float limit) {
  int busy = 0;
  for (uint32_t i = 0; i < 3; ++i) {
    const uint8_t free_byte = guest::LoadU8(base, st.scene + kSlotFree + i);
    if (free_byte > 1) return;  // not the layout this was measured on: leave the toast alone
    busy += free_byte == 0;
  }
  if (guest::LoadU32(base, st.scene + kActiveSlots) > 3) return;
  const auto now = std::chrono::steady_clock::now();
  if (busy > st.last_busy) st.started = now;  // a new toast began
  st.last_busy = busy;
  float target = 0.0f;
  if (busy > 0) {
    const float age = std::chrono::duration<float>(now - st.started).count();
    const float fade_from = std::max(0.2f, limit - kFadeSeconds);
    target = age < fade_from ? std::min(1.0f, age / kFadeInSeconds) : std::clamp((limit - age) / kFadeSeconds, 0.0f, 1.0f);
  }
  if (target == st.written) return;
  uint32_t bits;
  std::memcpy(&bits, &target, 4);
  guest::StoreU32(base, st.scene + kOpacity, bits);
  st.written = target;
}

void ToastTick(const PPCContext& ctx, uint8_t* base) {
  (void)ctx;
  static ToastState st;
  const float limit = float(std::clamp(REXCVAR_GET(gow2_toast_seconds), 0.0, 30.0));
  if (st.scene == 0 || (st.counter % 120) == 0) {
    const uint32_t found = FindGuestObject(base, "GearUIScene_Toast", true);
    if (found != st.scene) st.written = -1.0f;
    st.scene = found;
  }
  ++st.counter;
  if (st.scene == 0 || limit <= 0.0f) return;
  auto body = [&] { Apply(base, st, limit); };
  if (!guest_call::Guarded(body)) st.scene = 0;
}

[[maybe_unused]] const bool kToastRegistered = (AddEngineTickListener(&ToastTick), true);

}  // namespace
}  // namespace rex::glue
