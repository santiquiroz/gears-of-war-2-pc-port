// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_fps_cap.h"

#include <cstdint>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc.h>

REXCVAR_DEFINE_BOOL(gow2_idle_worker_sleep, true, "GameFixes",
                    "Idle worker: the engine's queued-work thread sleeps 1 ms when its queue is "
                    "empty instead of yielding in a loop, which used a whole CPU core. Takes "
                    "effect at once.");

inline constexpr uint32_t kWorkerIdleYieldReturn = 0x82334B14;
inline constexpr int64_t kIdleSleepNanos = 1000000;

REX_EXTERN(__imp__sub_82AA85B8);
// Guest Sleep(ms) (r3 = ms; 0 = yield).
REX_HOOK_RAW(sub_82AA85B8) {
  if (static_cast<uint32_t>(ctx.lr) != kWorkerIdleYieldReturn || ctx.r3.u32 != 0 ||
      !REXCVAR_GET(gow2_idle_worker_sleep)) {
    __imp__sub_82AA85B8(ctx, base);
    return;
  }
  static bool logged = false;
  if (!logged) {
    logged = true;
    REXLOG_INFO("IDLEWORKER sleeping {} ms when the queued-work thread is idle", 1);
  }
  rex::glue::BulkSleepNanos(kIdleSleepNanos);
  ctx.r3.u64 = 0;
}
