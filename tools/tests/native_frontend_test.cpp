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
    Expect(sizeof(rex::glue::native_frontend::NativeDrawSnapshot) == 2552, "snapshot is 2552 bytes on 64-bit");
  }

  using rex::glue::native_frontend::NopHeaderFor;
  using rex::glue::native_frontend::PlanSuppression;
  using rex::glue::native_frontend::SuppressPlan;
  SuppressPlan plan;

  // Type 0: rs 0x2200 x3 (covered), 0x2007 (not covered), one-register write to 0x4900 (covered), a run that
  // crosses 0x2005 -> 0x2006 (not covered), type-2 filler, fetch constants 0x4800 x6 (covered).
  const std::vector<uint32_t> type0 = {0x00022200, 1, 2, 3,
                                       0x00002007, 9,
                                       0x00018000 | 0x4900, 7, 8,
                                       0x00012005, 1, 2,
                                       0x80000000,
                                       0x00054800, 1, 2, 3, 4, 5, 6};
  PlanSuppression(type0, plan);
  Expect(plan.ok, "type-0 region parses");
  Expect(plan.nops == std::vector<uint32_t>({0, 6, 13}), "covered type-0 writes are NOPed, others kept");
  Expect(plan.kept == 2, "two type-0 packets kept, filler not counted");

  // Type 3: LOAD_ALU_CONSTANT, IM_LOAD, IM_LOAD_IMMEDIATE (2 dwords of microcode), INVALIDATE_STATE (kept),
  // SET_CONSTANT ALU 0x4004 x2 (covered), SET_CONSTANT REGISTERS 0x2007 (kept), SET_CONSTANT LOOP 0x4908 (covered),
  // WAIT_REG_MEM (kept).
  const std::vector<uint32_t> type3 = {0xC0022F00, 0x1000, 0, 4,
                                       0xC0012700, 0x2001, 16,
                                       0xC0032B00, 0, 2, 0xAA, 0xBB,
                                       0xC0003B00, 0x7FFF,
                                       0xC0022D00, 0x00000004, 1, 2,
                                       0xC0012D00, 0x00040007, 1,
                                       0xC0012D00, 0x00030000, 5,
                                       0xC0043C00, 3, 0xA31, 0, 0x80000000, 8};
  PlanSuppression(type3, plan);
  Expect(plan.ok, "type-3 region parses");
  Expect(plan.nops == std::vector<uint32_t>({0, 4, 7, 14, 21}), "constant and shader loads are NOPed");
  Expect(plan.kept == 3, "INVALIDATE_STATE, uncovered SET_CONSTANT and WAIT_REG_MEM kept");

  Expect(NopHeaderFor(0xC0022F00) == 0xC0021000, "NOP keeps the type-3 payload length");
  Expect(NopHeaderFor(0x00054800) == 0xC0051000, "NOP keeps the type-0 payload length");
  Expect(NopHeaderFor(0x00018000 | 0x4900) == 0xC0011000, "NOP of a one-register write");

  const std::vector<uint32_t> truncated = {0x00022200, 1, 2, 3, 0xC0012700, 0x2001};
  PlanSuppression(truncated, plan);
  Expect(!plan.ok && plan.nops.empty() && plan.kept == 0, "packet running past the region aborts");

  const std::vector<uint32_t> too_long(rex::glue::native_frontend::kMaxSuppressDwords + 1, 0x80000000u);
  PlanSuppression(too_long, plan);
  Expect(!plan.ok && plan.nops.empty(), "region over 16384 dwords aborts");

  PlanSuppression(std::vector<uint32_t>{}, plan);
  Expect(plan.ok && plan.nops.empty() && plan.kept == 0, "empty region is fine");

  // Type 1: header + 2 dwords, kept.
  const std::vector<uint32_t> type1 = {0x40000000, 1, 2, 0x00002200, 5};
  PlanSuppression(type1, plan);
  Expect(plan.ok && plan.nops == std::vector<uint32_t>({3}) && plan.kept == 1, "type-1 packets are 3 dwords, kept");

  std::printf("native_frontend_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
