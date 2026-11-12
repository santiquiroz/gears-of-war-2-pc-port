// gow2 - ReXGlue Recompiled Project
//
// Host-side guest reads and moves shared by the co-op leash and "Go to player 1".
#include "gow2_coop_pull.h"

#include <cmath>
#include <cstring>
#include <optional>
#include <vector>

#include "gow2_dev_commands.h"
#include "gow2_dev_guest_calls.h"
#include "gow2_guest.h"

namespace rex::glue::coop_pull {
namespace {

// Guest layout, measured in the running game.
constexpr uint32_t kGearPcDefaultPawnClass = 1784;
constexpr uint32_t kPcCinematicFlags = 864;
constexpr uint32_t kCinematicModeMask = 0x00002000;
constexpr uint32_t kControllerPawn = 444;
constexpr uint32_t kPawnHealth = 692;
constexpr uint32_t kActorHiddenFlags = 172;
constexpr uint32_t kHiddenMask = 0x40000000;
constexpr uint32_t kActorRotation = 96;
constexpr uint8_t kPhysicsFalling = 2;
constexpr uint32_t kSetLocationOutBytes = 16;
constexpr uint32_t kSetLocationReturnOffset = 12;

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

uint32_t DecodeU32Be(const std::vector<uint8_t>& bytes, size_t at) {
  return (uint32_t(bytes[at]) << 24) | (uint32_t(bytes[at + 1]) << 16) |
         (uint32_t(bytes[at + 2]) << 8) | bytes[at + 3];
}

std::vector<uint8_t> EncodeU32Be(uint32_t value) {
  std::vector<uint8_t> out;
  AppendU32Be(value, &out);
  return out;
}

std::vector<uint8_t> EncodeVectorBe(float x, float y, float z) {
  std::vector<uint8_t> out;
  AppendU32Be(FloatBits(x), &out);
  AppendU32Be(FloatBits(y), &out);
  AppendU32Be(FloatBits(z), &out);
  return out;
}

std::string DefaultPawnClassName(const uint8_t* base, uint32_t pc) {
  const uint32_t cls = guest::LoadU32(base, pc + kGearPcDefaultPawnClass);
  if (cls == 0) return "None";
  return dev_commands::ObjectName(base, dev_commands::kGameGlobals, cls);
}

bool ReadGodFlag(const uint8_t* base, uint32_t pc) {
  const uint32_t flags = guest::LoadU32(base, pc + dev_commands::layout::kControllerFlags);
  return (flags & dev_commands::layout::kGodModeMask) != 0;
}

bool ReadRiding(const uint8_t* base, uint32_t pawn) {
  if (pawn == 0) return false;
  if (dev_commands::ClassChainHas(base, dev_commands::kGameGlobals, pawn, "GearVehicle")) {
    return true;
  }
  return dev_commands::ClassChainHas(base, dev_commands::kGameGlobals, pawn, "GearWeaponPawn");
}

void FillPawnFields(const uint8_t* base, uint32_t pawn, coop_leash::LeashPc* out) {
  out->pawn_class = dev_commands::ClassName(base, dev_commands::kGameGlobals, pawn);
  out->health = int32_t(guest::LoadU32(base, pawn + kPawnHealth));
  out->hidden = (guest::LoadU32(base, pawn + kActorHiddenFlags) & kHiddenMask) != 0;
  out->x = LoadFloat(base, pawn + kActorLocation);
  out->y = LoadFloat(base, pawn + kActorLocation + 4);
  out->z = LoadFloat(base, pawn + kActorLocation + 8);
}

bool PlacePawn(const PPCContext& ctx, uint8_t* base, uint32_t pawn, uint32_t fn, float x, float y,
               float z) {
  std::vector<uint8_t> out;
  if (!guest_call::ProcessEventWithParms(ctx, base, pawn, fn, EncodeVectorBe(x, y, z), &out,
                                         kSetLocationOutBytes)) {
    return false;
  }
  if (out.size() < kSetLocationOutBytes) return false;
  return DecodeU32Be(out, kSetLocationReturnOffset) != 0;
}

void SetFalling(const PPCContext& ctx, uint8_t* base, uint32_t pawn) {
  static std::optional<uint64_t> name = guest_call::MakeName(ctx, base, "SetPhysics", false);
  if (!name.has_value()) return;
  const uint32_t fn = guest_call::FindFunction(ctx, base, pawn, *name);
  if (fn == 0) return;
  guest_call::ProcessEventWithParms(ctx, base, pawn, fn, {kPhysicsFalling, 0, 0, 0}, nullptr, 0);
}

void SendClientLocation(const PPCContext& ctx, uint8_t* base, uint32_t mover_pc,
                        uint32_t mover_pawn, float x, float y, float z) {
  static std::optional<uint64_t> name =
      guest_call::MakeName(ctx, base, "ClientSetLocation", false);
  if (!name.has_value()) return;
  uint32_t rx = 0, ry = 0, rz = 0;
  auto read = [&] {
    rx = guest::LoadU32(base, mover_pawn + kActorRotation);
    ry = guest::LoadU32(base, mover_pawn + kActorRotation + 4);
    rz = guest::LoadU32(base, mover_pawn + kActorRotation + 8);
  };
  if (!guest_call::Guarded(read)) return;
  const uint32_t fn = guest_call::FindFunction(ctx, base, mover_pc, *name);
  if (fn == 0) return;
  std::vector<uint8_t> parms = EncodeVectorBe(x, y, z);
  AppendU32Be(rx, &parms);
  AppendU32Be(ry, &parms);
  AppendU32Be(rz, &parms);
  guest_call::ProcessEventWithParms(ctx, base, mover_pc, fn, parms, nullptr, 0);
}

}  // namespace

float LoadFloat(const uint8_t* base, uint32_t addr) {
  const uint32_t bits = guest::LoadU32(base, addr);
  float out = 0;
  std::memcpy(&out, &bits, sizeof(out));
  return out;
}

coop_leash::LeashPc ReadLeashPc(const uint8_t* base, uint32_t pc) {
  coop_leash::LeashPc out;
  out.pc = pc;
  out.dpc = DefaultPawnClassName(base, pc);
  out.cinematic = (guest::LoadU32(base, pc + kPcCinematicFlags) & kCinematicModeMask) != 0;
  out.god = ReadGodFlag(base, pc);
  out.pawn = guest::LoadU32(base, pc + kControllerPawn);
  out.riding = ReadRiding(base, out.pawn);
  if (out.pawn != 0) FillPawnFields(base, out.pawn, &out);
  return out;
}

std::string PcName(const uint8_t* base, uint32_t pc) {
  std::string out = "?";
  auto read = [&] { out = dev_commands::ObjectName(base, dev_commands::kGameGlobals, pc); };
  if (!guest_call::Guarded(read)) return "?";
  return out;
}

bool RequestTeleportSpot(const PPCContext& ctx, uint8_t* base, uint32_t anchor_pc,
                         uint32_t mover_pawn) {
  static std::optional<uint64_t> name =
      guest_call::MakeName(ctx, base, "FindAvailableTeleportSpot", false);
  if (!name.has_value()) return false;
  const uint32_t fn = guest_call::FindFunction(ctx, base, anchor_pc, *name);
  // TestPawn @0, TeleportAnchor @4, bSkipRadiusSearch @8, return @12; zero is None/false.
  if (fn != 0) {
    guest_call::ProcessEventWithParms(ctx, base, anchor_pc, fn, EncodeU32Be(mover_pawn), nullptr,
                                      0);
  }
  return true;
}

bool IsNear(const uint8_t* base, uint32_t pawn, float x, float y, float z, float max_distance) {
  bool close = false;
  auto read = [&] {
    const double dx = double(LoadFloat(base, pawn + kActorLocation) - x);
    const double dy = double(LoadFloat(base, pawn + kActorLocation + 4) - y);
    const double dz = double(LoadFloat(base, pawn + kActorLocation + 8) - z);
    close = std::sqrt(dx * dx + dy * dy + dz * dz) <= max_distance;
  };
  if (!guest_call::Guarded(read)) return false;
  return close;
}

uint32_t FindSetLocation(const PPCContext& ctx, uint8_t* base, uint32_t pawn) {
  static std::optional<uint64_t> name = guest_call::MakeName(ctx, base, "SetLocation", false);
  if (!name.has_value()) return 0;
  return guest_call::FindFunction(ctx, base, pawn, *name);
}

bool PlaceAt(const PPCContext& ctx, uint8_t* base, uint32_t place_fn, uint32_t mover_pc,
             uint32_t mover_pawn, float x, float y, float z) {
  if (!PlacePawn(ctx, base, mover_pawn, place_fn, x, y, z)) return false;
  SetFalling(ctx, base, mover_pawn);
  SendClientLocation(ctx, base, mover_pc, mover_pawn, x, y, z);
  return true;
}

}  // namespace rex::glue::coop_pull
