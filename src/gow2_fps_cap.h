// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Frame-rate limiter pacing state (pure, no SDK).
#pragma once

#include <cmath>
#include <cstdint>

namespace rex::glue {

// The retail game runs at 30 fps; its own cap rests on the guest vblank, which vsync = false
// removes, so with the frame rate locked the limiter keeps 30 fps itself.
inline constexpr int kLockedFps = 30;

inline int CapFromRefreshRate(double refresh_hz) {
  if (!std::isfinite(refresh_hz) || refresh_hz <= 3.0) return 0;
  return static_cast<int>(std::floor(refresh_hz)) - 3;
}

inline constexpr int EffectiveFpsCap(bool unlocked, int unlocked_cap, int refresh_cap = 0,
                                     bool frame_generation = false) {
  if (!unlocked) return kLockedFps;
  if (unlocked_cap != -1) return unlocked_cap;
  return frame_generation ? refresh_cap / 2 : refresh_cap;
}

class FramePacer {
 public:
  int64_t SleepNanos(int64_t now_ns, int cap) {
    if (cap <= 0) {
      has_deadline_ = false;
      last_cap_ = 0;
      return 0;
    }
    const int64_t period = PeriodNanos(cap);
    if (!has_deadline_ || cap != last_cap_) {
      last_cap_ = cap;
      deadline_ = now_ns + period;
      has_deadline_ = true;
      return 0;
    }
    if (now_ns < deadline_) {
      const int64_t sleep = deadline_ - now_ns;
      deadline_ += period;
      return sleep;
    }
    deadline_ = now_ns + period;
    return 0;
  }

  void RecordFrameStart(int64_t now_ns) {
    if (has_deadline_) deadline_ = now_ns + PeriodNanos(last_cap_);
  }

 private:
  static int64_t PeriodNanos(int cap) { return (1000000000LL + cap - 1) / cap; }

  int64_t deadline_ = 0;
  int last_cap_ = 0;
  bool has_deadline_ = false;
};

void BulkSleepNanos(int64_t nanos);

}  // namespace rex::glue
