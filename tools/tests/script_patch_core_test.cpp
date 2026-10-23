// Tests for the script patcher core (src/gow2_script_patches.cpp) on a synthetic guest memory built
// from the rows themselves; no game files needed. Build target gow2_script_patch_test.
#include "gow2_script_patches.h"
#include "gow2_script_patch_body.h"
#include "gow2_script_patch_stack.h"

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

constexpr uint32_t kFieldObjectSize = 1024u;  // room for field offsets up to ~1000

void BuildObjectFieldObjects(FakeGuest& guest, const ObjectFieldRow& row) {
  uint32_t object = guest.Path(row.object.path, row.object.cls, kFieldObjectSize);
  guest.Put32(object + row.offset, 0u);
  guest.Path(row.value.path, row.value.cls);
  for (const FieldAnchor& anchor : row.anchors) {
    uint32_t holder = guest.Path(anchor.object.path, anchor.object.cls, kFieldObjectSize);
    guest.Put32(holder + anchor.offset, guest.Path(anchor.value.path, anchor.value.cls));
  }
}

void BuildGroupObjects(FakeGuest& guest, const PatchGroup& group) {
  // First so field objects get the large size before anything else creates them small.
  for (const ObjectFieldRow& row : group.object_fields) BuildObjectFieldObjects(guest, row);
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

bool PrefixMatches(FakeGuest& guest, uint32_t data, const std::vector<uint8_t>& old_bytes, uint32_t count) {
  for (uint32_t i = 0u; i < count; ++i) {
    if (guest.U8(data + i) != old_bytes[i]) return false;
  }
  return true;
}

void CheckByteEdits(FakeGuest& guest) {
  uint32_t at = guest.Path("GearGame.GearGameSP_Base.AtCapacity", "Function");
  Expect(guest.U8(guest.U32(at + 84u) + 0x01u) == 0x99, "AtCapacity +0x01 patched");
  Expect(guest.U8(guest.U32(at + 84u) + 0x08u) == 4, "AtCapacity +0x08 players");
  uint32_t invite = guest.Path("GearGame.GearMenuGame.ChangeCoopInviteSetting", "Function");
  Expect(guest.U8(guest.U32(invite + 84u) + 0x58u) == 4, "ChangeCoopInviteSetting +0x58 players");
  uint32_t lobby = guest.Path("GearGame.GearCampaignLobbyGame_Base.InitGame", "Function");
  Expect(guest.U8(guest.U32(lobby + 84u) + 0xA5u) == 4, "lobby InitGame +0xA5 players");
  uint32_t init = guest.Path("GearGame.GearGameSP_Base.InitGame", "Function");
  Expect(guest.U8(guest.U32(init + 84u) + 0xBBu) == 4, "SP InitGame +0xBB players");
  uint32_t def = guest.Path("GearGame.Default__GearCoopGameSettings", "GearCoopGameSettings");
  Expect(guest.U32(def + 132u) == 4u, "default +132 players");
}

void CheckSuggestGrow(FakeGuest& guest, uint32_t suggest, uint32_t old_data, const std::vector<uint8_t>& old_bytes) {
  uint32_t data = guest.U32(suggest + 84u);
  uint32_t num = guest.U32(suggest + 88u);
  uint32_t max = guest.U32(suggest + 92u);
  Expect(num == 318u && max == 318u, "Suggest Num==Max==318");
  Expect(data != old_data, "Suggest Data is new");
  Expect(PrefixMatches(guest, data, old_bytes, 0x84u), "Suggest first 0x84 bytes preserved");
  Expect(guest.U8(data + 0x84u) == 0x07 && guest.U8(data + 0x85u) == 0x01 && guest.U8(data + 0x86u) == 0x00,
         "Suggest tail jump to 0x0100");
  Expect(guest.U8(data + 0x87u) == 0x77 && guest.U8(data + 0x88u) == 0x1C, "Suggest tail != FindObject");
  uint32_t find = guest.Path("Core.Object.FindObject", "Function");
  Expect(guest.U32(data + 0x89u) == find, "Suggest tail FindObject operand");
  Expect(guest.U8(data + 0xC2u) == 0x04 && guest.U8(data + 0x100u) == 0x04, "Suggest tail returns");
  uint32_t ret = guest.Path("GearGame.GearGameSP_Base.SuggestPawnClassFor.ReturnValue", "ClassProperty");
  Expect(guest.U32(data + 0x139u) == ret, "Suggest tail ReturnValue operand");
  Expect(guest.U8(data + 317u) == 0x53, "Suggest last byte 0x53");
  Expect(PrefixMatches(guest, old_data, old_bytes, static_cast<uint32_t>(old_bytes.size())), "Suggest old buffer unchanged");
}

bool RawHostBytesAreZero(const uint8_t* base, uint32_t offset, uint32_t count) {
  for (uint32_t i = 0u; i < count; ++i) {
    if (base[offset + i] != 0u) return false;
  }
  return true;
}

ScriptRow MakeGrowPrefixEditRow(std::string_view id, uint16_t edit_offset) {
  ScriptRow row;
  row.id = id;
  row.function = {"Pkg.ClassG.FuncG", "Function"};
  row.script_size = 10u;
  row.byte_edits.push_back({edit_offset, static_cast<uint8_t>(0x9A), kPlayers});
  GrowEdit grow;
  grow.keep = 6u;
  grow.new_size = 14u;
  TailPiece head;
  head.bytes.push_back(static_cast<uint8_t>(0xAA));
  TailPiece text;
  text.bytes = AsciiZ("hi");
  TailPiece target;
  target.object = {"Pkg.ClassC.GrowTarget", "Class"};
  grow.tail.push_back(std::move(head));
  grow.tail.push_back(std::move(text));
  grow.tail.push_back(std::move(target));
  row.grow = std::move(grow);
  return row;
}

void TestRealGroupApplies() {
  PatchGroup group = CoopStage1Group(false);
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  uint32_t suggest = guest.Path("GearGame.GearGameSP_Base.SuggestPawnClassFor", "Function");
  uint32_t old_data = guest.U32(suggest + 84u);
  std::vector<uint8_t> old_bytes = ReadBytes(guest, old_data, guest.U32(suggest + 88u));
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(result.applied, "real group applies");
  Expect(result.lines.size() == 6u, "real group 6 lines");
  Expect(result.set_hash == GroupHash(group, 4), "real group hash matches");
  Expect(result.set_hash != 0u, "real group hash nonzero");
  CheckByteEdits(guest);
  CheckSuggestGrow(guest, suggest, old_data, old_bytes);
}

void TestSecondApplyIsAlreadyApplied() {
  PatchGroup group = CoopStage1Group(false);
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult first = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(first.applied, "first apply applies");
  uint32_t top = guest.Top();
  GroupResult second = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(second.applied, "second apply applies");
  bool all = second.lines.size() == 6u;
  for (const std::string& line : second.lines) {
    if (!EndsWithApplied(line)) {
      all = false;
      break;
    }
  }
  Expect(all, "second apply all already applied");
  Expect(guest.Top() == top, "second apply no allocation");
}

void TestWrongOldByteSkipsWholeGroup() {
  PatchGroup group = CoopStage1Group(false);
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  uint32_t init = guest.Path("GearGame.GearGameSP_Base.InitGame", "Function");
  guest.Put8(guest.U32(init + 84u) + 0xBBu, static_cast<uint8_t>(0x03));
  std::vector<ScriptSnap> snaps = SnapshotScripts(guest, group);
  uint32_t def = guest.Path("GearGame.Default__GearCoopGameSettings", "GearCoopGameSettings");
  uint32_t before = guest.U32(def + 132u);
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(!result.applied, "wrong old byte skips");
  Expect(result.failed_row == "S1-6", "wrong old byte failed_row S1-6");
  Expect(result.reason.find("+0x00BB") != std::string::npos, "wrong old byte reason offset");
  Expect(ScriptsUnchanged(guest, snaps), "wrong old byte scripts unchanged");
  Expect(guest.U32(def + 132u) == before, "wrong old byte default unchanged");
}

void TestWrongNumSkips() {
  PatchGroup group = CoopStage1Group(false);
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  uint32_t at = guest.Path("GearGame.GearGameSP_Base.AtCapacity", "Function");
  guest.Put32(at + 88u, 18u);
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(!result.applied, "wrong Num skips");
  Expect(result.failed_row == "S1-1", "wrong Num failed_row");
  Expect(result.reason.find("Script.Num") != std::string::npos, "wrong Num reason");
}

void TestWrongOperandSkips() {
  PatchGroup group = CoopStage1Group(false);
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  uint32_t invite = guest.Path("GearGame.GearMenuGame.ChangeCoopInviteSetting", "Function");
  uint32_t other = guest.Path("Engine.GameInfo.MaxPlayers", "IntProperty");
  guest.Put32(guest.U32(invite + 84u) + 0x53u, other);
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(!result.applied, "wrong operand skips");
  Expect(result.failed_row == "S1-3", "wrong operand failed_row");
  Expect(result.reason.find("points at") != std::string::npos, "wrong operand reason");
}

void TestFunctionFoundTwiceSkips() {
  PatchGroup group = CoopStage1Group(false);
  FakeGuest guest = BuildForGroup(group, [](FakeGuest& inner) {
    uint32_t outer = inner.Path("GearGame.GearGameSP_Base", "Class");
    inner.Object("AtCapacity", outer, inner.ClassFor("Function"));
  });
  GuestTables tables = guest.tables();
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(!result.applied, "duplicate function skips");
  Expect(result.failed_row == "S1-1", "duplicate function failed_row");
  Expect(result.reason.find("found 2 times") != std::string::npos, "duplicate function reason");
}

void TestCorruptedTableSkips() {
  PatchGroup good = CoopStage1Group(false);
  FakeGuest guest = BuildForGroup(good);
  GuestTables tables = guest.tables();
  PatchGroup bad = CoopStage1Group(false);
  uint8_t flipped = static_cast<uint8_t>(bad.scripts[0].checks[0].bytes[0] ^ 0xFFu);
  bad.scripts[0].checks[0].bytes[0] = flipped;
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, bad, 4, alloc);
  Expect(!result.applied, "corrupted table skips");
  Expect(result.failed_row == "S1-1", "corrupted table failed_row");
  Expect(result.reason.find("expected") != std::string::npos, "corrupted table reason");
}

void TestMaxDiffersSkipsGrow() {
  PatchGroup group = CoopStage1Group(false);
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  uint32_t suggest = guest.Path("GearGame.GearGameSP_Base.SuggestPawnClassFor", "Function");
  guest.Put32(suggest + 92u, 160u);
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(!result.applied, "wrong Max skips");
  Expect(result.failed_row == "S1-2", "wrong Max failed_row");
  Expect(result.reason.find("Script.Max") != std::string::npos, "wrong Max reason");
}

void TestAllocFailureSkips() {
  PatchGroup group = CoopStage1Group(false);
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  GuestAlloc alloc = [](uint32_t size) {
    (void)size;
    return 0u;
  };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(!result.applied, "alloc failure skips");
  Expect(result.failed_row == "S1-2", "alloc failure failed_row");
  Expect(result.reason.find("allocation") != std::string::npos, "alloc failure reason");
}

void TestMarcusClonesSwapsOperand() {
  PatchGroup group = CoopStage1Group(true);
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  uint32_t suggest = guest.Path("GearGame.GearGameSP_Base.SuggestPawnClassFor", "Function");
  uint32_t old_data = guest.U32(suggest + 84u);
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(result.applied, "marcus clones applies");
  Expect(guest.U32(suggest + 88u) == 145u, "marcus clones keeps 145");
  uint32_t data = guest.U32(suggest + 84u);
  Expect(data == old_data, "marcus clones data unchanged");
  uint32_t marcus = guest.Path("GearGame.GearPawn_COGMarcus", "Class");
  Expect(guest.U32(data + 0x86u) == marcus, "marcus clones operand swapped");
}

void TestUnreadableTablesSkip() {
  PatchGroup group = CoopStage1Group(false);
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  guest.Put32(tables.names + 4u, 0u);
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(!result.applied, "unreadable tables skip");
  Expect(result.reason.find("unreadable") != std::string::npos, "unreadable tables reason");
}

void TestWideAndCaseInsensitiveNames() {
  PatchGroup group = CoopStage1Group(false);
  FakeGuest guest;
  guest.Name("gearGameSP_base", true);
  BuildGroupObjects(guest, group);
  GuestTables tables = guest.Finish();
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(result.applied, "wide names apply");
}

void TestPlayersOutOfRangeSkips() {
  PatchGroup group = CoopStage1Group(false);
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 256, alloc);
  Expect(!result.applied, "players out of range skips");
  Expect(result.reason.find("out of range") != std::string::npos, "players out of range reason");
}

void TestHashDependsOnPlayers() {
  PatchGroup first = CoopStage1Group(false);
  PatchGroup second = CoopStage1Group(false);
  Expect(GroupHash(first, 4) != GroupHash(first, 5), "hash differs by players");
  Expect(GroupHash(first, 4) == GroupHash(second, 4), "hash stable across builds");
}

void TestRealGroupAppliesAtLargeCounts() {
  PatchGroup group = CoopStage1Group(false);
  uint64_t hash_4 = GroupHash(group, 4);
  for (int players : {8, 10}) {
    FakeGuest guest = BuildForGroup(group);
    GuestTables tables = guest.tables();
    GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
    GroupResult result = ApplyGroup(guest.base(), tables, group, players, alloc);
    Expect(result.applied, "real group applies at large counts");
    Expect(result.lines.size() == 6u, "real group 6 lines at large counts");
    Expect(result.set_hash == GroupHash(group, players), "real group hash matches at large counts");
    Expect(result.set_hash != hash_4, "real group hash differs from 4 players");
  }
  Expect(GroupHash(group, 8) != GroupHash(group, 10), "hash differs between 8 and 10 players");
}

void TestGuestBytesCrossThePhysicalBoundary() {
  FakeGuest guest;
  guest.Put8(0xDFFFFFFEu, 0x11);
  guest.Put8(0xDFFFFFFFu, 0x22);
  guest.Put8(0xE0000000u, 0x33);
  guest.Put8(0xE0000001u, 0x44);
  Expect(::rex::glue::guest::LoadU32(guest.base(), 0xDFFFFFFEu) == 0x11223344u, "cross-boundary LoadU32");
  std::vector<uint8_t> want{static_cast<uint8_t>(0x11), static_cast<uint8_t>(0x22),
                            static_cast<uint8_t>(0x33), static_cast<uint8_t>(0x44)};
  Expect(::rex::glue::guest::ReadBytes(guest.base(), 0xDFFFFFFEu, 4u) == want, "cross-boundary ReadBytes");
  ::rex::glue::guest::WriteBytes(guest.base(), 0xDFFFFFFFu,
                                 std::vector<uint8_t>{static_cast<uint8_t>(0xA1), static_cast<uint8_t>(0xA2)});
  Expect(guest.U8(0xDFFFFFFFu) == 0xA1, "cross-boundary WriteBytes low");
  Expect(guest.U8(0xE0000000u) == 0xA2, "cross-boundary WriteBytes high");
}

void TestGrowIntoPhysicalHeap() {
  PatchGroup group = CoopStage1Group(false);
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  uint32_t suggest = guest.Path("GearGame.GearGameSP_Base.SuggestPawnClassFor", "Function");
  uint32_t old_data = guest.U32(suggest + 84u);
  std::vector<uint8_t> old_bytes = ReadBytes(guest, old_data, guest.U32(suggest + 88u));
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.AllocPhysical(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(result.applied, "physical heap applies");
  Expect(guest.U32(suggest + 84u) == 0xE0000000u, "physical heap data at 0xE0000000");
  CheckSuggestGrow(guest, suggest, old_data, old_bytes);
  if (TestHostOffset(0xE0000000u) != 0u) {
    // Wrong place on purpose: untranslated host bytes must stay zero.
    Expect(RawHostBytesAreZero(guest.base(), 0xE0000000u, 318u), "physical heap wrong place untouched");
  }
}

void TestGrowWithPrefixEditUsesEditedPrefix() {
  ScriptRow row = MakeGrowPrefixEditRow("G-2", 0x02u);
  PatchGroup group;
  group.name = "test";
  group.scripts.push_back(std::move(row));
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  uint32_t func = guest.Path("Pkg.ClassG.FuncG", "Function");
  uint32_t old_data = guest.U32(func + 84u);
  std::vector<uint8_t> old_bytes = ReadBytes(guest, old_data, 10u);
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult first = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(first.applied, "grown prefix edit applies");
  Expect(first.lines.size() == 1u &&
             first.lines[0] == "G-2 Pkg.ClassG.FuncG: script grown to 14 bytes, with 1 write(s)",
         "grown prefix edit line");
  uint32_t data = guest.U32(func + 84u);
  Expect(guest.U32(func + 88u) == 14u && guest.U32(func + 92u) == 14u, "grown prefix edit Num==Max==14");
  Expect(guest.U8(data) == 0x0B && guest.U8(data + 1u) == 0x0B && guest.U8(data + 2u) == 0x04 &&
             guest.U8(data + 3u) == 0x0B && guest.U8(data + 4u) == 0x0B && guest.U8(data + 5u) == 0x0B,
         "grown prefix edit keeps edited prefix");
  Expect(guest.U8(data + 6u) == 0xAA, "grown prefix edit tail byte");
  Expect(guest.U8(data + 7u) == 'h' && guest.U8(data + 8u) == 'i' && guest.U8(data + 9u) == 0, "grown prefix edit tail text");
  uint32_t target = guest.Path("Pkg.ClassC.GrowTarget", "Class");
  Expect(guest.U32(data + 10u) == target, "grown prefix edit tail operand");
  Expect(guest.U8(old_data + 2u) == 0x9A, "grown prefix edit old byte 2 unchanged");
  Expect(PrefixMatches(guest, old_data, old_bytes, 10u), "grown prefix edit old buffer unchanged");
  uint32_t top = guest.Top();
  GroupResult second = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(second.applied, "grown prefix edit second applies");
  Expect(second.lines.size() == 1u && EndsWithApplied(second.lines[0]), "grown prefix edit second already applied");
  Expect(guest.Top() == top, "grown prefix edit second no allocation");
}

void TestGrowEditPastKeptPrefixSkips() {
  ScriptRow row = MakeGrowPrefixEditRow("G-3", 0x07u);
  PatchGroup group;
  group.name = "test";
  group.scripts.push_back(std::move(row));
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  uint32_t func = guest.Path("Pkg.ClassG.FuncG", "Function");
  uint32_t data = guest.U32(func + 84u);
  uint32_t num = guest.U32(func + 88u);
  uint32_t max = guest.U32(func + 92u);
  std::vector<uint8_t> bytes = ReadBytes(guest, data, num);
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(!result.applied, "past-prefix edit skips");
  Expect(result.failed_row == "G-3", "past-prefix edit failed_row");
  Expect(result.reason == "+0x0007 is past the kept prefix +0x0006", "past-prefix edit reason");
  Expect(guest.U32(func + 84u) == data && guest.U32(func + 88u) == num && guest.U32(func + 92u) == max,
         "past-prefix edit script unchanged");
  Expect(PrefixMatches(guest, data, bytes, 10u), "past-prefix edit bytes unchanged");
}

ObjectFieldRow MakeObjectFieldRow() {
  ObjectFieldRow row;
  row.id = "OF-1";
  row.object = {"Pkg.Default__Thing.Mesh", "SkeletalMeshComponent"};
  row.offset = 468u;
  row.value = {"Assets.Body", "SkeletalMesh"};
  FieldAnchor anchor;
  anchor.object = {"Pkg.Default__Hero.Mesh", "SkeletalMeshComponent"};
  anchor.offset = 468u;
  anchor.value = {"Assets.HeroBody", "SkeletalMesh"};
  row.anchors.push_back(anchor);
  return row;
}

PatchGroup MakeObjectFieldGroup() {
  PatchGroup group;
  group.name = "test";
  group.object_fields.push_back(MakeObjectFieldRow());
  return group;
}

ScriptRow MakeT1Row() {
  ScriptRow row;
  row.id = "T-1";
  row.function = {"Pkg.ClassA.FuncA", "Function"};
  row.script_size = 16u;
  row.byte_edits.push_back({0x02, 0x9A, kPlayers});
  return row;
}

void TestObjectFieldWritesBigEndian() {
  PatchGroup group = MakeObjectFieldGroup();
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(result.applied, "object field applies");
  Expect(result.lines.size() == 1u &&
             result.lines[0] == "OF-1 Pkg.Default__Thing.Mesh: 1 write(s)",
         "object field line");
  uint32_t object = guest.Path("Pkg.Default__Thing.Mesh", "SkeletalMeshComponent");
  uint32_t value = guest.Path("Assets.Body", "SkeletalMesh");
  Expect(guest.U32(object + 468u) == value, "object field holds value address");
  Expect(guest.U8(object + 468u) == static_cast<uint8_t>(value >> 24u), "object field big-endian");
}

void TestObjectFieldSecondApplyIsAlreadyApplied() {
  PatchGroup group = MakeObjectFieldGroup();
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult first = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(first.applied, "object field first applies");
  GroupResult second = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(second.applied, "object field second applies");
  Expect(second.lines.size() == 1u && EndsWithApplied(second.lines[0]),
         "object field second already applied");
}

void TestObjectFieldUnrelatedValueSkipsGroup() {
  PatchGroup group;
  group.name = "test";
  group.scripts.push_back(MakeT1Row());
  group.object_fields.push_back(MakeObjectFieldRow());
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  uint32_t object = guest.Path("Pkg.Default__Thing.Mesh", "SkeletalMeshComponent");
  guest.Put32(object + 468u, 0x12345678u);
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(!result.applied, "unrelated value skips");
  Expect(result.failed_row == "OF-1", "unrelated value failed_row");
  Expect(result.reason == "+0x01D4 points at 0x12345678, expected None or Assets.Body (SkeletalMesh)",
         "unrelated value reason");
  uint32_t func = guest.Path("Pkg.ClassA.FuncA", "Function");
  Expect(guest.U8(guest.U32(func + 84u) + 0x02u) == 0x9A, "unrelated value script unchanged");
}

void TestObjectFieldAnchorMismatchSkips() {
  PatchGroup group = MakeObjectFieldGroup();
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  uint32_t anchor = guest.Path("Pkg.Default__Hero.Mesh", "SkeletalMeshComponent");
  guest.Put32(anchor + 468u, 0u);
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(!result.applied, "anchor mismatch skips");
  Expect(result.failed_row == "OF-1", "anchor mismatch failed_row");
  Expect(result.reason ==
             "anchor Pkg.Default__Hero.Mesh (SkeletalMeshComponent) +0x01D4 points at 0x00000000, "
             "expected Assets.HeroBody (SkeletalMesh)",
         "anchor mismatch reason");
  uint32_t object = guest.Path("Pkg.Default__Thing.Mesh", "SkeletalMeshComponent");
  Expect(guest.U32(object + 468u) == 0u, "anchor mismatch field still None");
}

void TestObjectFieldMissingValueSkips() {
  PatchGroup built = MakeObjectFieldGroup();
  FakeGuest guest = BuildForGroup(built);
  GuestTables tables = guest.tables();
  PatchGroup group = MakeObjectFieldGroup();
  group.object_fields[0].value = {"Assets.Missing", "SkeletalMesh"};
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(!result.applied, "missing value skips");
  Expect(result.failed_row == "OF-1", "missing value failed_row");
  Expect(result.reason.find("found 0 times") != std::string::npos, "missing value reason");
  uint32_t object = guest.Path("Pkg.Default__Thing.Mesh", "SkeletalMeshComponent");
  Expect(guest.U32(object + 468u) == 0u, "missing value field still None");
}

void TestStage1HashUnchanged() {
  Expect(GroupHash(CoopStage1Group(false), 4) == 0x937c35f80cb274b7ull, "stage1 hash unchanged");
}

void TestObjectFieldsChangeHash() {
  PatchGroup full = MakeObjectFieldGroup();
  PatchGroup cleared = full;
  cleared.object_fields.clear();
  Expect(GroupHash(full, 4) != GroupHash(cleared, 4), "object fields change hash");
}

void TestObjectFieldLinesComeLast() {
  PatchGroup group;
  group.name = "test";
  group.scripts.push_back(MakeT1Row());
  group.object_fields.push_back(MakeObjectFieldRow());
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(result.applied, "script plus field applies");
  Expect(result.lines.size() == 2u, "script plus field 2 lines");
  bool ordered = result.lines.size() == 2u && result.lines[0].rfind("T-1 ", 0) == 0 &&
                 result.lines[1].rfind("OF-1 ", 0) == 0;
  Expect(ordered, "object field lines come last");
}

bool EndsWith(std::string_view line, std::string_view suffix) {
  return line.size() >= suffix.size() && line.substr(line.size() - suffix.size()) == suffix;
}

void TestCoopBodyGroupApplies() {
  PatchGroup group = CoopBodyGroup(false, false);
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(result.applied, "coop-body applies");
  Expect(result.lines.size() == 5u, "coop-body 5 lines");
  bool ordered = result.lines.size() == 5u && result.lines[0].rfind("CB-3 ", 0) == 0 &&
                 result.lines[1].rfind("CB-4 ", 0) == 0 && result.lines[2].rfind("CB-5 ", 0) == 0 &&
                 result.lines[3].rfind("CB-1 ", 0) == 0 && result.lines[4].rfind("CB-2 ", 0) == 0;
  Expect(ordered, "coop-body lines in order");
  Expect(result.lines.size() > 1u && EndsWith(result.lines[1], "script grown to 658 bytes"),
         "coop-body CB-4 grown to 658");
  Expect(group.name == "coop-body", "coop-body name");
  uint32_t suggest = guest.Path("GearGame.GearGameSP_Base.SuggestPawnClassFor", "Function");
  uint32_t suggest_data = guest.U32(suggest + 84u);
  Expect(guest.U8(suggest_data + 0x84u) == 0x06, "coop-body Suggest +0x84 Gear branch");
  Expect(guest.U8(suggest_data + 0x101u) == 0x20, "coop-body Suggest +0x101 class context");
  Expect(guest.U32(suggest_data + 0x102u) == guest.Path("GearGame.GearPawn_COGGear", "Class"),
         "coop-body Suggest +0x102 COGGear");
  uint32_t login = guest.Path("GearGame.GearGameSP_Base.PostLogin", "Function");
  uint32_t login_data = guest.U32(login + 84u);
  Expect(guest.U8(login_data + 0x3B3u) == 0x72, "coop-body PostLogin +0x3B3");
  Expect(guest.U8(login_data + 0x3B4u) == 0x13, "coop-body PostLogin +0x3B4");
  Expect(guest.U32(login_data + 0x3B5u) == guest.Path("GearGame.GearPawn_COGDom", "Class"),
         "coop-body PostLogin +0x3B5 Dom");
  Expect(guest.U32(login_data + 0x3C4u) == guest.Path("GearGame.GearPC.DefaultPawnClass", "ClassProperty"),
         "coop-body PostLogin +0x3C4 DefaultPawnClass");
  uint32_t restart = guest.Path("GearGame.GearGame.RestartPlayer", "Function");
  Expect(guest.U32(restart + 88u) == 658u && guest.U32(restart + 92u) == 658u,
         "coop-body RestartPlayer Num==Max==658");
  uint32_t mesh = guest.Path("GearGame.Default__GearPawn_COGGear.GearPawnMesh", "SkeletalMeshComponent");
  Expect(guest.U32(mesh + 480u) == guest.Path("COG_Dom.Game.COG_Dom_Game_CamSkel", "SkeletalMesh"),
         "coop-body mesh SkeletalMesh");
  Expect(guest.U32(mesh + 508u) == guest.Path("COG_Dom.COG_Dom_CamSkel_Physics", "PhysicsAsset"),
         "coop-body mesh PhysicsAsset");
}

void TestCoopBodyGroupSecondApplyIsAlreadyApplied() {
  PatchGroup group = CoopBodyGroup(false, false);
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult first = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(first.applied, "coop-body first applies");
  uint32_t top = guest.Top();
  GroupResult second = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(second.applied, "coop-body second applies");
  bool all = second.lines.size() == 5u;
  for (const std::string& line : second.lines) {
    if (!EndsWithApplied(line)) {
      all = false;
      break;
    }
  }
  Expect(all, "coop-body second all already applied");
  Expect(guest.Top() == top, "coop-body second no allocation");
}

void TestCoopBodyCarmineKeepsBenBranch() {
  PatchGroup group = CoopBodyGroup(true, false);
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(result.applied, "coop-body carmine applies");
  uint32_t suggest = guest.Path("GearGame.GearGameSP_Base.SuggestPawnClassFor", "Function");
  uint32_t data = guest.U32(suggest + 84u);
  Expect(guest.U8(data + 0x84u) == 0x07, "coop-body carmine +0x84 keeps Ben branch");
  Expect(guest.U8(data + 0x101u) == 0x20, "coop-body carmine +0x101 class context");
  Expect(!result.lines.empty() && result.lines[0].rfind("CB-3c ", 0) == 0,
         "coop-body carmine first line CB-3c");
}

void TestCoopBodyMarcusClonesSkipsClassRow() {
  PatchGroup group = CoopBodyGroup(false, true);
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(result.applied, "coop-body marcus clones applies");
  Expect(result.lines.size() == 4u, "coop-body marcus clones 4 lines");
  Expect(!result.lines.empty() && result.lines[0].rfind("CB-4 ", 0) == 0,
         "coop-body marcus clones first line CB-4");
}

void TestCoopBodySkipsOnUngrownSuggest() {
  PatchGroup group = CoopBodyGroup(false, false);
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  uint32_t suggest = guest.Path("GearGame.GearGameSP_Base.SuggestPawnClassFor", "Function");
  guest.Put32(suggest + 88u, 145u);
  uint32_t restart = guest.Path("GearGame.GearGame.RestartPlayer", "Function");
  uint32_t restart_data = guest.U32(restart + 84u);
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(!result.applied, "coop-body ungrown suggest skips");
  Expect(result.failed_row == "CB-3", "coop-body ungrown suggest failed_row CB-3");
  Expect(result.reason == "Script.Num is 145, expected 318", "coop-body ungrown suggest reason");
  uint32_t mesh = guest.Path("GearGame.Default__GearPawn_COGGear.GearPawnMesh", "SkeletalMeshComponent");
  Expect(guest.U32(mesh + 480u) == 0u, "coop-body ungrown suggest mesh still None");
  Expect(guest.U32(restart + 88u) == 387u, "coop-body ungrown suggest RestartPlayer Num still 387");
  Expect(guest.U32(restart + 84u) == restart_data, "coop-body ungrown suggest RestartPlayer data unchanged");
}

void TestCoopBodyAfterStage1OnOneGuest() {
  FakeGuest guest;
  BuildGroupObjects(guest, CoopStage1Group(false));
  PatchGroup partial = CoopBodyGroup(false, false);
  partial.scripts.erase(partial.scripts.begin());
  BuildGroupObjects(guest, partial);
  GuestTables tables = guest.Finish();
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult stage1 = ApplyGroup(guest.base(), tables, CoopStage1Group(false), 4, alloc);
  Expect(stage1.applied, "stage1 then coop-body stage1 applies");
  Expect(stage1.lines.size() == 6u, "stage1 then coop-body stage1 6 lines");
  GroupResult body = ApplyGroup(guest.base(), tables, CoopBodyGroup(false, false), 4, alloc);
  Expect(body.applied, "stage1 then coop-body body applies");
  Expect(body.lines.size() == 5u, "stage1 then coop-body body 5 lines");
  uint32_t suggest = guest.Path("GearGame.GearGameSP_Base.SuggestPawnClassFor", "Function");
  Expect(guest.U32(suggest + 88u) == 318u, "stage1 then coop-body Suggest Num 318");
  uint32_t data = guest.U32(suggest + 84u);
  Expect(guest.U8(data + 0x84u) == 0x06, "stage1 then coop-body Suggest +0x84 Gear branch");
  Expect(guest.U8(data + 0x101u) == 0x20, "stage1 then coop-body Suggest +0x101 class context");
  Expect(guest.U32(data + 0x102u) == guest.Path("GearGame.GearPawn_COGGear", "Class"),
         "stage1 then coop-body Suggest +0x102 COGGear");
  Expect(guest.U8(data + 0x100u) == 0x04, "stage1 then coop-body Suggest +0x100 return");
}

void TestCoopBodyCarmineAfterStage1OnOneGuest() {
  FakeGuest guest;
  BuildGroupObjects(guest, CoopStage1Group(false));
  PatchGroup partial = CoopBodyGroup(true, false);
  partial.scripts.erase(partial.scripts.begin());
  BuildGroupObjects(guest, partial);
  GuestTables tables = guest.Finish();
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult stage1 = ApplyGroup(guest.base(), tables, CoopStage1Group(false), 4, alloc);
  Expect(stage1.applied, "stage1 then carmine stage1 applies");
  Expect(stage1.lines.size() == 6u, "stage1 then carmine stage1 6 lines");
  GroupResult body = ApplyGroup(guest.base(), tables, CoopBodyGroup(true, false), 4, alloc);
  Expect(body.applied, "stage1 then carmine body applies");
  Expect(body.lines.size() == 5u, "stage1 then carmine body 5 lines");
  uint32_t suggest = guest.Path("GearGame.GearGameSP_Base.SuggestPawnClassFor", "Function");
  Expect(guest.U8(guest.U32(suggest + 84u) + 0x84u) == 0x07, "stage1 then carmine +0x84 keeps Ben branch");
}

void TestCoopRideGroupApplies() {
  PatchGroup group = CoopRideGroup();
  FakeGuest guest = BuildForGroup(group);
  GuestTables tables = guest.tables();
  GuestAlloc alloc = [&guest](uint32_t size) { return guest.Alloc(size); };
  GroupResult result = ApplyGroup(guest.base(), tables, group, 4, alloc);
  Expect(result.applied, "coop-ride applies");
  Expect(group.name == "coop-ride", "coop-ride name");
  Expect(result.lines.size() == 3u, "coop-ride 3 lines");
  Expect(result.lines.size() == 3u && EndsWith(result.lines[0], "script grown to 369 bytes") &&
             EndsWith(result.lines[1], "script grown to 451 bytes") && EndsWith(result.lines[2], "script grown to 532 bytes, with 2 write(s)"),
         "coop-ride lines grown to 369, 451 and 532");
  uint32_t enter = guest.Path("GearGame.GearPawn.OnGearEnterVehicle", "Function");
  uint32_t enter_data = guest.U32(enter + 84u);
  Expect(guest.U8(enter_data + 0xA6u) == 0x07 && guest.U8(enter_data + 0xA7u) == 0x01 &&
             guest.U8(enter_data + 0xA8u) == 0x6E,
         "coop-ride enter tail head");
  Expect(guest.U8(enter_data + 0x16Bu) == 0x27 && guest.U8(enter_data + 0x16Cu) == 0x31 &&
             guest.U8(enter_data + 0x16Du) == 0x30 && guest.U8(enter_data + 0x16Eu) == 0x04 &&
             guest.U8(enter_data + 0x16Fu) == 0x0B && guest.U8(enter_data + 0x170u) == 0x53,
         "coop-ride enter tail end");
  Expect(guest.U32(enter_data + 0xFCu) == guest.Path("GearGame.GearPC", "Class"),
         "coop-ride enter +0xFC GearPC");
  uint32_t leave = guest.Path("GearGame.GearPawn.OnGearExitVehicle", "Function");
  uint32_t leave_data = guest.U32(leave + 84u);
  Expect(guest.U8(leave_data + 0xD5u) == 0x07 && guest.U8(leave_data + 0xD6u) == 0x01 &&
             guest.U8(leave_data + 0xD7u) == 0xC0,
         "coop-ride exit tail head");
  Expect(guest.U8(leave_data + 0x1BBu) == 0x4A && guest.U8(leave_data + 0x1BCu) == 0x4A &&
             guest.U8(leave_data + 0x1BDu) == 0x16 && guest.U8(leave_data + 0x1BEu) == 0x31 &&
             guest.U8(leave_data + 0x1BFu) == 0x30 && guest.U8(leave_data + 0x1C0u) == 0x04 &&
             guest.U8(leave_data + 0x1C1u) == 0x0B && guest.U8(leave_data + 0x1C2u) == 0x53,
         "coop-ride exit tail end");
  Expect(guest.U32(leave_data + 0x1A8u) == guest.Path("GearGame.GearPC.FindAvailableTeleportSpot", "Function"),
         "coop-ride exit +0x1A8 FindAvailableTeleportSpot");
  uint32_t sync = guest.Path("GearGame.GearGameSP_Base.SyncKismetSetProperties", "Function");
  uint32_t sync_data = guest.U32(sync + 84u);
  Expect(guest.U8(sync_data + 0x1Du) == 0x06, "coop-ride sync +0x1D patched");
  Expect(guest.U8(sync_data + 0x1Fu) == 0xCB, "coop-ride sync +0x1F patched");
  Expect(guest.U8(sync_data + 0x1CBu) == 0x07 && guest.U8(sync_data + 0x1CCu) == 0x01 &&
             guest.U8(sync_data + 0x1CDu) == 0xC7,
         "coop-ride sync tail head");
  Expect(guest.U8(sync_data + 0x210u) == 0x06 && guest.U8(sync_data + 0x211u) == 0x00 &&
             guest.U8(sync_data + 0x212u) == 0x2C,
         "coop-ride sync tail jump");
  Expect(guest.U8(sync_data + 0x213u) == 0x53, "coop-ride sync last byte 0x53");
  Expect(guest.U32(sync + 88u) == 532u && guest.U32(sync + 92u) == 532u,
         "coop-ride sync Num==Max==532");
}

void TestFollowUpHashes() {
  Expect(GroupHash(CoopBodyGroup(false, false), 4) != GroupHash(CoopBodyGroup(true, false), 4),
         "follow-up body hash differs by carmine");
  Expect(GroupHash(CoopBodyGroup(false, false), 4) == GroupHash(CoopBodyGroup(false, false), 4),
         "follow-up body hash stable across builds");
  Expect(GroupHash(CoopRideGroup(), 4) != 0u, "follow-up ride hash nonzero");
  Expect(GroupHash(CoopStage1Group(false), 4) == 0x937c35f80cb274b7ull,
         "follow-up stage1 hash unchanged");
}

void TestStackTextOrderAndSpacing() {
  std::vector<StackEntry> entries = {
      {"coop", "937c35f80cb274b7"}, {"survival", "off"}, {"coop-scenes", "skipped"}};
  Expect(StackText(4, entries) == "players=4 coop=937c35f80cb274b7 survival=off coop-scenes=skipped",
         "stack text order and spacing");
  Expect(StackText(3, {}) == "players=3", "stack text without entries");
}

void TestStackHashKnownValues() {
  Expect(StackHash("") == 14695981039346656037ull, "stack hash empty is basis");
  Expect(StackHash("a") == 0xaf63dc4c8601ec8cull, "stack hash a");
}

void TestStackLineEndsWithCombined() {
  std::vector<StackEntry> entries = {{"coop", "937c35f80cb274b7"}, {"survival", "off"}};
  std::string text = StackText(4, entries);
  std::string want = text + " combined=" + FormatGroupHash(StackHash(text));
  Expect(StackLine(4, entries) == want, "stack line is text plus combined");
  Expect(EndsWith(StackLine(4, entries), " combined=" + FormatGroupHash(StackHash(text))),
         "stack line ends with combined");
}

void TestFormatGroupHashPads() {
  Expect(FormatGroupHash(0u) == "0000000000000000", "group hash zero pads to 16");
  Expect(FormatGroupHash(0xabul) == "00000000000000ab", "group hash pads short value");
  Expect(FormatGroupHash(0x937c35f80cb274b7ull) == "937c35f80cb274b7", "group hash full value");
}

void TestStackCombinedDiffersOnOneState() {
  std::vector<StackEntry> first = {{"coop", "937c35f80cb274b7"}, {"survival", "off"}};
  std::vector<StackEntry> second = first;
  second[1].state = "skipped";
  Expect(StackHash(StackText(4, first)) != StackHash(StackText(4, second)),
         "stack combined differs on one state");
}

// The default stack at 8 and 10 players (docs/COOP_STAGE3_PLAN.md section 5 step 2).
void TestStackCombinedAtLargeCounts() {
  Expect(GroupHash(CoopStage1Group(false), 8) == 0x60bb547961baa4f7ull, "stage1 hash at 8");
  Expect(GroupHash(CoopStage1Group(false), 10) == 0xe5a7b50e83bd01d9ull, "stage1 hash at 10");
  Expect(GroupHash(CoopBodyGroup(false, false), 8) == 0x86cb94f58e6c76d2ull, "body hash at 8");
  Expect(GroupHash(CoopBodyGroup(false, false), 10) == 0xd754a3c737744215ull, "body hash at 10");
  Expect(GroupHash(CoopRideGroup(), 8) == 0x9bf0050eec22acdcull, "ride hash at 8");
  Expect(GroupHash(CoopRideGroup(), 10) == 0x6e31696ad129342bull, "ride hash at 10");
  std::vector<StackEntry> eight = {
      {"coop", "60bb547961baa4f7"},      {"survival", "2ce334f51ab690d1"},
      {"split-revive", "off"},           {"coop-body", "86cb94f58e6c76d2"},
      {"coop-ride", "9bf0050eec22acdc"}, {"coop-scenes", "803cef5fa03d88f8"}};
  std::vector<StackEntry> ten = {
      {"coop", "e5a7b50e83bd01d9"},      {"survival", "eded09c52ff086aa"},
      {"split-revive", "off"},           {"coop-body", "d754a3c737744215"},
      {"coop-ride", "6e31696ad129342b"}, {"coop-scenes", "83634267c1bfe3bd"}};
  Expect(EndsWith(StackLine(8, eight), " combined=b2cb03575c3f3175"), "combined at 8");
  Expect(EndsWith(StackLine(10, ten), " combined=97bc5d0d82d8e6e2"), "combined at 10");
}

}  // namespace

int main() {
  TestRealGroupApplies();
  TestRealGroupAppliesAtLargeCounts();
  TestStackCombinedAtLargeCounts();
  TestSecondApplyIsAlreadyApplied();
  TestWrongOldByteSkipsWholeGroup();
  TestWrongNumSkips();
  TestWrongOperandSkips();
  TestFunctionFoundTwiceSkips();
  TestCorruptedTableSkips();
  TestMaxDiffersSkipsGrow();
  TestAllocFailureSkips();
  TestMarcusClonesSwapsOperand();
  TestUnreadableTablesSkip();
  TestWideAndCaseInsensitiveNames();
  TestPlayersOutOfRangeSkips();
  TestHashDependsOnPlayers();
  TestGuestBytesCrossThePhysicalBoundary();
  TestGrowIntoPhysicalHeap();
  TestGrowWithPrefixEditUsesEditedPrefix();
  TestGrowEditPastKeptPrefixSkips();
  TestObjectFieldWritesBigEndian();
  TestObjectFieldSecondApplyIsAlreadyApplied();
  TestObjectFieldUnrelatedValueSkipsGroup();
  TestObjectFieldAnchorMismatchSkips();
  TestObjectFieldMissingValueSkips();
  TestStage1HashUnchanged();
  TestObjectFieldsChangeHash();
  TestObjectFieldLinesComeLast();
  TestCoopBodyGroupApplies();
  TestCoopBodyGroupSecondApplyIsAlreadyApplied();
  TestCoopBodyCarmineKeepsBenBranch();
  TestCoopBodyMarcusClonesSkipsClassRow();
  TestCoopBodySkipsOnUngrownSuggest();
  TestCoopBodyAfterStage1OnOneGuest();
  TestCoopBodyCarmineAfterStage1OnOneGuest();
  TestCoopRideGroupApplies();
  TestFollowUpHashes();
  TestStackTextOrderAndSpacing();
  TestStackHashKnownValues();
  TestStackLineEndsWithCombined();
  TestFormatGroupHashPads();
  TestStackCombinedDiffersOnOneState();
  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return (g_failures == 0) ? 0 : 1;
}
