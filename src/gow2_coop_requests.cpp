// gow2 - ReXGlue Recompiled Project
//
// Engine-tick side of the co-op requests: see gow2_coop_requests.h and gow2_coop_requests_plan.h.
#include "gow2_coop_requests.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <rex/cvar.h>
#include <rex/logging.h>

#include "gow2_coop_leash_plan.h"
#include "gow2_coop_pull.h"
#include "gow2_coop_requests_plan.h"
#include "gow2_dev_commands.h"
#include "gow2_dev_guest_calls.h"
#include "gow2_dev_host_verbs.h"
#include "gow2_engine_tick.h"
#include "gow2_guest.h"
#include "gow2_script_patch_requests.h"

REXCVAR_DEFINE_BOOL(gow2_friendly_fire, false, "Coop",
                    "Campaign co-op: players can hurt each other only while every player has this "
                    "on. AI squadmates never take or deal friendly damage, and melee never targets "
                    "a teammate.");

namespace rex::glue::coop_requests {
namespace {

using Clock = std::chrono::steady_clock;
namespace layout = dev_commands::layout;

// Guest layout, measured with tools/ue3/layout.py on the retail packages.
constexpr uint32_t kWorldInfoGame = 836;
constexpr uint32_t kPcLastBroadcastString = 1308;
constexpr uint32_t kGameFriendlyFireWord = 972;
constexpr uint32_t kFriendlyFireMask = 0x80000000;
constexpr uint32_t kMaxRequestChars = 32;
constexpr auto kTickPeriod = std::chrono::milliseconds(250);
// A vote sent right after a teleport request would overwrite it before the host reads it.
constexpr auto kHoldVoteAfterTeleport = std::chrono::seconds(1);

std::atomic<bool> g_go_to_player1{false};

struct HumanPc {
  uint32_t pc = 0;
  bool is_local = false;
};

struct RemoteVote {
  bool on = false;
  Clock::time_point when{};
};

struct HostState {
  std::map<uint32_t, RemoteVote> votes;
  std::map<uint32_t, Clock::time_point> last_teleport;
  uint32_t ff_game = 0;
  std::optional<FriendlyFireDecision> logged;
};

struct SentVote {
  bool on = false;
  Clock::time_point when{};
};

struct ClientState {
  std::map<uint32_t, SentVote> sent;
  Clock::time_point hold_votes_until{};
};

double SecondsSince(Clock::time_point then, Clock::time_point now) {
  return std::chrono::duration<double>(now - then).count();
}

bool TakeTickTurn(Clock::time_point now) {
  static Clock::time_point last{};
  if (now - last < kTickPeriod) return false;
  last = now;
  return true;
}

bool IsLocalPc(const uint8_t* base, uint32_t pc) {
  const uint32_t player = guest::LoadU32(base, pc + layout::kPcPlayer);
  if (player == 0) return false;
  return dev_commands::ClassChainHas(base, dev_commands::kGameGlobals, player, "LocalPlayer");
}

std::vector<HumanPc> ReadHumans(const uint8_t* base, uint32_t world_info) {
  std::vector<HumanPc> out;
  for (uint32_t pc :
       dev_commands::PlayerControllers(base, dev_commands::kGameGlobals, world_info)) {
    if (!dev_commands::ClassChainHas(base, dev_commands::kGameGlobals, pc, "GearPC")) continue;
    out.push_back({pc, IsLocalPc(base, pc)});
  }
  return out;
}

std::optional<std::vector<HumanPc>> GuardedHumans(const uint8_t* base, uint32_t world_info) {
  std::optional<std::vector<HumanPc>> out;
  auto read = [&] { out = ReadHumans(base, world_info); };
  if (!guest_call::Guarded(read)) return std::nullopt;
  return out;
}

bool IsCampaignGame(const uint8_t* base, uint32_t world_info) {
  const uint32_t game = guest::LoadU32(base, world_info + kWorldInfoGame);
  if (game == 0) return false;
  return dev_commands::ClassChainHas(base, dev_commands::kGameGlobals, game, "GearGameSP_Base");
}

bool GuardedIsCampaignGame(const uint8_t* base, uint32_t world_info) {
  bool campaign = false;
  auto read = [&] { campaign = IsCampaignGame(base, world_info); };
  if (!guest_call::Guarded(read)) return false;
  return campaign;
}

// Reads LastBroadcastString[0] and empties it only when it holds a request, so chat keeps working.
std::optional<Request> TakeRequest(uint8_t* base, uint32_t pc) {
  std::optional<Request> request;
  auto take = [&] {
    const uint32_t data = guest::LoadU32(base, pc + kPcLastBroadcastString);
    const int32_t num = int32_t(guest::LoadU32(base, pc + kPcLastBroadcastString + 4));
    if (num <= 0) return;
    request = ParseCoopRequest(dev_commands::DecodeTchars(base, data, num, kMaxRequestChars));
    if (request.has_value()) guest::StoreU32(base, pc + kPcLastBroadcastString + 4, 0);
  };
  if (!guest_call::Guarded(take)) return std::nullopt;
  return request;
}

// ---- Go to player 1 (host) ----

struct TeleportScene {
  std::vector<coop_leash::LeashPc> pcs;
  std::optional<size_t> player1;
};

TeleportScene ReadScene(const uint8_t* base, const std::vector<HumanPc>& humans) {
  TeleportScene scene;
  std::vector<PlayerSlot> slots;
  for (const HumanPc& human : humans) {
    scene.pcs.push_back(coop_pull::ReadLeashPc(base, human.pc));
    slots.push_back({human.is_local, scene.pcs.back().dpc});
  }
  scene.player1 = PickPlayer1(slots);
  return scene;
}

std::optional<TeleportScene> GuardedScene(const uint8_t* base, const std::vector<HumanPc>& humans) {
  std::optional<TeleportScene> out;
  auto read = [&] { out = ReadScene(base, humans); };
  if (!guest_call::Guarded(read)) return std::nullopt;
  return out;
}

TeleportPc ToTeleportPc(const coop_leash::LeashPc& pc) {
  return {pc.pawn != 0 && pc.health > 0, pc.riding, pc.cinematic};
}

const coop_leash::LeashPc* FindPc(const TeleportScene& scene, uint32_t pc) {
  for (const coop_leash::LeashPc& entry : scene.pcs) {
    if (entry.pc == pc) return &entry;
  }
  return nullptr;
}

std::optional<double> SecondsSinceTeleport(const HostState& state, uint32_t pc,
                                           Clock::time_point now) {
  const auto it = state.last_teleport.find(pc);
  if (it == state.last_teleport.end()) return std::nullopt;
  return SecondsSince(it->second, now);
}

TeleportCheck BuildCheck(const TeleportScene& scene, const coop_leash::LeashPc& requester,
                         bool campaign_host, std::optional<double> seconds_since_last) {
  TeleportCheck check;
  check.campaign_host = campaign_host;
  check.player1_found = scene.player1.has_value();
  check.requester = ToTeleportPc(requester);
  check.seconds_since_last = seconds_since_last;
  if (!check.player1_found) return check;
  const coop_leash::LeashPc& player1 = scene.pcs[*scene.player1];
  check.requester_is_player1 = player1.pc == requester.pc;
  check.player1 = ToTeleportPc(player1);
  return check;
}

bool TryFallbackPlace(const PPCContext& ctx, uint8_t* base, const coop_leash::LeashPc& mover,
                      const coop_leash::LeashPc& player1) {
  const uint32_t place_fn = coop_pull::FindSetLocation(ctx, base, mover.pawn);
  if (place_fn == 0) return false;
  for (const auto [dx, dy] : coop_leash::FallbackOffsets()) {
    if (coop_pull::PlaceAt(ctx, base, place_fn, mover.pc, mover.pawn, player1.x + dx,
                           player1.y + dy, player1.z + coop_leash::kFallbackRaiseZ)) {
      return true;
    }
  }
  return false;
}

void MoveToPlayer1(const PPCContext& ctx, uint8_t* base, const coop_leash::LeashPc& mover,
                   const coop_leash::LeashPc& player1) {
  const std::string name = coop_pull::PcName(base, mover.pc);
  if (coop_pull::RequestTeleportSpot(ctx, base, player1.pc, mover.pawn) &&
      coop_pull::IsNear(base, mover.pawn, player1.x, player1.y, player1.z,
                        coop_leash::kPulledCloseEnough)) {
    REXLOG_INFO("COOPTP {} -> player 1 (spot)", name);
    return;
  }
  if (TryFallbackPlace(ctx, base, mover, player1)) {
    REXLOG_INFO("COOPTP {} -> player 1 (fallback)", name);
    return;
  }
  REXLOG_WARN("COOPTP {}: no free spot next to player 1 ({})", name,
              coop_pull::PcName(base, player1.pc));
}

void HandleTeleport(const PPCContext& ctx, uint8_t* base, const std::vector<HumanPc>& humans,
                    uint32_t requester_pc, bool campaign_host, HostState* state) {
  const std::optional<TeleportScene> scene = GuardedScene(base, humans);
  if (!scene.has_value()) return;
  const coop_leash::LeashPc* requester = FindPc(*scene, requester_pc);
  if (requester == nullptr) return;
  const Clock::time_point now = Clock::now();
  const TeleportCheck check =
      BuildCheck(*scene, *requester, campaign_host, SecondsSinceTeleport(*state, requester_pc, now));
  const TeleportVerdict verdict = CheckTeleport(check);
  if (verdict != TeleportVerdict::kOk) {
    REXLOG_INFO("COOPTP rejected {}: {}", coop_pull::PcName(base, requester_pc),
                TeleportVerdictText(verdict));
    return;
  }
  state->last_teleport[requester_pc] = now;
  MoveToPlayer1(ctx, base, *requester, scene->pcs[*scene->player1]);
}

void HandleLocalTeleports(const PPCContext& ctx, uint8_t* base, const std::vector<HumanPc>& humans,
                          bool campaign_host, HostState* state) {
  if (!g_go_to_player1.exchange(false)) return;
  for (const HumanPc& human : humans) {
    if (human.is_local) HandleTeleport(ctx, base, humans, human.pc, campaign_host, state);
  }
}

// ---- Friendly fire (host) ----

std::vector<FriendlyFireVote> CollectVotes(const std::vector<HumanPc>& humans,
                                           const HostState& state, Clock::time_point now) {
  const bool local_on = REXCVAR_GET(gow2_friendly_fire);
  std::vector<FriendlyFireVote> votes;
  for (const HumanPc& human : humans) {
    if (human.is_local) {
      votes.push_back({true, local_on, 0});
      continue;
    }
    const auto it = state.votes.find(human.pc);
    if (it == state.votes.end()) {
      votes.push_back({false, false, 0});
      continue;
    }
    votes.push_back({false, it->second.on, SecondsSince(it->second.when, now)});
  }
  return votes;
}

void PruneMissing(const std::vector<HumanPc>& humans, HostState* state) {
  std::erase_if(state->votes, [&](const auto& entry) {
    for (const HumanPc& human : humans) {
      if (human.pc == entry.first) return false;
    }
    return true;
  });
}

// Only the bit this code set is ever cleared, so a game that starts with it on stays retail.
void WriteFriendlyFire(uint8_t* base, uint32_t world_info, bool active, HostState* state) {
  auto write = [&] {
    const uint32_t game = guest::LoadU32(base, world_info + kWorldInfoGame);
    if (game == 0) return;
    if (state->ff_game != 0 && state->ff_game != game) state->ff_game = 0;
    const uint32_t addr = game + kGameFriendlyFireWord;
    const uint32_t word = guest::LoadU32(base, addr);
    const bool on = (word & kFriendlyFireMask) != 0;
    if (active && !on) {
      guest::StoreU32(base, addr, word | kFriendlyFireMask);
      state->ff_game = game;
    } else if (!active && on && state->ff_game == game) {
      guest::StoreU32(base, addr, word & ~kFriendlyFireMask);
      state->ff_game = 0;
    }
  };
  guest_call::Guarded(write);
}

bool SameDecision(const FriendlyFireDecision& a, const FriendlyFireDecision& b) {
  return a.active == b.active && a.opted_in == b.opted_in && a.humans == b.humans;
}

void LogDecision(const FriendlyFireDecision& decision, HostState* state) {
  if (state->logged.has_value() && SameDecision(*state->logged, decision)) return;
  state->logged = decision;
  if (decision.humans < 2) return;
  REXLOG_INFO("COOPFF {} ({}/{} opted in)", decision.active ? "active" : "inactive",
              decision.opted_in, decision.humans);
}

void UpdateFriendlyFire(uint8_t* base, uint32_t world_info, const std::vector<HumanPc>& humans,
                        bool campaign_host, HostState* state) {
  if (!script_patch::CoopFriendlyFireApplied()) return;
  PruneMissing(humans, state);
  const FriendlyFireDecision decision =
      DecideFriendlyFire(CollectVotes(humans, *state, Clock::now()), campaign_host);
  WriteFriendlyFire(base, world_info, decision.active, state);
  LogDecision(decision, state);
}

// ---- Host and client ticks ----

void ReadRemoteRequests(const PPCContext& ctx, uint8_t* base, const std::vector<HumanPc>& humans,
                        bool campaign_host, HostState* state) {
  for (const HumanPc& human : humans) {
    const std::optional<Request> request = TakeRequest(base, human.pc);
    if (!request.has_value()) continue;
    if (*request == Request::kGoToPlayer1) {
      HandleTeleport(ctx, base, humans, human.pc, campaign_host, state);
      continue;
    }
    state->votes[human.pc] = {*request == Request::kFriendlyFireOn, Clock::now()};
  }
}

void HostTick(const PPCContext& ctx, uint8_t* base, uint32_t world_info) {
  static HostState state;
  const std::optional<std::vector<HumanPc>> humans = GuardedHumans(base, world_info);
  if (!humans.has_value()) return;
  const bool campaign_host = GuardedIsCampaignGame(base, world_info);
  if (script_patch::CoopRequestsApplied()) {
    ReadRemoteRequests(ctx, base, *humans, campaign_host, &state);
  }
  HandleLocalTeleports(ctx, base, *humans, campaign_host, &state);
  UpdateFriendlyFire(base, world_info, *humans, campaign_host, &state);
}

void Send(const PPCContext& ctx, uint8_t* base, uint32_t pc, Request request) {
  guest_call::ConsoleCommand(ctx, base, pc, "mutate " + RequestText(request));
}

void SendTeleports(const PPCContext& ctx, uint8_t* base, const std::vector<HumanPc>& humans,
                   ClientState* state) {
  if (!g_go_to_player1.exchange(false)) return;
  for (const HumanPc& human : humans) Send(ctx, base, human.pc, Request::kGoToPlayer1);
  state->hold_votes_until = Clock::now() + kHoldVoteAfterTeleport;
  REXLOG_INFO("COOPTP asked the host to move {} local player(s) next to player 1", humans.size());
}

std::optional<bool> LastSent(const ClientState& state, uint32_t pc) {
  const auto it = state.sent.find(pc);
  if (it == state.sent.end()) return std::nullopt;
  return it->second.on;
}

double SecondsSinceSent(const ClientState& state, uint32_t pc, Clock::time_point now) {
  const auto it = state.sent.find(pc);
  if (it == state.sent.end()) return 0;
  return SecondsSince(it->second.when, now);
}

void SendVotes(const PPCContext& ctx, uint8_t* base, const std::vector<HumanPc>& humans,
               ClientState* state) {
  const Clock::time_point now = Clock::now();
  if (now < state->hold_votes_until) return;
  const bool on = REXCVAR_GET(gow2_friendly_fire);
  for (const HumanPc& human : humans) {
    if (!ShouldSendVote(on, LastSent(*state, human.pc), SecondsSinceSent(*state, human.pc, now))) {
      continue;
    }
    Send(ctx, base, human.pc, FriendlyFireRequest(on));
    state->sent[human.pc] = {on, now};
  }
}

void ClientTick(const PPCContext& ctx, uint8_t* base, uint32_t world_info) {
  static ClientState state;
  const std::optional<std::vector<HumanPc>> humans = GuardedHumans(base, world_info);
  if (!humans.has_value() || humans->empty()) return;
  SendTeleports(ctx, base, *humans, &state);
  SendVotes(ctx, base, *humans, &state);
}

bool IsClient(const uint8_t* base, uint32_t world_info) {
  return guest::LoadU8(base, world_info + layout::kWorldInfoNetMode) == layout::kNetModeClient;
}

void CoopRequestsTick(const PPCContext& ctx, uint8_t* base) {
  if (!script_patch::CoopRequestsApplied() && !script_patch::CoopFriendlyFireApplied()) {
    g_go_to_player1.store(false);
    return;
  }
  if (!TakeTickTurn(Clock::now())) return;
  const uint32_t world_info = dev_verbs::GuardedWorldInfo(base);
  if (world_info == 0) return;
  if (IsClient(base, world_info)) {
    if (script_patch::CoopRequestsApplied()) ClientTick(ctx, base, world_info);
    return;
  }
  HostTick(ctx, base, world_info);
}

[[maybe_unused]] const bool kRequestsRegistered =
    (rex::glue::AddEngineTickListener(&CoopRequestsTick), true);

}  // namespace

void RequestGoToPlayer1() { g_go_to_player1.store(true); }

}  // namespace rex::glue::coop_requests
