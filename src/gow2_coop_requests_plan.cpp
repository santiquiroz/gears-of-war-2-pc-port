// gow2 - ReXGlue Recompiled Project
//
// Pure rules for the co-op requests: see gow2_coop_requests_plan.h.
#include "gow2_coop_requests_plan.h"

#include <array>
#include <utility>

namespace rex::glue::coop_requests {
namespace {

constexpr std::array<std::pair<std::string_view, Request>, 3> kRequests = {{
    {"gow2 ff 0", Request::kFriendlyFireOff},
    {"gow2 ff 1", Request::kFriendlyFireOn},
    {"gow2 tp", Request::kGoToPlayer1},
}};

bool IsTooSoon(const std::optional<double>& seconds_since_last) {
  return seconds_since_last.has_value() && *seconds_since_last < kTeleportCooldownSeconds;
}

}  // namespace

std::optional<Request> ParseCoopRequest(std::string_view text) {
  for (const auto& [line, request] : kRequests) {
    if (text == line) return request;
  }
  return std::nullopt;
}

std::string RequestText(Request request) {
  for (const auto& [line, known] : kRequests) {
    if (known == request) return std::string(line);
  }
  return {};
}

Request FriendlyFireRequest(bool on) {
  return on ? Request::kFriendlyFireOn : Request::kFriendlyFireOff;
}

bool VoteCountsAsOn(const FriendlyFireVote& vote) {
  return vote.on && (vote.is_local || vote.age_seconds <= kVoteStaleSeconds);
}

FriendlyFireDecision DecideFriendlyFire(const std::vector<FriendlyFireVote>& votes,
                                        bool campaign_host) {
  FriendlyFireDecision decision;
  decision.humans = int(votes.size());
  for (const FriendlyFireVote& vote : votes) {
    if (VoteCountsAsOn(vote)) ++decision.opted_in;
  }
  decision.active = campaign_host && decision.humans >= 2 && decision.opted_in == decision.humans;
  return decision;
}

bool ShouldSendVote(bool on, std::optional<bool> last_sent, double seconds_since_sent) {
  if (!last_sent.has_value() || *last_sent != on) return true;
  return seconds_since_sent >= kVoteResendSeconds;
}

TeleportVerdict CheckTeleport(const TeleportCheck& check) {
  const std::array<std::pair<bool, TeleportVerdict>, 9> rules = {{
      {!check.campaign_host, TeleportVerdict::kNotCampaignHost},
      {!check.player1_found, TeleportVerdict::kNoPlayer1},
      {check.requester_is_player1, TeleportVerdict::kIsPlayer1},
      {!check.requester.pawn_alive, TeleportVerdict::kNoPawn},
      {!check.player1.pawn_alive, TeleportVerdict::kPlayer1NoPawn},
      {check.requester.riding, TeleportVerdict::kRiding},
      {check.player1.riding, TeleportVerdict::kPlayer1Riding},
      {check.requester.cinematic || check.player1.cinematic, TeleportVerdict::kCinematic},
      {IsTooSoon(check.seconds_since_last), TeleportVerdict::kTooSoon},
  }};
  for (const auto& [failed, verdict] : rules) {
    if (failed) return verdict;
  }
  return TeleportVerdict::kOk;
}

std::string_view TeleportVerdictText(TeleportVerdict verdict) {
  switch (verdict) {
    case TeleportVerdict::kOk: return "ok";
    case TeleportVerdict::kNotCampaignHost: return "not a campaign host";
    case TeleportVerdict::kNoPlayer1: return "no player 1";
    case TeleportVerdict::kIsPlayer1: return "is player 1";
    case TeleportVerdict::kNoPawn: return "no live pawn";
    case TeleportVerdict::kPlayer1NoPawn: return "player 1 has no live pawn";
    case TeleportVerdict::kRiding: return "rides a vehicle or turret";
    case TeleportVerdict::kPlayer1Riding: return "player 1 rides a vehicle or turret";
    case TeleportVerdict::kCinematic: return "cinematic";
    case TeleportVerdict::kTooSoon: return "less than 10 s since the last one";
  }
  return "?";
}

std::optional<size_t> PickPlayer1(const std::vector<PlayerSlot>& players) {
  for (size_t i = 0; i < players.size(); ++i) {
    if (players[i].default_pawn_class == kPlayer1PawnClass) return i;
  }
  for (size_t i = 0; i < players.size(); ++i) {
    if (players[i].is_local) return i;
  }
  return std::nullopt;
}

}  // namespace rex::glue::coop_requests
