// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Converts controller samples into menu commands without platform dependencies.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "gow2_options_model.h"

namespace rex::glue::options {

struct PadSample {
  uint16_t buttons = 0;
  float lx = 0;
  float ly = 0;
};

class RepeatGate {
 public:
  bool Update(bool held, int64_t now_ms);

 private:
  bool held_ = false;
  int64_t pressed_ms_ = 0;
  int64_t previous_ms_ = 0;
  uint64_t repeat_index_ = 0;
};

class ChordGate {
 public:
  bool Update(bool a, bool b, int64_t now_ms);

 private:
  friend class PadReader;
  bool active_ = false;
  bool latched_ = false;
  int64_t started_ms_ = 0;
  int64_t previous_ms_ = 0;
};

class PadReader {
 public:
  std::vector<Command> Update(const PadSample& pad, int64_t now_ms);
  bool ToggleChord(const PadSample& pad, int64_t now_ms);
  void Reset(const PadSample& pad);
  float QuitHoldProgress(int64_t now_ms) const;

 private:
  std::array<RepeatGate, 4> directions_{};
  std::array<bool, 4> blocked_directions_{};
  uint16_t previous_buttons_ = 0;
  bool quit_holding_ = false;
  bool quit_latched_ = false;
  bool quit_cancelled_ = false;
  int64_t quit_started_ms_ = 0;
  ChordGate chord_;
};

}  // namespace rex::glue::options
