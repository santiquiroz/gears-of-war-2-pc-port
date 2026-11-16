// gow2 - ReXGlue Recompiled Project
//
// Stage 2 group A (survival) of docs/COOP_STAGE2_SURVIVAL.md; the rows are in
// gow2_script_patch_survival.cpp, printed by tools/script_patch_survival.py.
#pragma once

#include "gow2_script_patches.h"

namespace rex::glue::script_patch {

// Rows S2A-1, S2A-2, S2A-3 and S2A-5, applied after the Stage 1 group. With
// extra_players_end_game S2A-2 is left out, so the retail game-over rule
// applies to every player (gow2_coop_extra_players_game_over).
PatchGroup CoopSurvivalGroup(bool extra_players_end_game);

// Row S2A-4 alone, off by default (gow2_coop_split_revive).
PatchGroup CoopSplitReviveGroup();

}  // namespace rex::glue::script_patch
