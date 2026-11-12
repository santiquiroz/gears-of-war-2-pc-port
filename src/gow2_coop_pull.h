// gow2 - ReXGlue Recompiled Project
//
// Host-side guest reads and moves shared by the co-op leash and "Go to player 1": a player's
// snapshot, the spot pull (GearPC.FindAvailableTeleportSpot) and placing a pawn at a point.
#pragma once

#include <cstdint>
#include <string>

#include <rex/ppc.h>

#include "gow2_coop_leash_plan.h"

namespace rex::glue::coop_pull {

// Guest layout, measured in the running game.
constexpr uint32_t kActorLocation = 84;

float LoadFloat(const uint8_t* base, uint32_t addr);
coop_leash::LeashPc ReadLeashPc(const uint8_t* base, uint32_t pc);
std::string PcName(const uint8_t* base, uint32_t pc);

// False only when the function name is unknown; the spot search itself reports nothing.
bool RequestTeleportSpot(const PPCContext& ctx, uint8_t* base, uint32_t anchor_pc,
                         uint32_t mover_pawn);
bool IsNear(const uint8_t* base, uint32_t pawn, float x, float y, float z, float max_distance);

uint32_t FindSetLocation(const PPCContext& ctx, uint8_t* base, uint32_t pawn);
// SetLocation, falling physics, and ClientSetLocation so a remote owner sees the move.
bool PlaceAt(const PPCContext& ctx, uint8_t* base, uint32_t place_fn, uint32_t mover_pc,
             uint32_t mover_pawn, float x, float y, float z);

}  // namespace rex::glue::coop_pull
