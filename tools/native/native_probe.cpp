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

// Mirror of the command processor's ALU constant registers 0x4000-0x47FF (vertex 0-1023, pixel 1024-2047), under
// g_parity_lock. Fed by the LOAD_ALU_CONSTANT emitter sub_8298F378 and the shadow flush sub_8298E5B8, in call order.
std::vector<uint32_t> g_alu_mirror(2048);

// GOW2_PARITY_DUMP_FRAME=<n>: for parity frame n and draws below kParityDumpDraws, also log the arrays behind
// vsc2, psc2 and fetch2 as PARITY GAMEDUMP lines (tools/native/parity_dump_diff.py). -1 when unset.
constexpr uint32_t kParityDumpDraws = 64;
const uint32_t kParityDumpFirst = [] {  // GOW2_PARITY_DUMP_FIRST: first draw of the window
  const char* value = std::getenv("GOW2_PARITY_DUMP_FIRST");
  return value != nullptr ? uint32_t(std::strtoul(value, nullptr, 10)) : 0u;
}();
const int64_t kParityDumpFrame = [] {
  const char* value = std::getenv("GOW2_PARITY_DUMP_FRAME");
  return value != nullptr && *value != 0 ? std::strtoll(value, nullptr, 10) : int64_t{-1};
}();

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

// The pixel shader the command processor last received inline (IM_LOAD_IMMEDIATE type 1, 9 dwords). sub_8298F600
// loads no pixel shader when A (dev+12692) is 0 (120.cpp: cmplwi cr6,r29,0; bne 0x8298f760 falls through to
// 8298F6C0 and sets the low 3 bits of RB_MODECONTROL 10580 to 5, depth only), so the command processor keeps the
// last one: Clear's at 0x8208D690 (sub_82981ED8, 48.cpp:20615-20700: lis r10,-32247; addi r29,r10,-10608; li r8,9;
// li r5,36) or sub_82982718's at 0x8208CF44 (130.cpp:20070-20110: lis r6,-32247; addi r30,r6,-12476; mr r4,r30;
// li r8,9). A draw with A != 0 replaces it with A's IM_LOAD. Under g_parity_lock.
constexpr ShaderMicrocode kClearPixelShader{0x8208D690u, 9};
constexpr ShaderMicrocode kHelperPixelShader{0x8208CF44u, 9};
ShaderMicrocode g_last_pixel_shader;

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

// sub_8298F378(r3 dev, r4 desc, r5 data) (22.cpp:20511-20693): header at desc + *(desc+20) (0 = nothing emitted);
// records {u16 start, u16 count, u32 offset} from header+20 to header+20+*(header+16), stopping at count 0. The
// packet is [Phys(offset + data), start·4, count]: count is in dwords (the ring copy moves count·4 bytes), the
// destination is register 0x4000 + start·4. Both paths (ring copy and direct packet) read the guest address
// offset + data. Copies into g_alu_mirror; the caller holds g_parity_lock.
void MirrorLoadAluConstants(const uint8_t* base, uint32_t desc, uint32_t data) {
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
    for (uint32_t i = 0; i < count && start * 4 + i < g_alu_mirror.size(); ++i) {
      g_alu_mirror[start * 4 + i] = LoadU32(base, source + 4 * i);
    }
  }
}

// sub_8298E5B8(r3 dev, r4 mask, r5 register base 0x4000/0x4400, r6 shadow) (72.cpp:19964-20173). The 64-bit mask is
// read from bit 63 down: each run of n set bits after a gap of z clear bits copies n·16 dwords from shadow +
// 64·group bytes to register r5 + 16·group, with group counting from the top bit (cntlzd). r5 - 0x4000 is the
// mirror offset (0 vertex, 1024 pixel). Callers never pass mask 0. The caller holds g_parity_lock.
void MirrorFlushAluShadow(const uint8_t* base, uint64_t mask, uint32_t register_base, uint32_t shadow) {
  using rex::glue::guest::LoadU32;
  if (register_base < 0x4000u) return;
  const uint32_t offset = register_base - 0x4000u;
  uint32_t group = 0;
  while (mask != 0) {
    const uint32_t gap = uint32_t(__builtin_clzll(mask));
    mask <<= gap;  // gap < 64 since mask != 0
    group += gap;
    const uint32_t groups = ~mask == 0 ? 64u : uint32_t(__builtin_clzll(~mask));
    for (uint32_t g = 0; g < groups; ++g) {
      for (uint32_t i = 0; i < 16; ++i) {
        const uint32_t index = offset + 16 * (group + g) + i;
        if (index < g_alu_mirror.size()) g_alu_mirror[index] = LoadU32(base, shadow + 64 * (group + g) + 4 * i);
      }
    }
    group += groups;
    mask = groups >= 64 ? 0 : (mask << groups);
  }
}

// sub_8297FD30 (78.cpp:20305-20330) and sub_82980210 (1.cpp:19272-19300) copy their vertices into a ring allocation
// and write fetch slot 31 (fetch constants 93-95, stream 0 = 95) straight into the ring with the type-0 header
// 0x000548BA (6 dwords at 0x48BA), leaving the fetch shadow alone. The address comes from sub_8298B5B0, not from
// 12460, so it is read back from the ring: the last such header in the 1024 dwords before the write pointer dev+48.
// Overlays those 6 dwords at fetch dword 186 and returns false when no header is found.
bool OverlayRingFetch(const uint8_t* base, uint32_t device, std::vector<uint32_t>& fetch) {
  using rex::glue::guest::LoadU32;
  const uint32_t write = LoadU32(base, device + 48);
  for (uint32_t back = 0; back < 1024; ++back) {
    const uint32_t at = write - 4 * back;
    if (LoadU32(base, at) != 0x000548BAu) continue;
    if (back < 6) return false;
    for (uint32_t i = 0; i < 6; ++i) fetch[186 + i] = LoadU32(base, at + 4 + 4 * i);
    return true;
  }
  return false;
}

// sub_829813F0 (57.cpp:20227-20470, rectangle list for Clear) flushes nothing and writes its state straight into
// the ring, then marks the shadows dirty (dev+16/24/32) without touching them. Its 22 dwords end at the write
// pointer dev+48: H = write - 84 holds 0x00054800 (fetch dwords 0-5), H+28 0x00025000 + 3 zeros, H+44 0x00012000
// + {r8, r9} (0x2000 RB_SURFACE_INFO, 0x2001 RB_COLOR_INFO), H+56 0x00002301 + value, H+64 0x00002200 + r10
// (RB_DEPTHCONTROL), H+72 0x00002208 + stack arg (RB_MODECONTROL), H+80 0xC0003601 DRAW_INDX_2 + initiator
// (prim 8 rectangle list, auto index, 3 vertices per rectangle). Overlays those on the shadows the hashes read;
// false when the layout is not there.
bool OverlayRectRing(const uint8_t* base, uint32_t device, std::vector<uint32_t>& surf, std::vector<uint32_t>& rs,
                     std::vector<uint32_t>& fetch) {
  using rex::glue::guest::LoadU32;
  const uint32_t h = LoadU32(base, device + 48) - 84;
  if (LoadU32(base, h) != 0x00054800u || LoadU32(base, h + 44) != 0x00012000u ||
      LoadU32(base, h + 64) != 0x00002200u || LoadU32(base, h + 72) != 0x00002208u ||
      LoadU32(base, h + 80) != 0xC0003601u) {
    return false;
  }
  for (uint32_t i = 0; i < 6; ++i) fetch[i] = LoadU32(base, h + 4 + 4 * i);
  surf[0] = LoadU32(base, h + 48);
  surf[1] = LoadU32(base, h + 52);
  rs[0] = LoadU32(base, h + 68);
  rs[8] = LoadU32(base, h + 76);
  return true;
}

std::vector<uint32_t> LoadDwords(const uint8_t* base, uint32_t address, uint32_t dwords) {
  std::vector<uint32_t> values(dwords);
  for (uint32_t i = 0; i < dwords; ++i) values[i] = rex::glue::guest::LoadU32(base, address + 4 * i);
  return values;
}

void LogParityDump(uint32_t frame, uint32_t draw, const char* block, const uint32_t* values, uint32_t dwords) {
  std::string hex;
  hex.reserve(8 * dwords);
  for (uint32_t i = 0; i < dwords; ++i) hex += fmt::format("{:08X}", values[i]);
  REXLOG_INFO("PARITY GAMEDUMP f={} d={} blk={} hex={}", frame, draw, block, hex);
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
  const bool dump = kParityDumpFrame >= 0 && g_parity_frame == uint64_t(kParityDumpFrame) &&
                    draw >= kParityDumpFirst && draw < kParityDumpFirst + kParityDumpDraws;
  if ((g_parity_frame % kParityFrameInterval != 0 && !dump) || draw >= kParityMaxDraws) return;

  // sub_82980B20 documented in section 4: prim = r4 & 0x3F, count = r7. The other three are logged raw.
  std::string prim = "-";
  std::string count = "-";
  if (name == "sub_82980B20") {
    prim = std::to_string(ctx.r4.u32 & 0x3F);
    count = std::to_string(ctx.r7.u32);
  } else if (name == "sub_829813F0") {
    // r4 rectangles, 3 vertices each (mulli r27,r28,21 / mullw r10,r28,r6 with r6 = 3<<16, initiator prim 8).
    prim = "8";
    count = std::to_string(3 * ctx.r4.u32);
  }
  const auto hash = [&](uint32_t offset, uint32_t dwords) { return HashDwords(base, device + offset, dwords); };
  const uint32_t a = rex::glue::guest::LoadU32(base, device + 12692);
  const uint32_t b = rex::glue::guest::LoadU32(base, device + 12696);
  const ShaderMicrocode code_a = a != 0 ? MicrocodeOfA(base, a) : ShaderMicrocode{};
  const ShaderMicrocode code_b = b != 0 ? MicrocodeOfB(base, b) : ShaderMicrocode{};
  if (a != 0) g_last_pixel_shader = code_a;
  const ShaderMicrocode code_p = g_last_pixel_shader;

  // Constants as the command processor holds them: the persistent mirror fed by sub_8298F378 and sub_8298E5B8.
  const std::vector<uint32_t>& constants = g_alu_mirror;

  std::string fetch2 = "-";
  std::vector<uint32_t> fetch = LoadDwords(base, device + 1152, 192);
  std::vector<uint32_t> surf = LoadDwords(base, device + 10368, 6);
  std::vector<uint32_t> rs = LoadDwords(base, device + 10548, 12);
  if (name == "sub_82980210" || name == "sub_8297FD30") {
    if (OverlayRingFetch(base, device, fetch)) fetch2 = fmt::format("{:08X}", HashValues(fetch.data(), 192));
  } else if (name == "sub_829813F0") {
    // surf and rs below then carry the registers it wrote into the ring; fetch2 the fetch dwords 0-5.
    if (OverlayRectRing(base, device, surf, rs, fetch)) fetch2 = fmt::format("{:08X}", HashValues(fetch.data(), 192));
  }
  REXLOG_INFO(
      "PARITY GAME f={} d={} fn={} prim={} n={} surf={:08X} prog={:08X} rs={:08X} vp={:08X} scis={:08X} "
      "fetch={:08X} vsc={:08X} psc={:08X} A={:08X} B={:08X} raw={:08X},{:08X},{:08X},{:08X},{:08X} "
      "Aaddr={:08X} Alen={:X} Baddr={:08X} Blen={:X} vsc2={:08X} psc2={:08X} fetch2={} Paddr={:08X} Plen={:X}",
      g_parity_frame, draw, name, prim, count, HashValues(surf.data(), 6), hash(10528, 2), HashValues(rs.data(), 12),
      hash(10444, 21),
      hash(10436, 2), hash(1152, 192), hash(1920, 1024), hash(6016, 1024), a, b,
      ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32, code_a.address, code_a.dwords, code_b.address,
      code_b.dwords, HashValues(constants.data(), 1024), HashValues(constants.data() + 1024, 1024), fetch2,
      code_p.address, code_p.dwords);
  if (dump) {
    LogParityDump(g_parity_frame, draw, "vsc", constants.data(), 1024);
    LogParityDump(g_parity_frame, draw, "psc", constants.data() + 1024, 1024);
    LogParityDump(g_parity_frame, draw, "fetch", fetch.data(), 192);
  }
}

// Called before every counted probe. Swap ends a frame; the four draws are logged in parity mode.
void OnParityProbe(std::string_view name, const PPCContext& ctx, const uint8_t* base) {
  if (name == "sub_8298D430") {
    std::lock_guard<std::mutex> hold(g_parity_lock);
    ++g_parity_frame;
    g_parity_draw = 0;
    return;
  }
  if (name == "sub_82982718") {
    std::lock_guard<std::mutex> hold(g_parity_lock);
    g_last_pixel_shader = kHelperPixelShader;
    return;
  }
  if (name == "sub_8298F378") {
    std::lock_guard<std::mutex> hold(g_parity_lock);
    MirrorLoadAluConstants(base, ctx.r4.u32, ctx.r5.u32);
    return;
  }
  if (name == "sub_8298E5B8") {
    std::lock_guard<std::mutex> hold(g_parity_lock);
    MirrorFlushAluShadow(base, ctx.r4.u64, ctx.r5.u32, ctx.r6.u32);
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
  if (kParityEnabled) {
    std::lock_guard<std::mutex> hold(g_parity_lock);
    g_last_pixel_shader = kClearPixelShader;
  }
}

// The shadow flush is not in probe_hooks.inc; the mirror needs it (counted nowhere).
REX_EXTERN(__imp__sub_8298E5B8);
REX_HOOK_RAW(sub_8298E5B8) {
  if (!kParityEnabled) {
    __imp__sub_8298E5B8(ctx, base);
    return;
  }
  const PPCContext args = ctx;
  __imp__sub_8298E5B8(ctx, base);
  OnParityProbe("sub_8298E5B8", args, base);
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
