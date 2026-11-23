// gow2 - ReXGlue Recompiled Project
//
// Host-side Centaur handout for co-op players 3+: while a hero drives a Centaur every extra on
// foot gets his own Centaur next to the hero's, and gets out of it next to a hero when the ride
// ends. Pure planning lives in gow2_coop_leash_plan.h; this file only runs the guest calls.
#pragma once

#include <cstdint>
#include <vector>

#include <rex/ppc.h>

#include "gow2_coop_leash_plan.h"

namespace rex::glue::coop_tanks {

bool Enabled();
bool DrivesOwnTank(uint32_t pc, uint32_t pawn);
void Tick(const PPCContext& ctx, uint8_t* base, const std::vector<coop_leash::LeashPc>& pcs,
          const coop_leash::LeashConfig& config);
bool RespawnNear(const PPCContext& ctx, uint8_t* base, const coop_leash::LeashPc& extra,
                 const coop_leash::LeashPc& hero);

}  // namespace rex::glue::coop_tanks
