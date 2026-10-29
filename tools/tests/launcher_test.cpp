// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_launcher.h"

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

int main() {
  using rex::glue::launcher::ShouldShowLauncher;
  static_assert(ShouldShowLauncher(true, 0));
  Expect(ShouldShowLauncher(true, 0), "a plain start shows the launcher");
  Expect(ShouldShowLauncher(true, -1), "a negative wait pid shows the launcher");
  Expect(!ShouldShowLauncher(true, 1234), "a relaunch goes straight into the game");
  Expect(!ShouldShowLauncher(false, 0), "gow2_launcher=false skips the launcher");
  std::printf("launcher_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
