// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Pure timing and label decisions for the cinematic skip hint.
#pragma once

#include <cstdint>
#include <string_view>

namespace rex::glue::cine_hint {

enum class Hint : uint8_t { kNone, kSkip, kHostOnly };

struct HintInputs {
  bool cinematic = false;
  bool host = false;
  uint64_t now_ms = 0;
  uint64_t last_input_ms = 0;
};

struct HintState {
  bool was_cinematic = false;
  uint64_t started_ms = 0;
  uint64_t baseline_input_ms = 0;
};

constexpr uint64_t kShowAtStartMs = 6000;
constexpr uint64_t kShowAfterInputMs = 4000;

Hint NextHint(const HintInputs& in, HintState* state);
std::string_view PadKeyLabel(int device);

}  // namespace rex::glue::cine_hint
