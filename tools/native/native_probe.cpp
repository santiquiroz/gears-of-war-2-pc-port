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
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
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

// Parity mode (GOW2_PARITY=1): for each draw, hash the device shadows the draw is about to flush and log them,
// so tools/native/parity_compare.py can pair them with the PARITY CP lines of the command processor by
// (frame, draw). Counting above stays on either way.
const bool kParityEnabled = [] {
  const char* value = std::getenv("GOW2_PARITY");
  return value != nullptr && std::string_view(value) == "1";
}();

constexpr uint32_t kParityFrameInterval = 300;  // log the draws of frames whose index is a multiple of this
constexpr uint32_t kParityMaxDraws = 2000;      // per logged frame
constexpr uint32_t kParityDeviceGlobal = 0x83105D40;  // guest global holding the D3DDevice (device layout doc, section 1)

std::mutex g_parity_lock;  // under it: g_parity_frame, g_parity_draw, g_parity_checked
uint32_t g_parity_frame = 0;  // Swap calls so far
uint32_t g_parity_draw = 0;   // draws so far in g_parity_frame
bool g_parity_checked = false;

// FNV-1a over the dwords as the guest stores them, read big-endian (LoadU32) and fed to the hash least-significant
// byte first, so the value is the host-endian one. Both sides must hash the same way for the hashes to compare.
uint32_t HashDwords(const uint8_t* base, uint32_t address, uint32_t dwords) {
  uint32_t hash = 2166136261u;
  for (uint32_t i = 0; i < dwords; ++i) {
    const uint32_t value = rex::glue::guest::LoadU32(base, address + 4 * i);
    for (uint32_t shift = 0; shift < 32; shift += 8) hash = (hash ^ ((value >> shift) & 0xFFu)) * 16777619u;
  }
  return hash;
}

// The four draws of tools/native/gen_probe.py's table whose device shadows are logged (device layout doc, sections 2-4).
bool IsParityDraw(std::string_view name) {
  return name == "sub_82980B20" || name == "sub_82980210" || name == "sub_8297FD30" || name == "sub_829813F0";
}

void LogParityDraw(std::string_view name, const PPCContext& ctx, const uint8_t* base) {
  const uint32_t device = ctx.r3.u32;
  std::lock_guard<std::mutex> hold(g_parity_lock);
  if (!g_parity_checked) {
    g_parity_checked = true;
    const uint32_t global = rex::glue::guest::LoadU32(base, kParityDeviceGlobal);
    if (global == device) {
      REXLOG_INFO("PARITY device {:08X} equals the global at {:08X}", device, kParityDeviceGlobal);
    } else {
      REXLOG_WARN("PARITY device {:08X} differs from the global at {:08X} ({:08X})", device, kParityDeviceGlobal, global);
    }
  }

  const uint32_t draw = g_parity_draw++;
  if (g_parity_frame % kParityFrameInterval != 0 || draw >= kParityMaxDraws) return;

  // sub_82980B20 documented in section 4: prim = r4 & 0x3F, count = r7. The other three are logged raw.
  std::string prim = "-";
  std::string count = "-";
  if (name == "sub_82980B20") {
    prim = std::to_string(ctx.r4.u32 & 0x3F);
    count = std::to_string(ctx.r7.u32);
  }
  const auto hash = [&](uint32_t offset, uint32_t dwords) { return HashDwords(base, device + offset, dwords); };
  REXLOG_INFO(
      "PARITY GAME f={} d={} fn={} prim={} n={} surf={:08X} prog={:08X} rs={:08X} vp={:08X} scis={:08X} "
      "fetch={:08X} vsc={:08X} psc={:08X} A={:08X} B={:08X} raw={:08X},{:08X},{:08X},{:08X},{:08X}",
      g_parity_frame, draw, name, prim, count, hash(10368, 6), hash(10528, 2), hash(10548, 12), hash(10444, 21),
      hash(10436, 2), hash(1152, 192), hash(1920, 1024), hash(6016, 1024),
      rex::glue::guest::LoadU32(base, device + 12692), rex::glue::guest::LoadU32(base, device + 12696),
      ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32);
}

// Called before every counted probe. Swap ends a frame; the four draws are logged in parity mode.
void OnParityProbe(std::string_view name, const PPCContext& ctx, const uint8_t* base) {
  if (name == "sub_8298D430") {
    std::lock_guard<std::mutex> hold(g_parity_lock);
    ++g_parity_frame;
    g_parity_draw = 0;
    return;
  }
  if (IsParityDraw(name)) LogParityDraw(name, ctx, base);
}

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
    if (!kParityEnabled) {                                       \
      __imp__##name(ctx, base);                                  \
      return;                                                    \
    }                                                            \
    /* After the call: the flush inside the draw updates the shadows it used. */ \
    const PPCContext args = ctx;                                 \
    __imp__##name(ctx, base);                                    \
    OnParityProbe(#name, args, base);                            \
  }
#include "probe_hooks.inc"
#undef PROBE
