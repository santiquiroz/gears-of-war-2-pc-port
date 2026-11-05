// gow2 - ReXGlue Recompiled Project
//
// Dev test command core: parses command lines and walks guest objects for targets.
#include "gow2_dev_commands.h"

#include <string>
#include <string_view>
#include <vector>

#include "gow2_guest.h"

namespace rex::glue::dev_commands {
namespace {

using rex::glue::guest::LoadU8;
using rex::glue::guest::LoadU16;
using rex::glue::guest::LoadU32;

bool IsBlank(char c) { return c == ' ' || c == '\t'; }

std::string_view TrimSpaces(std::string_view s) {
  size_t begin = 0;
  while (begin < s.size() && IsBlank(s[begin])) ++begin;
  size_t end = s.size();
  while (end > begin && IsBlank(s[end - 1])) --end;
  return s.substr(begin, end - begin);
}

char LowerChar(char c) { return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c; }

std::string LowerAscii(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = LowerChar(c);
  return out;
}

bool EqualsIgnoreAsciiCase(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (LowerChar(a[i]) != LowerChar(b[i])) return false;
  return true;
}

bool HasBadByte(std::string_view s) {
  for (char ch : s) {
    const unsigned char c = static_cast<unsigned char>(ch);
    if (c >= 0x80) return true;
    if (c < 0x20 && c != '\t') return true;
  }
  return false;
}

std::vector<std::string> SplitBlanks(std::string_view s) {
  std::vector<std::string> out;
  size_t i = 0;
  while (i < s.size()) {
    while (i < s.size() && IsBlank(s[i])) ++i;
    if (i >= s.size()) break;
    size_t end = i;
    while (end < s.size() && !IsBlank(s[end])) ++end;
    out.push_back(std::string(s.substr(i, end - i)));
    i = end;
  }
  return out;
}

Command MakeError(const std::string& message) {
  Command out;
  out.kind = CommandKind::kError;
  out.text = message;
  return out;
}

bool ParseIndexToken(std::string_view token, int* out) {
  if (token.size() < 2 || token[0] != '#') return false;
  int value = 0;
  for (size_t i = 1; i < token.size(); ++i) {
    if (token[i] < '0' || token[i] > '9') return false;
    value = value * 10 + (token[i] - '0');
    if (value > 64) return false;
  }
  if (value < 1) return false;
  *out = value;
  return true;
}

Target ParseTargetToken(std::string_view token) {
  const std::string lower = LowerAscii(token);
  if (lower == "local" || lower == "host") return Target{};
  if (lower == "all") {
    Target out;
    out.kind = TargetKind::kAll;
    return out;
  }
  int index = 0;
  if (ParseIndexToken(token, &index)) {
    Target out;
    out.kind = TargetKind::kIndex;
    out.index = index;
    return out;
  }
  Target out;
  out.kind = TargetKind::kName;
  out.name = std::string(token);
  return out;
}

bool IsKnownVerb(std::string_view verb) {
  return verb == "list" || verb == "selftest" || verb == "listmapchanges" || verb == "god" ||
         verb == "kill" || verb == "knockdown" || verb == "damage" || verb == "specialmove" ||
         verb == "call" || verb == "callpawn" ||
         verb == "mapchange" || verb == "mapcommit" || verb == "mark";
}

bool ArityOk(std::string_view verb, size_t nargs) {
  if (verb == "mark") return true;
  if (verb == "mapchange" || verb == "mapcommit") return nargs <= 1;
  if (verb == "kill" || verb == "knockdown") return nargs == 1;
  if (verb == "god" || verb == "call" || verb == "callpawn" || verb == "damage" ||
      verb == "specialmove")
    return nargs == 2;
  return nargs == 0;
}

std::string_view ArityText(std::string_view verb) {
  if (verb == "mapchange" || verb == "mapcommit") return "0 or 1";
  if (verb == "kill" || verb == "knockdown") return "1";
  if (verb == "god" || verb == "call" || verb == "callpawn" || verb == "damage" ||
      verb == "specialmove")
    return "2";
  return "0";
}

bool IsFunctionNameChar(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

bool IsValidFunctionName(std::string_view name) {
  if (name.empty() || name.size() > 64) return false;
  for (char c : name)
    if (!IsFunctionNameChar(c)) return false;
  return true;
}

Command ParseMark(std::string_view trimmed, const std::vector<std::string>& tokens) {
  Command out;
  out.kind = CommandKind::kHostVerb;
  out.text = "mark";
  size_t at = trimmed.find(tokens[0], 1) + tokens[0].size();
  while (at < trimmed.size() && IsBlank(trimmed[at])) ++at;
  if (at < trimmed.size()) out.args.push_back(std::string(trimmed.substr(at)));
  return out;
}

Command ParseMapChange(const std::vector<std::string>& tokens) {
  Command out;
  out.kind = CommandKind::kHostVerb;
  out.text = "mapchange";
  if (tokens.size() > 1) out.args.push_back(tokens[1]);
  return out;
}

Command ParseMapCommit(const std::vector<std::string>& tokens) {
  if (tokens.size() > 1 && LowerAscii(tokens[1]) != "force")
    return MakeError("!mapcommit takes nothing or force");
  Command out;
  out.kind = CommandKind::kHostVerb;
  out.text = "mapcommit";
  if (tokens.size() > 1) out.args.push_back("force");
  return out;
}

Command ParseGod(const std::vector<std::string>& tokens) {
  const std::string mode = LowerAscii(tokens[2]);
  if (mode != "on" && mode != "off")
    return MakeError("!god takes on or off, not '" + tokens[2] + "'");
  Command out;
  out.kind = CommandKind::kHostVerb;
  out.text = "god";
  out.target = ParseTargetToken(tokens[1]);
  out.args.push_back(mode);
  return out;
}

Command ParseKill(const std::vector<std::string>& tokens) {
  Command out;
  out.kind = CommandKind::kHostVerb;
  out.text = "kill";
  out.target = ParseTargetToken(tokens[1]);
  return out;
}

Command ParseKnockdown(const std::vector<std::string>& tokens) {
  Command out;
  out.kind = CommandKind::kHostVerb;
  out.text = "knockdown";
  out.target = ParseTargetToken(tokens[1]);
  return out;
}

bool IsDamageAmount(std::string_view text) {
  if (text.empty() || text.size() > 6 || text[0] == '0') return false;
  for (char c : text)
    if (c < '0' || c > '9') return false;
  return true;
}

Command ParseDamage(const std::vector<std::string>& tokens) {
  if (!IsDamageAmount(tokens[2]))
    return MakeError("!damage takes an amount from 1 to 999999, not '" + tokens[2] + "'");
  Command out;
  out.kind = CommandKind::kHostVerb;
  out.text = "damage";
  out.target = ParseTargetToken(tokens[1]);
  out.args.push_back(tokens[2]);
  return out;
}

bool IsSpecialMoveIndex(std::string_view text) {
  if (text.empty() || text.size() > 3 || text[0] == '0') return false;
  for (char c : text)
    if (c < '0' || c > '9') return false;
  return std::stoi(std::string(text)) <= 255;
}

Command ParseSpecialMove(const std::vector<std::string>& tokens) {
  if (!IsSpecialMoveIndex(tokens[2]))
    return MakeError("!specialmove takes a move from 1 to 255, not '" + tokens[2] + "'");
  Command out;
  out.kind = CommandKind::kHostVerb;
  out.text = "specialmove";
  out.target = ParseTargetToken(tokens[1]);
  out.args.push_back(tokens[2]);
  return out;
}

Command ParseCall(std::string_view verb, const std::vector<std::string>& tokens) {
  if (!IsValidFunctionName(tokens[2]))
    return MakeError("bad function name '" + tokens[2] + "'");
  Command out;
  out.kind = CommandKind::kHostVerb;
  out.text = std::string(verb);
  out.target = ParseTargetToken(tokens[1]);
  out.args.push_back(tokens[2]);
  return out;
}

Command ParseHostVerb(std::string_view trimmed) {
  const std::vector<std::string> tokens = SplitBlanks(trimmed.substr(1));
  const std::string verb = tokens.empty() ? std::string() : LowerAscii(tokens[0]);
  if (!IsKnownVerb(verb)) return MakeError("unknown verb !" + verb);
  if (!ArityOk(verb, tokens.size() - 1))
    return MakeError("!" + verb + " takes " + std::string(ArityText(verb)) + " argument(s)");
  if (verb == "mark") return ParseMark(trimmed, tokens);
  if (verb == "mapchange") return ParseMapChange(tokens);
  if (verb == "mapcommit") return ParseMapCommit(tokens);
  if (verb == "god") return ParseGod(tokens);
  if (verb == "kill") return ParseKill(tokens);
  if (verb == "knockdown") return ParseKnockdown(tokens);
  if (verb == "damage") return ParseDamage(tokens);
  if (verb == "specialmove") return ParseSpecialMove(tokens);
  if (verb == "call" || verb == "callpawn") return ParseCall(verb, tokens);
  Command out;
  out.kind = CommandKind::kHostVerb;
  out.text = verb;
  return out;
}

Command ParseAtCommand(std::string_view trimmed) {
  size_t end = 1;
  while (end < trimmed.size() && !IsBlank(trimmed[end])) ++end;
  const std::string_view token = trimmed.substr(1, end - 1);
  const std::string_view rest = TrimSpaces(trimmed.substr(end));
  if (rest.empty()) return MakeError("no command after @" + std::string(token));
  Command out;
  out.kind = CommandKind::kConsole;
  out.target = ParseTargetToken(token);
  out.text = std::string(rest);
  return out;
}

bool ContainsU32(const std::vector<uint32_t>& list, uint32_t value) {
  for (uint32_t entry : list)
    if (entry == value) return true;
  return false;
}

}  // namespace

Command ParseCommandLine(std::string_view line) {
  if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
  const std::string_view trimmed = TrimSpaces(line);
  if (trimmed.empty() || trimmed.front() == '#') return Command{};
  if (HasBadByte(trimmed)) return MakeError("non-ASCII or control character");
  if (trimmed.size() > 400) return MakeError("longer than 400 characters");
  if (trimmed.front() == '!') return ParseHostVerb(trimmed);
  if (trimmed.front() == '@') return ParseAtCommand(trimmed);
  Command out;
  out.kind = CommandKind::kConsole;
  out.text = std::string(trimmed);
  return out;
}

size_t CountCompleteLines(std::string_view content) {
  size_t count = 0;
  for (char c : content)
    if (c == '\n') ++count;
  return count;
}

FileLines TakeCompleteLines(std::string_view content, size_t consumed) {
  std::vector<std::string> all;
  size_t begin = 0;
  while (true) {
    const size_t newline = content.find('\n', begin);
    if (newline == std::string_view::npos) break;
    std::string_view segment = content.substr(begin, newline - begin);
    if (!segment.empty() && segment.back() == '\r') segment.remove_suffix(1);
    all.push_back(std::string(segment));
    begin = newline + 1;
  }
  FileLines out;
  if (all.size() < consumed) {
    out.truncated = true;
    return out;
  }
  out.lines.assign(all.begin() + ptrdiff_t(consumed), all.end());
  out.consumed = all.size();
  return out;
}

std::string FormatAck(size_t line_number, bool ok, std::string_view detail) {
  std::string clean(detail);
  for (char& c : clean)
    if (c == '\t' || c == '\r' || c == '\n') c = ' ';
  return std::to_string(line_number) + "\t" + (ok ? "ok" : "error") + "\t" + clean + "\n";
}

std::vector<uint8_t> EncodeTcharZ(std::string_view ascii) {
  std::vector<uint8_t> out;
  out.reserve(2 * ascii.size() + 2);
  for (char c : ascii) {
    out.push_back(0);
    out.push_back(static_cast<uint8_t>(c));
  }
  out.push_back(0);
  out.push_back(0);
  return out;
}

std::string DecodeTchars(const uint8_t* base, uint32_t data, int32_t num, uint32_t max_chars) {
  if (data == 0 || num <= 0) return "";
  uint32_t want = uint32_t(num - 1);
  if (want > max_chars) want = max_chars;
  std::string out;
  for (uint32_t i = 0; i < want; ++i) {
    const uint32_t c = LoadU16(base, data + 2 * i);
    if (c == 0) break;
    out.push_back(c < 128 ? char(c) : '?');
  }
  return out;
}

std::string NameText(const uint8_t* base, const Globals& g, uint32_t index, uint32_t number) {
  const uint32_t names = LoadU32(base, g.names);
  const uint32_t count = LoadU32(base, g.names + 4);
  if (names == 0 || index >= count) return "?";
  const uint32_t entry = LoadU32(base, names + 4 * index);
  if (entry == 0) return "?";
  const bool wide = (LoadU32(base, entry) & 1u) != 0;
  std::string out;
  for (uint32_t i = 0; i < 128; ++i) {
    const uint32_t at = entry + 8 + i * (wide ? 2u : 1u);
    const uint32_t c = wide ? LoadU16(base, at) : LoadU8(base, at);
    if (c == 0) break;
    out.push_back(c < 128 ? char(c) : '?');
  }
  if (number != 0) {
    out += "_";
    out += std::to_string(number - 1);
  }
  return out;
}

std::string ObjectName(const uint8_t* base, const Globals& g, uint32_t object) {
  if (object == 0) return "None";
  return NameText(base, g, LoadU32(base, object + layout::kObjName),
                  LoadU32(base, object + layout::kObjNumber));
}

std::string ClassName(const uint8_t* base, const Globals& g, uint32_t object) {
  if (object == 0) return "None";
  const uint32_t cls = LoadU32(base, object + layout::kObjClass);
  if (cls == 0) return "None";
  return ObjectName(base, g, cls);
}

std::string ObjectPath(const uint8_t* base, const Globals& g, uint32_t object) {
  if (object == 0) return "None";
  std::string path = ObjectName(base, g, object);
  uint32_t outer = LoadU32(base, object + layout::kObjOuter);
  for (int depth = 0; outer != 0 && depth < 8; ++depth) {
    path = ObjectName(base, g, outer) + "." + path;
    outer = LoadU32(base, outer + layout::kObjOuter);
  }
  return path;
}

bool ClassChainHas(const uint8_t* base, const Globals& g, uint32_t object,
                   std::string_view class_name) {
  if (object == 0) return false;
  uint32_t cls = LoadU32(base, object + layout::kObjClass);
  for (int i = 0; i < 32; ++i) {
    if (cls == 0) return false;
    if (EqualsIgnoreAsciiCase(ObjectName(base, g, cls), class_name)) return true;
    cls = LoadU32(base, cls + layout::kSuperField);
  }
  return false;
}

std::string StateName(const uint8_t* base, const Globals& g, uint32_t object) {
  if (object == 0) return "None";
  const uint32_t frame = LoadU32(base, object + layout::kObjStateFrame);
  if (frame == 0) return "None";
  const uint32_t node = LoadU32(base, frame + layout::kStateNode);
  if (node == 0) return "None";
  return ObjectName(base, g, node);
}

uint32_t WorldInfoOf(const uint8_t* base, const Globals& g) {
  const uint32_t world = LoadU32(base, g.world);
  if (world == 0) return 0;
  const uint32_t level = LoadU32(base, world + layout::kWorldLevel);
  if (level == 0) return 0;
  const uint32_t actors = LoadU32(base, level + layout::kLevelActors);
  if (actors == 0) return 0;
  return LoadU32(base, actors);
}

std::vector<uint32_t> ControllerList(const uint8_t* base, uint32_t world_info) {
  std::vector<uint32_t> out;
  if (world_info == 0) return out;
  uint32_t next = LoadU32(base, world_info + layout::kWorldInfoControllers);
  while (next != 0) {
    if (ContainsU32(out, next)) break;
    if (out.size() >= 256) break;
    out.push_back(next);
    next = LoadU32(base, next + layout::kControllerNext);
  }
  return out;
}

std::vector<uint32_t> PlayerControllers(const uint8_t* base, const Globals& g,
                                       uint32_t world_info) {
  std::vector<uint32_t> out;
  for (uint32_t controller : ControllerList(base, world_info))
    if (ClassChainHas(base, g, controller, "PlayerController")) out.push_back(controller);
  return out;
}

std::string PlayerName(const uint8_t* base, uint32_t controller) {
  if (controller == 0) return "";
  const uint32_t pri = LoadU32(base, controller + layout::kControllerPri);
  if (pri == 0) return "";
  return DecodeTchars(base, LoadU32(base, pri + layout::kPriPlayerName),
                      int32_t(LoadU32(base, pri + layout::kPriPlayerName + 4)), 64);
}

uint32_t LocalPlayerController(const uint8_t* base, const Globals& g, uint32_t engine) {
  if (engine == 0) return 0;
  const uint32_t num = LoadU32(base, engine + layout::kEnginePlayersNum);
  if (num < 1 || num > 4) return 0;
  const uint32_t data = LoadU32(base, engine + layout::kEnginePlayersData);
  if (data == 0) return 0;
  const uint32_t player = LoadU32(base, data);
  if (player == 0) return 0;
  const uint32_t pc = LoadU32(base, player + layout::kPlayerActor);
  if (pc == 0) return 0;
  if (!ClassChainHas(base, g, pc, "PlayerController")) return 0;
  if (LoadU32(base, pc + layout::kPcPlayer) != player) return 0;
  return pc;
}

std::vector<uint32_t> ResolveTargets(const uint8_t* base, const Globals& g, uint32_t engine,
                                     uint32_t world_info, const Target& t) {
  if (t.kind == TargetKind::kLocal) {
    const uint32_t pc = LocalPlayerController(base, g, engine);
    if (pc == 0) return {};
    return {pc};
  }
  const std::vector<uint32_t> pcs = PlayerControllers(base, g, world_info);
  if (t.kind == TargetKind::kAll) return pcs;
  if (t.kind == TargetKind::kIndex) {
    if (t.index < 1 || size_t(t.index) > pcs.size()) return {};
    return {pcs[size_t(t.index) - 1]};
  }
  std::vector<uint32_t> out;
  for (uint32_t pc : pcs)
    if (EqualsIgnoreAsciiCase(PlayerName(base, pc), t.name)) out.push_back(pc);
  return out;
}

std::string DescribeTargets(const uint8_t* base, const Globals& g,
                            const std::vector<uint32_t>& pcs) {
  std::string out;
  for (uint32_t pc : pcs) {
    if (!out.empty()) out += ", ";
    const std::string name = PlayerName(base, pc);
    if (name.empty()) {
      out += ObjectName(base, g, pc);
      continue;
    }
    out += name + " (" + ObjectName(base, g, pc) + ")";
  }
  return out;
}

std::vector<uint32_t> ObjectsOfClass(const uint8_t* base, const Globals& g,
                                     std::string_view class_name) {
  std::vector<uint32_t> out;
  const uint32_t objects = LoadU32(base, g.objects);
  const uint32_t count = LoadU32(base, g.objects + 4);
  if (objects == 0 || count > (1u << 22)) return out;
  for (uint32_t i = 0; i < count; ++i) {
    const uint32_t object = LoadU32(base, objects + 4 * i);
    if (object == 0) continue;
    if (!ClassChainHas(base, g, object, class_name)) continue;
    if (ObjectName(base, g, object).rfind("Default__", 0) == 0) continue;
    out.push_back(object);
  }
  return out;
}

bool VtableHasFunction(const uint8_t* base, uint32_t object, uint32_t function, uint32_t slots) {
  if (object == 0) return false;
  const uint32_t vtable = LoadU32(base, object);
  if (vtable == 0) return false;
  for (uint32_t i = 0; i < slots; ++i)
    if (LoadU32(base, vtable + 4 * i) == function) return true;
  return false;
}

std::string DescribeController(const uint8_t* base, const Globals& g, uint32_t controller) {
  const uint32_t pawn =
      controller == 0 ? 0 : LoadU32(base, controller + layout::kControllerPawn);
  const uint32_t flags =
      controller == 0 ? 0 : LoadU32(base, controller + layout::kControllerFlags);
  std::string out = ObjectName(base, g, controller);
  out += " class=";
  out += ClassName(base, g, controller);
  out += " pc=";
  out += ClassChainHas(base, g, controller, "PlayerController") ? "1" : "0";
  out += " player='";
  out += PlayerName(base, controller);
  out += "' pawn=";
  if (pawn == 0) {
    out += "None(None) health=-";
  } else {
    out += ObjectName(base, g, pawn);
    out += "(";
    out += ClassName(base, g, pawn);
    out += ") health=";
    out += std::to_string(int32_t(LoadU32(base, pawn + layout::kPawnHealth)));
  }
  out += " god=";
  out += (flags & layout::kGodModeMask) != 0 ? "1" : "0";
  out += " state=";
  out += StateName(base, g, controller);
  if (pawn != 0) out += " pawnstate=" + StateName(base, g, pawn);
  return out;
}

std::string TargetToken(const Target& t) {
  if (t.kind == TargetKind::kAll) return "all";
  if (t.kind == TargetKind::kIndex) return "#" + std::to_string(t.index);
  if (t.kind == TargetKind::kName) return t.name;
  return "local";
}

}  // namespace rex::glue::dev_commands
