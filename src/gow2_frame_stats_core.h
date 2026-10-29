// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace rex::glue {

struct FrameStats {
  uint64_t frames = 0;
  double average_fps = 0;
  double p50_ms = 0;
  double p95_ms = 0;
  double p99_ms = 0;
  uint64_t over_33_ms = 0;
  uint64_t over_50_ms = 0;
  uint64_t over_100_ms = 0;
};

class FrameStatsWindow {
 public:
  static constexpr int64_t kWindowUs = 5'000'000;

  std::optional<FrameStats> RecordFrame(int64_t now_us) {
    if (!started_) {
      Restart(now_us);
      return std::nullopt;
    }
    durations_us_.push_back(now_us - last_us_);
    last_us_ = now_us;
    if (now_us - start_us_ < kWindowUs) return std::nullopt;
    const FrameStats stats = Summarize(now_us - start_us_);
    Restart(now_us);
    return stats;
  }

 private:
  static double PercentileMs(const std::vector<int64_t>& sorted_us, size_t percent) {
    const size_t rank = (sorted_us.size() * percent + 99) / 100;
    return static_cast<double>(sorted_us[rank - 1]) / 1000.0;
  }

  uint64_t CountOver(int64_t threshold_us) const {
    return static_cast<uint64_t>(std::count_if(
        durations_us_.begin(), durations_us_.end(),
        [threshold_us](int64_t duration_us) { return duration_us > threshold_us; }));
  }

  FrameStats Summarize(int64_t elapsed_us) {
    std::sort(durations_us_.begin(), durations_us_.end());
    return {static_cast<uint64_t>(durations_us_.size()),
            static_cast<double>(durations_us_.size()) * 1'000'000.0 / elapsed_us,
            PercentileMs(durations_us_, 50), PercentileMs(durations_us_, 95),
            PercentileMs(durations_us_, 99), CountOver(33'000), CountOver(50'000),
            CountOver(100'000)};
  }

  void Restart(int64_t now_us) {
    started_ = true;
    start_us_ = now_us;
    last_us_ = now_us;
    durations_us_.clear();
  }

  bool started_ = false;
  int64_t start_us_ = 0;
  int64_t last_us_ = 0;
  std::vector<int64_t> durations_us_;
};

struct FrameReading {
  double fps = 0;
  double frame_ms = 0;
};

class RollingFps {
 public:
  static constexpr int64_t kWindowUs = 1'000'000;
  static constexpr int64_t kStaleUs = 2'000'000;

  std::optional<FrameReading> RecordFrame(int64_t now_us) {
    // A pause (menus, loading) starts over, so it never shows as one slow second.
    if (!started_ || now_us - last_us_ > kStaleUs) {
      Restart(now_us);
      latest_.reset();
      return std::nullopt;
    }
    ++frames_;
    last_us_ = now_us;
    if (now_us - start_us_ < kWindowUs) return std::nullopt;
    latest_ = Summarize(now_us - start_us_);
    Restart(now_us);
    return latest_;
  }

  std::optional<FrameReading> Latest(int64_t now_us) const {
    if (!started_ || now_us - last_us_ > kStaleUs) return std::nullopt;
    return latest_;
  }

 private:
  FrameReading Summarize(int64_t elapsed_us) const {
    const double frames = static_cast<double>(frames_);
    return {frames * 1'000'000.0 / elapsed_us, elapsed_us / frames / 1000.0};
  }

  void Restart(int64_t now_us) {
    started_ = true;
    start_us_ = now_us;
    last_us_ = now_us;
    frames_ = 0;
  }

  bool started_ = false;
  int64_t start_us_ = 0;
  int64_t last_us_ = 0;
  uint64_t frames_ = 0;
  std::optional<FrameReading> latest_;
};

}  // namespace rex::glue
