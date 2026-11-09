// gow2 - ReXGlue Recompiled Project
//
// Pure co-op leash planner, host-side and game-agnostic.
#include "gow2_coop_leash_plan.h"

#include <cmath>

namespace rex::glue::coop_leash {
namespace {

constexpr float kFallbackRings[] = {120.0f, 360.0f, 600.0f};
constexpr int kFallbackDirs[8][2] = {
    {0, -1}, {0, 1}, {-1, 0}, {1, 0}, {-1, -1}, {1, 1}, {-1, 1}, {1, -1}};
constexpr int kFallbackOffsetCount = 24;

bool PcListed(const std::vector<LeashPc>& pcs, uint32_t pc) {
  for (const LeashPc& entry : pcs) {
    if (entry.pc == pc) return true;
  }
  return false;
}

void PruneState(const std::vector<LeashPc>& pcs, LeashState* state) {
  for (auto it = state->far_seconds.begin(); it != state->far_seconds.end();) {
    if (PcListed(pcs, it->first)) {
      ++it;
    } else {
      it = state->far_seconds.erase(it);
    }
  }
  for (auto it = state->fallen_seconds.begin(); it != state->fallen_seconds.end();) {
    if (PcListed(pcs, it->first)) {
      ++it;
    } else {
      it = state->fallen_seconds.erase(it);
    }
  }
  for (auto it = state->immediate_cooldown.begin(); it != state->immediate_cooldown.end();) {
    if (PcListed(pcs, it->first)) {
      ++it;
    } else {
      it = state->immediate_cooldown.erase(it);
    }
  }
  for (auto it = state->last_distance.begin(); it != state->last_distance.end();) {
    if (PcListed(pcs, it->first)) {
      ++it;
    } else {
      it = state->last_distance.erase(it);
    }
  }
  for (auto it = state->last_z.begin(); it != state->last_z.end();) {
    if (PcListed(pcs, it->first)) {
      ++it;
    } else {
      it = state->last_z.erase(it);
    }
  }
  for (auto it = state->god_granted.begin(); it != state->god_granted.end();) {
    if (PcListed(pcs, *it)) {
      ++it;
    } else {
      it = state->god_granted.erase(it);
    }
  }
}

void PruneGranted(const std::vector<LeashPc>& pcs, std::set<uint32_t>* granted) {
  for (auto it = granted->begin(); it != granted->end();) {
    if (PcListed(pcs, *it)) {
      ++it;
    } else {
      it = granted->erase(it);
    }
  }
}

bool IsAnchorCandidate(const std::vector<LeashPc>& pcs, const LeashPc& candidate,
                       const LeashPc& extra, const LeashPc& hero, float max_distance) {
  if (candidate.pc == extra.pc) return false;
  if (candidate.pawn == 0) return false;
  if (candidate.cinematic || candidate.hidden) return false;
  if (candidate.health <= 0) return false;
  if (!HasGearPawnBody(candidate)) return false;
  if (IsFallen(pcs, candidate)) return false;
  return Distance(candidate, hero.x, hero.y, hero.z) <= max_distance;
}

bool IsOverlapBody(const LeashPc& candidate, uint32_t self) {
  if (candidate.pc == self) return false;
  if (candidate.pawn == 0) return false;
  if (candidate.health <= 0) return false;
  return HasGearPawnBody(candidate);
}

bool IsOverlapCandidate(const LeashPc& extra) {
  return IsExtra(extra) && extra.pawn != 0 && extra.health > 0 && HasGearPawnBody(extra) &&
         !extra.riding;
}

bool IsStandingReference(const LeashPc& p) {
  return p.pawn != 0 && p.health > 0 && HasGearPawnBody(p) && !p.riding && !p.cinematic &&
         !p.hidden;
}

const LeashPc* FindOverlap(const std::vector<LeashPc>& pcs, const LeashPc& extra) {
  for (const LeashPc& candidate : pcs) {
    if (!IsOverlapBody(candidate, extra.pc)) continue;
    if (Distance(extra, candidate.x, candidate.y, candidate.z) <= kOverlapDistance) {
      return &candidate;
    }
  }
  return nullptr;
}

bool IsFellBelowHeroes(const std::vector<LeashPc>& pcs, const LeashPc& extra) {
  bool any_hero = false;
  for (const LeashPc& candidate : pcs) {
    if (!IsUsableHero(candidate)) continue;
    any_hero = true;
    if (!(extra.z < candidate.z - kFellBelowHeroes)) return false;
  }
  return any_hero;
}

PullOrder MakeOverlapOrder(const LeashPc& extra, const LeashPc& hero, const LeashPc& anchor,
                           float distance, const LeashPc& overlapped) {
  PullOrder order{extra.pc, extra.pawn, hero.pc, hero.x, hero.y, hero.z, anchor.pc,
                  anchor.x, anchor.y, anchor.z, distance};
  order.reason = PullReason::kOverlap;
  order.overlap_pc = overlapped.pc;
  order.overlap_x = overlapped.x;
  order.overlap_y = overlapped.y;
  order.overlap_z = overlapped.z;
  return order;
}

PullOrder MakeFellOrder(const LeashPc& extra, const LeashPc& hero, const LeashPc& anchor,
                        float distance) {
  PullOrder order{extra.pc, extra.pawn, hero.pc, hero.x, hero.y, hero.z, anchor.pc,
                  anchor.x, anchor.y, anchor.z, distance};
  order.reason = PullReason::kFell;
  return order;
}

bool CooldownActive(const LeashState* state, uint32_t pc) {
  auto it = state->immediate_cooldown.find(pc);
  return it != state->immediate_cooldown.end() && it->second > 0;
}

void TickCooldown(LeashState* state, uint32_t pc) {
  auto it = state->immediate_cooldown.find(pc);
  if (it == state->immediate_cooldown.end()) return;
  if (--it->second <= 0) state->immediate_cooldown.erase(it);
}

bool TryImmediate(const std::vector<LeashPc>& pcs, const LeashPc& extra, const LeashPc& hero,
                  const LeashConfig& config, LeashState* state, std::vector<PullOrder>* orders) {
  const LeashPc* anchor = PickAnchor(pcs, extra, hero, config.distance);
  const float distance = Distance(extra, hero.x, hero.y, hero.z);
  if (const LeashPc* overlapped = FindOverlap(pcs, extra)) {
    orders->push_back(MakeOverlapOrder(extra, hero, *anchor, distance, *overlapped));
    state->immediate_cooldown[extra.pc] = config.seconds;
    return true;
  }
  if (IsFellBelowHeroes(pcs, extra)) {
    orders->push_back(MakeFellOrder(extra, hero, *anchor, distance));
    state->immediate_cooldown[extra.pc] = config.seconds;
    return true;
  }
  return false;
}

void CountFarSecond(const LeashPc& extra, const LeashPc& hero, const LeashPc& anchor, float distance,
                    int increment, const LeashConfig& config, std::map<uint32_t, int>* far_seconds,
                    std::vector<PullOrder>* orders) {
  const int counted = (*far_seconds)[extra.pc] + increment;
  if (counted < config.seconds) {
    (*far_seconds)[extra.pc] = counted;
    return;
  }
  (*far_seconds)[extra.pc] = 0;
  orders->push_back(PullOrder{extra.pc, extra.pawn, hero.pc, hero.x, hero.y, hero.z, anchor.pc,
                              anchor.x, anchor.y, anchor.z, distance});
}

void PlanFar(const std::vector<LeashPc>& pcs, const LeashPc& extra, const LeashPc& hero,
             const LeashConfig& config, LeashState* state, std::vector<PullOrder>* orders) {
  const float distance = Distance(extra, hero.x, hero.y, hero.z);
  const auto previous = state->last_distance.find(extra.pc);
  const bool growing = previous != state->last_distance.end() &&
                       distance - previous->second > kLeftBehindGrowth;
  state->last_distance[extra.pc] = distance;
  if (distance <= config.distance) {
    state->far_seconds[extra.pc] = 0;
    return;
  }
  const LeashPc* anchor = PickAnchor(pcs, extra, hero, config.distance);
  CountFarSecond(extra, hero, *anchor, distance, growing ? kLeftBehindWeight : 1, config,
                 &state->far_seconds, orders);
}

void PlanExtra(const std::vector<LeashPc>& pcs, const LeashPc& extra, const LeashConfig& config,
               LeashState* state, std::vector<PullOrder>* orders) {
  const LeashPc* hero = NearestHero(pcs, extra);
  if (IsOverlapCandidate(extra) && (extra.cinematic || extra.hidden) &&
      !CooldownActive(state, extra.pc)) {
    if (const LeashPc* overlapped = FindOverlap(pcs, extra)) {
      orders->push_back(MakeOverlapOrder(extra, *overlapped, *overlapped,
                                         Distance(extra, overlapped->x, overlapped->y,
                                                  overlapped->z),
                                         *overlapped));
      orders->back().cinematic = true;
      state->immediate_cooldown[extra.pc] = config.seconds;
      state->far_seconds[extra.pc] = 0;
      return;
    }
  }
  if (!IsLeashableExtra(extra) || hero == nullptr) {
    state->far_seconds[extra.pc] = 0;
    state->last_distance.erase(extra.pc);
    if (CooldownActive(state, extra.pc)) {
      TickCooldown(state, extra.pc);
    } else {
      state->immediate_cooldown.erase(extra.pc);
    }
    return;
  }
  if (CooldownActive(state, extra.pc)) {
    TickCooldown(state, extra.pc);
  } else {
    state->immediate_cooldown.erase(extra.pc);
    if (TryImmediate(pcs, extra, *hero, config, state, orders)) return;
  }
  PlanFar(pcs, extra, *hero, config, state, orders);
}

const LeashPc* NearestStandingReference(const std::vector<LeashPc>& pcs, const LeashPc& fallen) {
  const LeashPc* nearest = nullptr;
  float nearest_distance = 0;
  for (const LeashPc& candidate : pcs) {
    if (candidate.pc == fallen.pc || !IsStandingReference(candidate) || IsFallen(pcs, candidate)) {
      continue;
    }
    const float distance = Distance(fallen, candidate.x, candidate.y, candidate.z);
    if (nearest == nullptr || distance < nearest_distance) {
      nearest = &candidate;
      nearest_distance = distance;
    }
  }
  return nearest;
}

bool IsDropping(const LeashState& state, const LeashPc& pc) {
  const auto last_z = state.last_z.find(pc.pc);
  return last_z != state.last_z.end() && last_z->second - pc.z > kFallingDropPerSecond;
}

void PlanFallenHeroes(const std::vector<LeashPc>& pcs, const LeashConfig& config,
                      LeashState* state, std::vector<PullOrder>* orders) {
  for (const LeashPc& pc : pcs) {
    if (!IsHeroClass(pc.dpc)) continue;
    const bool dropping = IsDropping(*state, pc);
    state->last_z[pc.pc] = pc.z;
    if (CooldownActive(state, pc.pc)) {
      TickCooldown(state, pc.pc);
      state->fallen_seconds[pc.pc] = 0;
      continue;
    }
    if (!IsFallen(pcs, pc) || !dropping) {
      state->fallen_seconds[pc.pc] = 0;
      continue;
    }
    if (++state->fallen_seconds[pc.pc] < kHeroFallSeconds) continue;
    const LeashPc* anchor = NearestStandingReference(pcs, pc);
    if (anchor == nullptr) continue;
    PullOrder order{pc.pc, pc.pawn, anchor->pc, anchor->x, anchor->y, anchor->z,
                    anchor->pc, anchor->x, anchor->y, anchor->z,
                    Distance(pc, anchor->x, anchor->y, anchor->z)};
    order.reason = PullReason::kHeroFell;
    orders->push_back(order);
    state->fallen_seconds[pc.pc] = 0;
    state->immediate_cooldown[pc.pc] = config.seconds;
  }
}

bool AnyRidingHero(const std::vector<LeashPc>& pcs) {
  for (const LeashPc& pc : pcs) {
    if (IsRidingHero(pc)) return true;
  }
  return false;
}

void CollectGrants(const std::vector<LeashPc>& pcs, std::set<uint32_t>* granted,
                   GodOrders* orders) {
  for (const LeashPc& pc : pcs) {
    if (!IsExtraOnFoot(pc)) continue;
    if (!pc.god) {
      orders->grant.push_back(pc.pc);
      granted->insert(pc.pc);
    } else {
      granted->insert(pc.pc);
    }
  }
}

void CollectRevokes(const std::vector<LeashPc>& pcs, const std::set<uint32_t>& granted,
                    GodOrders* orders) {
  for (const LeashPc& pc : pcs) {
    if (granted.count(pc.pc) != 0 && pc.god) orders->revoke.push_back(pc.pc);
  }
}

}  // namespace

bool IsHeroClass(const std::string& dpc) {
  return dpc == "GearPawn_COGMarcus" || dpc == "GearPawn_COGDom";
}

bool IsExtra(const LeashPc& p) {
  return !p.dpc.empty() && p.dpc != "None" && !IsHeroClass(p.dpc);
}

bool IsUsableHero(const LeashPc& p) {
  return IsHeroClass(p.dpc) && p.pawn != 0 && !p.cinematic && !p.hidden && p.health > 0;
}

bool IsFallen(const std::vector<LeashPc>& pcs, const LeashPc& p) {
  if (p.pawn == 0 || p.health <= 0 || !HasGearPawnBody(p) || p.riding || p.cinematic || p.hidden) {
    return false;
  }
  bool has_reference = false;
  for (const LeashPc& candidate : pcs) {
    if (candidate.pc == p.pc || !IsStandingReference(candidate)) continue;
    has_reference = true;
    if (!(p.z < candidate.z - kFellBelowHeroes)) return false;
  }
  return has_reference;
}

bool IsLeashableExtra(const LeashPc& p) {
  return IsExtra(p) && p.pawn != 0 && !p.cinematic && !p.hidden && p.health > 0 &&
         HasGearPawnBody(p);
}

bool HasGearPawnBody(const LeashPc& p) { return p.pawn_class.rfind("GearPawn_", 0) == 0; }

bool IsRidingHero(const LeashPc& p) { return IsHeroClass(p.dpc) && p.pawn != 0 && p.riding; }

bool IsExtraOnFoot(const LeashPc& p) {
  return IsExtra(p) && p.pawn != 0 && p.health > 0 && HasGearPawnBody(p) && !p.riding;
}

float Distance(const LeashPc& a, float x, float y, float z) {
  const double dx = double(a.x - x);
  const double dy = double(a.y - y);
  const double dz = double(a.z - z);
  return float(std::sqrt(dx * dx + dy * dy + dz * dz));
}

const LeashPc* NearestHero(const std::vector<LeashPc>& pcs, const LeashPc& extra) {
  const LeashPc* best = nullptr;
  float best_distance = 0;
  for (const LeashPc& candidate : pcs) {
    if (!IsUsableHero(candidate)) continue;
    if (IsFallen(pcs, candidate)) continue;
    const float distance = Distance(extra, candidate.x, candidate.y, candidate.z);
    if (best == nullptr || distance < best_distance) {
      best = &candidate;
      best_distance = distance;
    }
  }
  return best;
}

const LeashPc* PickAnchor(const std::vector<LeashPc>& pcs, const LeashPc& extra,
                          const LeashPc& hero, float max_distance) {
  if (HasGearPawnBody(hero)) return &hero;
  const LeashPc* best = nullptr;
  float best_distance = 0;
  for (const LeashPc& candidate : pcs) {
    if (!IsAnchorCandidate(pcs, candidate, extra, hero, max_distance)) continue;
    const float distance = Distance(candidate, hero.x, hero.y, hero.z);
    if (best == nullptr || distance < best_distance) {
      best = &candidate;
      best_distance = distance;
    }
  }
  if (best == nullptr) return &hero;
  return best;
}

std::vector<PullOrder> PlanPulls(const std::vector<LeashPc>& pcs, const LeashConfig& config,
                                 LeashState* state) {
  std::vector<PullOrder> orders;
  if (state == nullptr) return orders;
  for (const LeashPc& pc : pcs) {
    if (!IsExtra(pc)) continue;
    PlanExtra(pcs, pc, config, state, &orders);
  }
  PlanFallenHeroes(pcs, config, state, &orders);
  PruneState(pcs, state);
  return orders;
}

GodOrders PlanGodMode(const std::vector<LeashPc>& pcs, std::set<uint32_t>* granted) {
  GodOrders orders;
  if (granted == nullptr) return orders;
  PruneGranted(pcs, granted);
  if (AnyRidingHero(pcs)) {
    CollectGrants(pcs, granted, &orders);
    return orders;
  }
  CollectRevokes(pcs, *granted, &orders);
  granted->clear();
  return orders;
}

std::vector<std::pair<float, float>> FallbackOffsets() {
  std::vector<std::pair<float, float>> offsets;
  offsets.reserve(kFallbackOffsetCount);
  for (float ring : kFallbackRings) {
    for (const auto& dir : kFallbackDirs) {
      offsets.emplace_back(float(dir[0]) * ring, float(dir[1]) * ring);
    }
  }
  return offsets;
}

}  // namespace rex::glue::coop_leash
