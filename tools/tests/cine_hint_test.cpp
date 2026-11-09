// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Tests the cinematic hint timing and controller labels without game data.
#include "gow2_cine_hint_plan.h"

#include <cstdio>

namespace {

using rex::glue::cine_hint::Hint;
using rex::glue::cine_hint::HintInputs;
using rex::glue::cine_hint::HintState;
using rex::glue::cine_hint::NextHint;
using rex::glue::cine_hint::PadKeyLabel;

int g_checks = 0;
int g_failures = 0;

void Check(bool ok, const char* what) {
  ++g_checks;
  if (ok) return;
  ++g_failures;
  std::fprintf(stderr, "FAIL: %s\n", what);
}

void TestHostTiming() {
  HintState state;
  Check(NextHint({true, true, 1000, 0}, &state) == Hint::kSkip, "host hint starts immediately");
  Check(NextHint({true, true, 6999, 0}, &state) == Hint::kSkip, "host hint lasts through 5999 ms");
  Check(NextHint({true, true, 7000, 0}, &state) == Hint::kNone, "host hint ends at 6000 ms");
  Check(NextHint({true, true, 8000, 8000}, &state) == Hint::kSkip, "host input shows hint");
  Check(NextHint({true, true, 11999, 8000}, &state) == Hint::kSkip, "host input hint lasts 3999 ms");
  Check(NextHint({true, true, 12000, 8000}, &state) == Hint::kNone, "host input hint ends at 4000 ms");
}

void TestClientInput() {
  HintState state;
  Check(NextHint({true, false, 1000, 900}, &state) == Hint::kNone,
        "input from before cinematic does not show client hint");
  Check(NextHint({true, false, 2000, 2000}, &state) == Hint::kHostOnly, "client input shows host-only hint");
  Check(NextHint({true, false, 5999, 2000}, &state) == Hint::kHostOnly, "client hint lasts through 3999 ms");
  Check(NextHint({true, false, 6000, 2000}, &state) == Hint::kNone, "client hint ends at 4000 ms");
}

void TestCinematicRestart() {
  HintState state;
  Check(NextHint({true, true, 1000, 0}, &state) == Hint::kSkip, "first cinematic starts hint window");
  Check(NextHint({false, true, 2000, 0}, &state) == Hint::kNone, "leaving cinematic returns no hint");
  Check(NextHint({true, true, 9000, 0}, &state) == Hint::kSkip, "new cinematic restarts hint window");
}

void TestPadLabels() {
  Check(PadKeyLabel(1) == "View", "Xbox Back label is View");
  Check(PadKeyLabel(2) == "Share", "PS4 Back label is Share");
  Check(PadKeyLabel(3) == "Create", "PS5 Back label is Create");
  Check(PadKeyLabel(0).empty(), "keyboard has no pad label");
}

}  // namespace

int main() {
  TestHostTiming();
  TestClientInput();
  TestCinematicRestart();
  TestPadLabels();
  std::printf("cine_hint_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
