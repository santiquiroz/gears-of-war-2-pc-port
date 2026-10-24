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
constexpr uint32_t kSnapshotMagic = 0x3353444E;  // 'NDS3'

constexpr uint32_t kDrawIndxHeader = 0xC0032201;  // type 3, DRAW_INDX (0x22), 4 payload dwords
constexpr uint32_t kNopHeader = 0xC0031000;       // type 3, NOP (0x10), 4 payload dwords
constexpr uint32_t kOpcodeDrawIndx = 0x22;
constexpr uint32_t kOpcodeDrawIndx2 = 0x36;
constexpr uint32_t kMaxDrawPayload = 4;           // draw_words; longer draws (inline indices) are left alone
constexpr uint32_t kMarkerDwords = 5;             // NOP header, 'NATV', pointer low, pointer high, 0
constexpr uint32_t kMaxSuppressDwords = 16384;    // longer regions are left alone

constexpr uint32_t kSnapshotFlagSuppressed = 1u << 0;  // this draw's library packets were turned into NOPs
constexpr uint32_t kSnapshotFlagShaders = 1u << 1;     // vs/ps are the shaders bound for this draw

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
  uint32_t flags;                  // kSnapshotFlagSuppressed, kSnapshotFlagShaders
  uint32_t reserved;
  // NDS3 (S3).
  uint32_t draw_opcode;            // kOpcodeDrawIndx or kOpcodeDrawIndx2
  uint32_t draw_word_count;        // payload dwords 1-4 in draw_words, the rest 0
};

#if INTPTR_MAX == INT64_MAX
static_assert(offsetof(NativeDrawSnapshot, alu) == 976);
static_assert(offsetof(NativeDrawSnapshot, window_offset) == 2000);
static_assert(offsetof(NativeDrawSnapshot, g2388) == 2272);
static_assert(offsetof(NativeDrawSnapshot, bool_consts) == 2368);
static_assert(offsetof(NativeDrawSnapshot, flags) == 2544);
static_assert(offsetof(NativeDrawSnapshot, draw_opcode) == 2552);
static_assert(sizeof(NativeDrawSnapshot) == 2560);
#endif

inline bool IsDrawOpcode(uint32_t opcode) {
  // DRAW_INDX, DRAW_INDX_BIN, DRAW_INDX_2_BIN, DRAW_INDX_2 (xenos.h PM4_DRAW_*).
  return opcode == kOpcodeDrawIndx || opcode == 0x34 || opcode == 0x35 || opcode == kOpcodeDrawIndx2;
}

struct LastDraw {
  size_t index;          // header index in emitted
  uint32_t opcode;       // kOpcodeDrawIndx or kOpcodeDrawIndx2
  uint32_t payload;      // payload dwords; over kMaxDrawPayload means unsupported
  uint32_t draws_seen;   // draw packets of any kind in emitted, this one included
  uint32_t other_draws;  // DRAW_INDX_BIN / DRAW_INDX_2_BIN packets among them
};

// emitted holds every dword one draw call wrote, from the write pointer at the call + 4 through the write pointer
// after it (dev+48 points at the last dword written: the library writes the ring with stwu), host order. Walks the
// PM4 packets from the start and returns the last DRAW_INDX or DRAW_INDX_2 packet; packets after it (state restores,
// bin masks) are allowed. nullopt when a packet runs past the end or there is no such draw.
inline std::optional<LastDraw> FindLastDraw(std::span<const uint32_t> emitted) {
  std::optional<LastDraw> last;
  uint32_t draws_seen = 0;
  uint32_t other_draws = 0;
  size_t i = 0;
  while (i < emitted.size()) {
    const uint32_t h = emitted[i];
    size_t length = 2 + ((h >> 16) & 0x3FFFu);  // types 0 and 3: header + count + 1 payload dwords
    if ((h >> 30) == 1) length = 3;
    if ((h >> 30) == 2) length = 1;
    if (length > emitted.size() - i) return std::nullopt;
    if ((h >> 30) == 3) {
      const uint32_t opcode = (h >> 8) & 0xFFu;
      if (IsDrawOpcode(opcode)) {
        ++draws_seen;
        if (opcode == kOpcodeDrawIndx || opcode == kOpcodeDrawIndx2) {
          last = LastDraw{i, opcode, uint32_t(length - 1), 0, 0};
        } else {
          ++other_draws;
        }
      }
    }
    i += length;
  }
  if (last) {
    last->draws_seen = draws_seen;
    last->other_draws = other_draws;
  }
  return last;
}

struct DrawPacket {
  size_t index;      // header index in emitted
  uint32_t opcode;   // kOpcodeDrawIndx or kOpcodeDrawIndx2
  uint32_t payload;  // payload dwords; over kMaxDrawPayload means unsupported
};

struct AllDraws {
  bool walk_ok = false;          // every packet fits in emitted
  bool other_draws = false;      // a DRAW_INDX_BIN / DRAW_INDX_2_BIN is present: the caller leaves everything alone
  std::vector<DrawPacket> draws;  // the DRAW_INDX / DRAW_INDX_2 packets, in ring order; empty when !walk_ok
};

// Same walk as FindLastDraw, listing every DRAW_INDX and DRAW_INDX_2 packet one hooked call wrote (Clear writes
// several through sub_829813F0).
inline void FindAllDraws(std::span<const uint32_t> emitted, AllDraws& out) {
  out.walk_ok = false;
  out.other_draws = false;
  out.draws.clear();
  size_t i = 0;
  while (i < emitted.size()) {
    const uint32_t h = emitted[i];
    size_t length = 2 + ((h >> 16) & 0x3FFFu);  // types 0 and 3: header + count + 1 payload dwords
    if ((h >> 30) == 1) length = 3;
    if ((h >> 30) == 2) length = 1;
    if (length > emitted.size() - i) {
      out.draws.clear();
      return;
    }
    if ((h >> 30) == 3) {
      const uint32_t opcode = (h >> 8) & 0xFFu;
      if (opcode == kOpcodeDrawIndx || opcode == kOpcodeDrawIndx2) {
        out.draws.push_back(DrawPacket{i, opcode, uint32_t(length - 1)});
      } else if (IsDrawOpcode(opcode)) {
        out.other_draws = true;
      }
    }
    i += length;
  }
  out.walk_ok = true;
}

inline AllDraws FindAllDraws(std::span<const uint32_t> emitted) {
  AllDraws out;
  FindAllDraws(emitted, out);
  return out;
}

// Segment k: the packets after draw k-1 (from 0 for the first draw) up to draw k's header. Packets after the last
// draw belong to no segment.
inline std::span<const uint32_t> DrawSegment(std::span<const uint32_t> emitted, const AllDraws& all, size_t k) {
  const size_t first = k == 0 ? 0 : all.draws[k - 1].index + all.draws[k - 1].payload + 1;
  return emitted.subspan(first, all.draws[k].index - first);
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

struct RegisterWrite {
  uint32_t reg;
  uint32_t value;
};

struct SuppressPlan {
  bool ok = false;                    // false: the region did not parse or holds another draw, NOP nothing
  uint32_t kept = 0;                  // type-0/1/3 packets left in place (type-2 padding not counted)
  std::vector<uint32_t> nops;         // indices of the packet headers to turn into NOPs, ascending
  std::vector<RegisterWrite> writes;  // what the NOPed type-0 and SET_CONSTANT packets wrote, in ring order
};

// region: the dwords one draw call wrote before its draw packet header, host order. Walks the PM4 packets and
// lists those whose effect the snapshot replaces: type-0 writes to covered registers only, LOAD_ALU_CONSTANT,
// IM_LOAD, IM_LOAD_IMMEDIATE, and SET_CONSTANT into covered registers. The register values of the type-0 and
// SET_CONSTANT ones go to plan.writes, to be folded into the snapshot (the shadows may not hold them). The last
// packet must end exactly at the region's end; another draw packet in the region aborts. nop_shaders false keeps
// IM_LOAD and IM_LOAD_IMMEDIATE (the snapshot does not carry the shaders, as for Clear's own two).
inline void PlanSuppression(std::span<const uint32_t> region, SuppressPlan& plan, bool nop_shaders = true) {
  plan.ok = false;
  plan.kept = 0;
  plan.nops.clear();
  plan.writes.clear();
  auto fail = [&plan] {
    plan.kept = 0;
    plan.nops.clear();
    plan.writes.clear();
  };
  if (region.size() > kMaxSuppressDwords) return;
  size_t i = 0;
  while (i < region.size()) {
    const uint32_t h = region[i];
    const uint32_t count = ((h >> 16) & 0x3FFFu) + 1;  // payload dwords for types 0 and 3
    size_t length = 1 + count;
    // Types 0 and 3 read their payload below: check it fits first.
    if ((h >> 30) != 1 && (h >> 30) != 2 && length > region.size() - i) return fail();
    bool nop = false;
    switch (h >> 30) {
      case 0: {
        const uint32_t base = h & 0x7FFFu;
        const bool one_register = (h & 0x8000u) != 0;
        nop = one_register ? SnapshotCoversRegister(base) : SnapshotCoversRange(base, count);
        if (nop) {
          for (uint32_t k = 0; k < count; ++k) {
            plan.writes.push_back({one_register ? base : base + k, region[i + 1 + k]});
          }
        }
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
        if (IsDrawOpcode(opcode)) return fail();  // the snapshot is the state of the last draw only
        if (opcode == 0x2F) {  // LOAD_ALU_CONSTANT
          nop = true;          // the mirror already follows sub_8298F378
        } else if (opcode == 0x27 || opcode == 0x2B) {  // IM_LOAD, IM_LOAD_IMMEDIATE
          nop = nop_shaders;
        } else if (opcode == 0x2D && count >= 2) {
          // SET_CONSTANT (SDK ExecutePacketType3_SET_CONSTANT): word 1 index in bits 0-10, type in bits 16-23;
          // count - 1 registers from the type's base.
          static constexpr uint32_t kSetConstantBase[5] = {0x4000, 0x4800, 0x4900, 0x4908, 0x2000};
          const uint32_t offset_type = region[i + 1];
          const uint32_t type = (offset_type >> 16) & 0xFFu;
          if (type < 5) {
            const uint32_t first = kSetConstantBase[type] + (offset_type & 0x7FFu);
            nop = SnapshotCoversRange(first, count - 1);
            if (nop) {
              for (uint32_t k = 0; k + 1 < count; ++k) plan.writes.push_back({first + k, region[i + 2 + k]});
            }
          }
        }
        break;
      }
    }
    if (length > region.size() - i) return fail();
    if (nop) {
      plan.nops.push_back(uint32_t(i));
    } else if ((h >> 30) != 2) {
      ++plan.kept;
    }
    i += length;
  }
  plan.ok = true;
}

// Plans segment k (DrawSegment) with PlanSuppression's rules and returns it; plan.nops index into the returned span,
// whose end is draw k's header (the region MarkerSpanStart takes).
inline std::span<const uint32_t> PlanSegment(std::span<const uint32_t> emitted, const AllDraws& all, size_t k,
                                             bool nop_shaders, SuppressPlan& plan) {
  const std::span<const uint32_t> region = DrawSegment(emitted, all, k);
  PlanSuppression(region, plan, nop_shaders);
  return region;
}

// The snapshot dword holding register r, nullptr for 0x4000-0x47FF (the ALU blocks, folded through the mirror) and
// registers the snapshot does not carry. Same register ranges as SnapshotCoversRegister.
inline uint32_t* SnapshotRegisterSlot(NativeDrawSnapshot& s, uint32_t r) {
  if (r >= 0x2000 && r <= 0x2005) return &s.surf[r - 0x2000];
  if (r == 0x2080) return &s.window_offset;
  if (r >= 0x2081 && r <= 0x2082) return &s.scissor[r - 0x2081];
  if (r >= 0x2100 && r <= 0x2114) return &s.vp[r - 0x2100];
  if (r >= 0x2180 && r <= 0x2181) return &s.prog[r - 0x2180];
  if (r >= 0x2200 && r <= 0x220B) return &s.rs[r - 0x2200];
  if (r >= 0x2280 && r <= 0x2294) return &s.g2280[r - 0x2280];
  if (r >= 0x2300 && r <= 0x2325) return &s.g2300[r - 0x2300];
  if (r >= 0x2380 && r <= 0x2387) return &s.g2380[r - 0x2380];
  if (r >= 0x2388 && r <= 0x239F) return &s.g2388[r - 0x2388];
  if (r >= 0x4800 && r <= 0x48BF) return &s.fetch[r - 0x4800];
  if (r >= 0x4900 && r <= 0x4927) return &s.bool_consts[r - 0x4900];
  return nullptr;
}

// Applies the non-ALU writes, in order, over a snapshot already filled from the shadows.
inline void FoldRegisterWrites(std::span<const RegisterWrite> writes, NativeDrawSnapshot& s) {
  for (const RegisterWrite& w : writes) {
    if (uint32_t* slot = SnapshotRegisterSlot(s, w.reg)) *slot = w.value;
  }
}

// Where the NATV marker starts, as an index into region (region.size() = the draw header itself). A draw of at least
// kMarkerDwords is rewritten in place. A shorter one takes over the NOPed packets right before it (they end where the
// next begins, no filler between) until the span has kMarkerDwords; nullopt when there are not enough of them.
inline std::optional<size_t> MarkerSpanStart(std::span<const uint32_t> region, const SuppressPlan& plan,
                                             uint32_t draw_dwords) {
  size_t start = region.size();
  if (draw_dwords >= kMarkerDwords) return start;
  if (!plan.ok) return std::nullopt;
  for (size_t k = plan.nops.size(); k-- > 0;) {
    const size_t header = plan.nops[k];
    if (header + 2 + ((region[header] >> 16) & 0x3FFFu) != start) break;
    start = header;
    if (region.size() - start + draw_dwords >= kMarkerDwords) return start;
  }
  return std::nullopt;
}

// Header of a NOP covering span_dwords dwords (header included).
inline uint32_t NopHeaderForSpan(uint32_t span_dwords) { return 0xC0001000u | ((span_dwords - 2) << 16); }

}  // namespace rex::glue::native_frontend
