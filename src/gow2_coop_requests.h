// gow2 - ReXGlue Recompiled Project
//
// Co-op requests (docs/COOP_N_PLAYERS.md, "Requests and friendly fire"): guests send their
// friendly-fire vote and "Go to player 1" with `mutate gow2 ...`; the campaign host reads them,
// turns friendly fire on only while every human opts in, and moves a stuck guest next to player 1.
#pragma once

namespace rex::glue::coop_requests {

// PC Options action: local players other than player 1 ask to be moved next to him.
void RequestGoToPlayer1();

}  // namespace rex::glue::coop_requests
