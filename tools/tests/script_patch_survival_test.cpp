// Tests for the Stage 2 survival rows (src/gow2_script_patch_survival.cpp) on a synthetic guest memory built from the rows themselves; no game files needed. Build target gow2_script_patch_survival_test.
#include "gow2_script_patch_survival.h"
#include "gow2_script_patches.h"

#include "gow2_guest.h"

#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#if !defined(_WIN32)
#ifndef MAP_ANONYMOUS
#ifdef MAP_ANON
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif
#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif
#endif

namespace {

using namespace rex::glue::script_patch;

int g_checks = 0;
int g_failures = 0;

void Expect(bool ok, std::string_view what) {
  ++g_checks;
  if (ok) return;
  ++g_failures;
  std::printf("FAIL: %.*s\n", static_cast<int>(what.size()), what.data());
}

// Independent of guest::PhysicalHostOffset so a wrong header fails the test.
constexpr uint32_t TestHostOffset(uint32_t addr) {
#if defined(_WIN32) || (defined(__APPLE__) && defined(__aarch64__))
  return addr >= 0xE0000000u ? 0x1000u : 0u;
#else
  (void)addr;
  return 0u;
#endif
}

constexpr size_t GuestReservationSize() { return 0x1'0000'0000ULL + 0x20000; }

constexpr uint32_t kLowCommitBytes = 4u * 1024u * 1024u;
constexpr uint32_t kHighCommitStart = 0xDFFF0000u;
constexpr uint32_t kHighCommitEnd = 0xE0020000u;
constexpr uint32_t kPhysAllocStart = 0xE0000000u;
constexpr uint32_t kPhysAllocEnd = 0xE0010000u;

uint8_t* ReserveGuestRange() {
#if defined(_WIN32)
  return static_cast<uint8_t*>(VirtualAlloc(nullptr, GuestReservationSize(), MEM_RESERVE, PAGE_NOACCESS));
#else
  void* p = mmap(nullptr, GuestReservationSize(), PROT_NONE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  return (p == MAP_FAILED) ? nullptr : static_cast<uint8_t*>(p);
#endif
}

bool CommitGuestRange(uint8_t* base, uint32_t start, uint32_t len) {
#if defined(_WIN32)
  return VirtualAlloc(base + start, len, MEM_COMMIT, PAGE_READWRITE) != nullptr;
#else
  return mprotect(base + start, len, PROT_READ | PROT_WRITE) == 0;
#endif
}

void ReleaseGuestRange(uint8_t* base) {
#if defined(_WIN32)
  VirtualFree(base, 0, MEM_RELEASE);
#else
  munmap(base, GuestReservationSize());
#endif
}

std::shared_ptr<uint8_t> MakeGuestMemory() {
  uint8_t* base = ReserveGuestRange();
  if (base == nullptr) return {};
  bool low = CommitGuestRange(base, 0u, kLowCommitBytes);
  bool high = CommitGuestRange(base, kHighCommitStart, kHighCommitEnd - kHighCommitStart);
  if (!low || !high) {
    ReleaseGuestRange(base);
    return {};
  }
  return std::shared_ptr<uint8_t>(base, ReleaseGuestRange);
}

std::vector<std::string_view> SplitDotted(std::string_view dotted) {
  std::vector<std::string_view> parts;
  size_t start = 0u;
  while (true) {
    size_t dot = dotted.find('.', start);
    size_t end = (dot == std::string_view::npos) ? dotted.size() : dot;
    parts.push_back(dotted.substr(start, end - start));
    if (dot == std::string_view::npos) break;
    start = dot + 1u;
  }
  return parts;
}

std::string ClassForPart(size_t total, size_t index, std::string_view cls) {
  if (total == 1u) return std::string(cls.data(), cls.size());
  if (index == 0u) return "Package";
  if (index + 1u == total) return std::string(cls.data(), cls.size());
  return "Class";
}

class FakeGuest {
 public:
  FakeGuest() : mem_(MakeGuestMemory()) {}
  uint8_t* base() { return mem_.get(); }
  const uint8_t* base() const { return mem_.get(); }
  uint8_t* Host(uint32_t addr) { return base() + addr + TestHostOffset(addr); }
  const uint8_t* Host(uint32_t addr) const { return base() + addr + TestHostOffset(addr); }
  uint32_t Alloc(uint32_t size) {
    uint32_t aligned = (brk_ + 15u) & ~15u;
    size_t need = static_cast<size_t>(aligned) + static_cast<size_t>(size);
    if (need > kLowCommitBytes) return 0u;
    brk_ = aligned + size;
    return aligned;
  }
  uint32_t AllocPhysical(uint32_t size) {
    uint32_t aligned = (phys_brk_ + 15u) & ~15u;
    uint64_t need = static_cast<uint64_t>(aligned) + size;
    if (need > kPhysAllocEnd) return 0u;
    phys_brk_ = aligned + size;
    return aligned;
  }
  uint32_t Top() const { return (brk_ + 15u) & ~15u; }
  uint8_t U8(uint32_t addr) const { return *Host(addr); }
  uint32_t U32(uint32_t addr) const {
    // One Host call per byte so a value crossing 0xE0000000 lands correctly.
    return (static_cast<uint32_t>(*Host(addr)) << 24u) |
           (static_cast<uint32_t>(*Host(addr + 1u)) << 16u) |
           (static_cast<uint32_t>(*Host(addr + 2u)) << 8u) |
           static_cast<uint32_t>(*Host(addr + 3u));
  }
  void Put8(uint32_t addr, uint8_t value) { *Host(addr) = value; }
  void Put32(uint32_t addr, uint32_t value) {
    // One Host call per byte so a value crossing 0xE0000000 lands correctly.
    *Host(addr) = static_cast<uint8_t>(value >> 24u);
    *Host(addr + 1u) = static_cast<uint8_t>(value >> 16u);
    *Host(addr + 2u) = static_cast<uint8_t>(value >> 8u);
    *Host(addr + 3u) = static_cast<uint8_t>(value);
  }
  uint32_t Name(std::string_view text, bool wide = false) {
    std::string folded;
    folded.reserve(text.size());
    for (char ch : text) {
      char lower = ch;
      if (lower >= 'A' && lower <= 'Z') lower = static_cast<char>(lower - 'A' + 'a');
      folded.push_back(lower);
    }
    // Fold case so a wide entry added first is reused.
    auto found = name_by_folded_.find(folded);
    if (found != name_by_folded_.end()) return found->second;
    uint32_t index = static_cast<uint32_t>(name_addrs_.size());
    uint32_t len = static_cast<uint32_t>(text.size());
    uint32_t size = wide ? (8u + len * 2u + 2u) : (8u + len + 1u);
    uint32_t addr = Alloc(size);
    if (addr != 0u) {
      Put32(addr, wide ? 1u : 0u);
      Put32(addr + 4u, 0u);
      if (wide) {
        for (size_t i = 0u; i < text.size(); ++i) {
          uint32_t at = addr + 8u + static_cast<uint32_t>(i) * 2u;
          Put8(at, static_cast<uint8_t>(0));
          Put8(at + 1u, static_cast<uint8_t>(text[i]));
        }
        Put8(addr + 8u + len * 2u, static_cast<uint8_t>(0));
        Put8(addr + 8u + len * 2u + 1u, static_cast<uint8_t>(0));
      } else {
        for (size_t i = 0u; i < text.size(); ++i) {
          Put8(addr + 8u + static_cast<uint32_t>(i), static_cast<uint8_t>(text[i]));
        }
        Put8(addr + 8u + len, static_cast<uint8_t>(0));
      }
    }
    name_addrs_.push_back(addr);
    name_by_folded_[folded] = index;
    return index;
  }
  uint32_t Object(std::string_view name, uint32_t outer, uint32_t cls, uint32_t size = 96u) {
    uint32_t index = Name(name, false);
    if (size < 96u) size = 96u;
    uint32_t addr = Alloc(size);
    if (addr != 0u) {
      Put32(addr + 40u, outer);
      Put32(addr + 44u, index);
      Put32(addr + 48u, 0u);
      Put32(addr + 52u, cls);
    }
    object_addrs_.push_back(addr);
    return addr;
  }
  uint32_t ClassFor(std::string_view cls) {
    EnsureBootstrap();
    std::string key(cls.data(), cls.size());
    auto found = class_cache_.find(key);
    if (found != class_cache_.end()) return found->second;
    std::string path_key = "Core." + key + "|Class";
    auto reused = path_cache_.find(path_key);
    if (reused != path_cache_.end()) {
      class_cache_[key] = reused->second;
      return reused->second;
    }
    uint32_t core = path_cache_.at("Core|Package");
    uint32_t class_cls = class_cache_.at("Class");
    uint32_t addr = Object(cls, core, class_cls);
    class_cache_[key] = addr;
    path_cache_[path_key] = addr;
    return addr;
  }
  uint32_t Path(std::string_view dotted, std::string_view cls, uint32_t last_size = 96u) {
    EnsureBootstrap();
    std::vector<std::string_view> parts = SplitDotted(dotted);
    uint32_t parent = 0u;
    std::string prefix;
    for (size_t i = 0u; i < parts.size(); ++i) {
      if (i > 0u) prefix.push_back('.');
      prefix.append(parts[i].data(), parts[i].size());
      std::string want = ClassForPart(parts.size(), i, cls);
      std::string key = prefix + "|" + want;
      auto found = path_cache_.find(key);
      if (found != path_cache_.end()) {
        parent = found->second;
        continue;
      }
      uint32_t cls_addr = ClassFor(want);
      uint32_t size = (i + 1u == parts.size()) ? last_size : 96u;
      uint32_t addr = Object(parts[i], parent, cls_addr, size);
      path_cache_[key] = addr;
      parent = addr;
    }
    return parent;
  }
  void SetScript(uint32_t function, const std::vector<uint8_t>& bytes, uint32_t max = 0u) {
    uint32_t size = static_cast<uint32_t>(bytes.size());
    uint32_t data = Alloc(size);
    if (data != 0u) {
      for (size_t i = 0u; i < bytes.size(); ++i) {
        Put8(data + static_cast<uint32_t>(i), bytes[i]);
      }
    }
    Put32(function + 84u, data);
    Put32(function + 88u, size);
    Put32(function + 92u, (max == 0u) ? size : max);
  }
  GuestTables Finish() {
    uint32_t names_data = Alloc(static_cast<uint32_t>(name_addrs_.size()) * 4u);
    for (size_t i = 0u; i < name_addrs_.size(); ++i) {
      Put32(names_data + static_cast<uint32_t>(i) * 4u, name_addrs_[i]);
    }
    uint32_t names_array = Alloc(12u);
    Put32(names_array, names_data);
    Put32(names_array + 4u, static_cast<uint32_t>(name_addrs_.size()));
    Put32(names_array + 8u, static_cast<uint32_t>(name_addrs_.size()));
    uint32_t objects_data = Alloc(static_cast<uint32_t>(object_addrs_.size()) * 4u);
    for (size_t i = 0u; i < object_addrs_.size(); ++i) {
      Put32(objects_data + static_cast<uint32_t>(i) * 4u, object_addrs_[i]);
    }
    uint32_t objects_array = Alloc(12u);
    Put32(objects_array, objects_data);
    Put32(objects_array + 4u, static_cast<uint32_t>(object_addrs_.size()));
    Put32(objects_array + 8u, static_cast<uint32_t>(object_addrs_.size()));
    tables_ = GuestTables{names_array, objects_array};
    return tables_;
  }
  GuestTables tables() const { return tables_; }

 private:
  void EnsureBootstrap() {
    if (bootstrapped_) return;
    bootstrapped_ = true;
    uint32_t core = Object("Core", 0u, 0u);
    uint32_t cls = Object("Class", core, 0u);
    // Class points at itself to break the bootstrap cycle.
    Put32(cls + 52u, cls);
    uint32_t pkg = Object("Package", core, cls);
    Put32(core + 52u, pkg);
    class_cache_["Class"] = cls;
    class_cache_["Package"] = pkg;
    path_cache_["Core|Package"] = core;
    path_cache_["Core.Class|Class"] = cls;
    path_cache_["Core.Package|Class"] = pkg;
  }
  std::shared_ptr<uint8_t> mem_;
  uint32_t brk_ = 0x1000u;
  uint32_t phys_brk_ = kPhysAllocStart;
  std::vector<uint32_t> name_addrs_;
  std::unordered_map<std::string, uint32_t> name_by_folded_;
  std::vector<uint32_t> object_addrs_;
  std::unordered_map<std::string, uint32_t> path_cache_;
  std::unordered_map<std::string, uint32_t> class_cache_;
  GuestTables tables_{0u, 0u};
  bool bootstrapped_ = false;
};

void PutU32BE(std::vector<uint8_t>& script, uint32_t offset, uint32_t value) {
  if (static_cast<size_t>(offset) + 4u > script.size()) return;
  script[offset] = static_cast<uint8_t>(value >> 24u);
  script[offset + 1u] = static_cast<uint8_t>(value >> 16u);
  script[offset + 2u] = static_cast<uint8_t>(value >> 8u);
  script[offset + 3u] = static_cast<uint8_t>(value);
}

void FillBytesChecks(std::vector<uint8_t>& script, const ScriptRow& row) {
  for (const BytesCheck& check : row.checks) {
    for (size_t i = 0u; i < check.bytes.size(); ++i) {
      uint32_t at = static_cast<uint32_t>(check.offset) + static_cast<uint32_t>(i);
      if (static_cast<size_t>(at) < script.size()) script[at] = check.bytes[i];
    }
  }
}

void FillOperandChecks(FakeGuest& guest, std::vector<uint8_t>& script, const ScriptRow& row) {
  for (const OperandCheck& check : row.operands) {
    uint32_t target = guest.Path(check.object.path, check.object.cls);
    PutU32BE(script, static_cast<uint32_t>(check.offset), target);
  }
}

void FillByteEdits(std::vector<uint8_t>& script, const ScriptRow& row) {
  for (const ByteEdit& edit : row.byte_edits) {
    if (static_cast<size_t>(edit.offset) < script.size()) {
      script[static_cast<size_t>(edit.offset)] = edit.old_value;
    }
  }
}

void FillOperandEdits(FakeGuest& guest, std::vector<uint8_t>& script, const ScriptRow& row) {
  for (const OperandEdit& edit : row.operand_edits) {
    uint32_t from = guest.Path(edit.from.path, edit.from.cls);
    guest.Path(edit.to.path, edit.to.cls);
    PutU32BE(script, static_cast<uint32_t>(edit.offset), from);
  }
}

void CreateGrowObjects(FakeGuest& guest, const ScriptRow& row) {
  if (!row.grow.has_value()) return;
  for (const TailPiece& piece : row.grow->tail) {
    if (!piece.object.path.empty()) guest.Path(piece.object.path, piece.object.cls);
  }
}

void BuildScriptObjects(FakeGuest& guest, const ScriptRow& row) {
  uint32_t func = guest.Path(row.function.path, row.function.cls);
  std::vector<uint8_t> script(row.script_size, static_cast<uint8_t>(0x0B));
  FillBytesChecks(script, row);
  FillOperandChecks(guest, script, row);
  FillByteEdits(script, row);
  FillOperandEdits(guest, script, row);
  CreateGrowObjects(guest, row);
  guest.SetScript(func, script);
}

void BuildDefaultObjects(FakeGuest& guest, const DefaultRow& row) {
  uint32_t size = row.offset + 4u;
  if (size < 96u) size = 96u;
  uint32_t obj = guest.Path(row.object.path, row.object.cls, size);
  guest.Put32(obj + row.offset, row.old_value);
}

void BuildGroupObjects(FakeGuest& guest, const PatchGroup& group) {
  for (const ScriptRow& row : group.scripts) BuildScriptObjects(guest, row);
  for (const DefaultRow& row : group.defaults) BuildDefaultObjects(guest, row);
}

FakeGuest BuildForGroup(const PatchGroup& group, std::function<void(FakeGuest&)> before_finish = {}) {
  FakeGuest guest;
  BuildGroupObjects(guest, group);
  if (before_finish) before_finish(guest);
  guest.Finish();
  return guest;
}

std::vector<uint8_t> ReadBytes(const FakeGuest& guest, uint32_t addr, uint32_t count) {
  std::vector<uint8_t> out;
  for (uint32_t i = 0u; i < count; ++i) out.push_back(guest.U8(addr + i));
  return out;
}

struct ScriptSnap {
  uint32_t func;
  uint32_t data;
  uint32_t num;
  uint32_t max;
  std::vector<uint8_t> bytes;
};

std::vector<ScriptSnap> SnapshotScripts(FakeGuest& guest, const PatchGroup& group) {
  std::vector<ScriptSnap> snaps;
  for (const ScriptRow& row : group.scripts) {
    uint32_t func = guest.Path(row.function.path, row.function.cls);
    uint32_t data = guest.U32(func + 84u);
    uint32_t num = guest.U32(func + 88u);
    uint32_t max = guest.U32(func + 92u);
    std::vector<uint8_t> bytes = ReadBytes(guest, data, num);
    ScriptSnap snap{func, data, num, max, std::move(bytes)};
    snaps.push_back(std::move(snap));
  }
  return snaps;
}

bool ScriptsUnchanged(FakeGuest& guest, const std::vector<ScriptSnap>& snaps) {
  for (const ScriptSnap& snap : snaps) {
    if (guest.U32(snap.func + 84u) != snap.data) return false;
    if (guest.U32(snap.func + 88u) != snap.num) return false;
    if (guest.U32(snap.func + 92u) != snap.max) return false;
    for (uint32_t i = 0u; i < snap.num; ++i) {
      if (guest.U8(snap.data + i) != snap.bytes[i]) return false;
    }
  }
  return true;
}

bool EndsWithApplied(const std::string& line) {
  std::string_view suffix = "already applied";
  if (line.size() < suffix.size()) return false;
  size_t base = line.size() - suffix.size();
  for (size_t i = 0u; i < suffix.size(); ++i) {
    if (line[base + i] != suffix[i]) return false;
  }
  return true;
}

std::vector<uint8_t> ScriptBytes(FakeGuest& g, std::string_view function_path) {
  uint32_t func = g.Path(function_path, "Function");
  return ReadBytes(g, g.U32(func + 84u), g.U32(func + 88u));
}

bool BytesAt(const std::vector<uint8_t>& s, size_t offset,
             std::initializer_list<uint8_t> expected) {
  if (offset + expected.size() > s.size()) return false;
  size_t i = offset;
  for (uint8_t b : expected) {
    if (s[i] != b) return false;
    ++i;
  }
  return true;
}

uint32_t U32BE(const std::vector<uint8_t>& s, size_t offset) {
  return (static_cast<uint32_t>(s[offset]) << 24u) |
         (static_cast<uint32_t>(s[offset + 1u]) << 16u) |
         (static_cast<uint32_t>(s[offset + 2u]) << 8u) | static_cast<uint32_t>(s[offset + 3u]);
}

void CheckSurvivalLines(const GroupResult& result) {
  Expect(result.applied, "survival applies");
  Expect(result.lines.size() == 4u, "survival 4 lines");
  if (result.lines.size() != 4u) return;
  Expect(result.lines[0] ==
             "S2A-1 GearGame.GearGameSP_Base.SpawnDefaultPawnFor: script grown to "
             "183 bytes, with 1 write(s)",
         "survival S2A-1 line");
  Expect(result.lines[1] ==
             "S2A-2 GearGame.GearPC.ProcessGameOver: script grown to "
             "159 bytes",
         "survival S2A-2 line");
  Expect(result.lines[2] ==
             "S2A-3 GearGame.GearPC.Destroyed: script grown to "
             "1275 bytes, with 2 write(s)",
         "survival S2A-3 line");
  Expect(result.lines[3] ==
             "S2A-5 GearGame.GearPC.SaveCheckpoint: script grown to "
             "496 bytes, with 3 write(s)",
         "survival S2A-5 line");
}

void CheckS2A1(FakeGuest& guest, uint32_t old_data) {
  uint32_t func = guest.Path("GearGame.GearGameSP_Base.SpawnDefaultPawnFor", "Function");
  Expect(guest.U32(func + 84u) != old_data, "S2A-1 Data is new");
  Expect(guest.U32(func + 88u) == 183u && guest.U32(func + 92u) == 183u, "S2A-1 Num==Max==183");
  std::vector<uint8_t> s = ScriptBytes(guest, "GearGame.GearGameSP_Base.SpawnDefaultPawnFor");
  Expect(BytesAt(s, 0x8B, {0x00, 0xA1}), "S2A-1 +0x8B");
  Expect(BytesAt(s, 0xA1, {0x14}), "S2A-1 +0xA1");
  Expect(BytesAt(s, 0xB3, {0x06, 0x00, 0x95}), "S2A-1 +0xB3");
  Expect(BytesAt(s, 0xB6, {0x53}), "S2A-1 +0xB6");
  uint32_t kill = guest.Path("Engine.Actor.bKillDuringLevelTransition", "BoolProperty");
  Expect(U32BE(s, 0xAE) == kill, "S2A-1 +0xAE operand");
  uint32_t pawn =
      guest.Path("GearGame.GearGameSP_Base.SpawnDefaultPawnFor.ResultPawn", "ObjectProperty");
  Expect(U32BE(s, 0xA4) == pawn, "S2A-1 +0xA4 operand");
}

void CheckS2A2(FakeGuest& guest, uint32_t old_data) {
  uint32_t func = guest.Path("GearGame.GearPC.ProcessGameOver", "Function");
  Expect(guest.U32(func + 84u) != old_data, "S2A-2 Data is new");
  Expect(guest.U32(func + 88u) == 159u && guest.U32(func + 92u) == 159u, "S2A-2 Num==Max==159");
  std::vector<uint8_t> s = ScriptBytes(guest, "GearGame.GearPC.ProcessGameOver");
  Expect(BytesAt(s, 0x00, {0x07, 0x00, 0x67}), "S2A-2 +0x00");
  Expect(BytesAt(s, 0x65, {0x04, 0x0B}), "S2A-2 +0x65");
  Expect(BytesAt(s, 0x9E, {0x53}), "S2A-2 +0x9E");
  uint32_t cls = guest.Path("GearGame.GearPC.DefaultPawnClass", "ClassProperty");
  Expect(U32BE(s, 0x07) == cls, "S2A-2 +0x07 operand");
}

void CheckS2A3(FakeGuest& guest, uint32_t old_data, const std::vector<uint8_t>& old) {
  uint32_t func = guest.Path("GearGame.GearPC.Destroyed", "Function");
  Expect(guest.U32(func + 84u) != old_data, "S2A-3 Data is new");
  Expect(guest.U32(func + 88u) == 1275u && guest.U32(func + 92u) == 1275u, "S2A-3 Num==Max==1275");
  std::vector<uint8_t> s = ScriptBytes(guest, "GearGame.GearPC.Destroyed");
  Expect(BytesAt(s, 0x63, {0x07, 0x04, 0x37}), "S2A-3 +0x63");
  Expect(BytesAt(s, 0x437, {0x07, 0x00, 0xAD}), "S2A-3 +0x437");
  Expect(BytesAt(s, 0x4F7, {0x06, 0x00, 0xA4}), "S2A-3 +0x4F7");
  Expect(BytesAt(s, 0x4FA, {0x53}), "S2A-3 +0x4FA");
  uint32_t dummy = guest.Path("GearGame.GearPC.bCheckpointDummy", "BoolProperty");
  Expect(s.size() >= 0x6Eu && U32BE(s, 0x6A) == dummy, "S2A-3 +0x6A dummy operand");
  size_t tail_end = s.size() < 0x4FBu ? s.size() : 0x4FBu;
  bool dummy_in_tail = false;
  for (size_t i = 0x437u; i + 4u <= tail_end; ++i) {
    if (U32BE(s, i) == dummy) {
      dummy_in_tail = true;
      break;
    }
  }
  Expect(!dummy_in_tail, "S2A-3 dummy case: tail has no bCheckpointDummy");
  bool prefix = s.size() >= 0x63u && old.size() >= 0x63u;
  for (size_t i = 0u; prefix && i < 0x63u; ++i) prefix = s[i] == old[i];
  Expect(prefix, "S2A-3 first 0x63 preserved");
}

void CheckS2A5(FakeGuest& guest, uint32_t old_data) {
  uint32_t func = guest.Path("GearGame.GearPC.SaveCheckpoint", "Function");
  Expect(guest.U32(func + 84u) != old_data, "S2A-5 Data is new");
  Expect(guest.U32(func + 88u) == 496u && guest.U32(func + 92u) == 496u, "S2A-5 Num==Max==496");
  std::vector<uint8_t> s = ScriptBytes(guest, "GearGame.GearPC.SaveCheckpoint");
  Expect(BytesAt(s, 0xD7, {0x06, 0x01, 0x87}), "S2A-5 +0xD7");
  Expect(BytesAt(s, 0x1E9, {0x06, 0x00, 0xDA}), "S2A-5 +0x1E9");
  Expect(BytesAt(s, 0x1EC, {0x30, 0x04, 0x0B}), "S2A-5 +0x1EC");
  Expect(BytesAt(s, 0x1EF, {0x53}), "S2A-5 +0x1EF");
}

void TestSurvivalGroupApplies() {
  PatchGroup group = CoopSurvivalGroup(false);
  FakeGuest guest = BuildForGroup(group);
  uint32_t s1 = guest.Path("GearGame.GearGameSP_Base.SpawnDefaultPawnFor", "Function");
  uint32_t s2 = guest.Path("GearGame.GearPC.ProcessGameOver", "Function");
  uint32_t s3 = guest.Path("GearGame.GearPC.Destroyed", "Function");
  uint32_t s5 = guest.Path("GearGame.GearPC.SaveCheckpoint", "Function");
  uint32_t d1 = guest.U32(s1 + 84u);
  uint32_t d2 = guest.U32(s2 + 84u);
  uint32_t d3 = guest.U32(s3 + 84u);
  uint32_t d5 = guest.U32(s5 + 84u);
  std::vector<uint8_t> old3 = ScriptBytes(guest, "GearGame.GearPC.Destroyed");
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), guest.tables(), group, 4, alloc);
  CheckSurvivalLines(result);
  CheckS2A1(guest, d1);
  CheckS2A2(guest, d2);
  CheckS2A3(guest, d3, old3);
  CheckS2A5(guest, d5);
  Expect(result.set_hash == GroupHash(group, 4), "survival hash matches");
}

void TestSurvivalSecondApplyIsAlreadyApplied() {
  PatchGroup group = CoopSurvivalGroup(false);
  FakeGuest guest = BuildForGroup(group);
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult first = ApplyGroup(guest.base(), guest.tables(), group, 4, alloc);
  Expect(first.applied, "survival first applies");
  uint32_t f1 = guest.Path("GearGame.GearGameSP_Base.SpawnDefaultPawnFor", "Function");
  uint32_t f2 = guest.Path("GearGame.GearPC.ProcessGameOver", "Function");
  uint32_t f3 = guest.Path("GearGame.GearPC.Destroyed", "Function");
  uint32_t f5 = guest.Path("GearGame.GearPC.SaveCheckpoint", "Function");
  uint32_t d1 = guest.U32(f1 + 84u);
  uint32_t d2 = guest.U32(f2 + 84u);
  uint32_t d3 = guest.U32(f3 + 84u);
  uint32_t d5 = guest.U32(f5 + 84u);
  uint32_t top = guest.Top();
  GroupResult second = ApplyGroup(guest.base(), guest.tables(), group, 4, alloc);
  Expect(second.applied, "survival second applies");
  bool all = second.lines.size() == 4u;
  for (const std::string& line : second.lines) {
    if (!EndsWithApplied(line)) {
      all = false;
      break;
    }
  }
  Expect(all, "survival second all already applied");
  Expect(guest.Top() == top, "survival second no allocation");
  Expect(guest.U32(f1 + 84u) == d1, "survival second S2A-1 Data same");
  Expect(guest.U32(f2 + 84u) == d2, "survival second S2A-2 Data same");
  Expect(guest.U32(f3 + 84u) == d3, "survival second S2A-3 Data same");
  Expect(guest.U32(f5 + 84u) == d5, "survival second S2A-5 Data same");
}

void TestCorruptS2A3SkipsWholeGroup() {
  PatchGroup group = CoopSurvivalGroup(false);
  FakeGuest guest = BuildForGroup(group, [](FakeGuest& inner) {
    uint32_t func = inner.Path("GearGame.GearPC.Destroyed", "Function");
    inner.Put8(inner.U32(func + 84u) + 0x66u, 0x83);
  });
  std::vector<ScriptSnap> snaps = SnapshotScripts(guest, group);
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), guest.tables(), group, 4, alloc);
  Expect(!result.applied, "corrupt S2A-3 skips");
  Expect(result.failed_row == "S2A-3", "corrupt S2A-3 failed_row");
  Expect(ScriptsUnchanged(guest, snaps), "corrupt S2A-3 scripts unchanged");
}

void TestSplitReviveGroupApplies() {
  PatchGroup group = CoopSplitReviveGroup();
  FakeGuest guest = BuildForGroup(group);
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), guest.tables(), group, 4, alloc);
  Expect(result.applied, "split-revive applies");
  Expect(result.lines.size() == 1u, "split-revive 1 line");
  if (result.lines.size() == 1u) {
    Expect(result.lines[0] ==
               "S2A-4 GearGame.GearGame.TeamHasAnyChanceOfSurvival: script grown to "
               "854 bytes, with 1 write(s)",
           "split-revive S2A-4 line");
  }
  uint32_t func = guest.Path("GearGame.GearGame.TeamHasAnyChanceOfSurvival", "Function");
  Expect(guest.U32(func + 88u) == 854u, "split-revive Num==854");
  std::vector<uint8_t> s = ScriptBytes(guest, "GearGame.GearGame.TeamHasAnyChanceOfSurvival");
  Expect(BytesAt(s, 0x259, {0x02, 0x97}), "split-revive +0x259");
  Expect(BytesAt(s, 0x297, {0x07, 0x02, 0x88}), "split-revive +0x297");
  Expect(BytesAt(s, 0x355, {0x53}), "split-revive +0x355");
}

void TestSurvivalHashStable() {
  Expect(GroupHash(CoopSurvivalGroup(false), 4) == GroupHash(CoopSurvivalGroup(false), 4),
         "survival hash stable");
  Expect(GroupHash(CoopSurvivalGroup(false), 4) != 0u, "survival hash nonzero");
  Expect(GroupHash(CoopSurvivalGroup(false), 4) != GroupHash(CoopStage1Group(false), 4),
         "survival hash differs from stage1");
  Expect(GroupHash(CoopSurvivalGroup(false), 4) != GroupHash(CoopSplitReviveGroup(), 4),
         "survival hash differs from split-revive");
}

std::string RowIds(const PatchGroup& group) {
  std::string ids;
  for (const ScriptRow& row : group.scripts) {
    if (!ids.empty()) ids.push_back(',');
    ids += row.id;
  }
  return ids;
}

void TestSurvivalGameOverFlagLeavesOutS2A2() {
  Expect(RowIds(CoopSurvivalGroup(false)) == "S2A-1,S2A-2,S2A-3,S2A-5",
         "survival rows with flag false");
  Expect(RowIds(CoopSurvivalGroup(true)) == "S2A-1,S2A-3,S2A-5",
         "survival rows with flag true");
  Expect(GroupHash(CoopSurvivalGroup(false), 4) != GroupHash(CoopSurvivalGroup(true), 4),
         "survival hash differs with flag true");
}

}  // namespace

int main() {
  TestSurvivalGroupApplies();
  TestSurvivalSecondApplyIsAlreadyApplied();
  TestCorruptS2A3SkipsWholeGroup();
  TestSplitReviveGroupApplies();
  TestSurvivalHashStable();
  TestSurvivalGameOverFlagLeavesOutS2A2();
  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures != 0 ? 1 : 0;
}
