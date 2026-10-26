// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_key_names.h"

#include <cstdio>
#include <string_view>

int g_checks = 0;
int g_failures = 0;
void Expect(bool ok, std::string_view what) {
  ++g_checks;
  if (ok) return;
  ++g_failures;
  std::printf("FAIL: %.*s\n", int(what.size()), what.data());
}

using namespace rex::glue::keynames;

int main() {
  Expect(MouseButtonsFollowTriggers("RMB", "LMB"), "the default triggers (RMB aim, LMB fire) swap");
  Expect(!MouseButtonsFollowTriggers("LMB", "RMB"), "LMB on the left trigger needs no swap");
  Expect(!MouseButtonsFollowTriggers("RMB", "Space"), "a right trigger without LMB needs no swap");
  Expect(MouseButtonsFollowTriggers("Q,RMB", "LMB"), "RMB as a second key of the left trigger swaps");
  Expect(!MouseButtonsFollowTriggers("Shift+RMB", "LMB"), "RMB only with a modifier needs no swap");
  Expect(!MouseButtonsFollowTriggers("", ""), "empty triggers need no swap");

  Expect(EngineMouseName("LeftMouseButton", true) == "RightMouseButton", "swapped LMB is RMB");
  Expect(EngineMouseName("RightMouseButton", true) == "LeftMouseButton", "swapped RMB is LMB");
  Expect(EngineMouseName("MiddleMouseButton", true) == "MiddleMouseButton", "MMB is never swapped");
  Expect(EngineMouseName("SpaceBar", true) == "SpaceBar", "keys are never swapped");
  Expect(EngineMouseName("LeftMouseButton", false) == "LeftMouseButton", "unswapped LMB stays");
  Expect(EngineMouseName("RightMouseButton", false) == "RightMouseButton", "unswapped RMB stays");
  Expect(EngineMouseName("SpaceBar", false) == "SpaceBar", "unswapped keys stay");

  std::printf("key_names_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
