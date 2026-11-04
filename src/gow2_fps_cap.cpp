// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Frame-rate limiter engine tick listener.
#include "gow2_fps_cap.h"

#include "gow2_engine_tick.h"

#ifdef _WIN32
#include <windows.h>
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#else
#include <time.h>
#endif

#include <chrono>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc.h>

REXCVAR_DEFINE_INT32(gow2_fps_cap, 0, "Display",
                     "Frame rate limit while the frame rate is unlocked (gow2_unlock_fps); 0 = no "
                     "limit. Takes effect at once.")
    .range(0, 1000);

namespace rex::glue {

void BulkSleepNanos(int64_t nanos) {
#ifdef _WIN32
  thread_local HANDLE timer = nullptr;
  thread_local bool tried = false;
  if (!tried) {
    tried = true;
    timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                   TIMER_ALL_ACCESS);
  }
  if (timer != nullptr) {
    LARGE_INTEGER due;
    due.QuadPart = -(nanos / 100);
    if (due.QuadPart == 0) return;
    if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE) &&
        WaitForSingleObject(timer, INFINITE) == WAIT_OBJECT_0) return;
  }
  const DWORD ms = static_cast<DWORD>(nanos / 1000000);
  if (ms > 0) Sleep(ms);
#else
  struct timespec req;
  req.tv_sec = nanos / 1000000000;
  req.tv_nsec = nanos % 1000000000;
  clock_nanosleep(CLOCK_MONOTONIC, 0, &req, nullptr);
#endif
}

namespace {

int64_t SteadyNowNanos() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void PreciseSleepNanos(int64_t nanos) {
  if (nanos <= 0) return;
  constexpr int64_t kSpin = 500000;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::nanoseconds(nanos);
  if (nanos > kSpin) BulkSleepNanos(nanos - kSpin);
  while (std::chrono::steady_clock::now() < deadline) {
  }
}

void FpsCapTick(const PPCContext& ctx, uint8_t* base) {
  (void)ctx;
  (void)base;
  static FramePacer pacer;
  static int logged_cap = 0;
  const int cap = EffectiveFpsCap(rex::cvar::Query<bool>("gow2_unlock_fps"), REXCVAR_GET(gow2_fps_cap));
  if (cap <= 0) {
    if (logged_cap != 0) {
      pacer.SleepNanos(0, 0);
      logged_cap = 0;
      REXLOG_INFO("FPSCAP off");
    }
    return;
  }
  if (cap != logged_cap) {
    logged_cap = cap;
    REXLOG_INFO("FPSCAP limit {} fps", cap);
  }
  PreciseSleepNanos(pacer.SleepNanos(SteadyNowNanos(), cap));
  pacer.RecordFrameStart(SteadyNowNanos());
}

[[maybe_unused]] const bool kFpsCapRegistered = (AddEngineTickListener(&FpsCapTick), true);

}  // namespace
}  // namespace rex::glue
