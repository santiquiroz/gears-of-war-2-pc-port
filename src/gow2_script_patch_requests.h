// gow2 - ReXGlue Recompiled Project
//
// Co-op groups that apply with any player count (docs/COOP_N_PLAYERS.md, "Requests and friendly
// fire"), each one fail-closed on its own. Rows in gow2_script_patch_requests_table.cpp, printed by
// tools/script_patch_requests.py.
#pragma once

#include "gow2_script_patches.h"

namespace rex::glue::script_patch {

// "coop-requests": ServerMutate stores its string in LastBroadcastString[0] on the server (RQ-1),
// the channel `mutate gow2 ...` uses to reach the host's port code.
PatchGroup CoopRequestsGroup();

// "coop-ff": with GearGame.bAllowFriendlyFire on outside versus, ReduceDamage lets friendly damage
// through only from a human player to a human-controlled pawn (FF-1).
PatchGroup CoopFriendlyFireGroup();

// Set once by the startup hook (gow2_script_patch_hook.cpp).
bool CoopRequestsApplied();
bool CoopFriendlyFireApplied();

}  // namespace rex::glue::script_patch
