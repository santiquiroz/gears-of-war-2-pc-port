// gow2 - ReXGlue Recompiled Project
//
// Pure rules for the co-op requests players send with `mutate gow2 ...`: the request text, the
// friendly-fire vote and the "Go to player 1" checks.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rex::glue::coop_requests {

constexpr double kVoteStaleSeconds = 15.0;
constexpr double kVoteResendSeconds = 5.0;
constexpr double kTeleportCooldownSeconds = 10.0;
constexpr std::string_view kPlayer1PawnClass = "GearPawn_COGMarcus";

enum class Request { kFriendlyFireOff, kFriendlyFireOn, kGoToPlayer1 };

std::optional<Request> ParseCoopRequest(std::string_view text);
std::string RequestText(Request request);
Request FriendlyFireRequest(bool on);

struct FriendlyFireVote {
  bool is_local = false;
  bool on = false;
  double age_seconds = 0;
};

struct FriendlyFireDecision {
  bool active = false;
  int opted_in = 0;
  int humans = 0;
};

bool VoteCountsAsOn(const FriendlyFireVote& vote);
FriendlyFireDecision DecideFriendlyFire(const std::vector<FriendlyFireVote>& votes,
                                        bool campaign_host);
bool ShouldSendVote(bool on, std::optional<bool> last_sent, double seconds_since_sent);

struct TeleportPc {
  bool pawn_alive = false;
  bool riding = false;
  bool cinematic = false;
};

struct TeleportCheck {
  bool campaign_host = false;
  bool player1_found = false;
  bool requester_is_player1 = false;
  TeleportPc requester;
  TeleportPc player1;
  std::optional<double> seconds_since_last;
};

enum class TeleportVerdict {
  kOk,
  kNotCampaignHost,
  kNoPlayer1,
  kIsPlayer1,
  kNoPawn,
  kPlayer1NoPawn,
  kRiding,
  kPlayer1Riding,
  kCinematic,
  kTooSoon,
};

TeleportVerdict CheckTeleport(const TeleportCheck& check);
std::string_view TeleportVerdictText(TeleportVerdict verdict);

struct PlayerSlot {
  bool is_local = false;
  std::string default_pawn_class;
};

std::optional<size_t> PickPlayer1(const std::vector<PlayerSlot>& players);

}  // namespace rex::glue::coop_requests
