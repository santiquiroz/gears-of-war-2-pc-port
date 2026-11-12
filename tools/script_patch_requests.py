"""Offline twin of the co-op request and friendly-fire groups in src/gow2_script_patch_requests_table.cpp.

python -I tools/script_patch_requests.py --check <dir with Core.upk, Engine.upk, GameFramework.upk, GearGame.upk>
python -I tools/script_patch_requests.py --cpp [--write]
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # -I drops the script dir
import script_patch as sp
import script_patch_body as body
import script_patch_survival as sv


def raw(hex_text):
    return sp.Raw(bytes.fromhex(hex_text))


SERVER_MUTATE = sp.Ref("Engine.PlayerController.ServerMutate", "Function")
MUTATE_STRING = sp.Ref("Engine.PlayerController.ServerMutate.MutateString", "StrProperty")
LAST_BROADCAST = sp.Ref("Engine.PlayerController.LastBroadcastString", "StrProperty")
WORLD_INFO = sp.Ref("Engine.Actor.WorldInfo", "ObjectProperty")
NET_MODE = sp.Ref("Engine.WorldInfo.NetMode", "ByteProperty")
GAME = sp.Ref("Engine.WorldInfo.Game", "ObjectProperty")

# LastBroadcastString[0] = MutateString; return
RQ1_KEEP = 0x0042
RQ1_TAIL = (raw("0F 1A 25 01"), LAST_BROADCAST, raw("00"), MUTATE_STRING, raw("04 0B 53"))
RQ1_GROWN_SIZE = RQ1_KEEP + sv.size(RQ1_TAIL)

REDUCE = sp.Ref("GearGame.GearGame.ReduceDamage", "Function")
INJURED = sp.Ref("GearGame.GearGame.ReduceDamage.injured", "ObjectProperty")
INSTIGATED_BY = sp.Ref("GearGame.GearGame.ReduceDamage.InstigatedBy", "ObjectProperty")
DAMAGE = sp.Ref("GearGame.GearGame.ReduceDamage.Damage", "IntProperty")
DAMAGE_TYPE = sp.Ref("GearGame.GearGame.ReduceDamage.DamageType", "ClassProperty")
IDX = sp.Ref("GearGame.GearGame.ReduceDamage.Idx", "IntProperty")
GDT = sp.Ref("GearGame.GearGame.ReduceDamage.GDT", "ClassProperty")
GEAR_DAMAGE_TYPE = sp.Ref("GearGame.GearDamageType", "Class")
ALLOW_FF = sp.Ref("GearGame.GearGame.bAllowFriendlyFire", "BoolProperty")
MP_BASE = sp.Ref("GearGame.GearGameMP_Base", "Class")
PLAYER_CONTROLLER = sp.Ref("Engine.PlayerController", "Class")
IS_HUMAN = sp.Ref("Engine.Pawn.IsHumanControlled", "Function")

FF1_KEEP = 0x0259


def _allow_ff():
    return sv.R(b"\x2D") + sv.inst(ALLOW_FF)


def _let_allow_ff(value):
    return sv.R(b"\x14") + _allow_ff() + sv.R(value)


def _not(expr):
    return sv.R(b"\x81") + expr + sv.R(b"\x16")


def _int_eq(a, b):
    return sv.R(b"\x9A") + a + b + sv.R(b"\x16")


def _not_versus():
    return sv.eq(sv.R(b"\x2E") + (MP_BASE,) + sv.R(b"\x17"), sv.NONE)


def _injured_is_human():
    call = sv.R(b"\x1C") + (IS_HUMAN,) + sv.R(b"\x16")
    return sv.ctx(sv.local(INJURED), call, 4)


def _instigator_is_player():
    return sv.ne(sv.R(b"\x2E") + (PLAYER_CONTROLLER,) + sv.local(INSTIGATED_BY), sv.NONE)


def _human_hits_human():
    both_set = sv.and_(sv.ne(sv.local(INJURED), sv.NONE), sv.ne(sv.local(INSTIGATED_BY), sv.NONE))
    return sv.and_(sv.and_(both_set, _injured_is_human()), _instigator_is_player())


def _suspend_ff_condition():
    return sv.and_(sv.and_(_allow_ff(), _not_versus()), _not(_human_hits_human()))


FF1_TAIL, FF1_LABELS, FF1_GROWN_SIZE = sv.assemble(FF1_KEEP, [
    # TAIL1, entered from +0x0010: friendly fire stays on only for one human hurting another.
    (None, lambda L: sv.jumpifnot(L["CHECK_INJURED"], _suspend_ff_condition())),
    (None, lambda L: _let_allow_ff(b"\x28")),
    (None, lambda L: sv.R(b"\x0F") + sv.local(IDX) + sv.R(b"\x26")),
    ("CHECK_INJURED", lambda L: sv.jumpifnot(0x006F, sv.ne(sv.local(INJURED), sv.NONE))),
    (None, lambda L: sv.jump(0x001B)),
    # TAIL2, entered from +0x01D9 by every path: put the flag back before Super.ReduceDamage.
    ("RESTORE", lambda L: sv.jumpifnot(L["CHECK_DAMAGE"], _int_eq(sv.local(IDX), sv.R(b"\x26")))),
    (None, lambda L: _let_allow_ff(b"\x27")),
    ("CHECK_DAMAGE", lambda L: sv.R(b"\x07\x02\x57\x97\x48") + (DAMAGE,) + sv.R(b"\x25\x16")),
    (None, lambda L: sv.jump(0x01E4)),
    (None, lambda L: sv.R(b"\x53")),
])
FF1_RESTORE = FF1_LABELS["RESTORE"]


def _jump_edits(offset, old_target, new_target):
    edits = [sp.ByteEdit(offset, 0x07, 0x06)]
    for i, (old, new) in enumerate(zip(old_target.to_bytes(2, "big"), new_target.to_bytes(2, "big"))):
        if old != new:
            edits.append(sp.ByteEdit(offset + 1 + i, old, new))
    return tuple(edits)


def _unchanged_jump_checks(offset, old_target, new_target):
    checks = []
    for i, (old, new) in enumerate(zip(old_target.to_bytes(2, "big"), new_target.to_bytes(2, "big"))):
        if old == new:
            checks.append(sp.Check(offset + 1 + i, bytes([old])))
    return tuple(checks)


ROWS = {
    "RQ-1": sp.ScriptRow("RQ-1", "ServerMutate on the server stores its string in LastBroadcastString[0] for the port to read: goto 0x0042, LastBroadcastString[0] = MutateString",
        SERVER_MUTATE, 67,
        checks=(sp.Check(0x00, bytes.fromhex("07 00")),
                sp.Check(0x03, bytes.fromhex("9A 38 3A 19 01")),
                sp.Check(0x0C, bytes.fromhex("00 05 00 01 01")),
                sp.Check(0x15, bytes.fromhex("38 3A 24 03 16")),
                sp.Check(0x1A, bytes.fromhex("04 0B")),
                sp.Check(0x1C, bytes.fromhex("19 19 01")),
                sp.Check(0x23, bytes.fromhex("00 05 00 04 01")),
                sp.Check(0x2C, bytes.fromhex("00 10 00 00 1B")),
                sp.Check(0x39, bytes.fromhex("00")),
                sp.Check(0x3E, bytes.fromhex("17 16 04 0B")),
                sp.Check(0x42, bytes.fromhex("53"))),
        operands=(sp.Operand(0x08, WORLD_INFO), sp.Operand(0x11, NET_MODE),
                  sp.Operand(0x1F, WORLD_INFO), sp.Operand(0x28, GAME),
                  sp.Operand(0x3A, MUTATE_STRING)),
        byte_edits=(sp.ByteEdit(0x02, 0x1C, RQ1_KEEP),),
        grow=sp.Grow(RQ1_KEEP, RQ1_GROWN_SIZE, RQ1_TAIL)),
    "FF-1": sp.ScriptRow("FF-1", "with bAllowFriendlyFire on outside versus, it is off for one call unless a human player hurts a human-controlled pawn: goto 0x0259 and 0x01D9 -> the restore tail",
        REDUCE, 602,
        checks=(sp.Check(0x0013, bytes.fromhex("77 00")),
                sp.Check(0x0019, bytes.fromhex("2A 16 07 00 4F 77 19 00")),
                sp.Check(0x006F, bytes.fromhex("0F 00")),
                sp.Check(0x0075, bytes.fromhex("13")),
                sp.Check(0x007A, bytes.fromhex("00")),
                sp.Check(0x007F, bytes.fromhex("07 01 D9")),
                sp.Check(0x00A8, bytes.fromhex("81 2D 01")),
                sp.Check(0x00AF, bytes.fromhex("16")),
                sp.Check(0x018F, bytes.fromhex("06 01 D9 07 01 D9")),
                sp.Check(0x01A7, bytes.fromhex("07 01 D9")),
                sp.Check(0x01DC, bytes.fromhex("97 48")),
                sp.Check(0x01E2, bytes.fromhex("25 16 0F 00")),
                sp.Check(0x01EA, bytes.fromhex("25")),
                sp.Check(0x0257, bytes.fromhex("04 0B")),
                sp.Check(0x0259, bytes.fromhex("53")))
            + _unchanged_jump_checks(0x0010, 0x006F, FF1_KEEP)
            + _unchanged_jump_checks(0x01D9, 0x0257, FF1_RESTORE),
        operands=(sp.Operand(0x0015, INJURED), sp.Operand(0x0021, INJURED),
                  sp.Operand(0x0071, GDT), sp.Operand(0x0076, GEAR_DAMAGE_TYPE),
                  sp.Operand(0x007B, DAMAGE_TYPE), sp.Operand(0x00AB, ALLOW_FF),
                  sp.Operand(0x01DE, DAMAGE), sp.Operand(0x01E6, IDX)),
        byte_edits=_jump_edits(0x0010, 0x006F, FF1_KEEP) + _jump_edits(0x01D9, 0x0257, FF1_RESTORE),
        grow=sp.Grow(FF1_KEEP, FF1_GROWN_SIZE, FF1_TAIL)),
}

ROW_ORDER = ("RQ-1", "FF-1")


class EngineModel(sp.PackageModel):
    def __init__(self, pkg_dir):
        super().__init__(pkg_dir)
        if "Engine" not in self.world.pkgs:
            raise FileNotFoundError(f"Engine.upk not found in {pkg_dir}")
        self.pkg = self.world.pkgs["Engine"]
        self.pkg_name = "Engine"


def check_table():
    for row in ROWS.values():
        try:
            sp._check_edits_in_kept_prefix(row)
        except sp.PatchSkip as e:
            raise ValueError(f"{e.row}: {e.reason}") from None


def render_cpp():
    check_table()
    lines = [
        "// gow2 - ReXGlue Recompiled Project",
        "//",
        "// The co-op request channel and friendly-fire rows of docs/COOP_N_PLAYERS.md. Printed by",
        "// `python -I tools/script_patch_requests.py --cpp`; edit the rows there, not here. Check them",
        "// against your own decompressed packages with `python -I tools/script_patch_requests.py --check <folder>`.",
        "",
        '#include "gow2_script_patch_requests.h"',
        "",
        "namespace rex::glue::script_patch {",
        "",
        "namespace {",
        "",
    ]
    for rid in ROW_ORDER:
        lines.extend(sp._script_lines(ROWS[rid]))
        lines.append("")
    lines.extend([
        "}  // namespace",
        "",
        "PatchGroup CoopRequestsGroup() {",
        "  return {",
        '      .name = "coop-requests",',
        "      .scripts = {RowRQ_1()},",
        "  };",
        "}",
        "",
        "PatchGroup CoopFriendlyFireGroup() {",
        "  return {",
        '      .name = "coop-ff",',
        "      .scripts = {RowFF_1()},",
        "  };",
        "}",
        "",
        "}  // namespace rex::glue::script_patch",
    ])
    return "\n".join(lines) + "\n"


def _load(model_class, pkg_dir):
    try:
        return model_class(pkg_dir)
    except Exception as e:
        print(f"error: cannot load packages from {pkg_dir}: {type(e).__name__}({e})", file=sys.stderr)
        return None


def check_package(pkg_dir):
    ue3script, _ = sp._ue3_modules()
    print(f"== script_patch_requests --check {pkg_dir}")
    engine = _load(EngineModel, pkg_dir)
    gear = _load(sp.PackageModel, pkg_dir)
    if engine is None or gear is None:
        return 1
    failed = False
    for row, model in ((ROWS["RQ-1"], engine), (ROWS["FF-1"], gear)):
        overlay = body.CoopOverlay(model, {})
        if not body._check_script_row(ue3script, model, overlay, row, 4):
            failed = True
            continue
        entry = sp._plan_script(model, row, 4)
        print(f"   {row.id}: script grown to {entry.grown_size} bytes, {entry.writes} write(s)")
        again = sp._plan_script(body.CoopOverlay(model, {entry.handle: entry.new}), row, 4)
        print(f"   {row.id} on the patched script: {again.writes} write(s)")
        if again.writes:
            failed = True
    print("== C++ table")
    print(render_cpp(), end="")
    return 1 if failed else 0


def parse_args(argv=None):
    p = argparse.ArgumentParser(description="Offline twin of the co-op request and friendly-fire patch groups.")
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
            out = Path(__file__).resolve().parent.parent / "src" / "gow2_script_patch_requests_table.cpp"
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
