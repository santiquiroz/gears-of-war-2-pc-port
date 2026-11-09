// gow2 - ReXGlue Recompiled Project
//
// Pure co-op leash planner: which extras stayed far from a human hero.
#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace rex::glue::coop_leash {

constexpr float kPulledCloseEnough = 1500.0f;
constexpr float kFallbackRaiseZ = 40.0f;
constexpr float kOverlapDistance = 60.0f;
constexpr float kFellBelowHeroes = 2000.0f;
constexpr int kHeroFallSeconds = 3;
constexpr float kFallingDropPerSecond = 300.0f;
constexpr float kLeftBehindGrowth = 300.0f;
constexpr int kLeftBehindWeight = 4;

struct LeashPc {
  uint32_t pc = 0;
  std::string dpc;
  bool cinematic = false;
  uint32_t pawn = 0;
  std::string pawn_class;
  int32_t health = 0;
  bool hidden = false;
  float x = 0, y = 0, z = 0;
  bool god = false;
  bool riding = false;
};

enum class PullReason { kFar, kOverlap, kFell, kHeroFell };

struct PullOrder {
  uint32_t extra_pc = 0;
  uint32_t extra_pawn = 0;
  uint32_t hero_pc = 0;
  float hero_x = 0, hero_y = 0, hero_z = 0;
  uint32_t anchor_pc = 0;
  float anchor_x = 0, anchor_y = 0, anchor_z = 0;
  float distance = 0;
  PullReason reason = PullReason::kFar;
  bool cinematic = false;
  uint32_t overlap_pc = 0;
  float overlap_x = 0, overlap_y = 0, overlap_z = 0;
};

struct LeashConfig {
  float distance = 4000.0f;
  int seconds = 8;
};

struct LeashState {
  std::map<uint32_t, int> far_seconds;
  std::map<uint32_t, int> fallen_seconds;
  std::map<uint32_t, int> immediate_cooldown;
  std::map<uint32_t, float> last_distance;
  std::map<uint32_t, float> last_z;
  std::set<uint32_t> god_granted;
};

struct GodOrders {
  std::vector<uint32_t> grant;
  std::vector<uint32_t> revoke;
};

bool IsHeroClass(const std::string& dpc);
bool IsExtra(const LeashPc& p);
bool IsUsableHero(const LeashPc& p);
bool IsFallen(const std::vector<LeashPc>& pcs, const LeashPc& p);
bool IsLeashableExtra(const LeashPc& p);
bool HasGearPawnBody(const LeashPc& p);
bool IsRidingHero(const LeashPc& p);
bool IsExtraOnFoot(const LeashPc& p);
float Distance(const LeashPc& a, float x, float y, float z);
const LeashPc* NearestHero(const std::vector<LeashPc>& pcs, const LeashPc& extra);
const LeashPc* PickAnchor(const std::vector<LeashPc>& pcs, const LeashPc& extra, const LeashPc& hero,
                          float max_distance);
std::vector<PullOrder> PlanPulls(const std::vector<LeashPc>& pcs, const LeashConfig& config,
                                 LeashState* state);
GodOrders PlanGodMode(const std::vector<LeashPc>& pcs, std::set<uint32_t>* granted);
std::vector<std::pair<float, float>> FallbackOffsets();

}  // namespace rex::glue::coop_leash
