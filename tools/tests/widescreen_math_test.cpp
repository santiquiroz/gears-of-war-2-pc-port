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
  Expect(AutoWidescreenSafe(false, "", "NVIDIA GeForce RTX 5080"), "auto path on NVIDIA is ROV");
  Expect(AutoWidescreenSafe(false, "", "Intel(R) Arc(TM) Graphics"), "auto path on Intel is ROV");
  Expect(!AutoWidescreenSafe(false, "", "AMD Radeon RX 7800 XT"), "auto path on AMD is host render targets");
  Expect(AutoWidescreenSafe(false, "rov", "AMD Radeon RX 7800 XT"), "explicit rov on AMD is allowed");
  Expect(!AutoWidescreenSafe(false, "rtv", "NVIDIA GeForce RTX 5080"), "explicit rtv on NVIDIA is not");
  Expect(!AutoWidescreenSafe(false, "", ""), "unknown GPU stays 16:9");
  Expect(AutoWidescreenSafe(true, "", "AMD Radeon RX 7800 XT"), "native renderer widens on AMD");
  using rex::glue::widescreen::AutoWidescreenSwitchesToRov;
  Expect(AutoWidescreenSwitchesToRov(false, "", "AMD Radeon RX 7800 XT"), "auto on AMD switches the unset path to ROV");
  Expect(AutoWidescreenSwitchesToRov(false, "", "AMD Radeon(TM) Graphics"), "auto on a Ryzen iGPU switches too");
  Expect(!AutoWidescreenSwitchesToRov(false, "rtv", "AMD Radeon RX 7800 XT"), "an explicit rtv on AMD is kept");
  Expect(!AutoWidescreenSwitchesToRov(false, "", "NVIDIA GeForce RTX 5080"), "NVIDIA is already ROV");
  Expect(!AutoWidescreenSwitchesToRov(false, "", ""), "unknown GPU is not switched");
  Expect(!AutoWidescreenSwitchesToRov(true, "", "AMD Radeon RX 7800 XT"), "native renderer keeps host render targets");

  using rex::glue::widescreen::AutoPicksHostRenderTargets;
  Expect(AutoPicksHostRenderTargets(false, "", "NVIDIA GeForce RTX 4070 Ti", 0.0), "16:9 on NVIDIA asks for host render targets");
  Expect(!AutoPicksHostRenderTargets(false, "rov", "NVIDIA GeForce RTX 4070 Ti", 0.0), "an explicit rov on NVIDIA is kept");
  Expect(!AutoPicksHostRenderTargets(false, "", "NVIDIA GeForce RTX 3070 Ti", 21.0 / 9.0), "ultrawide on NVIDIA keeps ROV");
  Expect(!AutoPicksHostRenderTargets(false, "", "NVIDIA GeForce RTX 3070 Ti", 1.6), "16:10 on NVIDIA keeps ROV");
  Expect(!AutoPicksHostRenderTargets(true, "", "NVIDIA GeForce RTX 4070 Ti", 0.0), "the native renderer is left alone");
  Expect(!AutoPicksHostRenderTargets(false, "", "AMD Radeon RX 7800 XT", 0.0), "AMD already uses host render targets");
  Expect(rex::glue::widescreen::ParseAspect("auto", 16.0 / 9.0) == 0.0, "a 16:9 display stays 16:9");
  using namespace rex::glue::widescreen;
  Expect(ParseAspect("auto", 1280.0 / 800.0) == 1280.0 / 800.0, "auto selects a 16:10 display");
  Expect(ParseAspect("16:10", 16.0 / 10.0) == 1.6, "16:10 parses as a narrow aspect");
  Expect(ParseAspect("4:3", 16.0 / 9.0) == 4.0 / 3.0, "4:3 parses as a narrow aspect");
  Expect(ParseAspect("1.6", 16.0 / 9.0) == 1.6, "decimal narrow aspect parses");
  Expect(BackBufferWidth(1.6, 720) == 1280, "16:10 keeps the original width");
  Expect(BackBufferHeight(1.6, 720, kVirtualEdramTiles) == 800, "16:10 renders taller");
  Expect(BackBufferHeight(4.0 / 3.0, 720, kVirtualEdramTiles) == 960, "4:3 renders taller");
  Expect(BackBufferHeight(4.0 / 3.0, 720, kConsoleEdramTiles) == 736, "4:3 clamps to standard EDRAM capacity");
  Expect(BackBufferWidth(16.0 / 9.0, 720) == 0, "16:9 keeps the game's original width result");
  Expect(BackBufferWidth(21.0 / 9.0, 704) == 1680, "21:9 keeps its existing width result");
  Expect(BackBufferHeight(21.0 / 9.0, 704, kVirtualEdramTiles) == 704, "21:9 keeps its existing height");
  const double half_fov = 0.75;
  Expect(WidenHalfFov(half_fov, 1280) == half_fov, "Vert+ keeps the 16:9 horizontal FOV");
  Expect(WidenHalfFov(half_fov, 1680) == std::atan(std::tan(half_fov) * (1680.0 / 1280.0)),
         "21:9 keeps its existing horizontal FOV widening");
  std::printf("widescreen_math_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
