// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_native_renderer_math.h"

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
  using rex::glue::native_renderer::UseNativeRenderer;
  Expect(UseNativeRenderer("on", false, "NVIDIA GeForce RTX 5080", false), "on is on for NVIDIA");
  Expect(!UseNativeRenderer("off", false, "AMD Radeon RX 7800 XT", false), "off is off for AMD");
  Expect(UseNativeRenderer("auto", false, "AMD Radeon RX 7800 XT", false), "auto on AMD with the path unset is on");
  Expect(!UseNativeRenderer("auto", true, "AMD Radeon RX 7800 XT", false), "auto on AMD with the path set is off");
  Expect(UseNativeRenderer("auto", false, "NVIDIA GeForce RTX 5080", false), "auto on NVIDIA is on");
  Expect(!UseNativeRenderer("auto", true, "NVIDIA GeForce GTX 1070", false), "auto on NVIDIA with the path set is off");
  Expect(!UseNativeRenderer("auto", false, "Intel(R) UHD Graphics 770", false), "auto on Intel is off");
  Expect(!UseNativeRenderer("auto", false, "", false), "auto with an unknown GPU is off");
  Expect(UseNativeRenderer("garbage", false, "AMD Radeon RX 7800 XT", false), "a garbage mode behaves like auto on AMD");
  Expect(UseNativeRenderer("garbage", false, "NVIDIA GeForce GTX 1070", false), "a garbage mode behaves like auto on NVIDIA");
  Expect(UseNativeRenderer("auto", false, "", true), "auto on Android with no GPU name is on");
  Expect(!UseNativeRenderer("auto", true, "", true), "auto on Android with the path set is off");
  Expect(!UseNativeRenderer("off", false, "", true), "off is off on Android");
  std::printf("native_renderer_math_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
