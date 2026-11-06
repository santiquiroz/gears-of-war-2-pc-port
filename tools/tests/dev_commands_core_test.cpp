// Tests for the dev commands core (src/gow2_dev_commands.cpp): command parsing, ack
// files and guest walkers over synthetic big-endian guest memory.
#include "gow2_dev_commands.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "gow2_guest.h"

namespace {

using rex::glue::dev_commands::Command;
using rex::glue::dev_commands::CommandKind;
using rex::glue::dev_commands::FileLines;
using rex::glue::dev_commands::Globals;
using rex::glue::dev_commands::Target;
using rex::glue::dev_commands::TargetKind;
using rex::glue::guest::LoadU32;
using rex::glue::guest::StoreU8;
using rex::glue::guest::StoreU32;

int g_checks = 0;
int g_failures = 0;

void Check(bool ok, const char* what) {
  ++g_checks;
  if (ok) return;
  ++g_failures;
  std::printf("FAIL: %s\n", what);
}

std::vector<uint8_t> g_memory(8 << 20);
uint8_t* g_base = g_memory.data();
Globals g_g{0x1000, 0x1010, 0x1020};
uint32_t g_next = 0x10000;

uint32_t Alloc(uint32_t size) {
  g_next = (g_next + 15) & ~15u;
  const uint32_t at = g_next;
  g_next += (size + 15) & ~15u;
  return at;
}

uint32_t MakeNameEntry(std::string_view text, bool wide) {
  const uint32_t at = Alloc(uint32_t(8 + (text.size() + 1) * (wide ? 2 : 1)));
  StoreU32(g_base, at, wide ? 1 : 0);
  for (size_t i = 0; i < text.size(); ++i) {
    if (wide) StoreU8(g_base, at + 8 + uint32_t(2 * i), 0);
    StoreU8(g_base, at + 8 + uint32_t(i) * (wide ? 2 : 1) + (wide ? 1 : 0),
            static_cast<uint8_t>(text[i]));
  }
  return at;
}

std::vector<uint32_t> g_name_entries;

uint32_t AddName(std::string_view text, bool wide = false) {
  g_name_entries.push_back(MakeNameEntry(text, wide));
  return uint32_t(g_name_entries.size() - 1);
}

uint32_t MakeObject(uint32_t name, uint32_t number, uint32_t outer, uint32_t cls, uint32_t size) {
  const uint32_t at = Alloc(size);
  StoreU32(g_base, at + 40, outer);
  StoreU32(g_base, at + 44, name);
  StoreU32(g_base, at + 48, number);
  StoreU32(g_base, at + 52, cls);
  return at;
}

void StoreBytes(uint32_t at, const std::vector<uint8_t>& bytes) {
  for (size_t i = 0; i < bytes.size(); ++i) StoreU8(g_base, at + uint32_t(i), bytes[i]);
}

uint32_t StoreTcharString(std::string_view text) {
  const std::vector<uint8_t> bytes =
      rex::glue::dev_commands::EncodeTcharZ(text);
  const uint32_t at = Alloc(uint32_t(bytes.size()));
  StoreBytes(at, bytes);
  return at;
}

uint32_t g_idx_none = 0;
uint32_t g_idx_pkg = 0;
uint32_t g_idx_class = 0;
uint32_t g_idx_gear_pc_sp = 0;
uint32_t g_idx_gear_ai_dom = 0;
uint32_t g_idx_pawn = 0;
uint32_t g_idx_walking = 0;
uint32_t g_idx_wide = 0;
uint32_t g_cls_class = 0;
uint32_t g_cls_gear_pc_sp = 0;
uint32_t g_cls_gear_ai_dom = 0;
uint32_t g_default_obj = 0;
uint32_t g_pc1 = 0;
uint32_t g_pc3 = 0;
uint32_t g_ai = 0;
uint32_t g_world = 0;
uint32_t g_world_info = 0;
uint32_t g_engine = 0;

uint32_t MakeClass(uint32_t name, uint32_t pkg, uint32_t cls_class, uint32_t super) {
  const uint32_t cls = MakeObject(name, 0, pkg, cls_class, 0x80);
  StoreU32(g_base, cls + 60, super);
  return cls;
}

uint32_t MakePri(std::string_view player_name) {
  const uint32_t pri = MakeObject(g_idx_none, 0, 0, 0, 0x200);
  StoreU32(g_base, pri + 464, StoreTcharString(player_name));
  StoreU32(g_base, pri + 468, uint32_t(player_name.size() + 1));
  return pri;
}

uint32_t MakePawn(uint32_t cls_pawn, uint32_t number, int32_t health) {
  const uint32_t pawn = MakeObject(g_idx_pawn, number, 0, cls_pawn, 0x300);
  StoreU32(g_base, pawn + 692, uint32_t(health));
  return pawn;
}

void SetupFixture() {
  g_idx_none = AddName("None");
  g_idx_pkg = AddName("Pkg");
  g_idx_class = AddName("Class");
  const uint32_t idx_controller = AddName("Controller");
  const uint32_t idx_pc = AddName("PlayerController");
  const uint32_t idx_gpc = AddName("GamePlayerController");
  const uint32_t idx_gear_pc = AddName("GearPC");
  g_idx_gear_pc_sp = AddName("GearPC_SP");
  const uint32_t idx_ai_controller = AddName("AIController");
  const uint32_t idx_gear_ai = AddName("GearAI");
  g_idx_gear_ai_dom = AddName("GearAI_Dom");
  g_idx_pawn = AddName("GearPawn_COGBenjaminCarmine");
  const uint32_t idx_default = AddName("Default__GearPC_SP");
  g_idx_walking = AddName("PlayerWalking");
  g_idx_wide = AddName("WideName", true);

  const uint32_t names_data = Alloc(uint32_t(4 * g_name_entries.size()));
  for (size_t i = 0; i < g_name_entries.size(); ++i)
    StoreU32(g_base, names_data + uint32_t(4 * i), g_name_entries[i]);
  StoreU32(g_base, g_g.names, names_data);
  StoreU32(g_base, g_g.names + 4, uint32_t(g_name_entries.size()));

  g_cls_class = MakeObject(g_idx_class, 0, 0, 0, 0x80);
  StoreU32(g_base, g_cls_class + 52, g_cls_class);
  const uint32_t pkg = MakeObject(g_idx_pkg, 0, 0, g_cls_class, 0x80);
  const uint32_t cls_controller = MakeClass(idx_controller, pkg, g_cls_class, 0);
  const uint32_t cls_pc = MakeClass(idx_pc, pkg, g_cls_class, cls_controller);
  const uint32_t cls_gpc = MakeClass(idx_gpc, pkg, g_cls_class, cls_pc);
  const uint32_t cls_gear_pc = MakeClass(idx_gear_pc, pkg, g_cls_class, cls_gpc);
  g_cls_gear_pc_sp = MakeClass(g_idx_gear_pc_sp, pkg, g_cls_class, cls_gear_pc);
  const uint32_t cls_ai = MakeClass(idx_ai_controller, pkg, g_cls_class, cls_controller);
  const uint32_t cls_gear_ai = MakeClass(idx_gear_ai, pkg, g_cls_class, cls_ai);
  g_cls_gear_ai_dom = MakeClass(g_idx_gear_ai_dom, pkg, g_cls_class, cls_gear_ai);
  const uint32_t cls_pawn = MakeClass(g_idx_pawn, pkg, g_cls_class, 0);

  g_default_obj = MakeObject(idx_default, 0, 0, g_cls_gear_pc_sp, 0x80);
  const uint32_t pri1 = MakePri("P1");
  const uint32_t pri3 = MakePri("P3");
  const uint32_t pawn1 = MakePawn(cls_pawn, 1, 100);
  const uint32_t pawn3 = MakePawn(cls_pawn, 2, 100);
  const uint32_t state = MakeObject(g_idx_walking, 0, 0, 0, 0x80);
  const uint32_t frame = Alloc(0x80);
  StoreU32(g_base, frame + 40, state);

  g_pc1 = MakeObject(g_idx_gear_pc_sp, 1, 0, g_cls_gear_pc_sp, 0x500);
  g_pc3 = MakeObject(g_idx_gear_pc_sp, 3, 0, g_cls_gear_pc_sp, 0x500);
  g_ai = MakeObject(g_idx_gear_ai_dom, 0, 0, g_cls_gear_ai_dom, 0x500);
  StoreU32(g_base, g_pc1 + 444, pawn1);
  StoreU32(g_base, g_pc1 + 448, pri1);
  StoreU32(g_base, g_pc1 + 456, g_pc3);
  StoreU32(g_base, g_pc3 + 24, frame);
  StoreU32(g_base, g_pc3 + 444, pawn3);
  StoreU32(g_base, g_pc3 + 448, pri3);
  StoreU32(g_base, g_pc3 + 456, g_ai);

  g_world = Alloc(0x100);
  const uint32_t level = Alloc(0x100);
  const uint32_t actors = Alloc(16);
  g_world_info = Alloc(0x400);
  StoreU32(g_base, g_g.world, g_world);
  StoreU32(g_base, g_world + 80, level);
  StoreU32(g_base, level + 60, actors);
  StoreU32(g_base, actors, g_world_info);
  StoreU32(g_base, g_world_info + 864, g_pc1);

  g_engine = Alloc(0x600);
  const uint32_t players = Alloc(16);
  const uint32_t local_player = Alloc(0x100);
  StoreU32(g_base, g_engine + 812, players);
  StoreU32(g_base, g_engine + 816, 1);
  StoreU32(g_base, players, local_player);
  StoreU32(g_base, local_player + 64, g_pc1);
  StoreU32(g_base, g_pc1 + 836, local_player);

  const std::vector<uint32_t> objects = {
      g_cls_class,     pkg,         cls_controller, cls_pc,        cls_gpc,
      cls_gear_pc,     g_cls_gear_pc_sp, cls_ai,    cls_gear_ai,   g_cls_gear_ai_dom,
      cls_pawn,        g_default_obj, pri1,        pri3,          pawn1,
      pawn3,           state,       g_pc1,         g_pc3,         g_ai};
  const uint32_t objects_data = Alloc(uint32_t(4 * objects.size()));
  for (size_t i = 0; i < objects.size(); ++i)
    StoreU32(g_base, objects_data + uint32_t(4 * i), objects[i]);
  StoreU32(g_base, g_g.objects, objects_data);
  StoreU32(g_base, g_g.objects + 4, uint32_t(objects.size()));
}

bool Contains(const std::vector<uint32_t>& list, uint32_t value) {
  for (uint32_t entry : list)
    if (entry == value) return true;
  return false;
}

bool HasText(const std::string& haystack, const char* needle) {
  return haystack.find(needle) != std::string::npos;
}

using rex::glue::dev_commands::ClassChainHas;
using rex::glue::dev_commands::ClassName;
using rex::glue::dev_commands::ControllerList;
using rex::glue::dev_commands::CountCompleteLines;
using rex::glue::dev_commands::DecodeTchars;
using rex::glue::dev_commands::DescribeController;
using rex::glue::dev_commands::DescribeTargets;
using rex::glue::dev_commands::EncodeTcharZ;
using rex::glue::dev_commands::FormatAck;
using rex::glue::dev_commands::LocalPlayerController;
using rex::glue::dev_commands::NameText;
using rex::glue::dev_commands::ObjectName;
using rex::glue::dev_commands::ObjectPath;
using rex::glue::dev_commands::ObjectsOfClass;
using rex::glue::dev_commands::PackName;
using rex::glue::dev_commands::ParseCommandLine;
using rex::glue::dev_commands::PlayerControllers;
using rex::glue::dev_commands::PlayerName;
using rex::glue::dev_commands::ResolveTargets;
using rex::glue::dev_commands::StateName;
using rex::glue::dev_commands::TakeCompleteLines;
using rex::glue::dev_commands::TargetToken;
using rex::glue::dev_commands::VtableHasFunction;
using rex::glue::dev_commands::WithGodMode;
using rex::glue::dev_commands::WorldInfoOf;

void TestParseBasics() {
  Check(ParseCommandLine("").kind == CommandKind::kSkip, "empty line skips");
  Check(ParseCommandLine("").text.empty(), "skip has empty text");
  Check(ParseCommandLine("# x").kind == CommandKind::kSkip, "comment skips");
  const Command suicide = ParseCommandLine("suicide");
  Check(suicide.kind == CommandKind::kConsole, "plain line is console");
  Check(suicide.target.kind == TargetKind::kLocal, "plain line targets local");
  Check(suicide.text == "suicide", "plain line keeps text");
  Check(ParseCommandLine("  suicide\r").text == "suicide", "CR stripped and trimmed");
  const Command god = ParseCommandLine("@all God");
  Check(god.kind == CommandKind::kConsole, "@all is console");
  Check(god.target.kind == TargetKind::kAll, "@all targets all");
  Check(god.text == "God", "@all keeps console text");
  const Command named = ParseCommandLine("@P3 LoadCheckpoint");
  Check(named.target.kind == TargetKind::kName && named.target.name == "P3",
        "@P3 targets name P3");
  Check(named.text == "LoadCheckpoint", "@P3 keeps console text");
  const Command indexed = ParseCommandLine("@#2 x");
  Check(indexed.target.kind == TargetKind::kIndex && indexed.target.index == 2,
        "@#2 targets index 2");
  const Command zero = ParseCommandLine("@#0 x");
  Check(zero.target.kind == TargetKind::kName && zero.target.name == "#0",
        "@#0 falls back to a name");
  Check(ParseCommandLine("@host x").target.kind == TargetKind::kLocal, "@host targets local");
  const Command missing = ParseCommandLine("@P3");
  Check(missing.kind == CommandKind::kError, "@P3 alone errors");
  Check(missing.text == "no command after @P3", "@P3 error names the token");
}

void TestParseHostVerbs() {
  const Command god = ParseCommandLine("!god P3 on");
  Check(god.kind == CommandKind::kHostVerb, "!god is a host verb");
  Check(god.text == "god", "!god verb lower-cased");
  Check(god.target.kind == TargetKind::kName && god.target.name == "P3",
        "!god moves the target out of args");
  Check(god.args.size() == 1 && god.args[0] == "on", "!god keeps on");
  const Command shout = ParseCommandLine("!GOD P3 OFF");
  Check(shout.text == "god" && shout.args.size() == 1 && shout.args[0] == "off",
        "!GOD P3 OFF normalizes case");
  Check(ParseCommandLine("!god P3 maybe").kind == CommandKind::kError, "!god rejects maybe");
  Check(ParseCommandLine("!kill").kind == CommandKind::kError, "!kill needs a target");
  Check(ParseCommandLine("!kill P2 extra").kind == CommandKind::kError, "!kill rejects extras");
  const Command knockdown = ParseCommandLine("!knockdown P2");
  Check(knockdown.kind == CommandKind::kHostVerb, "!knockdown is a host verb");
  Check(knockdown.text == "knockdown", "!knockdown keeps the verb");
  Check(knockdown.target.kind == TargetKind::kName && knockdown.target.name == "P2",
        "!knockdown targets name P2");
  Check(ParseCommandLine("!knockdown").kind == CommandKind::kError, "!knockdown needs a target");
  Check(ParseCommandLine("!knockdown a b").kind == CommandKind::kError,
        "!knockdown rejects a speed that is not a number");
  Check(ParseCommandLine("!knockdown a 0").kind == CommandKind::kError, "!knockdown rejects speed 0");
  Check(ParseCommandLine("!knockdown a 10000").kind == CommandKind::kError,
        "!knockdown rejects speed 10000");
  Check(ParseCommandLine("!knockdown a 800 1").kind == CommandKind::kError,
        "!knockdown rejects extras");
  const Command fast = ParseCommandLine("!knockdown local 800");
  Check(fast.kind == CommandKind::kHostVerb && fast.args.size() == 1 && fast.args[0] == "800",
        "!knockdown keeps the speed");
  Check(knockdown.args.empty(), "!knockdown without a speed has no arguments");
  const Command damage = ParseCommandLine("!damage local 2000");
  Check(damage.kind == CommandKind::kHostVerb && damage.text == "damage",
        "!damage is a host verb");
  Check(damage.target.kind == TargetKind::kLocal, "!damage targets local");
  Check(damage.args.size() == 1 && damage.args[0] == "2000", "!damage keeps the amount");
  Check(ParseCommandLine("!damage local").kind == CommandKind::kError, "!damage needs an amount");
  Check(ParseCommandLine("!damage local 0").kind == CommandKind::kError, "!damage rejects 0");
  Check(ParseCommandLine("!damage local -5").kind == CommandKind::kError, "!damage rejects -5");
  Check(ParseCommandLine("!damage local 1000000").kind == CommandKind::kError,
        "!damage rejects 7 digits");
  const Command move = ParseCommandLine("!specialmove P1 26");
  Check(move.kind == CommandKind::kHostVerb && move.text == "specialmove",
        "!specialmove is a host verb");
  Check(move.args.size() == 1 && move.args[0] == "26", "!specialmove keeps the move");
  Check(ParseCommandLine("!specialmove P1 256").kind == CommandKind::kError,
        "!specialmove rejects 256");
  Check(ParseCommandLine("!specialmove P1 0").kind == CommandKind::kError,
        "!specialmove rejects 0");
  const Command bad_call = ParseCommandLine("!call P1 Bad-Name");
  Check(bad_call.kind == CommandKind::kError, "!call rejects Bad-Name");
  Check(bad_call.text == "bad function name 'Bad-Name'", "!call error names the function");
  const Command call = ParseCommandLine("!call P1 TeleportToPlayer");
  Check(call.args.size() == 1 && call.args[0] == "TeleportToPlayer", "!call keeps the function");
  Check(call.target.kind == TargetKind::kName && call.target.name == "P1",
        "!call moves the target out of args");
  const Command list = ParseCommandLine("!list");
  Check(list.kind == CommandKind::kHostVerb && list.args.empty(), "!list runs bare");
  Check(ParseCommandLine("!list x").kind == CommandKind::kError, "!list rejects args");
  const Command map = ParseCommandLine("!mapchange");
  Check(map.kind == CommandKind::kHostVerb && map.args.empty(), "!mapchange runs bare");
  const Command map_to = ParseCommandLine("!mapchange SP_X_P");
  Check(map_to.args.size() == 1 && map_to.args[0] == "SP_X_P", "!mapchange keeps the level");
  const Command commit = ParseCommandLine("!mapcommit force");
  Check(commit.kind == CommandKind::kHostVerb, "!mapcommit force runs");
  Check(ParseCommandLine("!mapcommit now").kind == CommandKind::kError,
        "!mapcommit rejects now");
  const Command mark = ParseCommandLine("!mark a  b");
  Check(mark.args.size() == 1 && mark.args[0] == "a  b", "!mark keeps inner spacing");
  Check(ParseCommandLine("!mark").args.empty(), "!mark alone has no args");
  const Command spaced = ParseCommandLine("! mark x");
  Check(spaced.args.size() == 1 && spaced.args[0] == "x", "! mark keeps the text after the verb");
  const Command nope = ParseCommandLine("!nope");
  Check(nope.kind == CommandKind::kError && nope.text == "unknown verb !nope",
        "!nope names the verb");
  Check(ParseCommandLine(std::string("ab\xC3")).kind == CommandKind::kError,
        "non-ASCII byte errors");
  Check(ParseCommandLine(std::string(401, 'x')).kind == CommandKind::kError,
        "401 chars are too long");
  Check(ParseCommandLine(std::string(400, 'x')).kind == CommandKind::kConsole,
        "400 chars are console");
  Check(TargetToken(ParseCommandLine("@local x").target) == "local", "local round trip");
  Check(TargetToken(ParseCommandLine("@all x").target) == "all", "all round trip");
  Check(TargetToken(ParseCommandLine("@#2 x").target) == "#2", "#2 round trip");
  Check(TargetToken(ParseCommandLine("@P3 x").target) == "P3", "P3 round trip");
}

void TestFiles() {
  Check(CountCompleteLines("a\nb\nc") == 2, "two newlines");
  const FileLines first = TakeCompleteLines("a\nb\nc", 0);
  Check(first.lines.size() == 2 && first.lines[0] == "a" && first.lines[1] == "b",
        "first take returns a and b");
  Check(first.consumed == 2 && !first.truncated, "first take consumes 2");
  const FileLines rest = TakeCompleteLines("a\nb\nc\n", 2);
  Check(rest.lines.size() == 1 && rest.lines[0] == "c", "second take returns c");
  Check(rest.consumed == 3 && !rest.truncated, "second take consumes 3");
  const FileLines cr = TakeCompleteLines("x\r\n", 0);
  Check(cr.lines.size() == 1 && cr.lines[0] == "x", "CR stripped from lines");
  const FileLines cut = TakeCompleteLines("a\n", 3);
  Check(cut.truncated && cut.consumed == 0 && cut.lines.empty(), "short file truncates");
  Check(FormatAck(7, false, "bad\tthing\n") == "7\terror\tbad thing \n",
        "error ack sanitizes detail");
  Check(FormatAck(1, true, "") == "1\tok\t\n", "empty ok ack");
}

void TestTchar() {
  const std::vector<uint8_t> op = EncodeTcharZ("op");
  const std::vector<uint8_t> want_op = {0x00, 0x6F, 0x00, 0x70, 0x00, 0x00};
  Check(op == want_op, "EncodeTcharZ op bytes");
  const uint32_t at = StoreTcharString("P3");
  Check(DecodeTchars(g_base, at, 3, 64) == "P3", "round trip P3");
  Check(DecodeTchars(g_base, at, 0, 64).empty(), "num 0 decodes empty");
  Check(DecodeTchars(g_base, 0, 3, 64).empty(), "data 0 decodes empty");
  Check(DecodeTchars(g_base, at, 3, 1) == "P", "max_chars cuts the text");
}

void TestPackName() {
  Check(PackName(0x1234, 0) == 0x0000123400000000ull, "pack index only");
  Check(PackName(1, 2) == 0x0000000100000002ull, "pack index and number");
}

void TestNames() {
  Check(NameText(g_base, g_g, g_idx_wide, 0) == "WideName", "wide entry decodes");
  Check(NameText(g_base, g_g, g_idx_gear_pc_sp, 3) == "GearPC_SP_2", "number 3 appends _2");
  Check(NameText(g_base, g_g, 9999, 0) == "?", "out-of-range index is ?");
  Check(ObjectPath(g_base, g_g, g_cls_gear_pc_sp) == "Pkg.GearPC_SP", "class path has package");
  Check(ClassName(g_base, g_g, 0) == "None", "class of 0 is None");
  Check(ObjectName(g_base, g_g, g_pc1) == "GearPC_SP_0", "host object name");
  Check(ObjectName(g_base, g_g, g_pc3) == "GearPC_SP_2", "second object name");
}

void TestWalkers() {
  const uint32_t cyc1 = MakeObject(g_idx_none, 0, 0, 0, 0x500);
  const uint32_t cyc2 = MakeObject(g_idx_none, 0, 0, 0, 0x500);
  const uint32_t cyc_world = Alloc(0x400);
  StoreU32(g_base, cyc_world + 864, cyc1);
  StoreU32(g_base, cyc1 + 456, cyc2);
  StoreU32(g_base, cyc2 + 456, cyc1);
  Check(ControllerList(g_base, cyc_world).size() == 2, "controller list stops on a cycle");
  Check(ControllerList(g_base, g_world_info).size() == 3, "three controllers linked");
  const std::vector<uint32_t> pcs = PlayerControllers(g_base, g_g, g_world_info);
  Check(pcs.size() == 2, "two player controllers");
  Check(!Contains(pcs, g_ai), "AI controller excluded");

  Target all;
  all.kind = TargetKind::kAll;
  Check(ResolveTargets(g_base, g_g, g_engine, g_world_info, all).size() == 2,
        "kAll resolves two PCs");
  Target by_name;
  by_name.kind = TargetKind::kName;
  by_name.name = "p3";
  const std::vector<uint32_t> named =
      ResolveTargets(g_base, g_g, g_engine, g_world_info, by_name);
  Check(named.size() == 1 && named[0] == g_pc3, "kName p3 resolves case-insensitively");
  Target by_index;
  by_index.kind = TargetKind::kIndex;
  by_index.index = 2;
  const std::vector<uint32_t> second =
      ResolveTargets(g_base, g_g, g_engine, g_world_info, by_index);
  Check(second.size() == 1 && second[0] == pcs[1], "kIndex 2 gives the second PC");
  by_index.index = 3;
  Check(ResolveTargets(g_base, g_g, g_engine, g_world_info, by_index).empty(),
        "kIndex 3 is out of range");
  by_name.name = "P9";
  Check(ResolveTargets(g_base, g_g, g_engine, g_world_info, by_name).empty(),
        "kName P9 finds nobody");
  const std::vector<uint32_t> local =
      ResolveTargets(g_base, g_g, g_engine, g_world_info, Target{});
  Check(local.size() == 1 && local[0] == g_pc1, "kLocal gives the host PC");

  const uint32_t backlink = LoadU32(g_base, g_pc1 + 836);
  StoreU32(g_base, g_pc1 + 836, 0);
  Check(LocalPlayerController(g_base, g_g, g_engine) == 0, "broken backlink gives 0");
  StoreU32(g_base, g_pc1 + 836, backlink);
  StoreU32(g_base, g_engine + 816, 0);
  Check(LocalPlayerController(g_base, g_g, g_engine) == 0, "zero players give 0");
  StoreU32(g_base, g_engine + 816, 5);
  Check(LocalPlayerController(g_base, g_g, g_engine) == 0, "five players give 0");
  StoreU32(g_base, g_engine + 816, 1);
  Check(LocalPlayerController(g_base, g_g, g_engine) == g_pc1, "host PC found");

  Check(WorldInfoOf(g_base, g_g) == g_world_info, "world info found");
  const uint32_t level = LoadU32(g_base, g_world + 80);
  StoreU32(g_base, g_world + 80, 0);
  Check(WorldInfoOf(g_base, g_g) == 0, "missing level gives 0");
  StoreU32(g_base, g_world + 80, level);

  Check(WithGodMode(0x00000001, true) == 0x40000001u, "god mode sets the bit");
  Check(WithGodMode(0x40000001, false) == 1u, "god mode clears the bit");

  const std::vector<uint32_t> gear =
      ObjectsOfClass(g_base, g_g, "GearPC");
  Check(gear.size() == 2, "two GearPC instances");
  Check(Contains(gear, g_pc1) && Contains(gear, g_pc3), "both PCs found");
  Check(!Contains(gear, g_default_obj), "Default object skipped");

  const uint32_t obj = Alloc(0x40);
  const uint32_t vtable = Alloc(32);
  StoreU32(g_base, obj, vtable);
  StoreU32(g_base, vtable + 4 * 5, 0x12345678);
  Check(VtableHasFunction(g_base, obj, 0x12345678, 6), "slot 5 found with 6 slots");
  Check(!VtableHasFunction(g_base, obj, 0x12345678, 4), "slot 5 missed with 4 slots");
  const uint32_t no_vt = Alloc(0x40);
  Check(!VtableHasFunction(g_base, no_vt, 0x12345678, 6), "zero vtable misses");

  StoreU32(g_base, g_pc3 + 460, WithGodMode(LoadU32(g_base, g_pc3 + 460), true));
  const std::string desc = DescribeController(g_base, g_g, g_pc3);
  Check(HasText(desc, "player='P3'"), "describe has the player");
  Check(HasText(desc, "god=1"), "describe has god mode");
  Check(HasText(desc, "pc=1"), "describe has pc flag");
  Check(HasText(desc, "health=100"), "describe has health");
  Check(HasText(desc, "class=GearPC_SP"), "describe has the class");

  Check(StateName(g_base, g_g, g_pc1) == "None", "no state frame is None");
  Check(StateName(g_base, g_g, g_pc3) == "PlayerWalking", "state node names the state");
  Check(DescribeTargets(g_base, g_g, pcs) == "P1 (GearPC_SP_0), P3 (GearPC_SP_2)",
        "targets described");
  Check(PlayerName(g_base, g_ai).empty(), "AI controller has no name");
  Check(ClassChainHas(g_base, g_g, g_pc1, "playercontroller"), "chain match ignores case");
}

}  // namespace

int main() {
  SetupFixture();
  TestParseBasics();
  TestParseHostVerbs();
  TestFiles();
  TestTchar();
  TestPackName();
  TestNames();
  TestWalkers();
  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
