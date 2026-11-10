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
#include <string>

#include <SDL3/SDL.h>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc.h>
#include <rex/runtime.h>
#include <rex/ui/window.h>

REXCVAR_DEFINE_INT32(gow2_fps_cap, 0, "Display",
                     "Frame rate limit while the frame rate is unlocked (gow2_unlock_fps); -1 = monitor "
                     "refresh rate minus 3 for VRR (half of it with frame generation), 0 = no "
                     "limit. Takes effect at once.")
    .range(-1, 1000);

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

int DisplayRefreshCap() {
  SDL_DisplayID display = 0;
#ifdef _WIN32
  auto* runtime = rex::Runtime::instance();
  auto* game_window = runtime ? runtime->display_window() : nullptr;
  const void* native_handle = game_window ? game_window->GetNativeWindowHandle() : nullptr;
  if (native_handle) {
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    for (int i = 0; windows && i < count; ++i) {
      const auto properties = SDL_GetWindowProperties(windows[i]);
      if (SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr) != native_handle)
        continue;
      display = SDL_GetDisplayForWindow(windows[i]);
      break;
    }
    SDL_free(windows);
  }
#endif
  if (display == 0) {
    if (SDL_Window* window = SDL_GetKeyboardFocus()) display = SDL_GetDisplayForWindow(window);
  }
  if (display == 0) display = SDL_GetPrimaryDisplay();
  const SDL_DisplayMode* mode = display == 0 ? nullptr : SDL_GetCurrentDisplayMode(display);
  return mode ? CapFromRefreshRate(mode->refresh_rate) : 0;
}

bool FrameGenerationEnabled() {
  return rex::cvar::Query<std::string>("gow2_frame_generation") == "fsr3" &&
         rex::cvar::Query<std::string>("gow2_upscaler") != "off";
}

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
  static int selected_cap = 0;
  static int refresh_cap = 0;
  static bool frame_generation = false;
  static bool refresh_initialized = false;
  static bool unknown_refresh_logged = false;
  const int selected = REXCVAR_GET(gow2_fps_cap);
  if (!refresh_initialized || selected != selected_cap) {
    selected_cap = selected;
    refresh_initialized = true;
    frame_generation = selected == -1 && FrameGenerationEnabled();
    if (selected == -1) {
      refresh_cap = DisplayRefreshCap();
      if (refresh_cap == 0 && !unknown_refresh_logged) {
        REXLOG_WARN("FPSCAP monitor refresh rate is unknown; VRR cap is off");
        unknown_refresh_logged = true;
      }
    }
  }
  const bool unlocked = rex::cvar::Query<bool>("gow2_unlock_fps");
  const int cap = EffectiveFpsCap(unlocked, selected_cap, refresh_cap, frame_generation);
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
    if (unlocked && selected_cap == -1 && frame_generation)
      REXLOG_INFO("FPSCAP limit {} fps (frame generation)", cap);
    else
      REXLOG_INFO("FPSCAP limit {} fps", cap);
  }
  PreciseSleepNanos(pacer.SleepNanos(SteadyNowNanos(), cap));
  pacer.RecordFrameStart(SteadyNowNanos());
}

[[maybe_unused]] const bool kFpsCapRegistered = (AddEngineTickListener(&FpsCapTick), true);

}  // namespace
}  // namespace rex::glue
