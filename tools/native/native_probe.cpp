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

#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc.h>

#include "gow2_engine_tick.h"

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

void Report() {
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

#define PROBE(name, index, opcodes)                              \
  REX_EXTERN(__imp__##name);                                     \
  REX_HOOK_RAW(name) {                                           \
    g_calls[index].fetch_add(1, std::memory_order_relaxed);      \
    __imp__##name(ctx, base);                                    \
  }
#include "probe_hooks.inc"
#undef PROBE
