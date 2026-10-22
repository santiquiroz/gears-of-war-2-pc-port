// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
//
// Native renderer phase 0 (docs/NATIVE_RENDERER_STUDY.md): counting hooks on every function that
// builds PM4 packet headers (tools/native/probe_hooks.inc, from pm4_scan.py). Every 150 frames it logs
// NATIVEPROBE lines with the calls per frame of each function that ran, to compare with the draws the
// command processor counts (d3d12_stats_log). Built only with -DGOW2_NATIVE_PROBE=ON; never released.

#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <tuple>

#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc.h>

#include "gow2_engine_tick.h"
#include "gow2_guest.h"

namespace {

struct ProbeInfo {
  const char* name;
  const char* opcodes;
};

constexpr ProbeInfo kProbes[] = {
#define PROBE(name, index, opcodes) {#name, opcodes},
#include "probe_hooks.inc"
#undef PROBE
};

constexpr size_t kProbeCount = sizeof(kProbes) / sizeof(kProbes[0]);
constexpr uint32_t kFramesPerReport = 150;

std::array<std::atomic<uint64_t>, kProbeCount> g_calls{};
uint32_t g_frames = 0;

// sub_8298BAB0 emits INDIRECT_BUFFER for a command buffer: r4 -> {size | flags << 24, address}. Keyed by
// caller (the return address) and the buffer, to tell recorded buffers replayed every frame from new ones.
using ReplayKey = std::tuple<uint32_t, uint32_t, uint32_t>;  // caller, address, header word
std::mutex g_replay_lock;
std::map<ReplayKey, uint32_t> g_replays;
std::map<ReplayKey, uint32_t> g_replay_windows;  // how many report windows saw the key

void ReportReplays() {
  std::lock_guard<std::mutex> hold(g_replay_lock);
  for (const auto& [key, count] : g_replays) {
    const auto [caller, address, word0] = key;
    const uint32_t windows = ++g_replay_windows[key];
    REXLOG_INFO("NATIVEPROBE replay caller {:08X} buffer {:08X} dwords {} flags {:02X}: {:.2f}/frame, seen in {} windows",
                caller, address, word0 & 0xFFFFFF, word0 >> 24, double(count) / kFramesPerReport, windows);
  }
  g_replays.clear();
}

// sub_82981ED8, D3DDevice_ClearF(device r3, flags r4, D3DRECT* r5 or null for the whole target,
// D3DVECTOR4* colour r6, z f1, stencil r7). Keyed by flags and rectangle.
using ClearKey = std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>;  // flags, x1, y1, x2, y2
std::map<ClearKey, uint32_t> g_clears;  // under g_replay_lock

void ReportClears() {
  std::lock_guard<std::mutex> hold(g_replay_lock);
  for (const auto& [key, count] : g_clears) {
    const auto [flags, x1, y1, x2, y2] = key;
    const bool whole = x1 == 0 && y1 == 0 && x2 == 0 && y2 == 0;
    REXLOG_INFO("NATIVEPROBE clear flags {:08X} rect {}: {:.2f}/frame", flags,
                whole ? std::string("whole target") : fmt::format("{},{}-{},{}", x1, y1, x2, y2),
                double(count) / kFramesPerReport);
  }
  g_clears.clear();
}

void Report() {
  ReportReplays();
  ReportClears();
  for (size_t i = 0; i < kProbeCount; ++i) {
    const uint64_t calls = g_calls[i].exchange(0, std::memory_order_relaxed);
    if (calls == 0) continue;
    REXLOG_INFO("NATIVEPROBE {} {:.2f}/frame ({} calls) [{}]", kProbes[i].name,
                double(calls) / kFramesPerReport, calls, kProbes[i].opcodes);
  }
  REXLOG_INFO("NATIVEPROBE --- {} frames", kFramesPerReport);
}

void OnEngineTick(const PPCContext&, uint8_t*) {
  if (++g_frames % kFramesPerReport == 0) Report();
}

[[maybe_unused]] const bool kTickRegistered = (rex::glue::AddEngineTickListener(&OnEngineTick), true);

}  // namespace

REX_EXTERN(__imp__sub_8298BAB0);
REX_HOOK_RAW(sub_8298BAB0) {
  const uint32_t caller = uint32_t(ctx.lr);
  const uint32_t word0 = rex::glue::guest::LoadU32(base, ctx.r4.u32);
  const uint32_t address = rex::glue::guest::LoadU32(base, ctx.r4.u32 + 4);
  {
    std::lock_guard<std::mutex> hold(g_replay_lock);
    ++g_replays[ReplayKey{caller, address, word0}];
  }
  __imp__sub_8298BAB0(ctx, base);
}

REX_EXTERN(__imp__sub_82981ED8);
REX_HOOK_RAW(sub_82981ED8) {
  const uint32_t flags = ctx.r4.u32;
  const uint32_t rect = ctx.r5.u32;
  ClearKey key{flags, 0, 0, 0, 0};
  if (rect != 0) {
    key = ClearKey{flags, rex::glue::guest::LoadU32(base, rect), rex::glue::guest::LoadU32(base, rect + 4),
                   rex::glue::guest::LoadU32(base, rect + 8), rex::glue::guest::LoadU32(base, rect + 12)};
  }
  {
    std::lock_guard<std::mutex> hold(g_replay_lock);
    ++g_clears[key];
  }
  __imp__sub_82981ED8(ctx, base);
}

#define PROBE(name, index, opcodes)                              \
  REX_EXTERN(__imp__##name);                                     \
  REX_HOOK_RAW(name) {                                           \
    g_calls[index].fetch_add(1, std::memory_order_relaxed);      \
    __imp__##name(ctx, base);                                    \
  }
#include "probe_hooks.inc"
#undef PROBE
