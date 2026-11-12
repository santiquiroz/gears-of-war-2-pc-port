// gow2 - ReXGlue Recompiled Project
//
// Host-side leash for co-op players 3+: whoever stays far from the nearest human hero is moved
// next to him (spot pull, else one of 8 fallback spots).
#include <chrono>
#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <rex/cvar.h>
#include <rex/logging.h>

#include "gow2_coop_leash_plan.h"
#include "gow2_coop_pull.h"
#include "gow2_dev_commands.h"
#include "gow2_dev_guest_calls.h"
#include "gow2_engine_tick.h"
#include "gow2_guest.h"
#include "gow2_script_patch_groups.h"

#ifdef _WIN32
constexpr bool kCoopLeashDefault = true;
#else
constexpr bool kCoopLeashDefault = false;
#endif

REXCVAR_DEFINE_BOOL(gow2_coop_leash, kCoopLeashDefault, "Coop",
                    "Players 3+ who stay far from the nearest human Marcus or Dom are moved next "
                    "to him, and players 3+ on foot cannot be hurt while Marcus or Dom rides a "
                    "vehicle (host only; gow2_coop_players 3 or more). On by default on Windows "
                    "only: elsewhere a bad guest read would crash the game instead of being skipped. "
                    "A player of any slot who falls far below everyone else is moved next to the nearest player.");
REXCVAR_DEFINE_DOUBLE(gow2_coop_leash_distance, 4000.0, "Coop",
                      "Distance in game units beyond which a player 3+ counts as left behind");
REXCVAR_DEFINE_INT32(gow2_coop_leash_seconds, 8, "Coop",
                     "Seconds a player 3+ must stay left behind before the leash moves him");
REXCVAR_DECLARE(int32_t, gow2_coop_players);

namespace rex::glue {
namespace {

using coop_pull::kActorLocation;
using coop_pull::LoadFloat;
using coop_pull::PcName;

constexpr int kMinLeashPlayers = 3;
constexpr auto kLeashPeriod = std::chrono::seconds(1);

struct LeashSettings {
  bool ready = false;
  bool enabled = false;
  float distance = 0;
  int seconds = 0;
};

LeashSettings& TheSettings() {
  static LeashSettings settings;
  return settings;
}

void InitSettings(LeashSettings* settings) {
  if (settings->ready) return;
  settings->ready = true;
  if (!REXCVAR_GET(gow2_coop_leash)) return;
  if (REXCVAR_GET(gow2_coop_players) < kMinLeashPlayers) return;
  if (!script_patch::CoopStage1Applied()) {
    REXLOG_INFO("COOPLEASH off (the coop group was not applied)");
    return;
  }
  settings->enabled = true;
  settings->distance = float(REXCVAR_GET(gow2_coop_leash_distance));
  settings->seconds = REXCVAR_GET(gow2_coop_leash_seconds);
  REXLOG_INFO("COOPLEASH on: players 3+ further than {} units from the nearest human hero for "
              "{} s are moved next to him; anyone {} units below every other player for {} s is "
              "moved next to the nearest one",
              settings->distance, settings->seconds, coop_leash::kFellBelowHeroes,
              coop_leash::kHeroFallSeconds);
}

bool TakeSnapshotTurn() {
  static std::chrono::steady_clock::time_point last{};
  const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  if (now - last < kLeashPeriod) return false;
  last = now;
  return true;
}

bool IsCampaignController(const uint8_t* base, uint32_t pc) {
  return dev_commands::ClassChainHas(base, dev_commands::kGameGlobals, pc, "GearPC_SP");
}

std::optional<std::vector<coop_leash::LeashPc>> TakeSnapshot(const uint8_t* base) {
  const uint32_t world_info = dev_commands::WorldInfoOf(base, dev_commands::kGameGlobals);
  if (world_info == 0) return std::nullopt;
  if (guest::LoadU8(base, world_info + dev_commands::layout::kWorldInfoNetMode) ==
      dev_commands::layout::kNetModeClient) {
    return std::nullopt;
  }
  std::vector<coop_leash::LeashPc> pcs;
  for (uint32_t pc : dev_commands::PlayerControllers(base, dev_commands::kGameGlobals, world_info)) {
    if (!IsCampaignController(base, pc)) continue;
    pcs.push_back(coop_pull::ReadLeashPc(base, pc));
  }
  return pcs;
}

std::optional<std::vector<coop_leash::LeashPc>> GuardedSnapshot(const uint8_t* base) {
  std::optional<std::vector<coop_leash::LeashPc>> out;
  auto read = [&] { out = TakeSnapshot(base); };
  if (!guest_call::Guarded(read)) return std::nullopt;
  return out;
}

bool ExtraIsClose(const uint8_t* base, const coop_leash::PullOrder& order) {
  return coop_pull::IsNear(base, order.extra_pawn, order.anchor_x, order.anchor_y, order.anchor_z,
                           coop_leash::kPulledCloseEnough);
}

bool OverlapPullSucceeded(const uint8_t* base, const coop_leash::PullOrder& order) {
  bool ok = false;
  auto read = [&] {
    const float x = LoadFloat(base, order.extra_pawn + kActorLocation);
    const float y = LoadFloat(base, order.extra_pawn + kActorLocation + 4);
    const float z = LoadFloat(base, order.extra_pawn + kActorLocation + 8);
    const double ox = double(x - order.overlap_x);
    const double oy = double(y - order.overlap_y);
    const double oz = double(z - order.overlap_z);
    const double ax = double(x - order.anchor_x);
    const double ay = double(y - order.anchor_y);
    const double az = double(z - order.anchor_z);
    const bool clear = std::sqrt(ox * ox + oy * oy + oz * oz) > coop_leash::kOverlapDistance;
    const bool close = std::sqrt(ax * ax + ay * ay + az * az) <= coop_leash::kPulledCloseEnough;
    ok = clear && close;
  };
  if (!guest_call::Guarded(read)) return false;
  return ok;
}

bool SpotPullSucceeded(const uint8_t* base, const coop_leash::PullOrder& order) {
  if (order.reason == coop_leash::PullReason::kOverlap) return OverlapPullSucceeded(base, order);
  return ExtraIsClose(base, order);
}

int FellDepth(const std::vector<coop_leash::LeashPc>& pcs, const coop_leash::PullOrder& order) {
  for (const coop_leash::LeashPc& pc : pcs) {
    if (pc.pc != order.extra_pc) continue;
    return std::lround(double(order.hero_z - pc.z));
  }
  return 0;
}

std::string AnchorLabel(const uint8_t* base, const coop_leash::PullOrder& order) {
  const std::string anchor = PcName(base, order.anchor_pc);
  if (order.anchor_pc == order.hero_pc) return anchor;
  return anchor + " near " + PcName(base, order.hero_pc);
}

void LogSpotPull(const uint8_t* base, const coop_leash::PullOrder& order, int fell_below) {
  if (order.reason == coop_leash::PullReason::kOverlap) {
    REXLOG_INFO("COOPLEASH {} moved next to {} (was {} units away; overlapped {}{})",
                PcName(base, order.extra_pc), AnchorLabel(base, order),
                std::lround(order.distance), PcName(base, order.overlap_pc),
                order.cinematic ? "; during a cinematic" : "");
    return;
  }
  if (order.reason == coop_leash::PullReason::kHeroFell) {
    REXLOG_INFO("COOPLEASH {} rescued next to {} (fell {} units below every other player)",
                PcName(base, order.extra_pc), AnchorLabel(base, order), fell_below);
    return;
  }
  if (order.reason == coop_leash::PullReason::kFell) {
    REXLOG_INFO("COOPLEASH {} moved next to {} (was {} units away; fell {} units below every hero)",
                PcName(base, order.extra_pc), AnchorLabel(base, order),
                std::lround(order.distance), fell_below);
    return;
  }
  REXLOG_INFO("COOPLEASH {} moved next to {} (was {} units away)", PcName(base, order.extra_pc),
              AnchorLabel(base, order), std::lround(order.distance));
}

void LogFallbackPull(const uint8_t* base, const coop_leash::PullOrder& order, float dx, float dy,
                     int fell_below) {
  if (order.reason == coop_leash::PullReason::kOverlap) {
    REXLOG_INFO("COOPLEASH {} placed {},{} from {} (was {} units away; overlapped {}; no "
                "navigation point near him{})",
                PcName(base, order.extra_pc), dx, dy, AnchorLabel(base, order),
                std::lround(order.distance), PcName(base, order.overlap_pc),
                order.cinematic ? "; during a cinematic" : "");
    return;
  }
  if (order.reason == coop_leash::PullReason::kHeroFell) {
    REXLOG_INFO("COOPLEASH {} placed {},{} from {} (fell {} units below every other player; no "
                "navigation point near him)",
                PcName(base, order.extra_pc), dx, dy, AnchorLabel(base, order), fell_below);
    return;
  }
  if (order.reason == coop_leash::PullReason::kFell) {
    REXLOG_INFO("COOPLEASH {} placed {},{} from {} (was {} units away; fell {} units below every "
                "hero; no navigation point near him)",
                PcName(base, order.extra_pc), dx, dy, AnchorLabel(base, order),
                std::lround(order.distance), fell_below);
    return;
  }
  REXLOG_INFO("COOPLEASH {} placed {},{} from {} (was {} units away; no navigation point near him)",
              PcName(base, order.extra_pc), dx, dy, AnchorLabel(base, order),
              std::lround(order.distance));
}

bool TrySpotPull(const PPCContext& ctx, uint8_t* base, const coop_leash::PullOrder& order,
                 int fell_below) {
  if (!coop_pull::RequestTeleportSpot(ctx, base, order.anchor_pc, order.extra_pawn)) return false;
  if (!SpotPullSucceeded(base, order)) return false;
  LogSpotPull(base, order, fell_below);
  return true;
}

bool TryPlaceAt(const PPCContext& ctx, uint8_t* base, const coop_leash::PullOrder& order,
                uint32_t place_fn, float dx, float dy, int fell_below) {
  const float x = order.anchor_x + dx;
  const float y = order.anchor_y + dy;
  const float z = order.anchor_z + coop_leash::kFallbackRaiseZ;
  if (!coop_pull::PlaceAt(ctx, base, place_fn, order.extra_pc, order.extra_pawn, x, y, z)) {
    return false;
  }
  LogFallbackPull(base, order, dx, dy, fell_below);
  return true;
}

bool TryFallbackPull(const PPCContext& ctx, uint8_t* base, const coop_leash::PullOrder& order,
                     int fell_below) {
  const uint32_t place_fn = coop_pull::FindSetLocation(ctx, base, order.extra_pawn);
  if (place_fn == 0) return false;
  for (const auto [dx, dy] : coop_leash::FallbackOffsets()) {
    if (TryPlaceAt(ctx, base, order, place_fn, dx, dy, fell_below)) return true;
  }
  return false;
}

void PullExtra(const PPCContext& ctx, uint8_t* base, const coop_leash::PullOrder& order,
               int fell_below) {
  if (TrySpotPull(ctx, base, order, fell_below)) return;
  if (TryFallbackPull(ctx, base, order, fell_below)) return;
  REXLOG_WARN("COOPLEASH {}: no free spot next to {} ({} units away)", PcName(base, order.extra_pc),
              PcName(base, order.hero_pc), std::lround(order.distance));
}

bool SetGodMode(uint8_t* base, uint32_t pc, bool on) {
  auto write = [&] {
    const uint32_t flags = guest::LoadU32(base, pc + dev_commands::layout::kControllerFlags);
    guest::StoreU32(base, pc + dev_commands::layout::kControllerFlags,
                    dev_commands::WithGodMode(flags, on));
  };
  return guest_call::Guarded(write);
}

const coop_leash::LeashPc* FindRidingHero(const std::vector<coop_leash::LeashPc>& pcs) {
  for (const coop_leash::LeashPc& pc : pcs) {
    if (coop_leash::IsRidingHero(pc)) return &pc;
  }
  return nullptr;
}

void ApplyGrants(uint8_t* base, const std::vector<coop_leash::LeashPc>& pcs,
                 const coop_leash::GodOrders& orders) {
  const coop_leash::LeashPc* hero = FindRidingHero(pcs);
  for (uint32_t pc : orders.grant) {
    if (!SetGodMode(base, pc, true)) continue;
    std::string hero_name = "?";
    std::string vehicle = "?";
    if (hero != nullptr) {
      hero_name = PcName(base, hero->pc);
      vehicle = hero->pawn_class;
    }
    REXLOG_INFO("COOPLEASH {}: god mode on ({} rides {})", PcName(base, pc), hero_name, vehicle);
  }
}

void ApplyRevokes(uint8_t* base, const coop_leash::GodOrders& orders) {
  for (uint32_t pc : orders.revoke) {
    if (!SetGodMode(base, pc, false)) continue;
    REXLOG_INFO("COOPLEASH {}: god mode off (no hero rides)", PcName(base, pc));
  }
}

void ApplyGodOrders(uint8_t* base, const std::vector<coop_leash::LeashPc>& pcs,
                    const coop_leash::GodOrders& orders) {
  ApplyGrants(base, pcs, orders);
  ApplyRevokes(base, orders);
}

void LeashTick(const PPCContext& ctx, uint8_t* base) {
  LeashSettings& settings = TheSettings();
  InitSettings(&settings);
  if (!settings.enabled) return;
  if (!TakeSnapshotTurn()) return;
  const std::optional<std::vector<coop_leash::LeashPc>> snapshot = GuardedSnapshot(base);
  if (!snapshot.has_value()) return;
  static coop_leash::LeashState state;
  const coop_leash::LeashConfig config{settings.distance, settings.seconds};
  const std::vector<coop_leash::PullOrder> orders =
      coop_leash::PlanPulls(*snapshot, config, &state);
  for (const coop_leash::PullOrder& order : orders) {
    PullExtra(ctx, base, order, FellDepth(*snapshot, order));
  }
  const coop_leash::GodOrders god = coop_leash::PlanGodMode(*snapshot, &state.god_granted);
  ApplyGodOrders(base, *snapshot, god);
}

[[maybe_unused]] const bool kLeashRegistered =
    (rex::glue::AddEngineTickListener(&LeashTick), true);

}  // namespace
}  // namespace rex::glue
