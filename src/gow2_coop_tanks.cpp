// gow2 - ReXGlue Recompiled Project
//
// Host-side Centaur handout for co-op players 3+: see gow2_coop_tanks.h.
#include "gow2_coop_tanks.h"

#include <cmath>
#include <cstring>
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
#include "gow2_guest.h"

REXCVAR_DEFINE_BOOL(gow2_coop_tanks, true, "Coop",
                    "During the Centaur (tank) sections every player 3+ gets his own Centaur next "
                    "to Marcus's, and gets out of it next to him when the section ends (host only; "
                    "needs gow2_coop_leash)");

namespace rex::glue::coop_tanks {
namespace {
namespace layout = dev_commands::layout;

constexpr uint32_t kTankActorRotation = 96;
constexpr uint32_t kTankActorYaw = 100;
constexpr float kTankSpawnRaiseZ = 150.0f;
constexpr double kPi = 3.14159265358979323846;
constexpr double kYawUnits = 65536.0;
constexpr int kMaxActors = 100000;
constexpr uint32_t kSpawnOutBytes = 52;
constexpr uint32_t kSpawnReturnOffset = 48;
constexpr uint32_t kDriveOutBytes = 8;
constexpr uint32_t kDriveReturnOffset = 4;

constexpr int kTankSpots[][2] = {{-1100, 0}, {-1100, -900}, {-1100, 900}, {0, -1100}, {0, 1100},
                                 {-2200, 0}, {-2200, -900}, {-2200, 900}, {1300, 0}};

coop_leash::TankState& State() {
  static coop_leash::TankState state;
  return state;
}

const coop_leash::LeashPc* FindOrderedPc(const std::vector<coop_leash::LeashPc>& pcs,
                                         uint32_t pc) {
  for (const coop_leash::LeashPc& entry : pcs) {
    if (entry.pc == pc) return &entry;
  }
  return nullptr;
}

uint32_t FloatBits(float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

void AppendU32Be(uint32_t value, std::vector<uint8_t>* out) {
  out->push_back(uint8_t(value >> 24));
  out->push_back(uint8_t(value >> 16));
  out->push_back(uint8_t(value >> 8));
  out->push_back(uint8_t(value));
}

void AppendFloatBe(float value, std::vector<uint8_t>* out) { AppendU32Be(FloatBits(value), out); }

uint32_t DecodeU32Be(const std::vector<uint8_t>& bytes, size_t at) {
  return (uint32_t(bytes[at]) << 24) | (uint32_t(bytes[at + 1]) << 16) |
         (uint32_t(bytes[at + 2]) << 8) | bytes[at + 3];
}

uint32_t FindMemberFn(const PPCContext& ctx, uint8_t* base, uint32_t object, const char* text) {
  static std::map<std::string, std::optional<uint64_t>> names;
  std::optional<uint64_t>& name = names[text];
  if (!name.has_value()) name = guest_call::MakeName(ctx, base, text, false);
  if (!name.has_value()) return 0;
  return guest_call::FindFunction(ctx, base, object, *name);
}

bool ActorListed(const uint8_t* base, uint32_t actor) {
  bool listed = false;
  auto read = [&] {
    if (actor == 0) return;
    const uint32_t world = guest::LoadU32(base, dev_commands::kGameGlobals.world);
    if (world == 0) return;
    const uint32_t level = guest::LoadU32(base, world + layout::kWorldLevel);
    if (level == 0) return;
    const uint32_t data = guest::LoadU32(base, level + layout::kLevelActors);
    const int32_t num = int32_t(guest::LoadU32(base, level + layout::kLevelActors + 4));
    if (data == 0 || num <= 0 || num > kMaxActors) return;
    for (int32_t i = 0; i < num; ++i) {
      if (guest::LoadU32(base, data + uint32_t(i) * 4) == actor) {
        listed = true;
        return;
      }
    }
  };
  if (!guest_call::Guarded(read)) return false;
  return listed;
}

struct HeroVehicle {
  bool ok = false;
  uint32_t spawn_class = 0;
  float x = 0, y = 0, z = 0;
  uint32_t pitch = 0, yaw = 0, roll = 0;
};

HeroVehicle ReadHeroVehicle(const uint8_t* base, const coop_leash::LeashPc& hero) {
  HeroVehicle out;
  auto read = [&] {
    if (hero.pawn == 0) return;
    out.spawn_class = guest::LoadU32(base, hero.pawn + layout::kObjClass);
    out.x = coop_pull::LoadFloat(base, hero.pawn + coop_pull::kActorLocation);
    out.y = coop_pull::LoadFloat(base, hero.pawn + coop_pull::kActorLocation + 4);
    out.z = coop_pull::LoadFloat(base, hero.pawn + coop_pull::kActorLocation + 8);
    out.pitch = guest::LoadU32(base, hero.pawn + kTankActorRotation);
    out.yaw = guest::LoadU32(base, hero.pawn + kTankActorYaw);
    out.roll = guest::LoadU32(base, hero.pawn + kTankActorYaw + 4);
    out.ok = out.spawn_class != 0;
  };
  if (!guest_call::Guarded(read)) return {};
  return out;
}

uint32_t ControllerPawnOf(const uint8_t* base, uint32_t pc) {
  uint32_t pawn = 0;
  auto read = [&] { pawn = guest::LoadU32(base, pc + layout::kControllerPawn); };
  if (!guest_call::Guarded(read)) return 0;
  return pawn;
}

std::vector<uint8_t> SpawnParms(uint32_t spawn_class, float x, float y, float z, uint32_t pitch,
                                uint32_t yaw, uint32_t roll) {
  std::vector<uint8_t> parms;
  AppendU32Be(spawn_class, &parms);
  AppendU32Be(0, &parms);
  AppendU32Be(0, &parms);
  AppendU32Be(0, &parms);
  AppendFloatBe(x, &parms);
  AppendFloatBe(y, &parms);
  AppendFloatBe(z, &parms);
  AppendU32Be(pitch, &parms);
  AppendU32Be(yaw, &parms);
  AppendU32Be(roll, &parms);
  AppendU32Be(0, &parms);
  AppendU32Be(0, &parms);
  return parms;
}

uint32_t SpawnVehicle(const PPCContext& ctx, uint8_t* base, uint32_t world_info,
                      uint32_t spawn_class, float x, float y, float z, uint32_t pitch, uint32_t yaw,
                      uint32_t roll) {
  const uint32_t fn = FindMemberFn(ctx, base, world_info, "Spawn");
  if (fn == 0) return 0;
  std::vector<uint8_t> out;
  if (!guest_call::ProcessEventWithParms(ctx, base, world_info, fn,
                                         SpawnParms(spawn_class, x, y, z, pitch, yaw, roll), &out,
                                         kSpawnOutBytes)) {
    return 0;
  }
  if (out.size() < kSpawnOutBytes) return 0;
  return DecodeU32Be(out, kSpawnReturnOffset);
}

void RotateTankSpot(int forward, int right, double yaw_rad, float hx, float hy, float* x,
                    float* y) {
  const double c = std::cos(yaw_rad);
  const double s = std::sin(yaw_rad);
  *x = hx + float(double(forward) * c - double(right) * s);
  *y = hy + float(double(forward) * s + double(right) * c);
}

uint32_t SpawnNextToHero(const PPCContext& ctx, uint8_t* base, const HeroVehicle& hero,
                         int* spot) {
  const uint32_t world_info = dev_commands::WorldInfoOf(base, dev_commands::kGameGlobals);
  if (world_info == 0) return 0;
  // Yaw units: 65536 is a full turn.
  const double yaw_rad = double(hero.yaw) * (2.0 * kPi / kYawUnits);
  for (size_t i = 0; i < sizeof(kTankSpots) / sizeof(kTankSpots[0]); ++i) {
    float x = 0, y = 0;
    RotateTankSpot(kTankSpots[i][0], kTankSpots[i][1], yaw_rad, hero.x, hero.y, &x, &y);
    const uint32_t vehicle = SpawnVehicle(ctx, base, world_info, hero.spawn_class, x, y,
                                          hero.z + kTankSpawnRaiseZ, hero.pitch, hero.yaw,
                                          hero.roll);
    if (vehicle == 0) continue;
    if (spot != nullptr) *spot = int(i);
    return vehicle;
  }
  return 0;
}

bool TryDrive(const PPCContext& ctx, uint8_t* base, uint32_t vehicle, uint32_t pawn) {
  const uint32_t fn = FindMemberFn(ctx, base, vehicle, "TryToDrive");
  if (fn == 0) return false;
  std::vector<uint8_t> in;
  AppendU32Be(pawn, &in);
  std::vector<uint8_t> out;
  if (!guest_call::ProcessEventWithParms(ctx, base, vehicle, fn, in, &out, kDriveOutBytes)) {
    return false;
  }
  if (out.size() < kDriveOutBytes) return false;
  return DecodeU32Be(out, kDriveReturnOffset) != 0;
}

void ForceLeave(const PPCContext& ctx, uint8_t* base, uint32_t vehicle) {
  const uint32_t fn = FindMemberFn(ctx, base, vehicle, "DriverLeave");
  if (fn == 0) return;
  std::vector<uint8_t> in;
  AppendU32Be(1, &in);
  guest_call::ProcessEventWithParms(ctx, base, vehicle, fn, in, nullptr, 0);
}

void DestroyVehicle(const PPCContext& ctx, uint8_t* base, uint32_t vehicle) {
  const uint32_t fn = FindMemberFn(ctx, base, vehicle, "Destroy");
  if (fn == 0) return;
  guest_call::ProcessEventWithParms(ctx, base, vehicle, fn, {}, nullptr, 0);
}

bool SpawnTankFor(const PPCContext& ctx, uint8_t* base, const coop_leash::LeashPc& extra,
                  const coop_leash::LeashPc& hero) {
  const HeroVehicle ride = ReadHeroVehicle(base, hero);
  if (!ride.ok) {
    coop_leash::NoteTankFailure(extra.pc, &State());
    return false;
  }
  int spot = -1;
  const uint32_t vehicle = SpawnNextToHero(ctx, base, ride, &spot);
  if (vehicle == 0) {
    coop_leash::NoteTankFailure(extra.pc, &State());
    REXLOG_WARN("COOPTANK {}: no room for a {} next to {}", coop_pull::PcName(base, extra.pc),
                std::string(coop_leash::kTankClass), coop_pull::PcName(base, hero.pc));
    return false;
  }
  // After a respawn's DriverLeave the snapshot pawn is still the old vehicle; drive with the body.
  if (!TryDrive(ctx, base, vehicle, ControllerPawnOf(base, extra.pc))) {
    DestroyVehicle(ctx, base, vehicle);
    coop_leash::NoteTankFailure(extra.pc, &State());
    return false;
  }
  State().tanks[extra.pc] = vehicle;
  REXLOG_INFO("COOPTANK {}: own {} next to {} ({},{})", coop_pull::PcName(base, extra.pc),
              std::string(coop_leash::kTankClass), coop_pull::PcName(base, hero.pc),
              kTankSpots[spot][0], kTankSpots[spot][1]);
  return true;
}

bool PlaceNextToHero(const PPCContext& ctx, uint8_t* base, uint32_t mover_pc, uint32_t mover_pawn,
                     const coop_leash::LeashPc& hero) {
  if (coop_pull::RequestTeleportSpot(ctx, base, hero.pc, mover_pawn) &&
      coop_pull::IsNear(base, mover_pawn, hero.x, hero.y, hero.z,
                        coop_leash::kPulledCloseEnough)) {
    return true;
  }
  const uint32_t place_fn = coop_pull::FindSetLocation(ctx, base, mover_pawn);
  if (place_fn == 0) return false;
  for (const auto [dx, dy] : coop_leash::FallbackOffsets()) {
    if (coop_pull::PlaceAt(ctx, base, place_fn, mover_pc, mover_pawn, hero.x + dx, hero.y + dy,
                           hero.z + coop_leash::kFallbackRaiseZ)) {
      return true;
    }
  }
  return false;
}

void PlaceExtraByHero(const PPCContext& ctx, uint8_t* base,
                      const std::vector<coop_leash::LeashPc>& pcs,
                      const coop_leash::LeashPc& extra) {
  const coop_leash::LeashPc* anchor = coop_leash::NearestHero(pcs, extra);
  if (anchor == nullptr) anchor = coop_leash::FindTankDriver(pcs);
  if (anchor == nullptr) return;
  const uint32_t pawn = ControllerPawnOf(base, extra.pc);
  if (pawn == 0) return;
  PlaceNextToHero(ctx, base, extra.pc, pawn, *anchor);
}

void RemoveTank(const PPCContext& ctx, uint8_t* base, const coop_leash::LeashPc* extra,
                const std::vector<coop_leash::LeashPc>* pcs, uint32_t extra_pc, const char* reason,
                bool place) {
  const auto it = State().tanks.find(extra_pc);
  if (it == State().tanks.end()) return;
  const uint32_t vehicle = it->second;
  State().tanks.erase(it);
  if (!ActorListed(base, vehicle)) return;
  if (extra != nullptr && extra->pawn == vehicle) ForceLeave(ctx, base, vehicle);
  DestroyVehicle(ctx, base, vehicle);
  REXLOG_INFO("COOPTANK {}: out of his {} ({})", coop_pull::PcName(base, extra_pc),
              std::string(coop_leash::kTankClass), reason);
  if (!place || extra == nullptr || pcs == nullptr) return;
  PlaceExtraByHero(ctx, base, *pcs, *extra);
}

void RunTankOrder(const PPCContext& ctx, uint8_t* base,
                  const std::vector<coop_leash::LeashPc>& pcs, const coop_leash::TankOrder& order) {
  const coop_leash::LeashPc* extra = FindOrderedPc(pcs, order.extra_pc);
  if (order.action == coop_leash::TankAction::kForget) {
    State().tanks.erase(order.extra_pc);
    return;
  }
  if (order.action == coop_leash::TankAction::kRemove) {
    RemoveTank(ctx, base, extra, &pcs, order.extra_pc,
               extra == nullptr ? "left the game" : "no hero drives a tank", true);
    return;
  }
  const coop_leash::LeashPc* hero = FindOrderedPc(pcs, order.hero_pc);
  if (extra == nullptr || hero == nullptr) return;
  if (order.action == coop_leash::TankAction::kSpawn) SpawnTankFor(ctx, base, *extra, *hero);
  if (order.action == coop_leash::TankAction::kRespawn) RespawnNear(ctx, base, *extra, *hero);
}

}  // namespace

bool Enabled() { return REXCVAR_GET(gow2_coop_tanks); }

bool DrivesOwnTank(uint32_t pc, uint32_t pawn) {
  const auto it = State().tanks.find(pc);
  return it != State().tanks.end() && it->second == pawn;
}

void Tick(const PPCContext& ctx, uint8_t* base, const std::vector<coop_leash::LeashPc>& pcs,
          const coop_leash::LeashConfig& config) {
  if (!Enabled()) return;
  for (const coop_leash::TankOrder& order : coop_leash::PlanTanks(pcs, config, &State())) {
    RunTankOrder(ctx, base, pcs, order);
  }
}

bool RespawnNear(const PPCContext& ctx, uint8_t* base, const coop_leash::LeashPc& extra,
                 const coop_leash::LeashPc& hero) {
  RemoveTank(ctx, base, &extra, nullptr, extra.pc, "respawn", false);
  return SpawnTankFor(ctx, base, extra, hero);
}

}  // namespace rex::glue::coop_tanks
