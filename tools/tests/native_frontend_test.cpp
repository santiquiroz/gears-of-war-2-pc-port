// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Host-side test of the native frontend's DRAW_INDX search and snapshot layout (no game data, no SDK).
#include "gow2_native_frontend_snapshot.h"

#include <cstdint>
#include <cstdio>
#include <string_view>
#include <vector>

int g_checks = 0;
int g_failures = 0;
void Expect(bool ok, std::string_view what) { ++g_checks; if (ok) return; ++g_failures; std::printf("FAIL: %.*s\n", int(what.size()), what.data()); }

using rex::glue::native_frontend::FindTrailingDrawIndx;
using rex::glue::native_frontend::kDrawIndxHeader;

int main() {
  const std::vector<uint32_t> last = {0x11, 0x22, kDrawIndxHeader, 1, 2, 3, 4};
  const auto found = FindTrailingDrawIndx(last);
  Expect(found.has_value() && *found == 2, "DRAW_INDX ending at the write pointer is found");

  const std::vector<uint32_t> followed = {kDrawIndxHeader, 1, 2, 3, 4, 0xC0003601, 0x55};
  Expect(!FindTrailingDrawIndx(followed), "DRAW_INDX followed by another packet is not inline");

  const std::vector<uint32_t> off_by_one = {kDrawIndxHeader, 1, 2, 3, 4, 5};
  Expect(!FindTrailingDrawIndx(off_by_one), "header one dword too far back is rejected");

  const std::vector<uint32_t> none(32, 0x80000000u);
  Expect(!FindTrailingDrawIndx(none), "no header in 32 dwords");

  const std::vector<uint32_t> short_tail = {1, 2, 3};
  Expect(!FindTrailingDrawIndx(short_tail), "fewer than 5 dwords");

  std::vector<uint32_t> far(40, 0);
  far[0] = kDrawIndxHeader;  // beyond the 32-dword window
  Expect(!FindTrailingDrawIndx(far), "header beyond the scan window is ignored");

  if constexpr (sizeof(void*) == 8) {
    Expect(sizeof(rex::glue::native_frontend::NativeDrawSnapshot) == 2000, "snapshot is 2000 bytes on 64-bit");
  }

  std::printf("native_frontend_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
