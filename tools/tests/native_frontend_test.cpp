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

using rex::glue::native_frontend::FindLastDraw;
using rex::glue::native_frontend::kDrawIndxHeader;

int main() {
  const std::vector<uint32_t> last = {0x00002200, 7, kDrawIndxHeader, 1, 2, 3, 4};
  const auto found = FindLastDraw(last);
  Expect(found.has_value() && found->index == 2 && found->opcode == 0x22 && found->payload == 4 &&
             found->draws_seen == 1 && found->other_draws == 0,
         "DRAW_INDX ending at the write pointer is found");

  // sub_82980210 / sub_8297FD30: state restores and a bin mask after the draw.
  const std::vector<uint32_t> followed = {0x00002200, 7, kDrawIndxHeader, 1, 2, 3, 4,
                                          0xC0003B00, 0x55, 0x00012200, 1, 2, 0x80000000, 0x40000000, 8, 9};
  const auto found_followed = FindLastDraw(followed);
  Expect(found_followed && found_followed->index == 2 && found_followed->payload == 4 && found_followed->draws_seen == 1,
         "DRAW_INDX followed by other packets is found");

  // Two draws: the last one is returned, both counted; a bin draw in between counts as another.
  const std::vector<uint32_t> two = {kDrawIndxHeader, 1, 2, 3, 4, 0xC0003400, 1, 0x00002200, 5,
                                     0xC0003601, 0x00030088, 0x00002201, 6};
  const auto found_two = FindLastDraw(two);
  Expect(found_two && found_two->index == 9 && found_two->opcode == 0x36 && found_two->payload == 1 &&
             found_two->draws_seen == 3 && found_two->other_draws == 1,
         "last of several draws, all counted");

  const std::vector<uint32_t> truncated_after = {0x00002200, 7, kDrawIndxHeader, 1, 2, 3, 4, 0x00022200, 1};
  Expect(!FindLastDraw(truncated_after),"packet running past the end: nullopt");
  const std::vector<uint32_t> truncated_draw = {0x00002200, 7, kDrawIndxHeader, 1, 2, 3};
  Expect(!FindLastDraw(truncated_draw), "truncated draw packet: nullopt");

  const std::vector<uint32_t> none = {0x00002200, 7, 0xC0003B00, 0x55, 0x80000000, 0xC0003400, 1};
  Expect(!FindLastDraw(none), "no DRAW_INDX / DRAW_INDX_2 (bin draw only): nullopt");
  Expect(!FindLastDraw(std::vector<uint32_t>{}), "nothing emitted: nullopt");

  if constexpr (sizeof(void*) == 8) {
    Expect(sizeof(rex::glue::native_frontend::NativeDrawSnapshot) == 2560, "snapshot is 2560 bytes on 64-bit");
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

  // S3: trailing DRAW_INDX_2 (initiator only), and one with inline indices (unsupported by the caller).
  const std::vector<uint32_t> draw2 = {0x00002200, 7, 0xC0003601, 0x00030088};
  const auto found2 = FindLastDraw(draw2);
  Expect(found2 && found2->index == 2 && found2->opcode == 0x36 && found2->payload == 1, "trailing DRAW_INDX_2");
  const std::vector<uint32_t> inline_indices = {0xC0043601, 0x88, 1, 2, 3, 4};
  const auto found3 = FindLastDraw(inline_indices);
  Expect(found3 && found3->payload == 5, "DRAW_INDX_2 with 5 payload dwords is found, payload over 4");

  // A second draw in the region aborts suppression.
  const std::vector<uint32_t> second = {0x00002200, 1, kDrawIndxHeader, 1, 2, 3, 4, 0x00002201, 2};
  PlanSuppression(second, plan);
  Expect(!plan.ok && plan.nops.empty() && plan.writes.empty(), "another draw in the region aborts");
  const std::vector<uint32_t> bin = {0xC0003400, 1};
  PlanSuppression(bin, plan);
  Expect(!plan.ok, "DRAW_INDX_BIN in the region aborts");

  // Folding: covered type-0 writes and SET_CONSTANT land in the snapshot's arrays, in order; ALU writes are listed.
  const std::vector<uint32_t> fold = {0x00012200, 0x11, 0x22, 0x00008000 | 0x2208, 0x33,
                                      0xC0012D00, 0x00010002, 0x44, 0x00004010, 0x55};
  PlanSuppression(fold, plan);
  Expect(plan.ok && plan.nops == std::vector<uint32_t>({0, 3, 5, 8}), "fold region parses, all NOPed");
  rex::glue::native_frontend::NativeDrawSnapshot snapshot{};
  snapshot.rs[0] = 0xAA;
  snapshot.rs[8] = 0xBB;
  rex::glue::native_frontend::FoldRegisterWrites(plan.writes, snapshot);
  Expect(snapshot.rs[0] == 0x11 && snapshot.rs[1] == 0x22 && snapshot.rs[8] == 0x33, "type-0 writes folded into rs");
  Expect(snapshot.fetch[2] == 0x44, "SET_CONSTANT FETCH folded into fetch");
  Expect(!plan.writes.empty() && plan.writes.back().reg == 0x4010 && plan.writes.back().value == 0x55,
         "ALU write listed for the mirror");

  // Marker span: a 2-dword DRAW_INDX_2 takes over the NOPed packets before it.
  using rex::glue::native_frontend::MarkerSpanStart;
  const std::vector<uint32_t> before_short = {0x00025000, 0, 0, 0, 0x00002200, 5, 0x00002208, 6};
  PlanSuppression(before_short, plan);
  Expect(plan.ok && plan.nops == std::vector<uint32_t>({4, 6}), "rect region: 0x5000 kept, rs NOPed");
  const auto span = MarkerSpanStart(before_short, plan, 2);
  Expect(span && *span == 4, "span covers both NOPed packets and the draw (6 dwords)");
  Expect(rex::glue::native_frontend::NopHeaderForSpan(6) == 0xC0041000, "span NOP header");
  const auto in_place = MarkerSpanStart(before_short, plan, 5);
  Expect(in_place && *in_place == before_short.size(), "5-dword draw is rewritten in place");

  // short_draw: the packet right before is kept, or suppression aborted.
  const std::vector<uint32_t> kept_last = {0x00002200, 5, 0x00025000, 0, 0, 0};
  PlanSuppression(kept_last, plan);
  Expect(plan.ok && !MarkerSpanStart(kept_last, plan, 2), "kept packet before a short draw: short_draw");
  PlanSuppression(second, plan);
  Expect(!MarkerSpanStart(second, plan, 2), "aborted suppression with a short draw: short_draw");

  std::printf("native_frontend_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
