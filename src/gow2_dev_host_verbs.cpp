// gow2 - ReXGlue Recompiled Project
//
// Host-side '!' verbs of the dev command file (list, selftest, god, kill, knockdown, damage, specialmove, call, mark).
#include "gow2_dev_host_verbs.h"

#include "gow2_dev_map_change.h"
#include "gow2_dev_commands.h"
#include "gow2_dev_guest_calls.h"
#include "gow2_guest.h"

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include <rex/logging.h>
#include <rex/ppc.h>

namespace rex::glue::dev_verbs {
namespace {

using dev_commands::kGameGlobals;

std::string NoTargetText(const dev_commands::Target& target) {
  if (target.kind == dev_commands::TargetKind::kName)
    return "no player controller named " + target.name;
  return "no player controller matches " + dev_commands::TargetToken(target);
}

std::string JoinComma(const std::vector<std::string>& parts) {
  std::string out;
  for (const std::string& part : parts) {
    if (!out.empty()) out += ", ";
    out += part;
  }
  return out;
}

bool GuardedDescribe(const uint8_t* base, const std::vector<uint32_t>& pcs,
                     std::string* out) {
  auto walk = [&] { *out = dev_commands::DescribeTargets(base, kGameGlobals, pcs); };
  return guest_call::Guarded(walk);
}

VerbResult ListVerb(const uint8_t* base) {
  const uint32_t world_info = GuardedWorldInfo(base);
  if (world_info == 0) return Fail("no world yet");
  std::vector<std::string> lines;
  uint32_t players = 0;
  uint32_t netmode = 0;
  auto walk = [&] {
    const std::vector<uint32_t> controllers =
        dev_commands::ControllerList(base, world_info);
    for (size_t i = 0; i < controllers.size(); ++i)
      lines.push_back("DEVCMD   [" + std::to_string(i) + "] " +
                      dev_commands::DescribeController(base, kGameGlobals, controllers[i]));
    for (uint32_t controller : controllers)
      if (dev_commands::ClassChainHas(base, kGameGlobals, controller, "PlayerController"))
        ++players;
    netmode = guest::LoadU8(base, world_info + dev_commands::layout::kWorldInfoNetMode);
  };
  if (!guest_call::Guarded(walk)) return Fail("fault while reading guest objects");
  for (const std::string& line : lines) REXLOG_INFO("{}", line);
  return Ok(std::to_string(lines.size()) + " controllers, " + std::to_string(players) +
            " player controllers, netmode " + std::to_string(netmode));
}

bool DropDetailBit(const uint8_t* base, uint32_t gri) {
  return (guest::LoadU32(base, gri + dev_commands::layout::kGriDropDetailWord) &
          dev_commands::layout::kDropDetailMask) != 0;
}

std::optional<bool> GuardedDropDetailBit(const uint8_t* base, uint32_t gri) {
  bool bit = false;
  auto read = [&] { bit = DropDetailBit(base, gri); };
  if (!guest_call::Guarded(read)) return std::nullopt;
  return bit;
}

bool RestoreDropDetailBit(uint8_t* base, uint32_t gri, bool want) {
  uint32_t word = 0;
  auto read = [&] {
    word = guest::LoadU32(base, gri + dev_commands::layout::kGriDropDetailWord);
  };
  if (!guest_call::Guarded(read)) return false;
  if (want)
    word |= dev_commands::layout::kDropDetailMask;
  else
    word &= ~dev_commands::layout::kDropDetailMask;
  guest::StoreU32(base, gri + dev_commands::layout::kGriDropDetailWord, word);
  return true;
}

std::optional<uint32_t> GuardedGearGri(const uint8_t* base, uint32_t world_info) {
  uint32_t gri = 0;
  bool is_gear = false;
  auto walk = [&] {
    gri = guest::LoadU32(base, world_info + dev_commands::layout::kWorldInfoGri);
    is_gear = dev_commands::ClassChainHas(base, kGameGlobals, gri, "GearGRI");
  };
  if (!guest_call::Guarded(walk)) return std::nullopt;
  if (!is_gear) return uint32_t{0};
  return gri;
}

std::vector<uint32_t> OrderedSelfTestPcs(const uint8_t* base, uint32_t engine,
                                         uint32_t world_info) {
  std::vector<uint32_t> ordered;
  const uint32_t local = dev_commands::LocalPlayerController(base, kGameGlobals, engine);
  if (local != 0) ordered.push_back(local);
  for (uint32_t pc : dev_commands::PlayerControllers(base, kGameGlobals, world_info))
    if (pc != local) ordered.push_back(pc);
  return ordered;
}

std::optional<std::vector<uint32_t>> GuardedSelfTestPcs(const uint8_t* base, uint32_t engine,
                                                        uint32_t world_info) {
  std::optional<std::vector<uint32_t>> out;
  auto walk = [&] { out = OrderedSelfTestPcs(base, engine, world_info); };
  if (!guest_call::Guarded(walk)) return std::nullopt;
  return out;
}

struct SelfTestBits {
  bool b0 = false;
  bool b1 = false;
  bool b2 = false;
  bool calls_ok = false;
};

std::optional<SelfTestBits> ToggleDropDetailTwice(const PPCContext& ctx, uint8_t* base,
                                                  uint32_t gri, uint32_t pc) {
  const std::optional<bool> b0 = GuardedDropDetailBit(base, gri);
  if (!b0.has_value()) return std::nullopt;
  const std::optional<std::string> first =
      guest_call::ConsoleCommand(ctx, base, pc, "ToggleDropDetail");
  const std::optional<bool> b1 = GuardedDropDetailBit(base, gri);
  if (!b1.has_value()) return std::nullopt;
  const std::optional<std::string> second =
      guest_call::ConsoleCommand(ctx, base, pc, "ToggleDropDetail");
  const std::optional<bool> b2 = GuardedDropDetailBit(base, gri);
  if (!b2.has_value()) return std::nullopt;
  SelfTestBits bits;
  bits.b0 = *b0;
  bits.b1 = *b1;
  bits.b2 = *b2;
  bits.calls_ok = first.has_value() && second.has_value();
  return bits;
}

void LogSelfTestRoute(const std::string& who, const SelfTestBits& bits, bool pass,
                      bool restored) {
  if (pass) {
    REXLOG_INFO("DEVCMD selftest {}: route ok", who);
    return;
  }
  if (restored) {
    REXLOG_WARN("DEVCMD selftest {}: route FAILED (bit {} -> {} -> {}) (bit restored)", who,
                bits.b0, bits.b1, bits.b2);
    return;
  }
  REXLOG_WARN("DEVCMD selftest {}: route FAILED (bit {} -> {} -> {})", who, bits.b0,
              bits.b1, bits.b2);
}

std::optional<bool> SelfTestRoute(const PPCContext& ctx, uint8_t* base, uint32_t gri,
                                  uint32_t pc) {
  const std::optional<SelfTestBits> bits = ToggleDropDetailTwice(ctx, base, gri, pc);
  if (!bits.has_value()) return std::nullopt;
  const bool pass = bits->calls_ok && bits->b1 != bits->b0 && bits->b2 == bits->b0;
  bool restored = false;
  if (bits->b2 != bits->b0) {
    if (!RestoreDropDetailBit(base, gri, bits->b0)) return std::nullopt;
    restored = true;
  }
  std::string who;
  if (!GuardedDescribe(base, {pc}, &who)) return std::nullopt;
  LogSelfTestRoute(who, *bits, pass, restored);
  return pass;
}

VerbResult SelfTestVerb(const PPCContext& ctx, uint8_t* base, uint32_t engine) {
  const uint32_t world_info = GuardedWorldInfo(base);
  if (world_info == 0) return Fail("no world yet");
  const std::optional<uint32_t> gri = GuardedGearGri(base, world_info);
  if (!gri.has_value()) return Fail("fault while reading guest objects");
  if (*gri == 0) return Fail("no GearGRI (not in a campaign map)");
  const std::optional<std::vector<uint32_t>> pcs =
      GuardedSelfTestPcs(base, engine, world_info);
  if (!pcs.has_value()) return Fail("fault while reading guest objects");
  size_t passed = 0;
  for (uint32_t pc : *pcs) {
    const std::optional<bool> route = SelfTestRoute(ctx, base, *gri, pc);
    if (!route.has_value()) return Fail("fault while reading guest objects");
    if (*route) ++passed;
  }
  const std::string detail =
      std::to_string(passed) + "/" + std::to_string(pcs->size()) + " routes ok";
  if (passed == pcs->size()) return Ok(detail);
  return Fail(detail);
}

void SetGodMode(uint8_t* base, uint32_t pc, bool on) {
  const uint32_t flags =
      guest::LoadU32(base, pc + dev_commands::layout::kControllerFlags);
  guest::StoreU32(base, pc + dev_commands::layout::kControllerFlags,
                  dev_commands::WithGodMode(flags, on));
}

VerbResult GodVerb(uint8_t* base, uint32_t engine, const dev_commands::Command& command) {
  const uint32_t world_info = GuardedWorldInfo(base);
  if (!IsHost(base, world_info)) return Fail("only the host decides damage");
  const std::optional<std::vector<uint32_t>> targets =
      GuardedTargets(base, engine, world_info, command.target);
  if (!targets.has_value()) return Fail("fault while reading guest objects");
  if (targets->empty()) return Fail(NoTargetText(command.target));
  if (command.args.empty()) return Fail("god needs on or off");
  const bool on = command.args[0] == "on";
  for (uint32_t pc : *targets) SetGodMode(base, pc, on);
  std::string who;
  if (!GuardedDescribe(base, *targets, &who)) return Fail("fault while reading guest objects");
  return Ok(std::string(on ? "god on for " : "god off for ") + who);
}

struct KillTarget {
  uint32_t pc = 0;
  uint32_t pawn = 0;
  std::string who;
  std::string pawn_class;
};

// Reads first: guest calls cannot run inside Guarded.
bool GuardedKillTargets(const uint8_t* base, const std::vector<uint32_t>& pcs,
                        std::vector<KillTarget>* out) {
  auto walk = [&] {
    out->clear();
    for (uint32_t pc : pcs) {
      KillTarget entry;
      entry.pc = pc;
      entry.pawn = guest::LoadU32(base, pc + dev_commands::layout::kControllerPawn);
      entry.who = dev_commands::DescribeTargets(base, kGameGlobals, {pc});
      entry.pawn_class = dev_commands::ClassName(base, kGameGlobals, entry.pawn);
      out->push_back(entry);
    }
  };
  return guest_call::Guarded(walk);
}

void RunKillRoute(const PPCContext& ctx, uint8_t* base, const KillTarget& target,
                  uint64_t name, std::vector<std::string>* killed,
                  std::vector<std::string>* skipped) {
  if (target.pawn == 0) {
    skipped->push_back(target.who + " has no pawn");
    return;
  }
  const uint32_t fn = guest_call::FindFunction(ctx, base, target.pawn, name);
  if (fn == 0) {
    skipped->push_back(target.who + ": " + target.pawn_class + " has no function Suicide");
    return;
  }
  if (!guest_call::ProcessEvent(ctx, base, target.pawn, fn)) {
    skipped->push_back(target.who + ": guest call failed");
    return;
  }
  killed->push_back(target.who);
}

VerbResult KillVerb(const PPCContext& ctx, uint8_t* base, uint32_t engine,
                    const dev_commands::Command& command) {
  const uint32_t world_info = GuardedWorldInfo(base);
  if (!IsHost(base, world_info)) return Fail("only the host decides damage");
  const std::optional<std::vector<uint32_t>> targets =
      GuardedTargets(base, engine, world_info, command.target);
  if (!targets.has_value()) return Fail("fault while reading guest objects");
  if (targets->empty()) return Fail(NoTargetText(command.target));
  std::vector<KillTarget> prepared;
  if (!GuardedKillTargets(base, *targets, &prepared))
    return Fail("fault while reading guest objects");
  const std::optional<uint64_t> name = guest_call::MakeName(ctx, base, "Suicide", false);
  if (!name.has_value()) return Fail("no name Suicide");
  std::vector<std::string> killed;
  std::vector<std::string> skipped;
  for (const KillTarget& target : prepared)
    RunKillRoute(ctx, base, target, *name, &killed, &skipped);
  if (killed.empty()) return Fail(JoinComma(skipped));
  std::string detail = "killed " + JoinComma(killed);
  if (!skipped.empty()) detail += "; skipped " + JoinComma(skipped);
  return Ok(detail);
}

void AppendFloatBe(float value, std::vector<uint8_t>* out) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  out->push_back(uint8_t(bits >> 24));
  out->push_back(uint8_t(bits >> 16));
  out->push_back(uint8_t(bits >> 8));
  out->push_back(uint8_t(bits));
}

void AppendU32Be(uint32_t value, std::vector<uint8_t>* out) {
  out->push_back(uint8_t(value >> 24));
  out->push_back(uint8_t(value >> 16));
  out->push_back(uint8_t(value >> 8));
  out->push_back(uint8_t(value));
}

void RunPawnEventRoute(const PPCContext& ctx, uint8_t* base, const KillTarget& target,
                       uint64_t name, const std::string& function,
                       const std::vector<uint8_t>& parms, std::vector<std::string>* done,
                       std::vector<std::string>* skipped) {
  if (target.pawn == 0) {
    skipped->push_back(target.who + " has no pawn");
    return;
  }
  const uint32_t fn = guest_call::FindFunction(ctx, base, target.pawn, name);
  if (fn == 0) {
    skipped->push_back(target.who + ": " + target.pawn_class + " has no function " + function);
    return;
  }
  if (!guest_call::ProcessEventWithParms(ctx, base, target.pawn, fn, parms, nullptr, 0)) {
    skipped->push_back(target.who + ": guest call failed");
    return;
  }
  done->push_back(target.who);
}

VerbResult PawnEventVerb(const PPCContext& ctx, uint8_t* base, uint32_t engine,
                         const dev_commands::Command& command, const std::string& function,
                         const std::vector<uint8_t>& parms, const std::string& done_text) {
  const uint32_t world_info = GuardedWorldInfo(base);
  if (!IsHost(base, world_info)) return Fail("only the host decides damage");
  const std::optional<std::vector<uint32_t>> targets =
      GuardedTargets(base, engine, world_info, command.target);
  if (!targets.has_value()) return Fail("fault while reading guest objects");
  if (targets->empty()) return Fail(NoTargetText(command.target));
  std::vector<KillTarget> prepared;
  if (!GuardedKillTargets(base, *targets, &prepared))
    return Fail("fault while reading guest objects");
  const std::optional<uint64_t> name = guest_call::MakeName(ctx, base, function, false);
  if (!name.has_value()) return Fail("no name " + function);
  std::vector<std::string> done;
  std::vector<std::string> skipped;
  for (const KillTarget& target : prepared)
    RunPawnEventRoute(ctx, base, target, *name, function, parms, &done, &skipped);
  if (done.empty()) return Fail(JoinComma(skipped));
  std::string detail = done_text + " " + JoinComma(done);
  if (!skipped.empty()) detail += "; skipped " + JoinComma(skipped);
  return Ok(detail);
}

VerbResult KnockdownVerb(const PPCContext& ctx, uint8_t* base, uint32_t engine,
                         const dev_commands::Command& command) {
  // With a speed the push also lifts the pawn by half of it, like a heavy blast (a Mauler's flail).
  const bool lift = !command.args.empty();
  const float speed = lift ? float(std::stoul(command.args[0])) : 192.0f;
  std::vector<uint8_t> parms;
  AppendFloatBe(speed, &parms);
  AppendFloatBe(0.0f, &parms);
  AppendFloatBe(lift ? speed / 2 : 0.0f, &parms);
  for (int i = 0; i < 3; ++i) AppendFloatBe(0.0f, &parms);
  return PawnEventVerb(ctx, base, engine, command, "Knockdown", parms, "knocked down");
}

// TakeDamage(Damage, InstigatedBy, HitLocation, Momentum, DamageType, HitInfo, DamageCauser):
// 4 + 4 + 12 + 12 + 4 + 28 (TraceHitInfo) + 4 bytes; all but Damage stay zero / None.
constexpr size_t kTakeDamageParmsBytes = 68;

VerbResult DamageVerb(const PPCContext& ctx, uint8_t* base, uint32_t engine,
                      const dev_commands::Command& command) {
  std::vector<uint8_t> parms;
  AppendU32Be(uint32_t(std::stoul(command.args[0])), &parms);
  parms.resize(kTakeDamageParmsBytes, 0);
  return PawnEventVerb(ctx, base, engine, command, "TakeDamage", parms,
                       "damaged (" + command.args[0] + ")");
}

// ServerDoSpecialMove(byte NewMove, bool bForceMove, Pawn InInteractionPawn,
// int InSpecialMoveFlags, byte SMToChain): forced, no interaction pawn, no flags, no chain.
VerbResult SpecialMoveVerb(const PPCContext& ctx, uint8_t* base, uint32_t engine,
                           const dev_commands::Command& command) {
  std::vector<uint8_t> parms;
  AppendU32Be(uint32_t(std::stoul(command.args[0])) << 24, &parms);
  AppendU32Be(1, &parms);
  AppendU32Be(0, &parms);
  AppendU32Be(0, &parms);
  AppendU32Be(0, &parms);
  return PawnEventVerb(ctx, base, engine, command, "ServerDoSpecialMove", parms,
                       "special move " + command.args[0] + " on");
}

struct CallTarget {
  uint32_t object = 0;
  std::string who;
  std::string class_name;
};

bool GuardedCallTargets(const uint8_t* base, const std::vector<uint32_t>& pcs, bool on_pawn,
                        std::vector<CallTarget>* out) {
  auto walk = [&] {
    out->clear();
    for (uint32_t pc : pcs) {
      CallTarget entry;
      entry.object = on_pawn
                         ? guest::LoadU32(base, pc + dev_commands::layout::kControllerPawn)
                         : pc;
      entry.who = dev_commands::DescribeTargets(base, kGameGlobals, {pc});
      entry.class_name = dev_commands::ClassName(base, kGameGlobals, entry.object);
      out->push_back(entry);
    }
  };
  return guest_call::Guarded(walk);
}

std::optional<std::string> RunCallRoute(const PPCContext& ctx, uint8_t* base,
                                        const CallTarget& target, uint64_t name,
                                        const std::string& function) {
  if (target.object == 0) return target.who + " has no pawn";
  const uint32_t fn = guest_call::FindFunction(ctx, base, target.object, name);
  if (fn == 0) return target.class_name + " has no function " + function;
  if (!guest_call::ProcessEvent(ctx, base, target.object, fn))
    return "guest call failed for " + target.who;
  return std::nullopt;
}

VerbResult CallVerb(const PPCContext& ctx, uint8_t* base, uint32_t engine,
                    const dev_commands::Command& command, bool on_pawn) {
  const uint32_t world_info = GuardedWorldInfo(base);
  if (!IsHost(base, world_info) &&
      command.target.kind != dev_commands::TargetKind::kLocal)
    return Fail("remote targets need the host");
  const std::optional<std::vector<uint32_t>> targets =
      GuardedTargets(base, engine, world_info, command.target);
  if (!targets.has_value()) return Fail("fault while reading guest objects");
  if (targets->empty()) return Fail(NoTargetText(command.target));
  if (command.args.empty()) return Fail("call needs a function name");
  const std::string& function = command.args[0];
  std::vector<CallTarget> prepared;
  if (!GuardedCallTargets(base, *targets, on_pawn, &prepared))
    return Fail("fault while reading guest objects");
  const std::optional<uint64_t> name = guest_call::MakeName(ctx, base, function, false);
  if (!name.has_value()) return Fail("no name " + function);
  for (const CallTarget& target : prepared) {
    const std::optional<std::string> error = RunCallRoute(ctx, base, target, *name, function);
    if (error.has_value()) return Fail(*error);
  }
  std::string who;
  if (!GuardedDescribe(base, *targets, &who)) return Fail("fault while reading guest objects");
  return Ok(function + " on " + who + " (parameters zero)");
}

VerbResult MarkVerb(const dev_commands::Command& command) {
  const std::string text = command.args.empty() ? "" : command.args[0];
  REXLOG_INFO("DEVCMD mark {}", text);
  return Ok("mark " + text);
}

}  // namespace

VerbResult Ok(std::string detail) { return VerbResult{true, detail}; }

VerbResult Fail(std::string detail) { return VerbResult{false, detail}; }

bool IsHost(const uint8_t* base, uint32_t world_info) {
  if (world_info == 0) return false;
  return guest::LoadU8(base, world_info + dev_commands::layout::kWorldInfoNetMode) !=
         dev_commands::layout::kNetModeClient;
}

uint32_t GuardedWorldInfo(const uint8_t* base) {
  uint32_t world_info = 0;
  auto walk = [&] { world_info = dev_commands::WorldInfoOf(base, kGameGlobals); };
  if (!guest_call::Guarded(walk)) return 0;
  return world_info;
}

std::optional<std::vector<uint32_t>> GuardedTargets(const uint8_t* base, uint32_t engine,
                                                    uint32_t world_info,
                                                    const dev_commands::Target& target) {
  std::optional<std::vector<uint32_t>> out;
  auto walk = [&] {
    out = dev_commands::ResolveTargets(base, kGameGlobals, engine, world_info, target);
  };
  if (!guest_call::Guarded(walk)) return std::nullopt;
  return out;
}

VerbResult RunHostVerb(const PPCContext& ctx, uint8_t* base, uint32_t engine,
                       const dev_commands::Command& command) {
  const std::string& verb = command.text;
  if (verb == "list") return ListVerb(base);
  if (verb == "selftest") return SelfTestVerb(ctx, base, engine);
  if (verb == "god") return GodVerb(base, engine, command);
  if (verb == "kill") return KillVerb(ctx, base, engine, command);
  if (verb == "knockdown") return KnockdownVerb(ctx, base, engine, command);
  if (verb == "damage") return DamageVerb(ctx, base, engine, command);
  if (verb == "specialmove") return SpecialMoveVerb(ctx, base, engine, command);
  if (verb == "call") return CallVerb(ctx, base, engine, command, false);
  if (verb == "callpawn") return CallVerb(ctx, base, engine, command, true);
  if (verb == "mark") return MarkVerb(command);
  if (verb == "listmapchanges") return ListMapChanges(ctx, base, engine, command);
  if (verb == "mapchange") return MapChange(ctx, base, engine, command);
  if (verb == "mapcommit") return MapCommit(ctx, base, engine, command);
  return Fail("unknown verb");
}

}  // namespace rex::glue::dev_verbs
