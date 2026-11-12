// Tests for the co-op request rules (src/gow2_coop_requests_plan.cpp): pure host-side logic.
#include "gow2_coop_requests_plan.h"

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace {

using rex::glue::coop_requests::CheckTeleport;
using rex::glue::coop_requests::DecideFriendlyFire;
using rex::glue::coop_requests::FriendlyFireDecision;
using rex::glue::coop_requests::FriendlyFireRequest;
using rex::glue::coop_requests::FriendlyFireVote;
using rex::glue::coop_requests::ParseCoopRequest;
using rex::glue::coop_requests::PickPlayer1;
using rex::glue::coop_requests::PlayerSlot;
using rex::glue::coop_requests::Request;
using rex::glue::coop_requests::RequestText;
using rex::glue::coop_requests::ShouldSendVote;
using rex::glue::coop_requests::TeleportCheck;
using rex::glue::coop_requests::TeleportPc;
using rex::glue::coop_requests::TeleportVerdict;

int g_checks = 0;
int g_failures = 0;

void Check(bool ok, const char* what) {
  ++g_checks;
  if (ok) return;
  ++g_failures;
  std::printf("FAIL: %s\n", what);
}

void TestParse() {
  Check(ParseCoopRequest("gow2 ff 0") == Request::kFriendlyFireOff, "ff 0 parses");
  Check(ParseCoopRequest("gow2 ff 1") == Request::kFriendlyFireOn, "ff 1 parses");
  Check(ParseCoopRequest("gow2 tp") == Request::kGoToPlayer1, "tp parses");
  const char* bad[] = {"", "gow2", "gow2 ff", "gow2 ff 2", "gow2  tp", "GOW2 TP", "gow2 tp ",
                       " gow2 tp", "gow2 ff 1 1", "mutate gow2 tp", "hello"};
  for (const char* text : bad) Check(!ParseCoopRequest(text).has_value(), text);
}

void TestRequestText() {
  for (Request r : {Request::kFriendlyFireOff, Request::kFriendlyFireOn, Request::kGoToPlayer1}) {
    Check(ParseCoopRequest(RequestText(r)) == r, "RequestText round-trips");
  }
  Check(FriendlyFireRequest(true) == Request::kFriendlyFireOn, "vote on");
  Check(FriendlyFireRequest(false) == Request::kFriendlyFireOff, "vote off");
}

FriendlyFireVote Local(bool on) { return {true, on, 0}; }
FriendlyFireVote Remote(bool on, double age) { return {false, on, age}; }

void TestFriendlyFire() {
  FriendlyFireDecision d = DecideFriendlyFire({Local(true), Remote(true, 2)}, true);
  Check(d.active && d.opted_in == 2 && d.humans == 2, "both on: active");
  d = DecideFriendlyFire({Local(true), Remote(false, 2)}, true);
  Check(!d.active && d.opted_in == 1, "remote off: inactive");
  d = DecideFriendlyFire({Local(false), Remote(true, 2)}, true);
  Check(!d.active && d.opted_in == 1, "host off: inactive");
  d = DecideFriendlyFire({Local(true), Remote(true, 16)}, true);
  Check(!d.active && d.opted_in == 1, "stale remote vote counts as off");
  d = DecideFriendlyFire({Local(true), Remote(true, 15)}, true);
  Check(d.active, "15 s old vote still counts");
  d = DecideFriendlyFire({Local(true)}, true);
  Check(!d.active && d.humans == 1, "one human: inactive");
  d = DecideFriendlyFire({}, true);
  Check(!d.active && d.humans == 0, "nobody: inactive");
  d = DecideFriendlyFire({Local(true), Remote(true, 1)}, false);
  Check(!d.active && d.opted_in == 2, "not a campaign host: inactive");
  d = DecideFriendlyFire({Local(true), Local(true), Remote(true, 3)}, true);
  Check(d.active && d.humans == 3, "split screen host plus a friend, all on");
  d = DecideFriendlyFire({Local(true), Local(false), Remote(true, 3)}, true);
  Check(!d.active, "a local player off: inactive");
  Check(DecideFriendlyFire({Local(true), Local(true)}, true).active, "local players only");
  Check(!DecideFriendlyFire({Local(true), Remote(true, 99)}, true).active, "old vote");
  Check(DecideFriendlyFire({Remote(true, 99), Local(true)}, true).opted_in == 1, "local never stale");
}

void TestShouldSendVote() {
  Check(ShouldSendVote(false, std::nullopt, 0), "first vote is sent");
  Check(ShouldSendVote(true, false, 0.1), "a change is sent at once");
  Check(!ShouldSendVote(true, true, 4.9), "no resend before 5 s");
  Check(ShouldSendVote(true, true, 5.0), "resend after 5 s");
  Check(ShouldSendVote(false, false, 6), "an off vote is resent too");
}

TeleportCheck Eligible() {
  TeleportCheck c;
  c.campaign_host = true;
  c.player1_found = true;
  c.requester = TeleportPc{true, false, false};
  c.player1 = TeleportPc{true, false, false};
  return c;
}

void TestTeleport() {
  Check(CheckTeleport(Eligible()) == TeleportVerdict::kOk, "eligible");
  TeleportCheck c = Eligible();
  c.campaign_host = false;
  Check(CheckTeleport(c) == TeleportVerdict::kNotCampaignHost, "versus or client");
  c = Eligible();
  c.player1_found = false;
  Check(CheckTeleport(c) == TeleportVerdict::kNoPlayer1, "no player 1");
  c = Eligible();
  c.requester_is_player1 = true;
  Check(CheckTeleport(c) == TeleportVerdict::kIsPlayer1, "player 1 asks");
  c = Eligible();
  c.requester.pawn_alive = false;
  Check(CheckTeleport(c) == TeleportVerdict::kNoPawn, "dead requester");
  c = Eligible();
  c.player1.pawn_alive = false;
  Check(CheckTeleport(c) == TeleportVerdict::kPlayer1NoPawn, "dead player 1");
  c = Eligible();
  c.requester.riding = true;
  Check(CheckTeleport(c) == TeleportVerdict::kRiding, "requester rides");
  c = Eligible();
  c.player1.riding = true;
  Check(CheckTeleport(c) == TeleportVerdict::kPlayer1Riding, "player 1 rides");
  c = Eligible();
  c.player1.cinematic = true;
  Check(CheckTeleport(c) == TeleportVerdict::kCinematic, "cinematic");
  c = Eligible();
  c.requester.cinematic = true;
  Check(CheckTeleport(c) == TeleportVerdict::kCinematic, "requester in a cinematic");
  c = Eligible();
  c.seconds_since_last = 9.9;
  Check(CheckTeleport(c) == TeleportVerdict::kTooSoon, "rate limit");
  c.seconds_since_last = 10.0;
  Check(CheckTeleport(c) == TeleportVerdict::kOk, "10 s later");
}

void TestPickPlayer1() {
  std::vector<PlayerSlot> players = {{false, "GearPawn_COGDom"}, {true, "GearPawn_COGMarcus"}};
  Check(PickPlayer1(players) == 1u, "Marcus is player 1");
  players = {{false, "GearPawn_COGDom"}, {false, "GearPawn_COGMarcus"}, {true, "GearPawn_COGGear"}};
  Check(PickPlayer1(players) == 1u, "a remote Marcus is still player 1");
  players = {{false, "GearPawn_COGDom"}, {true, "None"}, {true, "GearPawn_COGGear"}};
  Check(PickPlayer1(players) == 1u, "no Marcus: the first local player");
  players = {{false, "GearPawn_COGDom"}};
  Check(!PickPlayer1(players).has_value(), "no Marcus and no local player");
  Check(!PickPlayer1({}).has_value(), "nobody");
}

}  // namespace

int main() {
  TestParse();
  TestRequestText();
  TestFriendlyFire();
  TestShouldSendVote();
  TestTeleport();
  TestPickPlayer1();
  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
