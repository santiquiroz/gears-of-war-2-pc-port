// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors

#include "gow2_frame_stats.h"
#include "gow2_frame_stats_core.h"

#include "gow2_android_thermal.h"

#include <chrono>
#include <cstdint>
#include <mutex>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc.h>
#include <rex/runtime.h>
#include <rex/system/function_dispatcher.h>

REXCVAR_DEFINE_BOOL(gow2_frame_log, false, "debug",
                    "Log the game's frame statistics every five seconds (FRAMESTATS lines), counted at the "
                    "player's input step, which runs once per game frame in gameplay");

namespace rex::glue {

namespace {

// PlayerInput's ProcessInput and its vtable slot, the same pair src/gow2_mouse_aim.cpp hooks.
constexpr uint32_t kProcessInputFn = 0x827FC468;
constexpr uint32_t kProcessInputSlot = 0x821763D0;

PPCFunc* g_process_input = nullptr;

int64_t NowUs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

uint32_t LoadGuestU32(const uint8_t* base, uint32_t addr) {
  const uint8_t* p = base + addr;
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

// The input step moves between guest threads across level loads.
std::mutex g_window_mutex;
FrameStatsWindow g_window;

void LogFrameStats(const FrameStats& stats) {
#if defined(__ANDROID__)
  const auto thermal = DescribeGpuAndThermalState();
#else
  constexpr auto thermal = "";
#endif
  REXLOG_INFO("FRAMESTATS frames={} fps={:.2f} p50_ms={:.3f} p95_ms={:.3f} p99_ms={:.3f} "
              "over_33_ms={} over_50_ms={} over_100_ms={} {}",
              stats.frames, stats.average_fps, stats.p50_ms, stats.p95_ms, stats.p99_ms,
              stats.over_33_ms, stats.over_50_ms, stats.over_100_ms, thermal);
}

RollingFps g_rolling;

void CountFrame() {
  const int64_t now = NowUs();
  std::lock_guard<std::mutex> lock(g_window_mutex);
  g_rolling.RecordFrame(now);
  if (!REXCVAR_GET(gow2_frame_log)) return;
  const auto stats = g_window.RecordFrame(now);
  if (stats) LogFrameStats(*stats);
}

void ProcessInputHook(PPCContext& ctx, uint8_t* base) {
  CountFrame();
  g_process_input(ctx, base);
}

}  // namespace

std::optional<FrameReading> LatestFrameReading() {
  const int64_t now = NowUs();
  std::lock_guard<std::mutex> lock(g_window_mutex);
  return g_rolling.Latest(now);
}

void InstallFrameStats() {
  const bool log = REXCVAR_GET(gow2_frame_log);
  rex::Runtime* runtime = rex::Runtime::instance();
  rex::runtime::FunctionDispatcher* dispatcher =
      runtime ? runtime->function_dispatcher() : nullptr;
  uint8_t* base = (runtime && runtime->memory()) ? runtime->memory()->virtual_membase() : nullptr;
  if (!dispatcher || !base || LoadGuestU32(base, kProcessInputSlot) != kProcessInputFn) {
    if (log) REXLOG_WARN("FRAMESTATS frame log off: the input step was not found");
    return;
  }
  g_process_input = dispatcher->GetFunction(kProcessInputFn);
  if (!g_process_input || !dispatcher->SetFunction(kProcessInputFn, &ProcessInputHook)) {
    if (log) REXLOG_WARN("FRAMESTATS frame log off: the input step could not be hooked");
    return;
  }
  if (log) {
    REXLOG_INFO("FRAMESTATS frame log on (every {} s)", FrameStatsWindow::kWindowUs / 1'000'000);
  }
}

}  // namespace rex::glue
