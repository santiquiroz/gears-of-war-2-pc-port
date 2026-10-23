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
#include <vector>

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
uint32_t HashValues(const uint32_t* values, uint32_t dwords) {
  uint32_t hash = 2166136261u;
  for (uint32_t i = 0; i < dwords; ++i) {
    for (uint32_t shift = 0; shift < 32; shift += 8) hash = (hash ^ ((values[i] >> shift) & 0xFFu)) * 16777619u;
  }
  return hash;
}

uint32_t HashDwords(const uint8_t* base, uint32_t address, uint32_t dwords) {
  std::vector<uint32_t> values(dwords);
  for (uint32_t i = 0; i < dwords; ++i) values[i] = rex::glue::guest::LoadU32(base, address + 4 * i);
  return HashValues(values.data(), dwords);
}

// The physical-address fixup the library applies before putting an address in a packet (120.cpp:20504-20510,
// 22.cpp sub_8298F378): clrlwi 3, plus 0x1000 when the top 12 bits are >= 0xE00 (rlwinm 12,20,31; addi 512;
// rlwinm 0,19,19).
uint32_t PhysicalAddress(uint32_t address) {
  return (address & 0x1FFFFFFFu) + ((((address >> 20) + 512u) & 0x1000u));
}

// Microcode of a shader object as its load packet names it: guest address (type bits stripped) and dwords.
struct ShaderMicrocode {
  uint32_t address = 0;
  uint32_t dwords = 0;
};

// A (dev+12692), IM_LOAD in sub_8298F600 (120.cpp:20489-20530): payload [(Phys(*(S_A+40) + *(A+24)) & 0x1FFFFFFE)
// | 1, *(S_A+44) >> 2] with S_A = A + *(A+64).
ShaderMicrocode MicrocodeOfA(const uint8_t* base, uint32_t a) {
  using rex::glue::guest::LoadU32;
  const uint32_t s = a + LoadU32(base, a + 64);
  return {PhysicalAddress(LoadU32(base, s + 40) + LoadU32(base, a + 24)) & ~3u, LoadU32(base, s + 44) >> 2};
}

// B (dev+12696): no IM_LOAD in sub_8298F600; its microcode goes through sub_8298F210 (18.cpp:19104-19160, called
// at 120.cpp:20785-20795 with r4 = 0, r10 = r24), an IM_LOAD_IMMEDIATE of type 0 copied from
// Phys(*(S+872) + *(B+32)) with *(S+876) >> 2 dwords, S = B + *(B + 896 + 8·r24), r24 = (*(B+872) & 0x20) != 0
// (120.cpp:20327-20336). When the shader cache sub_8298F4C0 hits instead, the command processor may load another copy.
ShaderMicrocode MicrocodeOfB(const uint8_t* base, uint32_t b) {
  using rex::glue::guest::LoadU32;
  const uint32_t variant = (LoadU32(base, b + 872) & 0x20u) != 0 ? 1u : 0u;
  const uint32_t s = b + LoadU32(base, b + 896 + 8 * variant);
  return {PhysicalAddress(LoadU32(base, s + 872) + LoadU32(base, b + 32)) & ~3u, LoadU32(base, s + 876) >> 2};
}

// LOAD_ALU_CONSTANT records of sub_8298F378 (22.cpp:20511-20693): header at desc + *(desc+20); records
// {u16 start, u16 count, u32 offset} from header+20 to header+20+*(header+16), stopping at count 0. The packet is
// [Phys(offset + data), start·4, count]: count is in dwords (the ring copy moves count·4 bytes), the destination is
// register 0x4000 + start·4. Overlaid on the 2048 dwords of 0x4000-0x47FF.
void OverlayAluConstants(const uint8_t* base, uint32_t desc, uint32_t data, std::vector<uint32_t>& registers) {
  using rex::glue::guest::LoadU32;
  const uint32_t header_offset = LoadU32(base, desc + 20);
  if (header_offset == 0) return;
  const uint32_t header = desc + header_offset;
  uint32_t record = header + 20;
  const uint32_t end = record + LoadU32(base, header + 16);
  for (uint32_t guard = 0; record < end && guard < 4096; ++guard, record += 8) {
    const uint32_t start_count = LoadU32(base, record);
    const uint32_t start = start_count >> 16;
    const uint32_t count = start_count & 0xFFFFu;
    if (count == 0) break;
    const uint32_t source = LoadU32(base, record + 4) + data;
    for (uint32_t i = 0; i < count && start * 4 + i < registers.size(); ++i) {
      registers[start * 4 + i] = LoadU32(base, source + 4 * i);
    }
  }
}

// sub_8297FD30 (78.cpp:20305-20330) and sub_82980210 (1.cpp:19272-19300) copy their vertices into a ring allocation
// and write fetch slot 31 (fetch constants 93-95, stream 0 = 95) straight into the ring with the type-0 header
// 0x000548BA (6 dwords at 0x48BA), leaving the fetch shadow alone. The address comes from sub_8298B5B0, not from
// 12460, so it is read back from the ring: the last such header in the 64 dwords before the write pointer dev+48.
// Overlays those 6 dwords at fetch dword 186 and returns false when no header is found.
bool OverlayRingFetch(const uint8_t* base, uint32_t device, std::vector<uint32_t>& fetch) {
  using rex::glue::guest::LoadU32;
  const uint32_t write = LoadU32(base, device + 48);
  for (uint32_t back = 0; back < 64; ++back) {
    const uint32_t at = write - 4 * back;
    if (LoadU32(base, at) != 0x000548BAu) continue;
    if (back < 6) return false;
    for (uint32_t i = 0; i < 6; ++i) fetch[186 + i] = LoadU32(base, at + 4 + 4 * i);
    return true;
  }
  return false;
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
  const uint32_t a = rex::glue::guest::LoadU32(base, device + 12692);
  const uint32_t b = rex::glue::guest::LoadU32(base, device + 12696);
  const ShaderMicrocode code_a = a != 0 ? MicrocodeOfA(base, a) : ShaderMicrocode{};
  const ShaderMicrocode code_b = b != 0 ? MicrocodeOfB(base, b) : ShaderMicrocode{};

  // Constants as the command processor holds them: shadows 1920/6016 (flushed through masks 0/8 by sub_8298E5B8)
  // under the LOAD_ALU_CONSTANT records of A (A+40, data *(A+24)) and B (B+872, data *(B+32)) (120.cpp:20468-20472,
  // 20716-20726, 20870-20876).
  std::vector<uint32_t> constants(2048);
  for (uint32_t i = 0; i < 1024; ++i) {
    constants[i] = rex::glue::guest::LoadU32(base, device + 1920 + 4 * i);
    constants[1024 + i] = rex::glue::guest::LoadU32(base, device + 6016 + 4 * i);
  }
  if (a != 0) OverlayAluConstants(base, a + 40, rex::glue::guest::LoadU32(base, a + 24), constants);
  if (b != 0) OverlayAluConstants(base, b + 872, rex::glue::guest::LoadU32(base, b + 32), constants);

  std::string fetch2 = "-";
  if (name == "sub_82980210" || name == "sub_8297FD30") {
    std::vector<uint32_t> fetch(192);
    for (uint32_t i = 0; i < 192; ++i) fetch[i] = rex::glue::guest::LoadU32(base, device + 1152 + 4 * i);
    if (OverlayRingFetch(base, device, fetch)) fetch2 = fmt::format("{:08X}", HashValues(fetch.data(), 192));
  }
  REXLOG_INFO(
      "PARITY GAME f={} d={} fn={} prim={} n={} surf={:08X} prog={:08X} rs={:08X} vp={:08X} scis={:08X} "
      "fetch={:08X} vsc={:08X} psc={:08X} A={:08X} B={:08X} raw={:08X},{:08X},{:08X},{:08X},{:08X} "
      "Aaddr={:08X} Alen={:X} Baddr={:08X} Blen={:X} vsc2={:08X} psc2={:08X} fetch2={}",
      g_parity_frame, draw, name, prim, count, hash(10368, 6), hash(10528, 2), hash(10548, 12), hash(10444, 21),
      hash(10436, 2), hash(1152, 192), hash(1920, 1024), hash(6016, 1024), a, b,
      ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32, code_a.address, code_a.dwords, code_b.address,
      code_b.dwords, HashValues(constants.data(), 1024), HashValues(constants.data() + 1024, 1024), fetch2);
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
