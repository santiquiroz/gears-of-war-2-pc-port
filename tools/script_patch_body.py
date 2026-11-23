"""Offline twin of the Stage 2 body/ride groups in src/gow2_script_patch_body_table.cpp.

python -I tools/script_patch_body.py --check <dir with Core.upk, Engine.upk, GameFramework.upk, GearGame.upk> [--players N]
python -I tools/script_patch_body.py --cpp [--write]
"""
import argparse
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))  # -I drops the script dir
import script_patch as sp
import script_patch_survival as sv


@dataclass(frozen=True)
class FieldAnchor:
    object: sp.Ref
    offset: int
    prop: str
    value: sp.Ref


@dataclass(frozen=True)
class ObjectFieldRow:
    id: str
    summary: str
    object: sp.Ref
    offset: int
    prop: str
    value: sp.Ref
    anchors: tuple = ()


@dataclass
class FieldPlan:
    row: ObjectFieldRow
    handle: object
    value: int
    writes: int


def plan_object_field(model, row):
    handle = sp._resolve_one(model, row.object, row.id)
    value = model.operand_value(sp._resolve_one(model, row.value, row.id))
    for anchor in row.anchors:
        anchor_handle = sp._resolve_one(model, anchor.object, row.id)
        anchor_value = model.operand_value(sp._resolve_one(model, anchor.value, row.id))
        found = model.read_field(anchor_handle, anchor.offset, anchor.prop)
        if found != anchor_value:
            raise sp.PatchSkip(row.id, f"anchor {anchor.object.path} ({anchor.object.cls}) "
                               f"+0x{anchor.offset:04X} points at 0x{found:08X}, expected "
                               f"{anchor.value.path} ({anchor.value.cls})")
    found = model.read_field(handle, row.offset, row.prop)
    if found == 0:
        return FieldPlan(row, handle, value, 1)
    if found == value:
        return FieldPlan(row, handle, value, 0)
    raise sp.PatchSkip(row.id, f"+0x{row.offset:04X} points at 0x{found:08X}, expected None or "
                       f"{row.value.path} ({row.value.cls})")


def plan_rows(model, rows, players):
    plans = []
    for row in rows:
        if isinstance(row, sp.ScriptRow):
            plans.append(sp._plan_script(model, row, players))
        else:
            plans.append(plan_object_field(model, row))
    return plans


def _script_plan_line(entry):
    head = f"{entry.row.id} {entry.row.function.path}: "
    if entry.grown_size:
        line = head + f"script grown to {entry.grown_size} bytes"
        if entry.writes > 0:
            line += f", with {entry.writes} write(s)"
        return line
    if entry.writes == 0:
        return head + "already applied"
    return head + f"{entry.writes} write(s)"


def row_lines(plans):
    lines = []
    for entry in plans:
        if isinstance(entry, sp.ScriptPlan):
            lines.append(_script_plan_line(entry))
        else:
            head = f"{entry.row.id} {entry.row.object.path}: "
            if entry.writes == 0:
                lines.append(head + "already applied")
            else:
                lines.append(head + f"{entry.writes} write(s)")
    return lines


BEN_CARMINE = sp.BEN_CARMINE
RED_SHIRT = sp.RED_SHIRT


def ascii_z(text):
    return text.encode("ascii") + b"\0"


COGGEAR_MESH = sp.Ref("GearGame.Default__GearPawn_COGGear.GearPawnMesh", "SkeletalMeshComponent")
DOM_MESH = sp.Ref("GearGame.Default__GearPawn_COGDom.GearPawnMesh", "SkeletalMeshComponent")
MARCUS_MESH = sp.Ref("GearGame.Default__GearPawn_COGMarcus.GearPawnMesh", "SkeletalMeshComponent")
DOM_SKEL = sp.Ref("COG_Dom.Game.COG_Dom_Game_CamSkel", "SkeletalMesh")
MARCUS_SKEL = sp.Ref("COG_MarcusFenix.COG_Marcus_Fenix_CamSkel", "SkeletalMesh")
DOM_PHYSICS = sp.Ref("COG_Dom.COG_Dom_CamSkel_Physics", "PhysicsAsset")
MARCUS_PHYSICS = sp.Ref("COG_MarcusFenix.COG_Marcus_Fenix_CamSkel_Physics", "PhysicsAsset")
# Measured in game (2026-10-16): tools/ue3/layout.py undercounts SkeletalMeshComponent by 12 bytes.
SKELETAL_MESH_OFFSET = 480
PHYSICS_ASSET_OFFSET = 508

SUGGEST = sp.Ref("GearGame.GearGameSP_Base.SuggestPawnClassFor", "Function")
RESTART = sp.Ref("GearGame.GearGame.RestartPlayer", "Function")
POST_LOGIN = sp.Ref("GearGame.GearGameSP_Base.PostLogin", "Function")
ENTER = sp.Ref("GearGame.GearPawn.OnGearEnterVehicle", "Function")
EXIT = sp.Ref("GearGame.GearPawn.OnGearExitVehicle", "Function")

NEW_PLAYER = sp.Ref("GearGame.GearGame.RestartPlayer.NewPlayer", "ObjectProperty")
PAWN = sp.Ref("Engine.Controller.Pawn", "ObjectProperty")
FIND_INVENTORY = sp.Ref("Engine.Pawn.FindInventoryType", "Function")
CREATE_INVENTORY = sp.Ref("Engine.Pawn.CreateInventory", "Function")
PISTOL = sp.Ref("GearGame.GearWeap_COGPistol", "Class")
SHOTGUN = sp.Ref("GearGame.GearWeap_Shotgun", "Class")
LOGIN_PC = sp.Ref("GearGame.GearGameSP_Base.PostLogin.PC", "ObjectProperty")
DEFAULT_PAWN_CLASS = sp.Ref("GearGame.GearPC.DefaultPawnClass", "ClassProperty")
ENTERED = sp.Ref("GearGame.GearPawn.OnGearEnterVehicle.bEntered", "BoolProperty")
ENTER_ACTION = sp.Ref("GearGame.GearPawn.OnGearEnterVehicle.inAction", "ObjectProperty")
ENTER_TARGET = sp.Ref("GearGame.SeqAct_GearEnterVehicle.TargetVehicle", "ObjectProperty")
ENTER_AI = sp.Ref("GearGame.GearPawn.OnGearEnterVehicle.AI", "ObjectProperty")
GEAR_VEHICLE = sp.Ref("GearGame.GearVehicle", "Class")
OBJECT_CLASS = sp.Ref("Core.Object.Class", "ClassProperty")
WORLD_INFO = sp.Ref("Engine.Actor.WorldInfo", "ObjectProperty")
ALL_CONTROLLERS = sp.Ref("Engine.WorldInfo.AllControllers", "Function")
GEAR_PC = sp.Ref("GearGame.GearPC", "Class")
GOD_MODE = sp.Ref("Engine.Controller.bGodMode", "BoolProperty")
EXIT_ACTION = sp.Ref("GearGame.GearPawn.OnGearExitVehicle.inAction", "ObjectProperty")
EXIT_VEHICLE = sp.Ref("GearGame.GearPawn.OnGearExitVehicle.GV", "ObjectProperty")
EXIT_TURRET = sp.Ref("GearGame.GearPawn.OnGearExitVehicle.GT", "ObjectProperty")
PAWN_CONTROLLER = sp.Ref("Engine.Pawn.Controller", "ObjectProperty")
TELEPORT_SPOT = sp.Ref("GearGame.GearPC.FindAvailableTeleportSpot", "Function")
M, D, GP, COG_GEAR = sp.MARCUS, sp.DOM, sp.GEAR_PAWN, sp.COG_GEAR
SYNC = sp.Ref("GearGame.GearGameSP_Base.SyncKismetSetProperties", "Function")
OTHER_PC = sp.Ref("GearGame.GearGameSP_Base.SyncKismetSetProperties.OtherPC", "ObjectProperty")
SYNC_PC = sp.Ref("GearGame.GearGameSP_Base.SyncKismetSetProperties.PC", "ObjectProperty")


def raw(hex_text):
    return sp.Raw(bytes.fromhex(hex_text))


SUGGEST_GROWN_CHECKS = (
    sp.Check(0x61, b"\x04\x20"), sp.Check(0x7E, b"\x04\x20"), sp.Check(0x85, b"\x01\x00\x77\x1C"),
    sp.Check(0x8D, b"\x1F"), sp.Check(0x8E, ascii_z(BEN_CARMINE)), sp.Check(0xBA, b"\x20"),
    sp.Check(0xBF, b"\x16\x2A\x16\x04\x13"), sp.Check(0xC8, b"\x1C"), sp.Check(0x100, b"\x04"),
    sp.Check(0x106, b"\x1C"), sp.Check(0x10B, b"\x1F"), sp.Check(0x10C, ascii_z(RED_SHIRT)),
    sp.Check(0x131, b"\x20"), sp.Check(0x136, b"\x16\x04\x3A"), sp.Check(0x13D, b"\x53"))
SUGGEST_GROWN_OPERANDS = (
    sp.Operand(0x63, M), sp.Operand(0x80, D), sp.Operand(0x89, sp.FIND_OBJECT),
    sp.Operand(0xBB, sp.CLASS_CLASS), sp.Operand(0xC4, GP), sp.Operand(0xC9, sp.FIND_OBJECT),
    sp.Operand(0x107, sp.FIND_OBJECT), sp.Operand(0x132, sp.CLASS_CLASS),
    sp.Operand(0x139, sp.SUGGEST_RETURN))
RETURN_COG_GEAR = (sp.ByteEdit(0x101, 0x13, 0x20),)
COG_GEAR_OPERAND = (sp.OperandEdit(0x102, GP, COG_GEAR),)
CB3C_CHECKS = SUGGEST_GROWN_CHECKS[:2] + (sp.Check(0x84, b"\x07"),) + SUGGEST_GROWN_CHECKS[2:]


def _cast_new_player_to_gearpc():
    return sv.R(b"\x2E") + (GEAR_PC,) + sv.local(NEW_PLAYER)


def _new_player_pawn():
    return sv.ctx(sv.local(NEW_PLAYER), sv.inst(PAWN), 4)


def _new_player_default_pawn_class():
    return sv.ctx(_cast_new_player_to_gearpc(), sv.inst(DEFAULT_PAWN_CLASS), 4)


def _cb4_spawn_guard():
    cast_ok = sv.ne(_cast_new_player_to_gearpc(), sv.NONE)
    return sv.and_(sv.and_(cast_ok, sv.is_extra(_new_player_default_pawn_class())),
                   sv.ne(_new_player_pawn(), sv.NONE))


def _pawn_find_inventory(weapon):
    member = sv.R(b"\x1C") + (FIND_INVENTORY,) + sv.objconst(weapon) + sv.R(b"\x4A\x16")
    return sv.ctx(_new_player_pawn(), member, 4)


def _pawn_create_inventory(weapon):
    member = sv.R(b"\x1C") + (CREATE_INVENTORY,) + sv.objconst(weapon) + sv.R(b"\x27\x16")
    return sv.ctx(_new_player_pawn(), member, 4)


CB4_TAIL, _CB4_LABELS, CB4_GROWN_SIZE = sv.assemble(0x0180, [
    (None, lambda L: sv.jumpifnot(L["RET"], _cb4_spawn_guard())),
    (None, lambda L: sv.jumpifnot(L["GNASHER"], sv.eq(_pawn_find_inventory(PISTOL), sv.NONE))),
    (None, lambda L: _pawn_create_inventory(PISTOL)),
    ("GNASHER", lambda L: sv.jumpifnot(L["RET"], sv.eq(_pawn_find_inventory(SHOTGUN), sv.NONE))),
    (None, lambda L: _pawn_create_inventory(SHOTGUN)),
    ("RET", lambda L: sv.R(b"\x04\x0B")),
    (None, lambda L: sv.R(b"\x53")),
])


def _other_default_pawn_class():
    return sv.ctx(sv.local(OTHER_PC), sv.inst(DEFAULT_PAWN_CLASS), 4)


def _other_is_hero():
    return sv.or_(sv.eq(_other_default_pawn_class(), sv.objconst(M)),
                  sv.eq(_other_default_pawn_class(), sv.objconst(D)))


def _sync_keep_other():
    return sv.and_(sv.ne(sv.local(OTHER_PC), sv.local(SYNC_PC)), _other_is_hero())


CR3_TAIL, _CR3_LABELS, CR3_GROWN_SIZE = sv.assemble(0x01CB, [
    (None, lambda L: sv.jumpifnot(0x01C7, _sync_keep_other())),
    (None, lambda L: sv.jump(0x002C)),
    (None, lambda L: sv.R(b"\x53")),
])

CR1_TAIL = (
    raw("07 01 6e 2d 00"), ENTERED,
    raw("07 01 6e 77 2e"), GEAR_VEHICLE, raw("19 00"), ENTER_ACTION,
    raw("00 05 00 04 01"), ENTER_TARGET,
    raw("2a 16 07 01 6e 84 77 13"), M, raw("01"), OBJECT_CLASS,
    raw("2a 16 18 00 0e 77 13"), D, raw("01"), OBJECT_CLASS,
    raw("2a 16 16 2f 19 01"), WORLD_INFO, raw("00 77 00 00 1c"), ALL_CONTROLLERS,
    raw("20"), GEAR_PC, raw("00"), ENTER_AI,
    raw("16 01 6d 07 01 6c 82 82 72 13"), M, raw("19 00"), ENTER_AI,
    raw("00 05 00 04 01"), DEFAULT_PAWN_CLASS,
    raw("2a 16 18 00 18 72 13"), D, raw("19 00"), ENTER_AI,
    raw("00 05 00 04 01"), DEFAULT_PAWN_CLASS,
    raw("2a 16 16 18 00 18 77 2e"), GP, raw("19 00"), ENTER_AI,
    raw("00 05 00 04 01"), PAWN,
    raw("2a 16 16 14 19 00"), ENTER_AI, raw("00 06 00 04 2d 01"), GOD_MODE,
    raw("27 31 30 04 0b 53"),
)

CR2_TAIL = (
    raw("07 01 c0 77 00"), EXIT_VEHICLE,
    raw("2a 16 07 01 c0 84 77 13"), M, raw("01"), OBJECT_CLASS,
    raw("2a 16 18 00 0e 77 13"), D, raw("01"), OBJECT_CLASS,
    raw("2a 16 16 2f 19 01"), WORLD_INFO, raw("00 b2 00 00 1c"), ALL_CONTROLLERS,
    raw("20"), GEAR_PC, raw("00"), EXIT_TURRET,
    raw("16 01 bf 07 01 be 82 82 82 19 00"), EXIT_TURRET,
    raw("00 06 00 04 2d 01"), GOD_MODE,
    raw("18 00 18 72 13"), M, raw("19 00"), EXIT_TURRET,
    raw("00 05 00 04 01"), DEFAULT_PAWN_CLASS,
    raw("2a 16 16 18 00 18 72 13"), D, raw("19 00"), EXIT_TURRET,
    raw("00 05 00 04 01"), DEFAULT_PAWN_CLASS,
    raw("2a 16 16 18 00 18 77 2e"), GP, raw("19 00"), EXIT_TURRET,
    raw("00 05 00 04 01"), PAWN,
    raw("2a 16 16 14 19 00"), EXIT_TURRET, raw("00 06 00 04 2d 01"), GOD_MODE,
    raw("28 19 2e"), GEAR_PC, raw("01"), PAWN_CONTROLLER, raw("00 17 00 04 1c"),
    TELEPORT_SPOT, raw("19 00"), EXIT_TURRET, raw("00 05 00 04 01"), PAWN,
    raw("4a 4a 16 31 30 04 0b 53"),
)

ROWS = {
    "CB-1": ObjectFieldRow("CB-1", "Default__GearPawn_COGGear.GearPawnMesh.SkeletalMesh: None -> Dom's body",
        COGGEAR_MESH, SKELETAL_MESH_OFFSET, "SkeletalMesh", DOM_SKEL,
        anchors=(FieldAnchor(DOM_MESH, SKELETAL_MESH_OFFSET, "SkeletalMesh", DOM_SKEL),
                 FieldAnchor(MARCUS_MESH, SKELETAL_MESH_OFFSET, "SkeletalMesh", MARCUS_SKEL))),
    "CB-2": ObjectFieldRow("CB-2", "Default__GearPawn_COGGear.GearPawnMesh.PhysicsAsset: None -> Dom's physics asset",
        COGGEAR_MESH, PHYSICS_ASSET_OFFSET, "PhysicsAsset", DOM_PHYSICS,
        anchors=(FieldAnchor(DOM_MESH, PHYSICS_ASSET_OFFSET, "PhysicsAsset", DOM_PHYSICS),
                 FieldAnchor(MARCUS_MESH, PHYSICS_ASSET_OFFSET, "PhysicsAsset", MARCUS_PHYSICS))),
    "CB-3": sp.ScriptRow("CB-3", "players 3+ are always GearPawn_COGGear: goto 0x0100, return class'GearPawn_COGGear'",
        SUGGEST, 318, checks=SUGGEST_GROWN_CHECKS, operands=SUGGEST_GROWN_OPERANDS,
        byte_edits=(sp.ByteEdit(0x84, 0x07, 0x06),) + RETURN_COG_GEAR,
        operand_edits=COG_GEAR_OPERAND),
    "CB-3c": sp.ScriptRow("CB-3c", "players 3+ are Ben Carmine where FindObject finds him, else GearPawn_COGGear",
        SUGGEST, 318, checks=CB3C_CHECKS, operands=SUGGEST_GROWN_OPERANDS,
        byte_edits=RETURN_COG_GEAR, operand_edits=COG_GEAR_OPERAND),
    "CB-4": sp.ScriptRow("CB-4", "after a player 3+ spawns (a GearPC whose DefaultPawnClass is not None, Marcus or Dom): give GearWeap_COGPistol and GearWeap_Shotgun when missing",
        RESTART, 387,
        checks=(sp.Check(0x00, bytes.fromhex("1C")), sp.Check(0x05, bytes.fromhex("00")),
                sp.Check(0x0A, bytes.fromhex("16")),
                sp.Check(0x0B, bytes.fromhex("07 01 80 77 19 00")),
                sp.Check(0x15, bytes.fromhex("00 05 00 04 01")),
                sp.Check(0x1E, bytes.fromhex("2A 16")),
                sp.Check(0xEC, bytes.fromhex("07 01 80")),
                sp.Check(0x17E, bytes.fromhex("31 30 04 0B 53"))),
        operands=(sp.Operand(0x01, sp.Ref("Engine.GameInfo.RestartPlayer", "Function")),
                  sp.Operand(0x06, NEW_PLAYER), sp.Operand(0x11, NEW_PLAYER),
                  sp.Operand(0x1A, PAWN)),
        grow=sp.Grow(0x0180, CB4_GROWN_SIZE, CB4_TAIL)),
    "CB-5": sp.ScriptRow("CB-5", "a joiner who is not Dom spawns at once: LocalPlayer(PC.Player) != None -> class<GearPawn_COGDom>(PC.DefaultPawnClass) == None",
        POST_LOGIN, 1690,
        checks=(sp.Check(0x3A4, bytes.fromhex("07 03 DD 84 81 2D 00")),
                sp.Check(0x3AF, bytes.fromhex("16 18 00 18")),
                sp.Check(0x3B9, bytes.fromhex("19 00")),
                sp.Check(0x3BF, bytes.fromhex("00 05 00 04 01")),
                sp.Check(0x3C8, bytes.fromhex("2A 16 16 1B")),
                sp.Check(0x3DD, bytes.fromhex("07 04 0D 19 00"))),
        operands=(sp.Operand(0x3AB, sp.Ref("GearGame.GearGameSP_Base.PostLogin.bFoundPawn", "BoolProperty")),
                  sp.Operand(0x3BB, LOGIN_PC), sp.Operand(0x3E2, LOGIN_PC)),
        byte_edits=(sp.ByteEdit(0x3B3, 0x77, 0x72), sp.ByteEdit(0x3B4, 0x2E, 0x13)),
        operand_edits=(sp.OperandEdit(0x3B5, sp.Ref("Engine.LocalPlayer", "Class"), D),
                       sp.OperandEdit(0x3C4, sp.Ref("Engine.PlayerController.Player", "ObjectProperty"),
                                      DEFAULT_PAWN_CLASS))),
    "CR-1": sp.ScriptRow("CR-1", "a hero enters a GearVehicle: god mode for every player 3+ on foot",
        ENTER, 169,
        checks=(sp.Check(0x00, bytes.fromhex("0F 00")), sp.Check(0x06, bytes.fromhex("01")),
                sp.Check(0x0B, bytes.fromhex("07 00 5B 77 2E")),
                sp.Check(0x14, bytes.fromhex("19 00")),
                sp.Check(0x1A, bytes.fromhex("00 05 00 04 01")),
                sp.Check(0x25, bytes.fromhex("14 2D 00")),
                sp.Check(0x66, bytes.fromhex("07 00 87 2D 00")),
                sp.Check(0x84, bytes.fromhex("06 00 A6")),
                sp.Check(0xA6, bytes.fromhex("04 0B 53"))),
        operands=(sp.Operand(0x02, ENTER_AI),
                  sp.Operand(0x07, sp.Ref("GearGame.GearPawn.MyGearAI", "ObjectProperty")),
                  sp.Operand(0x10, sp.Ref("Engine.Vehicle", "Class")),
                  sp.Operand(0x16, ENTER_ACTION), sp.Operand(0x1F, ENTER_TARGET),
                  sp.Operand(0x28, ENTERED), sp.Operand(0x6B, ENTERED)),
        grow=sp.Grow(0x00A6, 369, CR1_TAIL)),
    "CR-2": sp.ScriptRow("CR-2", "a hero leaves a GearVehicle: players 3+ in god mode lose it and are placed next to him",
        EXIT, 216,
        checks=(sp.Check(0x00, bytes.fromhex("07 00 D5 77 19 00")),
                sp.Check(0x0A, bytes.fromhex("00 05 00 04 01")),
                sp.Check(0x13, bytes.fromhex("2A 16")),
                sp.Check(0x6D, bytes.fromhex("0F 00")), sp.Check(0x73, bytes.fromhex("2E")),
                sp.Check(0x78, bytes.fromhex("19 00")),
                sp.Check(0x87, bytes.fromhex("07 00 D5 77 00")),
                sp.Check(0x90, bytes.fromhex("2A 16")),
                sp.Check(0xBC, bytes.fromhex("19 00")),
                sp.Check(0xD5, bytes.fromhex("04 0B 53"))),
        operands=(sp.Operand(0x06, EXIT_ACTION),
                  sp.Operand(0x0F, sp.Ref("GearGame.SeqAct_GearExitVehicle.TargetVehicle", "ObjectProperty")),
                  sp.Operand(0x6F, EXIT_VEHICLE), sp.Operand(0x74, GEAR_VEHICLE),
                  sp.Operand(0x7A, EXIT_ACTION), sp.Operand(0x8C, EXIT_VEHICLE),
                  sp.Operand(0xBE, EXIT_VEHICLE)),
        grow=sp.Grow(0x00D5, 451, CR2_TAIL)),
    "CR-3": sp.ScriptRow("CR-3", "a joiner copies Kismet's cinematic and god-mode flags from Marcus or Dom, never from a player 3+",
        SYNC, 460,
        checks=(sp.Check(0x00, bytes.fromhex("2F 19 01")),
                sp.Check(0x07, bytes.fromhex("01 BD 00 00 1C")),
                sp.Check(0x10, bytes.fromhex("20")),
                sp.Check(0x15, bytes.fromhex("00")),
                sp.Check(0x1A, bytes.fromhex("16 01 C8")),
                sp.Check(0x1E, bytes.fromhex("01")),
                sp.Check(0x20, bytes.fromhex("77 00")),
                sp.Check(0x26, bytes.fromhex("00")),
                sp.Check(0x2B, bytes.fromhex("16 14")),
                sp.Check(0x01C4, bytes.fromhex("06 01 C8 31 30 04 0B 53"))),
        operands=(sp.Operand(0x03, WORLD_INFO),
                  sp.Operand(0x0C, ALL_CONTROLLERS),
                  sp.Operand(0x11, GEAR_PC),
                  sp.Operand(0x16, OTHER_PC),
                  sp.Operand(0x22, OTHER_PC),
                  sp.Operand(0x27, SYNC_PC)),
        byte_edits=(sp.ByteEdit(0x1D, 0x07, 0x06), sp.ByteEdit(0x1F, 0xC7, 0xCB)),
        grow=sp.Grow(0x01CB, CR3_GROWN_SIZE, CR3_TAIL)),
}

ROW_ORDER = ("CB-1", "CB-2", "CB-3", "CB-3c", "CB-4", "CB-5", "CR-1", "CR-2", "CR-3")


def coop_client_rows():
    return [ROWS["CB-1"], ROWS["CB-2"]]


def coop_body_rows(carmine=False, marcus_clones=False):
    first = [] if marcus_clones else [ROWS["CB-3c" if carmine else "CB-3"]]
    return first + [ROWS["CB-4"], ROWS["CB-5"]]


def coop_ride_rows():
    return [ROWS["CR-1"], ROWS["CR-2"], ROWS["CR-3"]]


def check_table():
    for row in ROWS.values():
        if not isinstance(row, sp.ScriptRow):
            continue
        try:
            sp._check_edits_in_kept_prefix(row)
        except sp.PatchSkip as e:
            raise ValueError(f"{e.row}: {e.reason}") from None


def _field_lines(row):
    name = "Row" + row.id.replace("-", "_")
    lines = [f"// {row.summary}", f"ObjectFieldRow {name}() {{", "  return {"]
    lines.append(f'      .id = "{row.id}",')
    lines.append(f"      .object = {sp._ref_cpp(row.object)},")
    lines.append(f"      .offset = {row.offset},")
    lines.append(f"      .value = {sp._ref_cpp(row.value)},")
    lines.append("      .anchors = {")
    for anchor in row.anchors:
        lines.append(f"          {{{sp._ref_cpp(anchor.object)}, {anchor.offset}, "
                     f"{sp._ref_cpp(anchor.value)}}},")
    lines.append("      },")
    lines.append("  };")
    lines.append("}")
    return lines


def render_cpp():
    check_table()
    lines = [
        "// gow2 - ReXGlue Recompiled Project",
        "//",
        "// The Stage 2 follow-up rows of docs/COOP_N_PLAYERS.md (items 8, 9 and 10) for players 3+. Printed",
        "// by `python -I tools/script_patch_body.py --cpp`; edit the rows there, not here. Check them against",
        "// your own decompressed packages with `python -I tools/script_patch_body.py --check <folder>`.",
        "",
        '#include "gow2_script_patch_body.h"',
        "",
        "namespace rex::glue::script_patch {",
        "",
        "namespace {",
        "",
    ]
    for rid in ROW_ORDER:
        row = ROWS[rid]
        if isinstance(row, sp.ScriptRow):
            lines.extend(sp._script_lines(row))
        else:
            lines.extend(_field_lines(row))
        lines.append("")
    lines.extend([
        "}  // namespace",
        "",
        "PatchGroup CoopClientGroup() {",
        '  return {.name = "coop-client", .object_fields = {RowCB_1(), RowCB_2()}};',
        "}",
        "",
        "PatchGroup CoopBodyGroup(bool carmine, bool marcus_clones) {",
        '  PatchGroup group{.name = "coop-body"};',
        "  if (!marcus_clones) group.scripts.push_back(carmine ? RowCB_3c() : RowCB_3());",
        "  group.scripts.push_back(RowCB_4());",
        "  group.scripts.push_back(RowCB_5());",
        "  return group;",
        "}",
        "",
        "PatchGroup CoopRideGroup() {",
        "  return {",
        '      .name = "coop-ride",',
        "      .scripts = {RowCR_1(), RowCR_2(), RowCR_3()},",
        "  };",
        "}",
        "",
        "}  // namespace rex::glue::script_patch",
    ])
    return "\n".join(lines) + "\n"


COMPONENT_TAGS_AT = 16


class _TagNames:
    def __init__(self, pkg):
        self.pkg = pkg

    def name_at(self, idx, num):
        base = self.pkg.names[idx]
        return base if num == 0 else f"{base}_{num - 1}"

    def path(self, ref):
        return ref


class BodyModel(sp.PackageModel):
    def __init__(self, pkg_dir):
        super().__init__(pkg_dir)
        self._paths = {}

    def _full_path(self, index):
        if index in self._paths:
            return self._paths[index]
        if index > 0:
            outer = index
            while self.pkg.exports[outer - 1]["outer"] > 0:
                outer = self.pkg.exports[outer - 1]["outer"]
            if self.pkg.obj_class(outer) == "Package" and self.pkg.exports[outer - 1]["export_flags"] & 1:
                path = self.pkg.obj_path(index)
                self._paths[index] = path
                return path
        path = super()._full_path(index)
        self._paths[index] = path
        return path

    def read_field(self, handle, offset, prop):
        import cdo
        names = _TagNames(self.pkg)
        index = handle
        while index > 0:
            e = self.pkg.exports[index - 1]
            blob = self.pkg.b[e["serial_offset"]:e["serial_offset"] + e["serial_size"]]
            for name, _idx, typ, value in cdo.read_tags(names, blob, COMPONENT_TAGS_AT):
                if name == prop and typ == "ObjectProperty":
                    return value & 0xFFFFFFFF
            arch = e["archetype"]
            if arch < 0:
                raise sp.PatchSkip("-", f"{self._full_path(index)}: archetype is an import, "
                                   "its tags are not in this package")
            index = arch
        return 0


class CoopOverlay:
    def __init__(self, model, scripts):
        self.model = model
        self.scripts = scripts

    def script(self, handle):
        if handle in self.scripts:
            data = self.scripts[handle]
            return sp.ScriptArray(data, len(data), len(data))
        return self.model.script(handle)

    def is_overlaid(self, handle):
        return handle in self.scripts

    def __getattr__(self, name):
        return getattr(self.model, name)


def _decode_old_lines(ue3script, model, overlay, handle):
    before = model.world.read_function(model.pkg, handle)
    if overlay.is_overlaid(handle):
        old_lines, _, _ = sp._decode_standalone(ue3script, model, handle, before.path,
                                                overlay.scripts[handle])
    else:
        old_lines, _, _ = ue3script.decompile(model.world, before)
    return before, old_lines


def _decode_new_lines(ue3script, model, overlay, handle, before, new_script):
    own_size = before.code_end - before.code_start
    if len(new_script) == own_size and not overlay.is_overlaid(handle):
        return sp._decode_spliced(ue3script, model, handle, new_script)
    return sp._decode_standalone(ue3script, model, handle, before.path, new_script)


def _check_script_row(ue3script, model, overlay, row, players):
    print(f"== {row.id} {row.function.path} ScriptSize={row.script_size}")
    try:
        entry = sp._plan_script(overlay, row, players)
    except sp.PatchSkip as e:
        print(f"   SKIP {e.row}: {e.reason}")
        return False
    try:
        before, old_lines = _decode_old_lines(ue3script, model, overlay, entry.handle)
    except Exception as e:
        print(f"   original decode failed: {e}")
        return False
    try:
        new_lines, ok, consumed = _decode_new_lines(ue3script, model, overlay, entry.handle,
                                                   before, entry.new)
    except Exception as e:
        print(f"   patched decode failed: {e}")
        return False
    sp._show_diff(old_lines, new_lines)
    good = ok and consumed == len(entry.new)
    print(f"   decodes to its end: {good} at 0x{consumed:X} of 0x{len(entry.new):X}")
    if row.grow is not None:
        targets = sp._kept_targets(old_lines, row.grow.keep)
        print(f"   jumps in the kept prefix: {[f'0x{t:X}' for t in sorted(targets)]}")
        bad = [t for t in targets if t > row.grow.keep]
        if bad:
            print(f"   jumps into the replaced tail: {[f'0x{t:X}' for t in sorted(bad)]}")
            return False
    return good


def _chain_length(model, handle):
    count = 0
    index = handle
    while index > 0:
        count += 1
        index = model.pkg.exports[index - 1]["archetype"]
    return count


def _check_anchor(model, anchor):
    handle = sp._resolve_one(model, anchor.object, "-")
    value = model.operand_value(sp._resolve_one(model, anchor.value, "-"))
    found = model.read_field(handle, anchor.offset, anchor.prop)
    if found != value:
        print(f"   anchor {anchor.object.path}.{anchor.prop} points at 0x{found:08X}, "
              f"expected {anchor.value.path} ({anchor.value.cls})")
        return False
    print(f"   anchor {anchor.object.path}.{anchor.prop} = {anchor.value.path}")
    return True


def _check_field_row(model, row):
    try:
        entry = plan_object_field(model, row)
    except sp.PatchSkip as e:
        print(f"   SKIP {e.row}: {e.reason}")
        return False
    try:
        current = model.read_field(entry.handle, row.offset, row.prop)
    except sp.PatchSkip as e:
        print(f"   SKIP {e.row}: {e.reason}")
        return False
    if current != 0:
        print(f"   SKIP {row.id}: +0x{row.offset:04X} holds 0x{current:08X}, expected None")
        return False
    print(f"== {row.id} {row.object.path}.{row.prop}: None along the archetype chain "
          f"({_chain_length(model, entry.handle)} exports)")
    ok = True
    for anchor in row.anchors:
        try:
            if not _check_anchor(model, anchor):
                ok = False
        except sp.PatchSkip as e:
            print(f"   SKIP {e.row}: {e.reason}")
            ok = False
    return ok


GROUP_TITLES = ("coop-client", "coop-body", "coop-body (gow2_coop_extra_carmine)", "coop-ride")


def _group_row_sets():
    return [coop_client_rows(), coop_body_rows(), coop_body_rows(carmine=True), coop_ride_rows()]


def check_package(pkg_dir, players):
    ue3script, _ = sp._ue3_modules()
    print(f"== script_patch_body --check {pkg_dir} players={players}")
    try:
        model = BodyModel(pkg_dir)
    except OSError as e:
        print(f"error: cannot load packages from {pkg_dir}: {e}", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"error: cannot load packages from {pkg_dir}: {type(e).__name__}({e})", file=sys.stderr)
        return 1
    try:
        stage1 = sp.plan_group(model, sp.coop_stage1_rows(False), players)
    except sp.PatchSkip as e:
        print(f"SKIP {e.row}: {e.reason}")
        return 1
    overlay = CoopOverlay(model, {e.handle: e.new for e in stage1.entries
                                  if isinstance(e, sp.ScriptPlan)})
    failed = False
    for rid in ("CB-3", "CB-3c", "CB-4", "CB-5", "CR-1", "CR-2", "CR-3"):
        if not _check_script_row(ue3script, model, overlay, ROWS[rid], players):
            failed = True
    for rid in ("CB-1", "CB-2"):
        if not _check_field_row(model, ROWS[rid]):
            failed = True
    print("== groups")
    for title, rows in zip(GROUP_TITLES, _group_row_sets()):
        print(f"== group {title}")
        try:
            plans = plan_rows(overlay, rows, players)
        except sp.PatchSkip as e:
            print(f"   SKIP {e.row}: {e.reason}")
            failed = True
            continue
        for line in row_lines(plans):
            print(f"   {line}")
    print("== C++ table")
    print(render_cpp(), end="")
    return 1 if failed else 0


def parse_args(argv=None):
    p = argparse.ArgumentParser(description="Offline twin of the Stage 2 body/ride patch groups.")
    p.add_argument("--check", metavar="PKGDIR", default=None,
                   help="check the rows against decompressed packages in PKGDIR")
    p.add_argument("--cpp", action="store_true", help="print the C++ table file")
    p.add_argument("--write", action="store_true", help="write the C++ table file (with --cpp)")
    p.add_argument("--players", type=int, default=4, help="player count 3..255 (default 4)")
    return p.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    if not 3 <= args.players <= 255:
        print(f"error: --players must be 3..255, got {args.players}", file=sys.stderr)
        return 2
    if args.write and not args.cpp:
        print("error: --write needs --cpp", file=sys.stderr)
        return 2
    if args.cpp:
        text = render_cpp()
        if args.write:
            out = Path(__file__).resolve().parent.parent / "src" / "gow2_script_patch_body_table.cpp"
            with open(out, "w", encoding="utf-8", newline="\n") as f:
                f.write(text)
            print(str(out))
        else:
            sp._write_stdout(text)
        return 0
    if args.check is None:
        print("error: need --check PKGDIR or --cpp", file=sys.stderr)
        return 2
    return check_package(args.check, args.players)


if __name__ == "__main__":
    sys.exit(main())
