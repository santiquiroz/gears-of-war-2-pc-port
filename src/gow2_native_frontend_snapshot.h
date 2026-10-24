// gow2 - ReXGlue Recompiled Project
//
// Native frontend S2 (docs/superpowers/specs/2026-10-23-full-native-renderer-design.md, "S2 design"): the draw
// snapshot the game hands to the command processor through a rewritten DRAW_INDX packet, and the walk that finds
// the library packets the snapshot makes redundant (S2b+c). The SDK defines the identical struct; field order and
// types must match on both sides.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rex::glue::native_frontend {

constexpr uint32_t kPacketMagic = 0x5654414E;    // 'NATV'
constexpr uint32_t kSnapshotMagic = 0x3253444E;  // 'NDS2'

constexpr uint32_t kDrawIndxHeader = 0xC0032201;  // type 3, DRAW_INDX (0x22), 4 payload dwords
constexpr uint32_t kNopHeader = 0xC0031000;       // type 3, NOP (0x10), 4 payload dwords
constexpr uint32_t kDrawIndxDwords = 5;           // header + payload
constexpr uint32_t kMaxScanDwords = 32;
constexpr uint32_t kMaxSuppressDwords = 16384;    // longer regions are left alone

constexpr uint32_t kSnapshotFlagSuppressed = 1u;  // this draw's library packets were turned into NOPs

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
  // NDS2 (S2b+c). Register counts are the ones sub_82980B20's writers emit (183.cpp:22400-22507).
  uint32_t window_offset;          // 0x2080; the scissor packet 0x00022080 writes 0 there (173.cpp:20214-20236)
  uint32_t g2280[21];              // 0x2280-0x2294, shadow dev+10596 (mask 24 bits 34-54, 183.cpp:22400-22420)
  uint32_t g2300[38];              // 0x2300-0x2325, shadow dev+10680 (mask 32 bits 0-37, 183.cpp:22431-22445)
  uint32_t g2380[8];               // 0x2380-0x2387, shadow dev+10832 (mask 32 bits 38-45, 183.cpp:22449-22467)
  uint32_t g2388[24];              // 0x2388-0x239F, shadow dev+10272: sub_8298E2E0 writes 4 registers per mask bit
                                   // (mask 32 bits 49-54, 183.cpp:22471-22485; 56.cpp:20414-20641)
  uint32_t bool_consts[40];        // 0x4900-0x4927 (8 BOOL + 32 LOOP), shadow dev+10112: the mask lis r4,-256 is
                                   // 0xFFFFFFFFFF000000, 40 bits (183.cpp:22489-22507)
  uint32_t vs_address, vs_dwords;  // vertex shader = object B (dev+12696), 0 when B is 0
  uint32_t ps_address, ps_dwords;  // pixel shader = object A (dev+12692), 0 when A is 0
  uint32_t flags;                  // kSnapshotFlagSuppressed
  uint32_t reserved;
};

#if INTPTR_MAX == INT64_MAX
static_assert(offsetof(NativeDrawSnapshot, alu) == 976);
static_assert(offsetof(NativeDrawSnapshot, window_offset) == 2000);
static_assert(offsetof(NativeDrawSnapshot, g2388) == 2272);
static_assert(offsetof(NativeDrawSnapshot, bool_consts) == 2368);
static_assert(offsetof(NativeDrawSnapshot, flags) == 2544);
static_assert(sizeof(NativeDrawSnapshot) == 2552);
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

// Registers the snapshot carries; a library packet that only writes these can become a NOP.
inline bool SnapshotCoversRegister(uint32_t r) {
  return (r >= 0x2000 && r <= 0x2005) || (r >= 0x2080 && r <= 0x2082) || (r >= 0x2100 && r <= 0x2114) ||
         (r >= 0x2180 && r <= 0x2181) || (r >= 0x2200 && r <= 0x220B) || (r >= 0x2280 && r <= 0x2294) ||
         (r >= 0x2300 && r <= 0x2325) || (r >= 0x2380 && r <= 0x239F) || (r >= 0x4000 && r <= 0x47FF) ||
         (r >= 0x4800 && r <= 0x48BF) || (r >= 0x4900 && r <= 0x4927);
}

inline bool SnapshotCoversRange(uint32_t first, uint32_t count) {
  for (uint32_t i = 0; i < count; ++i) {
    if (!SnapshotCoversRegister(first + i)) return false;
  }
  return count != 0;
}

// Type-3 NOP consuming the same payload as the packet whose header is h (count field in bits 16-29 for types 0
// and 3).
inline uint32_t NopHeaderFor(uint32_t h) { return 0xC0001000u | (h & 0x3FFF0000u); }

struct SuppressPlan {
  bool ok = false;             // false: the region did not parse, NOP nothing
  uint32_t kept = 0;           // type-0/1/3 packets left in place (type-2 padding not counted)
  std::vector<uint32_t> nops;  // indices of the packet headers to turn into NOPs
};

// region: the dwords one DrawIndexed call wrote before its DRAW_INDX header, host order. Walks the PM4 packets and
// lists those whose effect the snapshot replaces: type-0 writes to covered registers only, LOAD_ALU_CONSTANT,
// IM_LOAD, IM_LOAD_IMMEDIATE, and SET_CONSTANT into covered registers. The last packet must end exactly at the
// region's end.
inline void PlanSuppression(std::span<const uint32_t> region, SuppressPlan& plan) {
  plan.ok = false;
  plan.kept = 0;
  plan.nops.clear();
  if (region.size() > kMaxSuppressDwords) return;
  size_t i = 0;
  while (i < region.size()) {
    const uint32_t h = region[i];
    const uint32_t count = ((h >> 16) & 0x3FFFu) + 1;  // payload dwords for types 0 and 3
    size_t length = 1 + count;
    bool nop = false;
    switch (h >> 30) {
      case 0: {
        const uint32_t base = h & 0x7FFFu;
        nop = (h & 0x8000u) != 0 ? SnapshotCoversRegister(base) : SnapshotCoversRange(base, count);
        break;
      }
      case 1:  // two registers, header + 2 dwords
        length = 3;
        break;
      case 2:  // filler
        length = 1;
        break;
      default: {
        const uint32_t opcode = (h >> 8) & 0xFFu;
        if (opcode == 0x2F || opcode == 0x27 || opcode == 0x2B) {  // LOAD_ALU_CONSTANT, IM_LOAD, IM_LOAD_IMMEDIATE
          nop = true;
        } else if (opcode == 0x2D && count >= 2 && i + 1 < region.size()) {
          // SET_CONSTANT (SDK ExecutePacketType3_SET_CONSTANT): word 1 index in bits 0-10, type in bits 16-23;
          // count - 1 registers from the type's base.
          static constexpr uint32_t kSetConstantBase[5] = {0x4000, 0x4800, 0x4900, 0x4908, 0x2000};
          const uint32_t offset_type = region[i + 1];
          const uint32_t type = (offset_type >> 16) & 0xFFu;
          if (type < 5) nop = SnapshotCoversRange(kSetConstantBase[type] + (offset_type & 0x7FFu), count - 1);
        }
        break;
      }
    }
    if (length > region.size() - i) {
      plan.kept = 0;
      plan.nops.clear();
      return;
    }
    if (nop) {
      plan.nops.push_back(uint32_t(i));
    } else if ((h >> 30) != 2) {
      ++plan.kept;
    }
    i += length;
  }
  plan.ok = true;
}

}  // namespace rex::glue::native_frontend
