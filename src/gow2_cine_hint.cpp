// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Reads the current cinematic and network role from the game on engine ticks.
#include "gow2_cine_hint.h"

#include <atomic>
#include <chrono>

#include <rex/cvar.h>
#include <rex/ppc.h>

#include "gow2_dev_commands.h"
#include "gow2_dev_guest_calls.h"
#include "gow2_engine_tick.h"
#include "gow2_guest.h"

REXCVAR_DEFINE_BOOL(gow2_skip_hint, true, "Display",
                    "While a cinematic plays, show how to skip it (Back on a controller, the keyboard key of Back, Tab by default). Only the host can skip; clients see a note when they press a button.");

namespace rex::glue {
namespace {

constexpr uint32_t kPcCinematicFlags = 864;
constexpr uint32_t kCinematicModeMask = 0x00002000;
constexpr auto kReadPeriod = std::chrono::milliseconds(100);
std::atomic<bool> g_cinematic{false};
std::atomic<bool> g_host{false};

void Publish(CineNow state) {
  g_cinematic.store(state.cinematic, std::memory_order_release);
  g_host.store(state.host, std::memory_order_release);
}

void ReadCinematic(const PPCContext& ctx, uint8_t* base) {
  const uint32_t engine = ctx.r3.u32;
  const uint32_t pc = dev_commands::LocalPlayerController(base, dev_commands::kGameGlobals, engine);
  if (pc == 0) {
    Publish({false, false});
    return;
  }
  const bool cinematic = (guest::LoadU32(base, pc + kPcCinematicFlags) & kCinematicModeMask) != 0;
  const uint32_t world_info = dev_commands::WorldInfoOf(base, dev_commands::kGameGlobals);
  const bool host = world_info != 0 &&
                    guest::LoadU8(base, world_info + dev_commands::layout::kWorldInfoNetMode) !=
                        dev_commands::layout::kNetModeClient;
  Publish({cinematic, host});
}

void CineHintTick(const PPCContext& ctx, uint8_t* base) {
  static auto last_read = std::chrono::steady_clock::time_point{};
  const auto now = std::chrono::steady_clock::now();
  if (now - last_read < kReadPeriod) return;
  last_read = now;
  auto read = [&] { ReadCinematic(ctx, base); };
  if (!guest_call::Guarded(read)) Publish({false, false});
}

#ifdef _WIN32
// Windows only, like gow2_coop_leash: elsewhere a bad guest read would crash instead of being skipped.
[[maybe_unused]] const bool kCineHintRegistered = (AddEngineTickListener(&CineHintTick), true);
#endif

}  // namespace

CineNow CurrentCinematic() {
  return {g_cinematic.load(std::memory_order_acquire), g_host.load(std::memory_order_acquire)};
}

}  // namespace rex::glue
