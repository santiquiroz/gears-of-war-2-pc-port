// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_prompt_device.h"

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

using namespace rex::glue::prompt_device;

Inputs Auto(uint64_t pad_ms, int pad_type, uint64_t kbm_ms, bool connected) {
  Inputs in;
  in.mode = "auto";
  in.pad_ms = pad_ms;
  in.pad_type = pad_type;
  in.kbm_ms = kbm_ms;
  in.pad_connected = connected;
  return in;
}

Inputs Forced(const char* mode, int pad_type) {
  Inputs in = Auto(5000, pad_type, 9000, true);
  in.mode = mode;
  return in;
}

int main() {
  Expect(PadDevice(0) == Device::kXbox, "an unknown pad shows Xbox buttons");
  Expect(PadDevice(2) == Device::kXbox, "an Xbox 360 pad shows Xbox buttons");
  Expect(PadDevice(3) == Device::kXbox, "an Xbox One pad shows Xbox buttons");
  Expect(PadDevice(4) == Device::kPlayStation4, "a PS3 pad shows PS4 buttons");
  Expect(PadDevice(5) == Device::kPlayStation4, "a PS4 pad shows PS4 buttons");
  Expect(PadDevice(6) == Device::kPlayStation5, "a PS5 pad shows PS5 buttons");

  Expect(Choose(Forced("keyboard", 6)) == Device::kKeyboard, "keyboard mode wins over a PS5 pad");
  Expect(Choose(Forced("controller", 6)) == Device::kXbox, "controller mode shows Xbox buttons with a PS5 pad");
  Expect(Choose(Forced("playstation", 5)) == Device::kPlayStation4, "playstation mode with a PS4 pad is PS4");
  Expect(Choose(Forced("playstation", 4)) == Device::kPlayStation4, "playstation mode with a PS3 pad is PS4");
  Expect(Choose(Forced("playstation", 6)) == Device::kPlayStation5, "playstation mode with a PS5 pad is PS5");
  Expect(Choose(Forced("playstation", 0)) == Device::kPlayStation5, "playstation mode with no pad is PS5");

  Expect(Choose(Auto(0, 3, 0, true)) == Device::kXbox, "untouched with an Xbox pad connected is Xbox");
  Expect(Choose(Auto(0, 6, 0, true)) == Device::kPlayStation5, "untouched with a PS5 pad connected is PS5");
  Expect(Choose(Auto(0, 0, 0, false)) == Device::kKeyboard, "untouched with no pad is keyboard");
  Expect(Choose(Auto(9000, 5, 4000, true)) == Device::kPlayStation4, "the pad used after the keyboard wins");
  Expect(Choose(Auto(4000, 5, 9000, true)) == Device::kKeyboard, "the keyboard used after the pad wins");
  Expect(Choose(Auto(4000, 2, 4000, true)) == Device::kXbox, "equal times go to the pad");
  Expect(Choose(Auto(0, 6, 4000, true)) == Device::kKeyboard, "a connected pad never used loses to the keyboard");
  Inputs garbage = Auto(9000, 6, 4000, true);
  garbage.mode = "garbage";
  Expect(Choose(garbage) == Device::kPlayStation5, "a garbage mode behaves like auto");

  Expect(std::string_view(VariantFor(Device::kKeyboard)).empty(), "keyboard has no variant");
  Expect(std::string_view(VariantFor(Device::kXbox)).empty(), "Xbox has no variant");
  Expect(std::string_view(VariantFor(Device::kPlayStation4)) == "ps4", "PS4 uses replace_ps4");
  Expect(std::string_view(VariantFor(Device::kPlayStation5)) == "ps5", "PS5 uses replace_ps5");
  Expect(IsPlayStation(Device::kPlayStation4) && IsPlayStation(Device::kPlayStation5), "PS4 and PS5 are PlayStation");
  Expect(!IsPlayStation(Device::kXbox) && !IsPlayStation(Device::kKeyboard), "Xbox and keyboard are not PlayStation");

  std::printf("prompt_device_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
