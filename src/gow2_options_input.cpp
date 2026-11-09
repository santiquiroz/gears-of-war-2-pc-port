// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Implements controller repeat, button edges, and the menu toggle chord.
#include "gow2_options_input.h"

#include <algorithm>

namespace rex::glue::options {
namespace {

constexpr uint16_t kBack = 0x0020;
constexpr uint16_t kStart = 0x0010;
constexpr int64_t kQuitHoldMs = 700;
constexpr std::array<Command, 4> kDirections{
    Command::kUp, Command::kDown, Command::kLeft, Command::kRight};

bool HasButton(const PadSample& pad, uint16_t bit) {
  return (pad.buttons & bit) != 0;
}

bool HasChord(const PadSample& pad) {
  return HasButton(pad, kBack) && HasButton(pad, kStart);
}

std::array<bool, 4> HeldDirections(const PadSample& pad) {
  return {HasButton(pad, 0x0001) || pad.ly > 0.5f,
          HasButton(pad, 0x0002) || pad.ly < -0.5f,
          HasButton(pad, 0x0004) || pad.lx < -0.5f,
          HasButton(pad, 0x0008) || pad.lx > 0.5f};
}

uint64_t Elapsed(int64_t now_ms, int64_t start_ms) {
  return static_cast<uint64_t>(now_ms) - static_cast<uint64_t>(start_ms);
}

void AppendButtonCommands(std::vector<Command>& commands, uint16_t pressed) {
  constexpr std::array<uint16_t, 6> bits{0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200};
  constexpr std::array<Command, 6> actions{
      Command::kAccept, Command::kBack, Command::kResetRow,
      Command::kSave, Command::kTabPrev, Command::kTabNext};
  for (std::size_t i = 0; i < bits.size(); ++i) {
    if ((pressed & bits[i]) != 0) commands.push_back(actions[i]);
  }
}

}  // namespace

bool RepeatGate::Update(bool held, int64_t now_ms) {
  if (!held) {
    held_ = false;
    return false;
  }
  if (!held_) {
    held_ = true;
    pressed_ms_ = previous_ms_ = now_ms;
    repeat_index_ = 0;
    return true;
  }
  if (now_ms < previous_ms_) {
    pressed_ms_ = now_ms;
    repeat_index_ = 0;
  }
  previous_ms_ = now_ms;
  const uint64_t elapsed = Elapsed(now_ms, pressed_ms_);
  if (elapsed < 400) return false;
  const uint64_t index = 1 + (elapsed - 400) / 90;
  if (index == repeat_index_) return false;
  repeat_index_ = index;
  return true;
}

bool ChordGate::Update(bool a, bool b, int64_t now_ms) {
  if (!a && !b) {
    active_ = latched_ = false;
    return false;
  }
  if (latched_) return false;
  if (!a || !b) {
    active_ = false;
    return false;
  }
  if (!active_ || now_ms < previous_ms_) {
    active_ = true;
    started_ms_ = now_ms;
  }
  previous_ms_ = now_ms;
  if (Elapsed(now_ms, started_ms_) < 300) return false;
  latched_ = true;
  return true;
}

std::vector<Command> PadReader::Update(const PadSample& pad, int64_t now_ms) {
  std::vector<Command> commands;
  const bool view = HasButton(pad, kBack);
  const bool start = HasButton(pad, kStart);
  if (!view) {
    quit_holding_ = quit_latched_ = quit_cancelled_ = false;
  } else {
    if (!quit_holding_ && !quit_latched_) {
      quit_holding_ = true;
      quit_started_ms_ = now_ms;
      quit_cancelled_ = false;
    }
    if (start) quit_cancelled_ = true;
    if (quit_holding_ && !quit_latched_ && !quit_cancelled_ &&
        now_ms >= quit_started_ms_ && Elapsed(now_ms, quit_started_ms_) >= kQuitHoldMs) {
      commands.push_back(Command::kQuit);
      quit_latched_ = true;
      quit_holding_ = false;
    }
  }
  const auto held = HeldDirections(pad);
  for (std::size_t i = 0; i < held.size(); ++i) {
    if (!held[i]) blocked_directions_[i] = false;
    const bool fire = directions_[i].Update(held[i] && !blocked_directions_[i], now_ms);
    if (fire) commands.push_back(kDirections[i]);
  }
  const uint16_t pressed = pad.buttons & static_cast<uint16_t>(~previous_buttons_);
  previous_buttons_ = pad.buttons;
  AppendButtonCommands(commands, pressed);
  if (HasChord(pad)) commands.clear();
  return commands;
}

bool PadReader::ToggleChord(const PadSample& pad, int64_t now_ms) {
  return chord_.Update(HasButton(pad, kBack), HasButton(pad, kStart), now_ms);
}

void PadReader::Reset(const PadSample& pad) {
  previous_buttons_ = pad.buttons;
  directions_ = {};
  blocked_directions_ = HeldDirections(pad);
  chord_ = {};
  chord_.latched_ = HasButton(pad, kBack) || HasButton(pad, kStart);
  quit_holding_ = false;
  quit_latched_ = HasButton(pad, kBack);
  quit_cancelled_ = false;
  quit_started_ms_ = 0;
}

float PadReader::QuitHoldProgress(int64_t now_ms) const {
  if (!quit_holding_ || quit_latched_ || quit_cancelled_ || now_ms < quit_started_ms_) return 0.f;
  return std::min(1.f, static_cast<float>(Elapsed(now_ms, quit_started_ms_)) /
      static_cast<float>(kQuitHoldMs));
}

}  // namespace rex::glue::options
