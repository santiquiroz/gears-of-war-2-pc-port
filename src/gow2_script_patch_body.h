// gow2 - ReXGlue Recompiled Project
//
// The Stage 2 follow-up groups of docs/COOP_N_PLAYERS.md for players 3+ (items 8, 9 and 10). They
// apply after the "coop" group, each one fail-closed on its own. Rows in
// gow2_script_patch_body_table.cpp, printed by tools/script_patch_body.py.
#pragma once

#include "gow2_script_patches.h"

namespace rex::glue::script_patch {

// rows CB-1, CB-2: players 3+ look like Dom; run on every peer, clients spawn
// those pawns from this template
PatchGroup CoopClientGroup();

// "coop-body": the class (CB-3, or CB-3c with carmine), the loadout (CB-4)
// and the spawn without Right Trigger (CB-5) of players 3+. marcus_clones drops CB-3 and CB-3c.
PatchGroup CoopBodyGroup(bool carmine, bool marcus_clones);

// "coop-ride": players 3+ on foot cannot be hurt while a hero rides a vehicle (CR-1) and join him
// when he gets out (CR-2).
PatchGroup CoopRideGroup();

}  // namespace rex::glue::script_patch
