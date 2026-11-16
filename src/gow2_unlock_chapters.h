// gow2 - ReXGlue Recompiled Project
//
// Campaign chapters always unlocked: the chapter-select menu checks always pass.
#pragma once

#include "gow2_script_patches.h"

#include <rex/ppc.h>

#include <cstdint>

namespace rex::glue::script_patch {

PatchGroup UnlockChaptersGroup();

}  // namespace rex::glue::script_patch

namespace rex::glue {

void ApplyUnlockChaptersPatch(PPCContext& ctx, uint8_t* base);

}  // namespace rex::glue
