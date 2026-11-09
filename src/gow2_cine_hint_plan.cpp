// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Pure timing and label decisions for the cinematic skip hint.
#include "gow2_cine_hint_plan.h"

namespace rex::glue::cine_hint {

Hint NextHint(const HintInputs& in, HintState* state) {
  if (!in.cinematic) {
    state->was_cinematic = false;
    return Hint::kNone;
  }
  if (!state->was_cinematic) {
    state->was_cinematic = true;
    state->started_ms = in.now_ms;
    state->baseline_input_ms = in.last_input_ms;
  }
  const bool recent_input =
      in.last_input_ms > state->baseline_input_ms &&
      (in.last_input_ms > in.now_ms || in.now_ms - in.last_input_ms < kShowAfterInputMs);
  if (in.host) {
    if (in.now_ms - state->started_ms < kShowAtStartMs || recent_input) return Hint::kSkip;
    return Hint::kNone;
  }
  return recent_input ? Hint::kHostOnly : Hint::kNone;
}

std::string_view PadKeyLabel(int device) {
  switch (device) {
    case 1: return "View";
    case 2: return "Share";
    case 3: return "Create";
    default: return "";
  }
}

}  // namespace rex::glue::cine_hint
