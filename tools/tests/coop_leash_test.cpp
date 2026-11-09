// Tests for the co-op leash planner (src/gow2_coop_leash_plan.cpp): pure host-side logic.
#include "gow2_coop_leash_plan.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

using rex::glue::coop_leash::FallbackOffsets;
using rex::glue::coop_leash::GodOrders;
using rex::glue::coop_leash::LeashConfig;
using rex::glue::coop_leash::LeashPc;
using rex::glue::coop_leash::LeashState;
using rex::glue::coop_leash::PickAnchor;
using rex::glue::coop_leash::PlanGodMode;
using rex::glue::coop_leash::PlanPulls;
using rex::glue::coop_leash::PullOrder;
using rex::glue::coop_leash::PullReason;

int g_checks = 0;
int g_failures = 0;

void Check(bool ok, const char* what) {
  ++g_checks;
  if (ok) return;
  ++g_failures;
  std::printf("FAIL: %s\n", what);
}

constexpr uint32_t kMarcusPc = 11;
constexpr uint32_t kMarcusPawn = 101;
constexpr uint32_t kDomPc = 22;
constexpr uint32_t kDomPawn = 102;
constexpr uint32_t kExtraPc = 33;
constexpr uint32_t kExtraPawn = 103;
constexpr uint32_t kFootPc = 44;
constexpr uint32_t kFootPawn = 104;

LeashPc MakeHero(uint32_t pc, uint32_t pawn, const char* dpc, float x) {
  LeashPc out;
  out.pc = pc;
  out.dpc = dpc;
  out.pawn = pawn;
  out.pawn_class = dpc;
  out.health = 100;
  out.x = x;
  return out;
}

LeashPc MakeMarcus(float x) { return MakeHero(kMarcusPc, kMarcusPawn, "GearPawn_COGMarcus", x); }

LeashPc MakeDom(float x) { return MakeHero(kDomPc, kDomPawn, "GearPawn_COGDom", x); }

LeashPc MakeMarcusInCentaur(float x) {
  LeashPc out = MakeMarcus(x);
  out.pawn_class = "Vehicle_Centaur";
  return out;
}

LeashPc MakeRidingMarcus(float x) {
  LeashPc out = MakeMarcusInCentaur(x);
  out.riding = true;
  return out;
}

LeashPc MakeTurretMarcus(float x) {
  LeashPc out = MakeMarcus(x);
  out.pawn_class = "GearTurret";
  out.riding = false;
  return out;
}

LeashPc MakeExtraAt(uint32_t pc, uint32_t pawn, float x) {
  LeashPc out;
  out.pc = pc;
  out.dpc = "GearPawn_COGGear";
  out.pawn = pawn;
  out.pawn_class = "GearPawn_COGGear";
  out.health = 100;
  out.x = x;
  return out;
}

LeashPc MakeExtra(float x) { return MakeExtraAt(kExtraPc, kExtraPawn, x); }

LeashPc MakeFootExtra(float x) { return MakeExtraAt(kFootPc, kFootPawn, x); }

LeashPc MakeVehicleExtra(float x) {
  LeashPc out = MakeExtra(x);
  out.pawn_class = "Vehicle_Centaur";
  out.riding = true;
  return out;
}

bool AnyOrdersIn(const std::vector<LeashPc>& pcs, int calls, LeashState* state) {
  const LeashConfig config;
  for (int i = 0; i < calls; ++i) {
    if (!PlanPulls(pcs, config, state).empty()) return true;
  }
  return false;
}

void TestPullAfterEightSeconds() {
  const std::vector<LeashPc> pcs{MakeMarcus(0), MakeExtra(5000)};
  LeashState state;
  Check(!AnyOrdersIn(pcs, 7, &state), "7 far seconds give no order");
  const LeashConfig config;
  const std::vector<PullOrder> orders = PlanPulls(pcs, config, &state);
  Check(orders.size() == 1, "the 8th far second gives exactly one order");
  if (orders.size() != 1) return;
  Check(orders[0].extra_pc == kExtraPc, "the order names the extra pc");
  Check(orders[0].extra_pawn == kExtraPawn, "the order names the extra pawn");
  Check(orders[0].hero_pc == kMarcusPc, "the order names Marcus");
  Check(orders[0].hero_x == 0 && orders[0].hero_y == 0 && orders[0].hero_z == 0,
        "the order carries the hero position");
  Check(orders[0].anchor_pc == kMarcusPc, "the anchor is Marcus on foot");
  Check(orders[0].anchor_x == 0 && orders[0].anchor_y == 0 && orders[0].anchor_z == 0,
        "the order carries the anchor position");
  Check(std::fabs(orders[0].distance - 5000) < 1, "the order carries the distance");
  Check(orders[0].reason == PullReason::kFar, "a far order has reason kFar");
  Check(state.far_seconds[kExtraPc] == 0, "the counter is back to 0 after the pull");
}

void TestReturnResetsCounter() {
  const LeashConfig config;
  LeashState state;
  const std::vector<LeashPc> away{MakeMarcus(0), MakeExtra(5000)};
  Check(!AnyOrdersIn(away, 5, &state), "5 far seconds give no order");
  Check(state.far_seconds[kExtraPc] == 5, "the counter reaches 5");
  const std::vector<LeashPc> back{MakeMarcus(0), MakeExtra(3900)};
  Check(PlanPulls(back, config, &state).empty(), "back within range gives no order");
  Check(state.far_seconds[kExtraPc] == 0, "coming back resets the counter to 0");
  // 3900 -> 4100 is a step out of range, not a jump: it counts as one far second
  const std::vector<LeashPc> out_again{MakeMarcus(0), MakeExtra(4100)};
  Check(!AnyOrdersIn(out_again, 7, &state), "7 more far seconds give no order");
  Check(PlanPulls(out_again, config, &state).size() == 1, "8 more far seconds pull again");
}

void CheckVariantNeverPulled(LeashPc extra, const char* no_order, const char* counter_zero) {
  const LeashConfig config;
  LeashState state;
  const std::vector<LeashPc> pcs{MakeMarcus(0), extra};
  bool ordered = false;
  for (int i = 0; i < 10; ++i) {
    if (!PlanPulls(pcs, config, &state).empty()) ordered = true;
  }
  Check(!ordered, no_order);
  Check(state.far_seconds[kExtraPc] == 0, counter_zero);
}

void TestUnleashableExtras() {
  LeashPc cinematic = MakeExtra(5000);
  cinematic.cinematic = true;
  CheckVariantNeverPulled(cinematic, "a cinematic extra gets no order",
                           "a cinematic extra keeps counter 0");
  LeashPc hidden = MakeExtra(5000);
  hidden.hidden = true;
  CheckVariantNeverPulled(hidden, "a hidden extra gets no order",
                           "a hidden extra keeps counter 0");
  LeashPc dead = MakeExtra(5000);
  dead.health = 0;
  CheckVariantNeverPulled(dead, "a dead extra gets no order", "a dead extra keeps counter 0");
  LeashPc pawnless = MakeExtra(5000);
  pawnless.pawn = 0;
  CheckVariantNeverPulled(pawnless, "an extra without pawn gets no order",
                           "an extra without pawn keeps counter 0");
  LeashPc driver = MakeExtra(5000);
  driver.pawn_class = "Vehicle_Centaur";
  CheckVariantNeverPulled(driver, "an extra in a vehicle gets no order",
                           "an extra in a vehicle keeps counter 0");
}

void TestNearestHero() {
  const LeashConfig config;
  const std::vector<LeashPc> pcs{MakeMarcus(9000), MakeDom(4500), MakeExtra(0)};
  LeashState state;
  Check(!AnyOrdersIn(pcs, 7, &state), "7 far seconds give no order with two heroes");
  const std::vector<PullOrder> orders = PlanPulls(pcs, config, &state);
  Check(orders.size() == 1, "the 8th far second pulls towards the nearer hero");
  if (orders.size() == 1) {
    Check(orders[0].hero_pc == kDomPc, "the nearer hero Dom is named");
    Check(orders[0].hero_x == 4500, "the order carries Dom's position");
    Check(orders[0].anchor_pc == kDomPc, "the anchor is Dom on foot");
  }
  std::vector<LeashPc> dom_cine = pcs;
  dom_cine[1].cinematic = true;
  LeashState state2;
  Check(!AnyOrdersIn(dom_cine, 7, &state2), "7 far seconds give no order with Dom cinematic");
  const std::vector<PullOrder> orders2 = PlanPulls(dom_cine, config, &state2);
  Check(orders2.size() == 1, "the 8th far second still pulls with Dom cinematic");
  if (orders2.size() == 1) {
    Check(orders2[0].hero_pc == kMarcusPc, "Marcus is used when Dom is cinematic");
    Check(orders2[0].anchor_pc == kMarcusPc, "the anchor is Marcus on foot");
  }
}

void TestNoUsableHero() {
  LeashPc marcus = MakeMarcus(0);
  marcus.hidden = true;
  const std::vector<LeashPc> pcs{marcus, MakeExtra(5000)};
  LeashState state;
  const LeashConfig config;
  bool ordered = false;
  for (int i = 0; i < 10; ++i) {
    if (!PlanPulls(pcs, config, &state).empty()) ordered = true;
  }
  Check(!ordered, "no usable hero gives no order");
  Check(state.far_seconds[kExtraPc] == 0, "no usable hero keeps counter 0");
}

void TestHeroesAndNoneNeverPulled() {
  LeashPc none = MakeExtra(9000);
  none.pc = 44;
  none.dpc = "None";
  LeashPc empty = MakeExtra(9000);
  empty.pc = 55;
  empty.dpc.clear();
  const std::vector<LeashPc> pcs{MakeMarcus(0), MakeDom(9000), none, empty};
  LeashState state;
  const LeashConfig config;
  bool ordered = false;
  for (int i = 0; i < 10; ++i) {
    if (!PlanPulls(pcs, config, &state).empty()) ordered = true;
  }
  Check(!ordered, "heroes and None or empty dpc never get orders");
  Check(state.far_seconds.empty(), "heroes and None or empty dpc leave no far counters");
  Check(state.immediate_cooldown.empty(), "heroes and None or empty dpc leave no cooldown");
}

void TestHeroFallRescue() {
  std::vector<LeashPc> pcs{MakeMarcus(0), MakeDom(0), MakeExtra(1000)};
  LeashState state;
  const LeashConfig config;
  const float dom_z[] = {-2100, -2600, -3100, -3600, -4100};
  for (int i = 0; i < 3; ++i) {
    pcs[1].z = dom_z[i];
    const std::vector<PullOrder> orders = PlanPulls(pcs, config, &state);
    bool hero_fell_order = false;
    for (const PullOrder& order : orders) {
      if (order.reason == PullReason::kHeroFell) hero_fell_order = true;
    }
    Check(!hero_fell_order, "a falling hero gets no rescue before the threshold");
  }
  pcs[1].z = dom_z[3];
  const std::vector<PullOrder> orders = PlanPulls(pcs, config, &state);
  Check(orders.size() == 1, "call 4 gives exactly one fallen hero rescue");
  if (orders.size() == 1) {
    Check(orders[0].reason == PullReason::kHeroFell, "the rescue reason is kHeroFell");
    Check(orders[0].extra_pc == kDomPc, "the rescue names Dom");
    Check(orders[0].anchor_pc == kMarcusPc, "the rescue anchor is Marcus");
    Check(orders[0].extra_pc != kExtraPc, "the regular extra is not named by the rescue");
  }
  pcs[1].z = dom_z[4];
  const std::vector<PullOrder> cooldown = PlanPulls(pcs, config, &state);
  bool hero_fell_order = false;
  for (const PullOrder& order : cooldown) {
    if (order.reason == PullReason::kHeroFell) hero_fell_order = true;
  }
  Check(!hero_fell_order, "call 4 is inside the rescue cooldown");
}

void TestHeroFallResetsWhenBack() {
  const LeashConfig config;
  LeashState state;
  std::vector<LeashPc> pcs{MakeMarcus(0), MakeDom(0)};
  const float dom_z[] = {-2100, -2600, -3100, 0, -2100, -2600};
  for (float z : dom_z) {
    pcs[1].z = z;
    const std::vector<PullOrder> orders = PlanPulls(pcs, config, &state);
    bool hero_fell_order = false;
    for (const PullOrder& order : orders) {
      if (order.reason == PullReason::kHeroFell) hero_fell_order = true;
    }
    Check(!hero_fell_order, "returning to the standing level resets the fall count");
  }
}

void TestHeroFallNeedsReference() {
  LeashPc marcus = MakeMarcus(0);
  marcus.cinematic = true;
  LeashPc dom = MakeDom(0);
  const std::vector<LeashPc> pcs{marcus, dom};
  LeashState state;
  const LeashConfig config;
  bool hero_fell_order = false;
  for (int i = 1; i <= 5; ++i) {
    dom.z = -5000.0f - 500.0f * (i - 1);
    for (const PullOrder& order : PlanPulls(pcs, config, &state)) {
      if (order.reason == PullReason::kHeroFell) hero_fell_order = true;
    }
  }
  Check(!hero_fell_order, "a cinematic only reference cannot trigger hero rescue");
}

void TestHeroOnLowerFloorNotRescued() {
  const LeashConfig config;
  LeashState state;
  std::vector<LeashPc> pcs{MakeMarcus(0), MakeExtra(1000), MakeDom(0)};
  pcs[2].z = -2500;
  bool hero_fell_order = false;
  for (int i = 0; i < 8; ++i) {
    for (const PullOrder& order : PlanPulls(pcs, config, &state)) {
      if (order.reason == PullReason::kHeroFell) hero_fell_order = true;
    }
  }
  Check(!hero_fell_order, "a hero standing on a lower floor is not rescued");
}

void TestExtraNotPulledToFallenHero() {
  LeashPc dom = MakeDom(6000);
  dom.z = -5000;
  const std::vector<LeashPc> pcs{MakeMarcus(0), dom, MakeExtra(5000)};
  LeashState state;
  const LeashConfig config;
  bool extra_ordered = false;
  for (int i = 0; i < 8; ++i) {
    for (const PullOrder& order : PlanPulls(pcs, config, &state)) {
      if (order.extra_pc != kExtraPc) continue;
      extra_ordered = true;
      Check(order.hero_pc == kMarcusPc, "an extra is never pulled to fallen Dom");
    }
  }
  Check(extra_ordered, "the regular extra eventually gets an order");
}

void TestCinematicOverlap() {
  LeashPc marcus = MakeMarcus(0);
  marcus.cinematic = true;
  marcus.hidden = true;
  LeashPc extra = MakeExtra(0);
  extra.cinematic = true;
  extra.hidden = true;
  const std::vector<LeashPc> pcs{marcus, extra};
  LeashState state;
  const LeashConfig config;
  const std::vector<PullOrder> orders = PlanPulls(pcs, config, &state);
  Check(orders.size() == 1, "a cinematic overlap gets one immediate order");
  if (orders.size() == 1) {
    Check(orders[0].reason == PullReason::kOverlap, "the cinematic overlap reason is kOverlap");
    Check(orders[0].cinematic, "the overlap order records cinematic handling");
    Check(orders[0].anchor_pc == kMarcusPc, "the overlapped Marcus is the anchor");
    Check(orders[0].overlap_pc == kMarcusPc, "the overlap target is Marcus");
  }
  Check(PlanPulls(pcs, config, &state).empty(), "the next call is inside cooldown");
}

void TestDisappearedPcPruned() {
  const LeashConfig config;
  LeashState state;
  const std::vector<LeashPc> both{MakeMarcus(0), MakeExtra(5000)};
  PlanPulls(both, config, &state);
  Check(state.far_seconds.find(kExtraPc) != state.far_seconds.end(), "a far extra is tracked");
  const std::vector<LeashPc> alone{MakeMarcus(0)};
  Check(PlanPulls(alone, config, &state).empty(), "Marcus alone gives no order");
  Check(state.far_seconds.find(kExtraPc) == state.far_seconds.end(),
        "a pc that disappears is erased");
}

void TestAnchorIsHeroWhenHeroOnFoot() {
  const LeashPc hero = MakeMarcus(0);
  const LeashPc extra = MakeExtra(5000);
  const std::vector<LeashPc> pcs{hero, extra};
  const LeashPc* anchor = PickAnchor(pcs, extra, pcs[0], LeashConfig().distance);
  Check(anchor != nullptr, "PickAnchor returns someone when the hero is on foot");
  if (anchor == nullptr) return;
  Check(anchor->pc == kMarcusPc, "the anchor is the hero on foot");
}

void TestAnchorIsFootExtraWhenHeroDrives() {
  const std::vector<LeashPc> pcs{MakeMarcusInCentaur(0), MakeFootExtra(312), MakeExtra(17517)};
  LeashState state;
  Check(!AnyOrdersIn(pcs, 7, &state), "7 far seconds give no order when the hero drives");
  const LeashConfig config;
  const std::vector<PullOrder> orders = PlanPulls(pcs, config, &state);
  Check(orders.size() == 1, "the 8th far second pulls the stranded extra");
  if (orders.size() != 1) return;
  Check(orders[0].hero_pc == kMarcusPc, "the hero is still Marcus in the tank");
  Check(orders[0].anchor_pc == kFootPc, "the anchor is the extra on foot");
  Check(orders[0].anchor_x == 312 && orders[0].anchor_y == 0 && orders[0].anchor_z == 0,
        "the anchor carries the on-foot position");
  Check(std::fabs(orders[0].distance - 17517) < 1, "the distance is measured from Marcus");
}

void TestAnchorFallsBackToHero() {
  const LeashConfig config;
  LeashPc foot_cine = MakeFootExtra(312);
  foot_cine.cinematic = true;
  const std::vector<LeashPc> cine{MakeMarcusInCentaur(0), foot_cine, MakeExtra(17517)};
  LeashState state_cine;
  Check(!AnyOrdersIn(cine, 7, &state_cine), "7 far seconds give no order with a cinematic bystander");
  const std::vector<PullOrder> cine_orders = PlanPulls(cine, config, &state_cine);
  Check(cine_orders.size() == 1, "the 8th far second pulls with a cinematic bystander");
  if (cine_orders.size() == 1) {
    Check(cine_orders[0].anchor_pc == kMarcusPc, "a cinematic bystander is not the anchor");
  }
  const std::vector<LeashPc> away{MakeMarcusInCentaur(0), MakeFootExtra(5000), MakeExtra(17517)};
  LeashState state_away;
  Check(!AnyOrdersIn(away, 7, &state_away), "7 far seconds give no order with a far bystander");
  const std::vector<PullOrder> away_orders = PlanPulls(away, config, &state_away);
  Check(away_orders.size() == 2, "both far extras get orders with a far bystander");
  for (const PullOrder& order : away_orders) {
    Check(order.anchor_pc == kMarcusPc, "a bystander 5000 away is not the anchor");
  }
}

void TestStrandedExtraNeverOwnAnchor() {
  const std::vector<LeashPc> pcs{MakeMarcusInCentaur(0), MakeExtra(17517)};
  LeashState state;
  Check(!AnyOrdersIn(pcs, 7, &state), "7 far seconds give no order with no bystander");
  const LeashConfig config;
  const std::vector<PullOrder> orders = PlanPulls(pcs, config, &state);
  Check(orders.size() == 1, "the 8th far second pulls with no bystander");
  if (orders.size() != 1) return;
  Check(orders[0].anchor_pc != kExtraPc, "the stranded extra is never its own anchor");
  Check(orders[0].anchor_pc == kMarcusPc, "the anchor falls back to the hero");
}

void TestOverlapImmediate() {
  const std::vector<LeashPc> pcs{MakeMarcus(0), MakeExtra(0)};
  LeashState state;
  const LeashConfig config;
  const std::vector<PullOrder> orders = PlanPulls(pcs, config, &state);
  Check(orders.size() == 1, "overlap with Marcus gives an immediate order");
  if (orders.size() != 1) return;
  Check(orders[0].reason == PullReason::kOverlap, "an overlap order has reason kOverlap");
  Check(orders[0].overlap_pc == kMarcusPc, "an overlap order names Marcus");
  Check(orders[0].overlap_x == 0 && orders[0].overlap_y == 0 && orders[0].overlap_z == 0,
        "an overlap order carries the overlapped position");
  Check(orders[0].extra_pc == kExtraPc, "an overlap order names the extra");
}

void TestOverlapCooldown() {
  const std::vector<LeashPc> pcs{MakeMarcus(0), MakeExtra(0)};
  LeashState state;
  const LeashConfig config;
  Check(PlanPulls(pcs, config, &state).size() == 1, "the first tick overlaps");
  Check(PlanPulls(pcs, config, &state).empty(), "the next tick gives no second immediate order");
}

void TestOverlapWithExtra() {
  const std::vector<LeashPc> pcs{MakeMarcus(0), MakeExtra(1000), MakeFootExtra(1030)};
  LeashState state;
  const LeashConfig config;
  const std::vector<PullOrder> orders = PlanPulls(pcs, config, &state);
  Check(orders.size() == 2, "two extras overlapping each other both get orders at once");
  for (const PullOrder& order : orders) {
    Check(order.reason == PullReason::kOverlap, "an extra-extra overlap has reason kOverlap");
  }
}

void TestFellImmediate() {
  LeashPc extra = MakeExtra(0);
  extra.z = -2100;
  const std::vector<LeashPc> pcs{MakeMarcus(0), MakeDom(5000), extra};
  LeashState state;
  const LeashConfig config;
  const std::vector<PullOrder> orders = PlanPulls(pcs, config, &state);
  Check(orders.size() == 1, "2100 below every hero gives an immediate order");
  if (orders.size() != 1) return;
  Check(orders[0].reason == PullReason::kFell, "a fell order has reason kFell");
  Check(orders[0].extra_pc == kExtraPc, "a fell order names the extra");
}

void TestFellOneHeroNoOrder() {
  LeashPc dom = MakeDom(0);
  dom.z = -2100;
  LeashPc extra = MakeExtra(1000);
  extra.z = -2100;
  const std::vector<LeashPc> pcs{MakeMarcus(0), dom, extra};
  LeashState state;
  const LeashConfig config;
  Check(PlanPulls(pcs, config, &state).empty(),
        "2100 below one hero but level with the other gives no immediate order");
}

void CheckImmediateNever(LeashPc extra, const char* what) {
  const LeashConfig config;
  LeashState state;
  const std::vector<LeashPc> pcs{MakeMarcus(0), extra};
  bool ordered = false;
  for (int i = 0; i < 3; ++i) {
    if (!PlanPulls(pcs, config, &state).empty()) ordered = true;
  }
  Check(!ordered, what);
}

void TestImmediateNeverForUnleashable() {
  LeashPc cinematic = MakeExtra(1000);
  cinematic.cinematic = true;
  CheckImmediateNever(cinematic, "a cinematic extra never gets an immediate order");
  LeashPc hidden = MakeExtra(1000);
  hidden.hidden = true;
  CheckImmediateNever(hidden, "a hidden extra never gets an immediate order");
  LeashPc dead = MakeExtra(0);
  dead.health = 0;
  CheckImmediateNever(dead, "a dead extra never gets an immediate order");
}

void TestLeftBehindGrowing() {
  const LeashConfig config;
  LeashState state;
  const std::vector<float> positions{5000, 5400, 5800};
  bool ordered = false;
  for (float x : positions) {
    const std::vector<LeashPc> pcs{MakeMarcus(0), MakeExtra(x)};
    for (const PullOrder& order : PlanPulls(pcs, config, &state)) {
      if (order.extra_pc == kExtraPc) ordered = true;
    }
  }
  Check(ordered, "a growing distance pulls the extra within three calls");
}

void TestGodGrantWhenHeroRides() {
  const std::vector<LeashPc> pcs{MakeRidingMarcus(0), MakeExtra(1000)};
  std::set<uint32_t> granted;
  const GodOrders orders = PlanGodMode(pcs, &granted);
  Check(orders.grant.size() == 1, "an extra on foot is granted when Marcus rides");
  if (!orders.grant.empty()) Check(orders.grant[0] == kExtraPc, "the grant names the extra");
  Check(orders.revoke.empty(), "no revoke while a hero rides");
  Check(granted.count(kExtraPc) != 0, "a granted extra is tracked");
}

void TestGodAdoptAndRevoke() {
  LeashPc god_extra = MakeExtra(1000);
  god_extra.god = true;
  const std::vector<LeashPc> riding{MakeRidingMarcus(0), god_extra};
  std::set<uint32_t> granted;
  const GodOrders adopted = PlanGodMode(riding, &granted);
  Check(adopted.grant.empty(), "an extra already god gets no grant");
  Check(adopted.revoke.empty(), "no revoke while a hero rides");
  Check(granted.count(kExtraPc) != 0, "an extra already god is adopted");
  const std::vector<LeashPc> on_foot{MakeMarcus(0), god_extra};
  const GodOrders revoked = PlanGodMode(on_foot, &granted);
  Check(revoked.grant.empty(), "no grant when no hero rides");
  Check(revoked.revoke.size() == 1, "an adopted extra is revoked later");
  if (!revoked.revoke.empty()) Check(revoked.revoke[0] == kExtraPc, "the revoke names the extra");
  Check(granted.empty(), "granted is cleared when no hero rides");
}

void TestGodRevokeOnlyGranted() {
  LeashPc first = MakeExtra(1000);
  first.god = true;
  LeashPc second = MakeFootExtra(1200);
  second.god = true;
  const std::vector<LeashPc> pcs{MakeMarcus(0), first, second};
  std::set<uint32_t> granted{kExtraPc};
  const GodOrders orders = PlanGodMode(pcs, &granted);
  Check(orders.revoke.size() == 1, "only granted pcs are revoked");
  if (!orders.revoke.empty()) Check(orders.revoke[0] == kExtraPc, "the revoke names the granted pc");
  Check(granted.empty(), "granted is cleared after revoking");
}

void TestGodHeroNeverRevoked() {
  LeashPc marcus = MakeMarcus(0);
  marcus.god = true;
  LeashPc extra = MakeExtra(1000);
  extra.god = true;
  const std::vector<LeashPc> pcs{marcus, extra};
  std::set<uint32_t> granted{kExtraPc};
  const GodOrders orders = PlanGodMode(pcs, &granted);
  bool hero_revoked = false;
  for (uint32_t pc : orders.revoke) {
    if (pc == kMarcusPc) hero_revoked = true;
  }
  Check(!hero_revoked, "a hero with god never revoked");
  Check(orders.revoke.size() == 1, "only the granted extra is revoked");
}

void TestGodVehicleAndDeadNotGranted() {
  const std::vector<LeashPc> vehicle{MakeRidingMarcus(0), MakeVehicleExtra(1000)};
  std::set<uint32_t> granted_vehicle;
  const GodOrders vehicle_orders = PlanGodMode(vehicle, &granted_vehicle);
  Check(vehicle_orders.grant.empty(), "an extra in a vehicle is not granted");
  Check(granted_vehicle.empty(), "an extra in a vehicle is not tracked");
  LeashPc dead = MakeExtra(1000);
  dead.health = 0;
  const std::vector<LeashPc> dead_pcs{MakeRidingMarcus(0), dead};
  std::set<uint32_t> granted_dead;
  const GodOrders dead_orders = PlanGodMode(dead_pcs, &granted_dead);
  Check(dead_orders.grant.empty(), "a dead extra is not granted");
  Check(granted_dead.empty(), "a dead extra is not tracked");
}

void TestGodDisappearedPruned() {
  const std::vector<LeashPc> pcs{MakeRidingMarcus(0)};
  std::set<uint32_t> granted{kExtraPc};
  const GodOrders orders = PlanGodMode(pcs, &granted);
  Check(orders.grant.empty(), "no grant without a listed extra");
  Check(granted.empty(), "a pc that disappears is pruned");
}

void TestGodTurretGrantsNothing() {
  const std::vector<LeashPc> pcs{MakeTurretMarcus(0), MakeExtra(1000)};
  std::set<uint32_t> granted;
  const GodOrders orders = PlanGodMode(pcs, &granted);
  Check(orders.grant.empty(), "a hero on a turret grants nothing");
  Check(orders.revoke.empty(), "a hero on a turret revokes nothing");
  Check(granted.empty(), "a hero on a turret tracks nothing");
}

void TestFallbackOffsets() {
  const std::vector<std::pair<float, float>> offsets = FallbackOffsets();
  Check(offsets.size() == 24, "FallbackOffsets has 24 entries");
  if (offsets.size() != 24) return;
  Check(offsets[0].first == 0 && offsets[0].second == -120, "the first offset is (0,-120)");
  Check(offsets[8].first == 0 && offsets[8].second == -360, "entry 8 is (0,-360)");
  Check(offsets[16].first == 0 && offsets[16].second == -600, "entry 16 is (0,-600)");
}

}  // namespace

int main() {
  TestPullAfterEightSeconds();
  TestReturnResetsCounter();
  TestUnleashableExtras();
  TestNearestHero();
  TestNoUsableHero();
  TestHeroesAndNoneNeverPulled();
  TestHeroFallRescue();
  TestHeroFallResetsWhenBack();
  TestHeroFallNeedsReference();
  TestHeroOnLowerFloorNotRescued();
  TestExtraNotPulledToFallenHero();
  TestCinematicOverlap();
  TestDisappearedPcPruned();
  TestAnchorIsHeroWhenHeroOnFoot();
  TestAnchorIsFootExtraWhenHeroDrives();
  TestAnchorFallsBackToHero();
  TestStrandedExtraNeverOwnAnchor();
  TestOverlapImmediate();
  TestOverlapCooldown();
  TestOverlapWithExtra();
  TestFellImmediate();
  TestFellOneHeroNoOrder();
  TestImmediateNeverForUnleashable();
  TestLeftBehindGrowing();
  TestGodGrantWhenHeroRides();
  TestGodAdoptAndRevoke();
  TestGodRevokeOnlyGranted();
  TestGodHeroNeverRevoked();
  TestGodVehicleAndDeadNotGranted();
  TestGodDisappearedPruned();
  TestGodTurretGrantsNothing();
  TestFallbackOffsets();
  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
