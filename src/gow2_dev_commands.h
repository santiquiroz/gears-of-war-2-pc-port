// gow2 - ReXGlue Recompiled Project
//
// Portable core for dev test commands: line parsing, ack files and guest walkers.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rex::glue::dev_commands {

enum class TargetKind { kLocal, kAll, kName, kIndex };

struct Target {
  TargetKind kind = TargetKind::kLocal;
  std::string name;
  int index = 0;
  bool operator==(const Target&) const = default;
};

enum class CommandKind { kSkip, kConsole, kHostVerb, kError };

struct Command {
  CommandKind kind = CommandKind::kSkip;
  Target target;
  std::string text;
  std::vector<std::string> args;
  std::optional<Target> instigator;  // !damage <target> <amount> <from>
};

Command ParseCommandLine(std::string_view line);

size_t CountCompleteLines(std::string_view content);

struct FileLines {
  std::vector<std::string> lines;
  size_t consumed = 0;
  bool truncated = false;
};

FileLines TakeCompleteLines(std::string_view content, size_t consumed);

std::string FormatAck(size_t line_number, bool ok, std::string_view detail);

std::vector<uint8_t> EncodeTcharZ(std::string_view ascii);

std::string DecodeTchars(const uint8_t* base, uint32_t data, int32_t num, uint32_t max_chars);

constexpr uint64_t PackName(uint32_t index, uint32_t number) {
  return (uint64_t(index) << 32) | number;
}

struct Globals {
  uint32_t names;
  uint32_t objects;
  uint32_t world;
};

constexpr Globals kGameGlobals{0x83136834u, 0x83136860u, 0x8312380Cu};

namespace layout {
constexpr uint32_t kObjStateFrame = 24, kStateNode = 40, kObjOuter = 40, kObjName = 44,
                   kObjNumber = 48, kObjClass = 52, kSuperField = 60, kWorldLevel = 80,
                   kLevelActors = 60, kWorldInfoGri = 792, kWorldInfoNetMode = 796,
                   kWorldInfoControllers = 864, kControllerPawn = 444, kControllerPri = 448,
                   kControllerNext = 456, kControllerFlags = 460, kGodModeMask = 0x40000000,
                   kPcPlayer = 836, kPriPlayerName = 464, kPawnHealth = 692,
                   kEnginePlayersData = 812, kEnginePlayersNum = 816,
                   kEnginePendingLevels = 1252, kPlayerActor = 64, kGriDropDetailWord = 604,
                   kDropDetailMask = 0x00200000, kPrepareMainLevel = 248,
                   kPrepareSecondaryNum = 260, kPrepareFlags = 268, kNetModeClient = 3;
}

std::string NameText(const uint8_t* base, const Globals& g, uint32_t index, uint32_t number);
std::string ObjectName(const uint8_t* base, const Globals& g, uint32_t object);
std::string ClassName(const uint8_t* base, const Globals& g, uint32_t object);
std::string ObjectPath(const uint8_t* base, const Globals& g, uint32_t object);
bool ClassChainHas(const uint8_t* base, const Globals& g, uint32_t object,
                   std::string_view class_name);
std::string StateName(const uint8_t* base, const Globals& g, uint32_t object);
uint32_t WorldInfoOf(const uint8_t* base, const Globals& g);
std::vector<uint32_t> ControllerList(const uint8_t* base, uint32_t world_info);
std::vector<uint32_t> PlayerControllers(const uint8_t* base, const Globals& g, uint32_t world_info);
std::string PlayerName(const uint8_t* base, uint32_t controller);
uint32_t LocalPlayerController(const uint8_t* base, const Globals& g, uint32_t engine);
std::vector<uint32_t> ResolveTargets(const uint8_t* base, const Globals& g, uint32_t engine,
                                     uint32_t world_info, const Target& t);
std::string DescribeTargets(const uint8_t* base, const Globals& g,
                            const std::vector<uint32_t>& pcs);
constexpr uint32_t WithGodMode(uint32_t flags, bool on) {
  return on ? (flags | layout::kGodModeMask) : (flags & ~layout::kGodModeMask);
}
std::vector<uint32_t> ObjectsOfClass(const uint8_t* base, const Globals& g,
                                     std::string_view class_name);
bool VtableHasFunction(const uint8_t* base, uint32_t object, uint32_t function, uint32_t slots);
std::string DescribeController(const uint8_t* base, const Globals& g, uint32_t controller);
std::string TargetToken(const Target& t);

}  // namespace rex::glue::dev_commands
