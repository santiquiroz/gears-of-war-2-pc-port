"""Stage 2 group A (survival) offline twin of the guest-memory script patches.

python -I tools/script_patch_survival.py --check <dir with Core.upk, Engine.upk, GameFramework.upk, GearGame.upk>
python -I tools/script_patch_survival.py --cpp
python -I tools/script_patch_survival.py --cpp --write
"""
import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import script_patch as sp

DPC = sp.Ref("GearGame.GearPC.DefaultPawnClass", "ClassProperty")
MARCUS = sp.Ref("GearGame.GearPawn_COGMarcus", "Class")
DOM = sp.Ref("GearGame.GearPawn_COGDom", "Class")
PAWN = sp.Ref("Engine.Controller.Pawn", "ObjectProperty")
OBJ_CLASS = sp.Ref("Core.Object.Class", "ClassProperty")
WORLDINFO = sp.Ref("Engine.Actor.WorldInfo", "ObjectProperty")
ALL_CONTROLLERS = sp.Ref("Engine.WorldInfo.AllControllers", "Function")
GEARPC = sp.Ref("GearGame.GearPC", "Class")
KILL_FLAG = sp.Ref("Engine.Actor.bKillDuringLevelTransition", "BoolProperty")


def size(pieces):
    return sum(len(p.data) if isinstance(p, sp.Raw) else 4 for p in pieces)


def R(b):
    return (sp.Raw(b),)


def w(v):
    v &= 0xFFFF
    return bytes([v >> 8, v & 0xFF])


def and_(a, b):
    return R(b"\x82") + a + R(b"\x18" + w(size(b) + 1)) + b + R(b"\x16")


def or_(a, b):
    return R(b"\x84") + a + R(b"\x18" + w(size(b) + 1)) + b + R(b"\x16")


def inst(ref):
    return R(b"\x01") + (ref,)


def local(ref):
    return R(b"\x00") + (ref,)


def ctx(obj, member, member_size):
    return R(b"\x19") + obj + R(w(size(member)) + b"\x00" + bytes([member_size])) + member


def ne(a, b):
    return R(b"\x77") + a + b + R(b"\x16")


def eq(a, b):
    return R(b"\x72") + a + b + R(b"\x16")


def objconst(ref):
    return R(b"\x20") + (ref,)


NONE = R(b"\x2A")


def is_extra(dpc_expr):
    # None counts as a hero: fail toward retail.
    return and_(and_(ne(dpc_expr, NONE), ne(dpc_expr, objconst(MARCUS))), ne(dpc_expr, objconst(DOM)))


def jumpifnot(target, cond):
    return R(b"\x07" + w(target)) + cond


def jump(target):
    return R(b"\x06" + w(target))


def assemble(keep, stmts):
    class Labels(dict):
        def __missing__(self, key):
            return 0
    labels = Labels()
    for _ in range(3):
        at = keep
        out = []
        for label, build in stmts:
            if label:
                labels[label] = at
            pieces = build(labels)
            out += list(pieces)
            at += size(pieces)
    return tuple(out), labels, at


SDP = "GearGame.GearGameSP_Base.SpawnDefaultPawnFor"
SDP_RESULT = sp.Ref(SDP + ".ResultPawn", "ObjectProperty")
t1, _, _ = assemble(0x00A1, [
    ("set", lambda L: R(b"\x14") + ctx(local(SDP_RESULT), R(b"\x2D") + inst(KILL_FLAG), 4) + R(b"\x28")),
    (None, lambda L: jump(0x0095)),
    (None, lambda L: R(b"\x53")),
])
S2A_1 = sp.ScriptRow(
    "S2A-1", "player pawns survive chapter travel: ResultPawn.bKillDuringLevelTransition = false",
    sp.Ref(SDP, "Function"), 162,
    checks=(sp.Check(0x0061, b"\x0F\x00"), sp.Check(0x0067, b"\x1C"), sp.Check(0x008A, b"\x07\x00"),
            sp.Check(0x008D, b"\x72\x00"), sp.Check(0x0093, b"\x2A\x16\x04\x00"), sp.Check(0x009B, b"\x04\x3A"),
            sp.Check(0x00A1, b"\x53")),
    operands=(sp.Operand(0x0063, SDP_RESULT), sp.Operand(0x0068, sp.Ref("Engine.Actor.Spawn", "Function")),
              sp.Operand(0x008F, SDP_RESULT), sp.Operand(0x0097, SDP_RESULT),
              sp.Operand(0x009D, sp.Ref(SDP + ".ReturnValue", "ObjectProperty"))),
    byte_edits=(sp.ByteEdit(0x008C, 0x95, 0xA1),),
    grow=sp.Grow(0x00A1, 183, t1))

PGO = "GearGame.GearPC.ProcessGameOver"
PGO_PC = sp.Ref(PGO + ".PC", "ObjectProperty")
RESTART = R(b"\x1C") + (sp.Ref("GearGame.GearGameSP_Base.RestartPlayer", "Function"),) + R(b"\x17\x16")
GAME = ctx(inst(WORLDINFO), inst(sp.Ref("Engine.WorldInfo.Game", "ObjectProperty")), 4)
CAST_GAME = R(b"\x2E") + (sp.Ref("GearGame.GearGameSP_Base", "Class"),) + GAME


def foreach_header(L):
    member = R(b"\x1C") + (ALL_CONTROLLERS,) + objconst(GEARPC) + local(PGO_PC) + R(b"\x16")
    member_start = L["foreach"] + 1 + 1 + 5 + 4
    return (R(b"\x2F\x19") + inst(WORLDINFO) + R(w(L["pop"] - member_start) + b"\x00\x00") + member
            + R(w(L["pop"])))


t2, _, _ = assemble(0x0000, [
    ("start", lambda L: jumpifnot(L["hero"], is_extra(inst(DPC)))),
    (None, lambda L: jumpifnot(L["ret"], and_(R(b"\x81\x2D") + inst(sp.Ref("Engine.Actor.bPendingDelete", "BoolProperty")) + R(b"\x16"),
                                                 eq(inst(PAWN), NONE)))),
    (None, lambda L: ctx(CAST_GAME, RESTART, 0)),
    ("ret", lambda L: R(b"\x04\x0B")),
    ("hero", lambda L: R(b"\x1C") + (sp.Ref("GearGame.GearPC.PlayAnyaCoopDeathSound", "Function"),) + R(b"\x16")),
    ("foreach", foreach_header),
    (None, lambda L: ctx(local(PGO_PC), R(b"\x1C") + (sp.Ref("GearGame.GearPC.ClientGameOver", "Function"),) + R(b"\x16"), 0)),
    (None, lambda L: R(b"\x31")),
    ("pop", lambda L: R(b"\x30")),
    (None, lambda L: R(b"\x04\x0B\x53")),
])
S2A_2 = sp.ScriptRow(
    "S2A-2", "game over: a dead player 3+ respawns near a teammate instead; Marcus and Dom keep the retail rule",
    sp.Ref(PGO, "Function"), 64,
    checks=(sp.Check(0x0000, b"\x1B"), sp.Check(0x0009, b"\x16\x2F\x19\x01"), sp.Check(0x0011, b"\x00\x27\x00\x00\x1C"),
            sp.Check(0x001A, b"\x20"), sp.Check(0x001F, b"\x00"), sp.Check(0x0024, b"\x16\x00\x3C\x19\x00"),
            sp.Check(0x002D, b"\x00\x0A\x00\x00\x1B"), sp.Check(0x003A, b"\x16\x31\x30\x04\x0B\x53")),
    operands=(sp.Operand(0x000D, WORLDINFO), sp.Operand(0x0016, ALL_CONTROLLERS), sp.Operand(0x001B, GEARPC),
              sp.Operand(0x0020, PGO_PC), sp.Operand(0x0029, PGO_PC)),
    grow=sp.Grow(0x0000, 159, t2))

DES = "GearGame.GearPC.Destroyed"
DES_GEAR = sp.Ref(DES + ".Gear", "ObjectProperty")
DES_OLD = sp.Ref(DES + ".OldPawn", "ObjectProperty")
ISM = sp.Ref("GearGame.GearPawn.IsDoingSpecialMove", "Function")
DUMMY = sp.Ref("GearGame.GearPC.bCheckpointDummy", "BoolProperty")
GEAR_CLASS = ctx(local(DES_GEAR), inst(OBJ_CLASS), 4)


def special_move(n):
    return ctx(local(DES_GEAR), R(b"\x1C") + (ISM,) + R(b"\x24" + bytes([n]) + b"\x16"), 4)


t3, _, _ = assemble(0x0437, [
    ("t0", lambda L: jumpifnot(0x00AD, and_(and_(is_extra(inst(DPC)),
                                                  ne(GEAR_CLASS, objconst(MARCUS))), ne(GEAR_CLASS, objconst(DOM))))),
    ("t1", lambda L: jumpifnot(L["super"], ne(R(b"\x2E") + (sp.Ref("GearGame.GearPawn", "Class"),) + inst(PAWN), NONE))),
    ("t2", lambda L: jumpifnot(L["t3"], or_(special_move(42), special_move(43)))),
    (None, lambda L: jump(0x00AD)),
    ("t3", lambda L: R(b"\x0F") + local(DES_OLD) + inst(PAWN)),
    (None, lambda L: R(b"\x1C") + (sp.Ref("GearGame.GearPC.UnPossess", "Function"),) + R(b"\x16")),
    (None, lambda L: ctx(local(DES_OLD), R(b"\x61\x17\x16"), 4)),
    ("super", lambda L: jump(0x00A4)),
    (None, lambda L: R(b"\x53")),
])
S2A_3 = sp.ScriptRow(
    "S2A-3", "a leaving player 3+, or a checkpoint dummy holding a player 3+'s record: UnPossess and Destroy his pawn; no AI Dom, no SeqEvent_CoopPlayerLeft",
    sp.Ref(DES, "Function"), 1080,
    checks=(sp.Check(0x0063, b"\x07"), sp.Check(0x0066, b"\x82\x81\x2D\x01"), sp.Check(0x00A4, b"\x1C"),
            sp.Check(0x00A9, b"\x16\x06\x04\x35\x07\x01\x16\x84\x19\x00"), sp.Check(0x01B2, b"\x0F\x00"),
            sp.Check(0x01B8, b"\x01"), sp.Check(0x01BD, b"\x1B"), sp.Check(0x01C6, b"\x16\x0F\x00"), sp.Check(0x01CD, b"\x1C"),
            sp.Check(0x01D2, b"\x20"), sp.Check(0x0435, b"\x04\x0B\x53")),
    operands=(sp.Operand(0x006A, DUMMY), sp.Operand(0x00A5, sp.Ref("Engine.PlayerController.Destroyed", "Function")),
              sp.Operand(0x00B3, DES_GEAR), sp.Operand(0x00BC, ISM), sp.Operand(0x01B4, DES_OLD), sp.Operand(0x01B9, PAWN),
              sp.Operand(0x01C9, sp.Ref(DES + ".AI", "ObjectProperty")), sp.Operand(0x01CE, sp.Ref("Engine.Actor.Spawn", "Function")),
              sp.Operand(0x01D3, sp.Ref("GearGame.GearAI_Dom", "Class"))),
    byte_edits=(sp.ByteEdit(0x0064, 0x00, 0x04), sp.ByteEdit(0x0065, 0xAD, 0x37)),
    grow=sp.Grow(0x0437, 1275, t3))

SCP = "GearGame.GearPC.SaveCheckpoint"
SCP_PC = sp.Ref(SCP + ".PC", "ObjectProperty")
PC_DPC = ctx(local(SCP_PC), inst(DPC), 4)
PC_PAWN = ctx(local(SCP_PC), inst(PAWN), 4)
t5, _, _ = assemble(0x0187, [
    ("t0", lambda L: jumpifnot(L["refuse"], and_(is_extra(PC_DPC), eq(PC_PAWN, NONE)))),
    (None, lambda L: jump(0x00DA)),
    ("refuse", lambda L: R(b"\x30\x04\x0B")),
    (None, lambda L: R(b"\x53")),
])
S2A_5 = sp.ScriptRow(
    "S2A-5", "checkpoints: a dead player 3+ waiting to respawn does not block the save",
    sp.Ref(SCP, "Function"), 392,
    checks=(sp.Check(0x0065, b"\x2F\x19\x01"), sp.Check(0x006C, b"\x00\x6B\x00\x00\x1C"), sp.Check(0x0075, b"\x20"),
            sp.Check(0x007A, b"\x00"), sp.Check(0x007F, b"\x16\x00\xDB\x07\x00\xDA\x84\x19\x00"),
            sp.Check(0x00DA, b"\x31\x30\x0F\x00"), sp.Check(0x0185, b"\x04\x0B\x53")),
    operands=(sp.Operand(0x0068, WORLDINFO), sp.Operand(0x0071, ALL_CONTROLLERS), sp.Operand(0x0076, GEARPC),
              sp.Operand(0x007B, SCP_PC), sp.Operand(0x0088, SCP_PC), sp.Operand(0x0091, sp.Ref("Engine.Actor.IsTimerActive", "Function"))),
    byte_edits=(sp.ByteEdit(0x00D7, 0x30, 0x06), sp.ByteEdit(0x00D8, 0x04, 0x01), sp.ByteEdit(0x00D9, 0x0B, 0x87)),
    grow=sp.Grow(0x0187, 496, t5))

THA = "GearGame.GearGame.TeamHasAnyChanceOfSurvival"
THA_PC = sp.Ref(THA + ".PC", "ObjectProperty")
THA_DB = sp.Ref(THA + ".DBNOPawn", "ObjectProperty")
THA_FOUND = sp.Ref(THA + ".bFoundReviver", "BoolProperty")
COOP_SPLIT = sp.Ref("GearGame.GearGame.bInCoopSplit", "BoolProperty")
SOLO_SPLIT = sp.Ref("GearGame.GearGame.bInSoloSplit", "BoolProperty")
GPRI = sp.Ref("GearGame.GearPRI", "Class")
SQUAD = sp.Ref("GearGame.GearPRI.SquadName", "NameProperty")


def squad_of(owner, pri_prop):
    pri = ctx(owner, inst(pri_prop), 4)
    return ctx(R(b"\x2E") + (GPRI,) + pri, inst(SQUAD), 8)


THA_COND = and_(and_(and_(and_(ne(local(THA_PC), NONE), R(b"\x81\x2D") + inst(SOLO_SPLIT) + R(b"\x16")),
                              R(b"\x2D") + inst(COOP_SPLIT)),
                         is_extra(ctx(local(THA_PC), inst(DPC), 4))),
                R(b"\xFE") + squad_of(local(THA_PC), sp.Ref("Engine.Controller.PlayerReplicationInfo", "ObjectProperty"))
                + squad_of(local(THA_DB), sp.Ref("Engine.Pawn.PlayerReplicationInfo", "ObjectProperty")) + R(b"\x16"))
t4, _, _ = assemble(0x0297, [
    ("t0", lambda L: jumpifnot(0x0288, THA_COND)),
    (None, lambda L: R(b"\x14\x2D") + local(THA_FOUND) + R(b"\x27")),
    (None, lambda L: jump(0x0289)),
    (None, lambda L: R(b"\x53")),
])
S2A_4 = sp.ScriptRow(
    "S2A-4", "co-op splits: a standing player 3+ in the downed player's squad counts as a reviver",
    sp.Ref(THA, "Function"), 664,
    checks=(sp.Check(0x0248, b"\x0F\x00"), sp.Check(0x024E, b"\x2E"), sp.Check(0x0253, b"\x00"), sp.Check(0x0258, b"\x07\x02"),
            sp.Check(0x025B, b"\x82\x82\x77\x00"), sp.Check(0x0263, b"\x2A\x16\x18\x00\x09\x81\x2D\x01"),
            sp.Check(0x026F, b"\x16\x16\x18\x00\x09\x81\x2D\x01"), sp.Check(0x027B, b"\x16\x16\x14\x2D\x00"),
            sp.Check(0x0284, b"\x27\x06\x02\x89\x31\x30\x04\x2D\x00"), sp.Check(0x0291, b"\x04\x3A"), sp.Check(0x0297, b"\x53")),
    operands=(sp.Operand(0x024A, THA_PC), sp.Operand(0x024F, GEARPC), sp.Operand(0x0254, sp.Ref(THA + ".C", "ObjectProperty")),
              sp.Operand(0x025F, THA_PC), sp.Operand(0x026B, COOP_SPLIT), sp.Operand(0x0277, SOLO_SPLIT),
              sp.Operand(0x0280, THA_FOUND), sp.Operand(0x028D, THA_FOUND),
              sp.Operand(0x0293, sp.Ref(THA + ".ReturnValue", "BoolProperty"))),
    byte_edits=(sp.ByteEdit(0x025A, 0x88, 0x97),),
    grow=sp.Grow(0x0297, 854, t4))

ROWS = {"S2A-1": S2A_1, "S2A-2": S2A_2, "S2A-3": S2A_3, "S2A-5": S2A_5, "S2A-4": S2A_4}
SURVIVAL_ORDER = ("S2A-1", "S2A-2", "S2A-3", "S2A-5")
SPLIT_REVIVE_ORDER = ("S2A-4",)
CPP_ORDER = ("S2A-1", "S2A-2", "S2A-3", "S2A-5", "S2A-4")


class SurvivalPackageModel(sp.PackageModel):
    def __init__(self, pkg_dir):
        super().__init__(pkg_dir)
        self._kill_index = None

    def find(self, ref):
        if ref == KILL_FLAG and self._kill_index is not None:
            return [self._kill_index]
        hits = super().find(ref)
        if hits:
            return hits
        if ref != KILL_FLAG:
            return []
        eng = self.world.pkgs["Engine"]
        exists = [i for i in range(len(eng.exports))
                  if "Engine." + eng.obj_path(i + 1) == ref.path and eng.obj_class(i + 1) == ref.cls]
        if len(exists) != 1:
            return []
        outer = super().find(sp.Ref("Engine.Actor", "Class"))
        if len(outer) != 1:
            return []
        self.pkg.imports.append({"class_package": "Core", "class": "BoolProperty",
                                 "outer": outer[0], "name": "bKillDuringLevelTransition"})
        self._kill_index = -len(self.pkg.imports)
        return [self._kill_index]


def render_cpp():
    for row in ROWS.values():
        try:
            sp._check_edits_in_kept_prefix(row)
        except sp.PatchSkip as e:
            raise ValueError(f"{e.row}: {e.reason}") from None
    lines = [
        "// gow2 - ReXGlue Recompiled Project",
        "//",
        "// The Stage 2 group A (survival) rows of docs/COOP_STAGE2_SURVIVAL.md. Printed by",
        "// `python -I tools/script_patch_survival.py --cpp`; edit the rows there, not here. Check them",
        "// against your own decompressed packages with `python -I tools/script_patch_survival.py --check <folder>`.",
        "",
        '#include "gow2_script_patch_survival.h"',
        "",
        "namespace rex::glue::script_patch {",
        "",
        "namespace {",
        "",
    ]
    for rid in CPP_ORDER:
        lines.extend(sp._script_lines(ROWS[rid]))
        lines.append("")
    lines.extend([
        "}  // namespace",
        "",
        "PatchGroup CoopSurvivalGroup(bool extra_players_end_game) {",
        '  PatchGroup group{.name = "survival"};',
        "  group.scripts.push_back(RowS2A_1());",
        "  if (!extra_players_end_game) group.scripts.push_back(RowS2A_2());",
        "  group.scripts.push_back(RowS2A_3());",
        "  group.scripts.push_back(RowS2A_5());",
        "  return group;",
        "}",
        "",
        "PatchGroup CoopSplitReviveGroup() {",
        "  return {",
        '      .name = "split-revive",',
        "      .scripts = {RowS2A_4()},",
        "  };",
        "}",
        "",
        "}  // namespace rex::glue::script_patch",
    ])
    return "\n".join(lines) + "\n"


def survival_rows(extra_players_end_game=False):
    ids = [i for i in SURVIVAL_ORDER if i != "S2A-2"] if extra_players_end_game else SURVIVAL_ORDER
    return [ROWS[i] for i in ids]


def split_revive_rows():
    return [ROWS[i] for i in SPLIT_REVIVE_ORDER]


def jump_targets_on_statements(ue3script, model, entry, new_script):
    lines, _, _ = sp._decode_standalone(ue3script, model, entry.handle, entry.row.function.path, new_script)
    starts = {int(line.strip().split(":")[0], 16) for line in lines}
    ok = True
    for line in lines:
        for target in re.findall(r"goto 0x([0-9A-Fa-f]+)", line):
            if int(target, 16) not in starts:
                ok = False
    print(f"   jumps land on statements: {ok}")
    return ok


def append_row_ends_in_end_of_script(model, entry, new_script):
    original = model.script(entry.handle).data
    ok = original[entry.row.grow.keep] == 0x53 and new_script[-1] == 0x53
    print(f"   append row ends in EndOfScript: {ok}")
    return ok


def check_package(pkg_dir):
    ue3script, _ = sp._ue3_modules()
    print(f"== script_patch_survival --check {pkg_dir}")
    try:
        model = SurvivalPackageModel(pkg_dir)
    except OSError as e:
        print(f"error: cannot load packages from {pkg_dir}: {e}", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"error: cannot load packages from {pkg_dir}: {type(e).__name__}({e})", file=sys.stderr)
        return 1
    try:
        survival_plan = sp.plan_group(model, survival_rows(), 4)
        split_plan = sp.plan_group(model, split_revive_rows(), 4)
    except sp.PatchSkip as e:
        print(f"SKIP {e.row}: {e.reason}")
        return 1
    failed = False
    for title, plan in (("survival", survival_plan), ("split-revive", split_plan)):
        print(f"== group {title}")
        for line in sp.group_lines(plan):
            print(f"   {line}")
        patched = sp.apply_plan(model, plan)
        for entry in plan.entries:
            new_script = patched[entry.handle]
            if not sp._check_script_row(ue3script, model, entry, new_script):
                failed = True
            if not jump_targets_on_statements(ue3script, model, entry, new_script):
                failed = True
            if entry.row.grow is not None and entry.row.grow.keep == entry.row.script_size - 1:
                if not append_row_ends_in_end_of_script(model, entry, new_script):
                    failed = True
    print("== C++ table")
    print(render_cpp(), end="")
    return 1 if failed else 0


def parse_args(argv=None):
    p = argparse.ArgumentParser(description="Offline twin of the survival script patches.")
    p.add_argument("--check", metavar="PKGDIR", default=None,
                   help="check the rows against decompressed packages in PKGDIR")
    p.add_argument("--cpp", action="store_true", help="print the C++ table file")
    p.add_argument("--write", action="store_true", help="write the C++ table file (with --cpp)")
    return p.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    if args.write and not args.cpp:
        print("error: --write needs --cpp", file=sys.stderr)
        return 2
    if args.cpp:
        text = render_cpp()
        if args.write:
            out = Path(__file__).resolve().parent.parent / "src" / "gow2_script_patch_survival.cpp"
            with open(out, "w", encoding="utf-8", newline="\n") as f:
                f.write(text)
            print(str(out))
        else:
            sp._write_stdout(text)
        return 0
    if args.check is None:
        print("error: need --check PKGDIR or --cpp", file=sys.stderr)
        return 2
    return check_package(args.check)


if __name__ == "__main__":
    sys.exit(main())
