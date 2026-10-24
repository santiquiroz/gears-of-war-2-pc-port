// gow2 - ReXGlue Recompiled Project
//
// Native frontend S2 (docs/superpowers/specs/2026-10-23-full-native-renderer-design.md, "S2 design"): the draw
// snapshot the game hands to the command processor through a rewritten DRAW_INDX packet. The SDK defines the
// identical struct; field order and types must match on both sides.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace rex::glue::native_frontend {

constexpr uint32_t kPacketMagic = 0x5654414E;    // 'NATV'
constexpr uint32_t kSnapshotMagic = 0x3153444E;  // 'NDS1'

constexpr uint32_t kDrawIndxHeader = 0xC0032201;  // type 3, DRAW_INDX (0x22), 4 payload dwords
constexpr uint32_t kNopHeader = 0xC0031000;       // type 3, NOP (0x10), 4 payload dwords
constexpr uint32_t kDrawIndxDwords = 5;           // header + payload
constexpr uint32_t kMaxScanDwords = 32;

struct NativeDrawSnapshot {
  uint32_t magic;
  uint32_t size;
  uint64_t sequence;
  uint32_t draw_words[4];
  uint32_t surf[6];
  uint32_t prog[2];
  uint32_t rs[12];
  uint32_t vp[21];
  uint32_t scissor[2];
  uint32_t fetch[192];
  const uint32_t* alu[128];
};

#if INTPTR_MAX == INT64_MAX
static_assert(offsetof(NativeDrawSnapshot, alu) == 976);
static_assert(sizeof(NativeDrawSnapshot) == 2000);
#endif

// tail holds the last ring dwords up to and including the one at the device's write pointer (dev+48), host order.
// The library writes the ring with stwu, so dev+48 points at the last dword written, not past it. Returns the index
// in tail of the DRAW_INDX header when that packet is the last one written (its last payload dword is tail.back()),
// nullopt otherwise. Looks back at most kMaxScanDwords for the nearest header.
inline std::optional<size_t> FindTrailingDrawIndx(std::span<const uint32_t> tail) {
  const size_t n = tail.size();
  const size_t limit = n < kMaxScanDwords ? n : kMaxScanDwords;
  for (size_t back = kDrawIndxDwords; back <= limit; ++back) {
    if (tail[n - back] != kDrawIndxHeader) continue;
    if (back == kDrawIndxDwords) return n - back;
    return std::nullopt;  // nearest DRAW_INDX is followed by other packets
  }
  return std::nullopt;
}

}  // namespace rex::glue::native_frontend
