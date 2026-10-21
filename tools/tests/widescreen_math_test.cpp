// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_widescreen_math.h"

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
  using rex::glue::widescreen::AutoWidescreenSafe;
  Expect(AutoWidescreenSafe("", "NVIDIA GeForce RTX 5080"), "auto path on NVIDIA is ROV");
  Expect(AutoWidescreenSafe("", "Intel(R) Arc(TM) Graphics"), "auto path on Intel is ROV");
  Expect(!AutoWidescreenSafe("", "AMD Radeon RX 7800 XT"), "auto path on AMD is host render targets");
  Expect(AutoWidescreenSafe("rov", "AMD Radeon RX 7800 XT"), "explicit rov on AMD is allowed");
  Expect(!AutoWidescreenSafe("rtv", "NVIDIA GeForce RTX 5080"), "explicit rtv on NVIDIA is not");
  Expect(!AutoWidescreenSafe("", ""), "unknown GPU stays 16:9");
  using rex::glue::widescreen::AutoWidescreenSwitchesToRov;
  Expect(AutoWidescreenSwitchesToRov("", "AMD Radeon RX 7800 XT"), "auto on AMD switches the unset path to ROV");
  Expect(AutoWidescreenSwitchesToRov("", "AMD Radeon(TM) Graphics"), "auto on a Ryzen iGPU switches too");
  Expect(!AutoWidescreenSwitchesToRov("rtv", "AMD Radeon RX 7800 XT"), "an explicit rtv on AMD is kept");
  Expect(!AutoWidescreenSwitchesToRov("", "NVIDIA GeForce RTX 5080"), "NVIDIA is already ROV");
  Expect(!AutoWidescreenSwitchesToRov("", ""), "unknown GPU is not switched");
  Expect(rex::glue::widescreen::ParseAspect("auto", 16.0 / 9.0) == 0.0, "a 16:9 display stays 16:9");
  std::printf("widescreen_math_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
