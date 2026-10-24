// gow2 - ReXGlue Recompiled Project
//
// Native frontend S2a, game side (docs/superpowers/specs/2026-10-23-full-native-renderer-design.md, "S2 design";
// device offsets from docs/NATIVE_RENDERER_DEVICE_LAYOUT.md). After DrawIndexed (sub_82980B20) writes its DRAW_INDX
// packet, the packet is rewritten in place into a NOP of the same length carrying 'NATV' and a host pointer to a
// NativeDrawSnapshot of the device state, so the command processor sees the draw in ring order. The ALU constants
// come from a mirror fed by the two functions that emit them, kept as 128 copy-on-write blocks of 16 dwords.
// S2b+c: with gow2_native_frontend_suppress, the state, constant and shader packets the same call wrote before the
// DRAW_INDX become NOPs too (PlanSuppression), and the snapshot carries the shaders and the remaining groups.

#include "gow2_native_frontend_snapshot.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc.h>

#include "gow2_engine_tick.h"
#include "gow2_guest.h"

REXCVAR_DEFINE_STRING(gow2_native_frontend, "off", "Display",
                      "Native frontend: DrawIndexed draws reach the GPU from the device state instead of the "
                      "library's packets. auto (off for now), on or off. Read once at start.");
REXCVAR_DEFINE_BOOL(gow2_native_frontend_suppress, true, "Display",
                    "Native frontend: turn the library's state, constant and shader packets of each rewritten "
                    "draw into NOPs, so they reach the GPU only from the snapshot. Read once at start.");

// tools/native/native_probe.cpp hooks the same functions; CMake defines GOW2_NATIVE_PROBE_BUILD when it adds it.
#if !defined(GOW2_NATIVE_PROBE_BUILD)

namespace rex::glue::native_frontend {
namespace {

using rex::glue::guest::LoadU32;
using rex::glue::guest::StoreU32;

constexpr uint32_t kFramesPerReport = 300;
constexpr uint32_t kBlocks = 128;          // 0x4000-0x47FF: 2048 dwords
constexpr uint32_t kBlockDwords = 16;
constexpr uint32_t kMirrorDwords = kBlocks * kBlockDwords;
constexpr uint32_t kArenaCount = 4;        // the command processor runs at most 2 frames behind
constexpr uint32_t kMaxSnapshotsPerArena = 4096;
constexpr size_t kBlockBytes = kBlockDwords * sizeof(uint32_t);
constexpr size_t kChunkBytes = size_t(1) << 20;
constexpr size_t kArenaCapBytes = size_t(kMaxSnapshotsPerArena) * (sizeof(NativeDrawSnapshot) + 64 * 1024);

// Device shadows (NATIVE_RENDERER_DEVICE_LAYOUT.md sections 2-4).
constexpr uint32_t kDevWritePointer = 48;
constexpr uint32_t kDevFetch = 1152;
constexpr uint32_t kDevSurf = 10368;
constexpr uint32_t kDevScissorTl = 10436;
constexpr uint32_t kDevScissorBr = 10440;
constexpr uint32_t kDevVp = 10444;
constexpr uint32_t kDevProg = 10528;
constexpr uint32_t kDevRs = 10548;
constexpr uint32_t kDevG2280 = 10596;
constexpr uint32_t kDevG2300 = 10680;
constexpr uint32_t kDevG2380 = 10832;
constexpr uint32_t kDevG2388 = 10272;
constexpr uint32_t kDevBoolConsts = 10112;
constexpr uint32_t kDevShaderA = 12692;  // pixel shader object
constexpr uint32_t kDevShaderB = 12696;  // vertex shader object

bool Enabled() {
  // Read once: the mirror only stays right if it sees every constant write from the start.
  static const bool enabled = [] {
    const std::string mode = REXCVAR_GET(gow2_native_frontend);
    const bool on = mode == "on";  // auto = off until S3 passes
    REXLOG_INFO("NATIVEFRONT game gow2_native_frontend '{}' -> {}", mode, on ? "on" : "off");
    return on;
  }();
  return enabled;
}

bool Suppress() {
  static const bool suppress = [] {
    const bool on = REXCVAR_GET(gow2_native_frontend_suppress);
    REXLOG_INFO("NATIVEFRONT game gow2_native_frontend_suppress -> {}", on ? "on" : "off");
    return on;
  }();
  return suppress;
}

// The shader formulas below are copies of the ones verified in tools/native/native_probe.cpp.

// The physical-address fixup the library applies before putting an address in a packet (120.cpp:20504-20510):
// clrlwi 3, plus 0x1000 when the top 12 bits are >= 0xE00.
uint32_t PhysicalAddress(uint32_t address) {
  return (address & 0x1FFFFFFFu) + ((((address >> 20) + 512u) & 0x1000u));
}

// A (dev+12692), IM_LOAD in sub_8298F600 (120.cpp:20489-20530): [Phys(*(S_A+40) + *(A+24)), *(S_A+44) >> 2] with
// S_A = A + *(A+64).
void MicrocodeOfA(const uint8_t* base, uint32_t a, uint32_t& address, uint32_t& dwords) {
  const uint32_t s = a + LoadU32(base, a + 64);
  address = PhysicalAddress(LoadU32(base, s + 40) + LoadU32(base, a + 24)) & ~3u;
  dwords = LoadU32(base, s + 44) >> 2;
}

// B (dev+12696), IM_LOAD_IMMEDIATE through sub_8298F210 (18.cpp:19104-19160, 120.cpp:20327-20336, 20785-20795):
// [Phys(*(S+872) + *(B+32)), *(S+876) >> 2] with S = B + *(B + 896 + 8·variant), variant = (*(B+872) & 0x20) != 0.
void MicrocodeOfB(const uint8_t* base, uint32_t b, uint32_t& address, uint32_t& dwords) {
  const uint32_t variant = (LoadU32(base, b + 872) & 0x20u) != 0 ? 1u : 0u;
  const uint32_t s = b + LoadU32(base, b + 896 + 8 * variant);
  address = PhysicalAddress(LoadU32(base, s + 872) + LoadU32(base, b + 32)) & ~3u;
  dwords = LoadU32(base, s + 876) >> 2;
}

struct Arena {
  std::vector<std::unique_ptr<uint8_t[]>> chunks;
  size_t chunk = 0;  // chunk being filled
  size_t used = 0;   // bytes used in it
  size_t bytes = 0;  // bytes handed out since the reset
  uint32_t snapshots = 0;

  void Reset() {
    chunk = 0;
    used = 0;
    bytes = 0;
    snapshots = 0;
  }

  // size <= kChunkBytes; the caller has checked the cap.
  void* Allocate(size_t size) {
    used = (used + 7) & ~size_t(7);
    if (chunk < chunks.size() && used + size > kChunkBytes) {
      ++chunk;
      used = 0;
    }
    if (chunk == chunks.size()) chunks.push_back(std::make_unique<uint8_t[]>(kChunkBytes));
    void* out = chunks[chunk].get() + used;
    used += size;
    bytes += size;
    return out;
  }
};

std::mutex g_lock;  // the mirror, the arenas and the sequence
std::array<uint32_t, kMirrorDwords> g_work{};
std::array<bool, kBlocks> g_dirty{};
std::array<const uint32_t*, kBlocks> g_block{};
std::array<Arena, kArenaCount> g_arenas;
uint32_t g_current = 0;
uint64_t g_sequence = 0;
bool g_mirror_initialized = false;

std::atomic<uint64_t> g_rewritten{0};
std::atomic<uint64_t> g_not_inline{0};
std::atomic<uint64_t> g_arena_full{0};
std::atomic<uint64_t> g_suppressed_packets{0};
std::atomic<uint64_t> g_kept_packets{0};
std::atomic<uint64_t> g_aborted{0};
uint32_t g_frames = 0;

void InitializeMirrorLocked() {
  if (g_mirror_initialized) return;
  g_mirror_initialized = true;
  g_dirty.fill(true);
  g_block.fill(nullptr);
}

void WriteMirror(uint32_t index, uint32_t value) {
  if (index >= kMirrorDwords) return;
  g_work[index] = value;
  g_dirty[index / kBlockDwords] = true;
}

// Same walk as MirrorLoadAluConstants in tools/native/native_probe.cpp. sub_8298F378(r3 dev, r4 desc, r5 data)
// (22.cpp:20511-20693): header at desc + *(desc+20) (0 = nothing emitted); records {u16 start, u16 count, u32 offset}
// from header+20 to header+20+*(header+16), stopping at count 0; count dwords from offset + data go to register
// 0x4000 + start*4. The caller holds g_lock.
void MirrorLoadAluConstants(const uint8_t* base, uint32_t desc, uint32_t data) {
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
    for (uint32_t i = 0; i < count && start * 4 + i < kMirrorDwords; ++i) {
      WriteMirror(start * 4 + i, LoadU32(base, source + 4 * i));
    }
  }
}

// Same walk as MirrorFlushAluShadow in tools/native/native_probe.cpp. sub_8298E5B8(r3 dev, r4 mask, r5 register base
// 0x4000/0x4400, r6 shadow) (72.cpp:19964-20173): from bit 63 down, each run of n set bits after z clear bits copies
// n*16 dwords from shadow + 64*group bytes to register r5 + 16*group. The caller holds g_lock.
void MirrorFlushAluShadow(const uint8_t* base, uint64_t mask, uint32_t register_base, uint32_t shadow) {
  if (register_base < 0x4000u) return;
  const uint32_t offset = register_base - 0x4000u;
  uint32_t group = 0;
  while (mask != 0) {
    const uint32_t gap = uint32_t(__builtin_clzll(mask));
    mask <<= gap;  // gap < 64 since mask != 0
    group += gap;
    const uint32_t groups = ~mask == 0 ? 64u : uint32_t(__builtin_clzll(~mask));
    for (uint32_t g = 0; g < groups; ++g) {
      for (uint32_t i = 0; i < kBlockDwords; ++i) {
        WriteMirror(offset + 16 * (group + g) + i, LoadU32(base, shadow + 64 * (group + g) + 4 * i));
      }
    }
    group += groups;
    if (groups >= 64) break;
    mask <<= groups;
  }
}

// Copies the dirty blocks into the current arena and allocates the snapshot; nullptr when the arena is full.
// The caller holds g_lock.
NativeDrawSnapshot* AllocateSnapshotLocked() {
  InitializeMirrorLocked();
  Arena& arena = g_arenas[g_current];
  size_t dirty = 0;
  for (bool d : g_dirty) dirty += d ? 1 : 0;
  const size_t need = sizeof(NativeDrawSnapshot) + dirty * kBlockBytes + 8 * (dirty + 1);
  if (arena.snapshots >= kMaxSnapshotsPerArena || arena.bytes + need > kArenaCapBytes) return nullptr;
  for (uint32_t b = 0; b < kBlocks; ++b) {
    if (!g_dirty[b]) continue;
    auto* copy = static_cast<uint32_t*>(arena.Allocate(kBlockBytes));
    std::memcpy(copy, &g_work[b * kBlockDwords], kBlockBytes);
    g_block[b] = copy;
    g_dirty[b] = false;
  }
  ++arena.snapshots;
  return static_cast<NativeDrawSnapshot*>(arena.Allocate(sizeof(NativeDrawSnapshot)));
}

void ReadDwords(const uint8_t* base, uint32_t address, uint32_t* out, uint32_t count) {
  for (uint32_t i = 0; i < count; ++i) out[i] = LoadU32(base, address + 4 * i);
}

// After sub_82980B20 returned. Its inline path writes the header and the 4 payload dwords with stwu (pre-increment)
// and stores the pointer to the last one at dev+48 (183.cpp:22656-22681, 22817-22818), so the header sits 16 bytes
// before the write pointer. before is dev+48 at the call, so this call's packets start at before + 4.
void OnDrawIndexed(uint8_t* base, uint32_t device, uint32_t before) {
  const uint32_t write = LoadU32(base, device + kDevWritePointer);
  if (write < 4 * kMaxScanDwords) {
    g_not_inline.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  std::array<uint32_t, kMaxScanDwords> tail{};
  const uint32_t first = write - 4 * (kMaxScanDwords - 1);
  ReadDwords(base, first, tail.data(), kMaxScanDwords);
  const auto index = FindTrailingDrawIndx(tail);
  if (!index) {
    g_not_inline.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  const uint32_t header = first + 4 * uint32_t(*index);

  // S2b+c: plan the NOPs before touching the ring; nothing is written unless the snapshot exists.
  thread_local std::vector<uint32_t> region;
  thread_local SuppressPlan plan;
  bool suppress = false;
  if (Suppress()) {
    const uint32_t start = before + 4;
    // write < before: the ring wrapped (sub_8298C1A0) during the call.
    if (write >= before && header >= start && (header - start) / 4 <= kMaxSuppressDwords) {
      region.resize((header - start) / 4);
      ReadDwords(base, start, region.data(), uint32_t(region.size()));
      PlanSuppression(region, plan);
      suppress = plan.ok;
    }
    if (!suppress) g_aborted.fetch_add(1, std::memory_order_relaxed);
  }

  NativeDrawSnapshot* snapshot = nullptr;
  {
    std::lock_guard<std::mutex> hold(g_lock);
    snapshot = AllocateSnapshotLocked();
    if (snapshot == nullptr) {
      g_arena_full.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    snapshot->magic = kSnapshotMagic;
    snapshot->size = uint32_t(sizeof(NativeDrawSnapshot));
    snapshot->sequence = ++g_sequence;
    for (uint32_t b = 0; b < kBlocks; ++b) snapshot->alu[b] = g_block[b];
  }
  ReadDwords(base, header + 4, snapshot->draw_words, 4);
  ReadDwords(base, device + kDevSurf, snapshot->surf, 6);
  ReadDwords(base, device + kDevProg, snapshot->prog, 2);
  ReadDwords(base, device + kDevRs, snapshot->rs, 12);
  ReadDwords(base, device + kDevVp, snapshot->vp, 21);
  snapshot->scissor[0] = LoadU32(base, device + kDevScissorTl);
  snapshot->scissor[1] = LoadU32(base, device + kDevScissorBr);
  ReadDwords(base, device + kDevFetch, snapshot->fetch, 192);
  snapshot->window_offset = 0;
  ReadDwords(base, device + kDevG2280, snapshot->g2280, 21);
  ReadDwords(base, device + kDevG2300, snapshot->g2300, 38);
  ReadDwords(base, device + kDevG2380, snapshot->g2380, 8);
  ReadDwords(base, device + kDevG2388, snapshot->g2388, 24);
  ReadDwords(base, device + kDevBoolConsts, snapshot->bool_consts, 40);
  snapshot->vs_address = snapshot->vs_dwords = 0;
  snapshot->ps_address = snapshot->ps_dwords = 0;
  if (const uint32_t b = LoadU32(base, device + kDevShaderB); b != 0) {
    MicrocodeOfB(base, b, snapshot->vs_address, snapshot->vs_dwords);
  }
  if (const uint32_t a = LoadU32(base, device + kDevShaderA); a != 0) {
    MicrocodeOfA(base, a, snapshot->ps_address, snapshot->ps_dwords);
  }
  snapshot->flags = 0;
  snapshot->reserved = 0;

  if (suppress) {
    const uint32_t start = before + 4;
    for (uint32_t i : plan.nops) StoreU32(base, start + 4 * i, NopHeaderFor(region[i]));
    snapshot->flags |= kSnapshotFlagSuppressed;
    g_suppressed_packets.fetch_add(plan.nops.size(), std::memory_order_relaxed);
    g_kept_packets.fetch_add(plan.kept, std::memory_order_relaxed);
  }

  const uint64_t pointer = uint64_t(reinterpret_cast<uintptr_t>(snapshot));
  StoreU32(base, header, kNopHeader);
  StoreU32(base, header + 4, kPacketMagic);
  StoreU32(base, header + 8, uint32_t(pointer));
  StoreU32(base, header + 12, uint32_t(pointer >> 32));
  StoreU32(base, header + 16, 0);
  g_rewritten.fetch_add(1, std::memory_order_relaxed);
}

void OnEngineTick(const PPCContext&, uint8_t*) {
  if (!Enabled()) return;
  {
    std::lock_guard<std::mutex> hold(g_lock);
    InitializeMirrorLocked();
    g_current = (g_current + 1) % kArenaCount;
    g_arenas[g_current].Reset();
    // Blocks may point into the arena just reset: copy every block again on the next snapshot.
    g_dirty.fill(true);
  }
  if (++g_frames % kFramesPerReport != 0) return;
  size_t arena_bytes = 0;
  {
    std::lock_guard<std::mutex> hold(g_lock);
    for (const Arena& arena : g_arenas) arena_bytes += arena.chunks.size() * kChunkBytes;
  }
  REXLOG_INFO("NATIVEFRONT game rewritten {} not_inline {} arena_kb {} suppressed_packets {} kept_packets {} "
              "aborted {}",
              g_rewritten.exchange(0, std::memory_order_relaxed),
              g_not_inline.exchange(0, std::memory_order_relaxed), arena_bytes / 1024,
              g_suppressed_packets.exchange(0, std::memory_order_relaxed),
              g_kept_packets.exchange(0, std::memory_order_relaxed),
              g_aborted.exchange(0, std::memory_order_relaxed));
  const uint64_t full = g_arena_full.exchange(0, std::memory_order_relaxed);
  if (full != 0) REXLOG_INFO("NATIVEFRONT game arena_full {}", full);
}

[[maybe_unused]] const bool kTickRegistered = (rex::glue::AddEngineTickListener(&OnEngineTick), true);

}  // namespace
}  // namespace rex::glue::native_frontend

// LOAD_ALU_CONSTANT emitter.
REX_EXTERN(__imp__sub_8298F378);
REX_HOOK_RAW(sub_8298F378) {
  if (!rex::glue::native_frontend::Enabled()) {
    __imp__sub_8298F378(ctx, base);
    return;
  }
  const uint32_t desc = ctx.r4.u32;
  const uint32_t data = ctx.r5.u32;
  __imp__sub_8298F378(ctx, base);
  std::lock_guard<std::mutex> hold(rex::glue::native_frontend::g_lock);
  rex::glue::native_frontend::MirrorLoadAluConstants(base, desc, data);
}

// ALU constant shadow flush.
REX_EXTERN(__imp__sub_8298E5B8);
REX_HOOK_RAW(sub_8298E5B8) {
  if (!rex::glue::native_frontend::Enabled()) {
    __imp__sub_8298E5B8(ctx, base);
    return;
  }
  const uint64_t mask = ctx.r4.u64;
  const uint32_t register_base = ctx.r5.u32;
  const uint32_t shadow = ctx.r6.u32;
  __imp__sub_8298E5B8(ctx, base);
  std::lock_guard<std::mutex> hold(rex::glue::native_frontend::g_lock);
  rex::glue::native_frontend::MirrorFlushAluShadow(base, mask, register_base, shadow);
}

// DrawIndexed.
REX_EXTERN(__imp__sub_82980B20);
REX_HOOK_RAW(sub_82980B20) {
  if (!rex::glue::native_frontend::Enabled()) {
    __imp__sub_82980B20(ctx, base);
    return;
  }
  const uint32_t device = ctx.r3.u32;
  const uint32_t before = rex::glue::guest::LoadU32(base, device + rex::glue::native_frontend::kDevWritePointer);
  __imp__sub_82980B20(ctx, base);
  rex::glue::native_frontend::OnDrawIndexed(base, device, before);
}

#endif  // !defined(GOW2_NATIVE_PROBE_BUILD)
