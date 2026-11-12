// gow2 - ReXGlue Recompiled Project
//
// Applies the script patch groups of gow2_script_patches.h right after the engine has loaded its
// native script packages, by replacing the recompiled FStartupPackages::LoadAll.

#include "gow2_script_patches.h"
#include "gow2_script_patch_groups.h"
#include "gow2_script_patch_stack.h"
#include "gow2_script_patch_survival.h"
#include "gow2_script_patch_body.h"
#include "gow2_coop_census.h"
#include "gow2_coop_network.h"
#include "gow2_script_patch_requests.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc.h>
#include <rex/runtime.h>
#include <rex/system/function_dispatcher.h>
#include <rex/system/xmemory.h>

#include "gow2_guest.h"

REXCVAR_DEFINE_INT32(gow2_coop_players, 2, "Coop",
                     "Campaign co-op player cap. 2 keeps the retail game; 3 to 255 apply the co-op "
                     "patch groups (Stage 1 and 2, docs/COOP_N_PLAYERS.md) for that many players and "
                     "turn System Link on. Every player, host and friends, must use the same value and "
                     "the same build")
    .range(2, 255);
REXCVAR_DEFINE_BOOL(gow2_coop_marcus_clones, false, "Coop",
                    "Debug (row D-1): players 3+ spawn as Marcus clones instead of Ben Carmine or a "
                    "RedShirt. Breaks Kismet's hero lookups; for network bring-up only");
REXCVAR_DEFINE_BOOL(gow2_coop_patch_fail_test, false, "Coop",
                    "Debug: corrupt one expected byte of the co-op patch rows. The whole group must "
                    "then be skipped and the game stays retail");
REXCVAR_DEFINE_BOOL(gow2_coop_survival, true, "Coop",
                    "With gow2_coop_players 3+: players 3+ keep their pawn through chapter travel, "
                    "respawn near a teammate instead of ending the game, leave without an AI Dom "
                    "taking their body, and do not block checkpoints while dead (Stage 2 group A, "
                    "docs/COOP_STAGE2_SURVIVAL.md). false keeps Stage 1 only");
REXCVAR_DEFINE_BOOL(gow2_coop_split_revive, false, "Coop",
                    "Debug, row S2A-4: in co-op splits a standing player 3+ of the downed "
                    "player's squad counts as a reviver");
REXCVAR_DEFINE_BOOL(gow2_coop_extra_carmine, false, "Coop",
                    "Players 3+ are Ben Carmine where his class is loaded (chapters 2, 5-11) and "
                    "GearPawn_COGGear elsewhere; off makes them always GearPawn_COGGear with Dom's "
                    "body, which exists in every chapter on every peer");
REXCVAR_DEFINE_BOOL(gow2_coop_vehicle_ride, true, "Coop",
                    "While Marcus or Dom rides a vehicle, players 3+ on foot cannot be hurt, and they "
                    "are brought to him when he gets out (rows CR-1 and CR-2)");
REXCVAR_DEFINE_BOOL(gow2_coop_body, true, "Coop",
                    "Players 3+ get Dom's body on GearPawn_COGGear, the pistol and Gnasher at every "
                    "spawn, and spawn without Right Trigger (rows CB-1 to CB-5); off keeps the "
                    "Stage 1 behaviour");
REXCVAR_DEFINE_BOOL(gow2_coop_requests, true, "Coop",
                    "Campaign hosts read the co-op requests guests send (friendly fire vote, Go to "
                    "player 1) and apply friendly fire when every player opts in (rows RQ-1 and "
                    "FF-1); off keeps ServerMutate and ReduceDamage retail");

namespace rex::glue {

static_assert(guest::PhysicalHostOffset(0xDFFFFFFFu) ==
                      rex::memory::detail::PhysicalHostOffset(0xDFFFFFFFu) &&
                  guest::PhysicalHostOffset(0xE0000000u) ==
                      rex::memory::detail::PhysicalHostOffset(0xE0000000u),
              "gow2_guest.h must place the 0xE0000000 heap where the runtime maps it");

namespace {

using script_patch::GroupResult;
using script_patch::PatchGroup;

constexpr uint32_t kGNames = 0x83136834;
constexpr uint32_t kGObjObjects = 0x83136860;
constexpr uint32_t kGMalloc = 0x8310B12C;
constexpr uint32_t kMallocSlot = 4;
// The alignment the engine passes when it allocates a package buffer (0x8226F610).
constexpr uint32_t kMallocAlign = 8;
constexpr int kFirstCoopCount = 3;
constexpr int kLastCoopCount = 255;

// Written once by the startup hook below; read by CoopStage1Applied() and CoopStackHash().
std::atomic<bool> g_coop_stage1_applied{false};
std::atomic<uint64_t> g_coop_stack_hash{0};
std::atomic<bool> g_coop_requests_applied{false};
std::atomic<bool> g_coop_ff_applied{false};

uint32_t GuestMalloc(PPCContext& ctx, uint8_t* base, uint32_t size) {
  rex::Runtime* runtime = rex::Runtime::instance();
  rex::runtime::FunctionDispatcher* dispatcher = runtime ? runtime->function_dispatcher() : nullptr;
  const uint32_t gmalloc = guest::LoadU32(base, kGMalloc);
  if (!dispatcher || gmalloc == 0) return 0;
  const uint32_t vtable = guest::LoadU32(base, gmalloc);
  if (vtable == 0) return 0;
  PPCFunc* malloc_fn = dispatcher->GetFunction(guest::LoadU32(base, vtable + kMallocSlot));
  if (!malloc_fn) return 0;
  PPCContext call = ctx;
  call.r3.u64 = gmalloc;
  call.r4.u64 = size;
  call.r5.u64 = kMallocAlign;
  malloc_fn(call, base);
  return call.r3.u32;
}

void CorruptFirstExpectedByte(PatchGroup* group) {
  if (group->scripts.empty() || group->scripts[0].checks.empty()) return;
  script_patch::BytesCheck& check = group->scripts[0].checks[0];
  if (check.bytes.empty()) return;
  check.bytes[0] = uint8_t(~check.bytes[0]);
  REXLOG_WARN("SCRIPTPATCH coop: fail test on, row {} expects {:02X} at +0x{:04X}",
              group->scripts[0].id, check.bytes[0], check.offset);
}

struct ApplyCall {
  uint8_t* base;
  const PatchGroup* group;
  int players;
  const script_patch::GuestAlloc* alloc;
  GroupResult* result;
};

void RunApply(ApplyCall* call) {
  *call->result = script_patch::ApplyGroup(call->base, {kGNames, kGObjObjects}, *call->group,
                                           call->players, *call->alloc);
}

// False when reading the guest tables faulted (a wrong layout), before anything was written.
bool GuardedApply(ApplyCall* call) {
#ifdef _WIN32
  __try {
    RunApply(call);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
#else
  RunApply(call);
  return true;
#endif
}

bool ApplyPatchGroup(uint8_t* base, const PatchGroup& group, int players,
                     const script_patch::GuestAlloc& alloc, GroupResult* result) {
  ApplyCall call{base, &group, players, &alloc, result};
  return GuardedApply(&call);
}

void LogResult(std::string_view tag, std::string_view when_skipped, const GroupResult& result,
               int players) {
  if (!result.applied) {
    REXLOG_WARN("SCRIPTPATCH {}: skipped ({}: {}); {}", tag, result.failed_row, result.reason,
                when_skipped);
    return;
  }
  for (const std::string& line : result.lines) REXLOG_INFO("SCRIPTPATCH {} {}", tag, line);
  REXLOG_INFO("SCRIPTPATCH {}: {} rows applied for {} players, set hash {:016x}", tag,
              result.lines.size(), players, result.set_hash);
}

std::string StateForResult(const GroupResult& result) {
  return result.applied ? script_patch::FormatGroupHash(result.set_hash) : "skipped";
}

void RecordOff(std::vector<script_patch::StackEntry>& stack, std::string_view name) {
  stack.push_back({std::string(name), "off"});
}

GroupResult ApplyAndLogGroup(uint8_t* base, const PatchGroup& group, int players,
                             const script_patch::GuestAlloc& alloc, std::string_view when_skipped,
                             std::vector<script_patch::StackEntry>& stack) {
  GroupResult result;
  if (!ApplyPatchGroup(base, group, players, alloc, &result)) {
    REXLOG_WARN("SCRIPTPATCH {}: skipped (fault while reading guest objects); {}", group.name,
                when_skipped);
  } else {
    LogResult(group.name, when_skipped, result, players);
  }
  stack.push_back({std::string(group.name), StateForResult(result)});
  return result;
}

void ApplySurvivalGroups(uint8_t* base, const script_patch::GuestAlloc& alloc, bool coop_applied,
                         int players, std::vector<script_patch::StackEntry>& stack) {
  if (!coop_applied) {
    REXLOG_INFO("SCRIPTPATCH survival: off (the coop group was not applied)");
    RecordOff(stack, "survival");
    RecordOff(stack, "split-revive");
    return;
  }
  if (!REXCVAR_GET(gow2_coop_survival)) {
    REXLOG_INFO("SCRIPTPATCH survival: off (gow2_coop_survival=false)");
    RecordOff(stack, "survival");
    RecordOff(stack, "split-revive");
    return;
  }
  const GroupResult survival = ApplyAndLogGroup(base, script_patch::CoopSurvivalGroup(), players,
                                                alloc, "Stage 1 stays applied", stack);
  if (survival.applied && REXCVAR_GET(gow2_coop_split_revive)) {
    ApplyAndLogGroup(base, script_patch::CoopSplitReviveGroup(), players, alloc,
                     "the survival group stays applied", stack);
  } else {
    RecordOff(stack, "split-revive");
  }
}

void ApplyRegisteredGroup(const script_patch::CoopGroupEntry& entry, uint8_t* base,
                          const script_patch::GuestAlloc& alloc, bool coop_applied, int players,
                          std::vector<script_patch::StackEntry>& stack) {
  if (!coop_applied) {
    REXLOG_INFO("SCRIPTPATCH {}: off (the coop group was not applied)", entry.name);
    RecordOff(stack, entry.name);
    return;
  }
  std::optional<PatchGroup> group = entry.make();
  if (!group) {
    // The factory logs its own off line; the hook never tried this group.
    RecordOff(stack, entry.name);
    return;
  }
  ApplyAndLogGroup(base, *group, players, alloc, "the other groups stay applied", stack);
}

void ApplyRegisteredGroups(uint8_t* base, const script_patch::GuestAlloc& alloc, bool coop_applied,
                           int players, std::vector<script_patch::StackEntry>& stack) {
  for (const script_patch::CoopGroupEntry& entry : script_patch::CoopGroupsByName()) {
    ApplyRegisteredGroup(entry, base, alloc, coop_applied, players, stack);
  }
}

constexpr std::string_view kBodyFallback = "players 3+ keep the Stage 1 behaviour";
constexpr std::string_view kRideFallback = "vehicles stay retail";

void ApplyCoopBody(uint8_t* base, int players, const script_patch::GuestAlloc& alloc,
                   std::vector<script_patch::StackEntry>& stack) {
  if (!REXCVAR_GET(gow2_coop_body)) {
    REXLOG_INFO("SCRIPTPATCH coop-body: off (gow2_coop_body=false)");
    RecordOff(stack, "coop-body");
    return;
  }
  const PatchGroup body =
      script_patch::CoopBodyGroup(REXCVAR_GET(gow2_coop_extra_carmine),
                                  REXCVAR_GET(gow2_coop_marcus_clones));
  ApplyAndLogGroup(base, body, players, alloc, kBodyFallback, stack);
}

void ApplyCoopFollowUps(uint8_t* base, bool coop_applied, int players,
                        const script_patch::GuestAlloc& alloc,
                        std::vector<script_patch::StackEntry>& stack) {
  if (!coop_applied) {
    REXLOG_INFO("SCRIPTPATCH coop-body: off (the coop group was not applied)");
    REXLOG_INFO("SCRIPTPATCH coop-ride: off (the coop group was not applied)");
    RecordOff(stack, "coop-body");
    RecordOff(stack, "coop-ride");
    return;
  }
  ApplyCoopBody(base, players, alloc, stack);
  if (!REXCVAR_GET(gow2_coop_vehicle_ride)) {
    REXLOG_INFO("SCRIPTPATCH coop-ride: off (gow2_coop_vehicle_ride=false)");
    RecordOff(stack, "coop-ride");
    return;
  }
  ApplyAndLogGroup(base, script_patch::CoopRideGroup(), players, alloc, kRideFallback, stack);
}

// The game links the prebuilt SDK and swaps in the patched runtime, so the cvar is the only way in.
void PublishCoopHashToSystemLink(uint64_t combined) {
  if (rex::cvar::SetFlagByName("net_coop_hash", std::to_string(combined))) return;
  REXLOG_WARN("SCRIPTPATCH stack: this runtime has no net_coop_hash (SDK patch 0012), so System "
              "Link does not compare co-op hashes");
}

// Host-only rows that change nothing on the wire, so they stay out of the System Link hash.
void ApplyRequestGroups(const script_patch::GuestAlloc& alloc, uint8_t* base, int players) {
  if (!REXCVAR_GET(gow2_coop_requests)) {
    REXLOG_INFO("SCRIPTPATCH coop-requests: off (gow2_coop_requests=false)");
    return;
  }
  std::vector<script_patch::StackEntry> unused;
  const GroupResult requests = ApplyAndLogGroup(base, script_patch::CoopRequestsGroup(), players,
                                                alloc, "guests cannot send requests", unused);
  const GroupResult ff = ApplyAndLogGroup(base, script_patch::CoopFriendlyFireGroup(), players,
                                          alloc, "friendly fire stays retail", unused);
  g_coop_requests_applied.store(requests.applied, std::memory_order_release);
  g_coop_ff_applied.store(ff.applied, std::memory_order_release);
}

void ApplyStartupScriptPatches(PPCContext& ctx, uint8_t* base) {
  REXLOG_INFO("SCRIPTPATCH startup script packages loaded");
  StartCoopCensus(base);
  const int players = REXCVAR_GET(gow2_coop_players);
  ApplyRequestGroups([&ctx, base](uint32_t size) { return GuestMalloc(ctx, base, size); }, base,
                     players);
  if (players < kFirstCoopCount || players > kLastCoopCount) {
    REXLOG_INFO("SCRIPTPATCH coop: off (gow2_coop_players={})", players);
    return;
  }
  PatchGroup group = script_patch::CoopStage1Group(REXCVAR_GET(gow2_coop_marcus_clones));
  if (REXCVAR_GET(gow2_coop_patch_fail_test)) CorruptFirstExpectedByte(&group);
  const script_patch::GuestAlloc alloc = [&ctx, base](uint32_t size) {
    return GuestMalloc(ctx, base, size);
  };
  std::vector<script_patch::StackEntry> stack;
  const GroupResult coop =
      ApplyAndLogGroup(base, group, players, alloc, "the game stays retail", stack);
  const bool coop_applied = coop.applied;
  ApplySurvivalGroups(base, alloc, coop_applied, players, stack);
  ApplyCoopFollowUps(base, coop_applied, players, alloc, stack);
  ApplyRegisteredGroups(base, alloc, coop_applied, players, stack);
  REXLOG_INFO("SCRIPTPATCH stack: {}", script_patch::StackLine(players, stack));
  const uint64_t hash = script_patch::StackHash(script_patch::StackText(players, stack));
  g_coop_stack_hash.store(hash, std::memory_order_release);
  g_coop_stage1_applied.store(coop_applied, std::memory_order_release);
  PublishCoopHashToSystemLink(hash);
}

}  // namespace

namespace script_patch {

bool CoopStage1Applied() { return g_coop_stage1_applied.load(std::memory_order_acquire); }
uint64_t CoopStackHash() { return g_coop_stack_hash.load(std::memory_order_acquire); }
bool CoopRequestsApplied() { return g_coop_requests_applied.load(std::memory_order_acquire); }
bool CoopFriendlyFireApplied() { return g_coop_ff_applied.load(std::memory_order_acquire); }

}  // namespace script_patch

void EnableSystemLinkForCoop() {
  const int players = REXCVAR_GET(gow2_coop_players);
  if (players < kFirstCoopCount) return;
  if (rex::cvar::GetFlagSource("net_link_active") != rex::cvar::Source::kDefault) return;
  if (rex::cvar::SetFlagByName("net_link_active", "true"))
    REXLOG_INFO("SCRIPTPATCH coop: gow2_coop_players={} turns System Link on (net_link_active=true)",
                players);
}

}  // namespace rex::glue

REX_EXTERN(__imp__sub_8283F780);

// FStartupPackages::LoadAll (one call, 0x8284075C): when it returns, Core, Engine, GameFramework,
// IpDrv, GearGame and OnlineSubsystemLive are loaded and no gameplay script has run.
REX_HOOK_RAW(sub_8283F780) {
  __imp__sub_8283F780(ctx, base);
  rex::glue::ApplyStartupScriptPatches(ctx, base);
}
