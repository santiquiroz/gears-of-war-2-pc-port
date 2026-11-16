import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import pytest

import script_patch as sp
import script_patch_survival as sv


class FakeModel:
    def __init__(self):
        self.objects = {}
        self.values = {}
        self.scripts = {}
        self.ints = {}

    def add(self, ref, script=None, int_value=None):
        handle = len(self.values)
        self.objects.setdefault(ref, []).append(handle)
        self.values[handle] = 0x80001000 + handle
        if script is not None:
            self.scripts[handle] = script
        if int_value is not None:
            self.ints[handle] = int_value
        return handle

    def find(self, ref):
        return list(self.objects.get(ref, []))

    def operand_value(self, handle):
        return self.values[handle]

    def script(self, handle):
        return self.scripts[handle]

    def read_int(self, handle, row):
        return self.ints[handle]


PATH_CLASS = {
    "GearGame.GearGameSP_Base.SpawnDefaultPawnFor.ResultPawn": "ObjectProperty",
    "Engine.Actor.bKillDuringLevelTransition": "BoolProperty",
    "GearGame.GearPC.DefaultPawnClass": "ClassProperty",
    "GearGame.GearPawn_COGMarcus": "Class",
    "GearGame.GearPawn_COGDom": "Class",
    "Engine.Actor.bPendingDelete": "BoolProperty",
    "Engine.Controller.Pawn": "ObjectProperty",
    "GearGame.GearGameSP_Base": "Class",
    "Engine.Actor.WorldInfo": "ObjectProperty",
    "Engine.WorldInfo.Game": "ObjectProperty",
    "GearGame.GearGameSP_Base.RestartPlayer": "Function",
    "GearGame.GearPC.PlayAnyaCoopDeathSound": "Function",
    "Engine.WorldInfo.AllControllers": "Function",
    "GearGame.GearPC": "Class",
    "GearGame.GearPC.ProcessGameOver.PC": "ObjectProperty",
    "GearGame.GearPC.ClientGameOver": "Function",
    "GearGame.GearPC.bCheckpointDummy": "BoolProperty",
    "GearGame.GearPC.Destroyed.Gear": "ObjectProperty",
    "Core.Object.Class": "ClassProperty",
    "GearGame.GearPawn": "Class",
    "GearGame.GearPawn.IsDoingSpecialMove": "Function",
    "GearGame.GearPC.Destroyed.OldPawn": "ObjectProperty",
    "GearGame.GearPC.UnPossess": "Function",
    "GearGame.GearPC.SaveCheckpoint.PC": "ObjectProperty",
    "GearGame.GearGame.TeamHasAnyChanceOfSurvival.PC": "ObjectProperty",
    "GearGame.GearGame.bInSoloSplit": "BoolProperty",
    "GearGame.GearGame.bInCoopSplit": "BoolProperty",
    "GearGame.GearPRI": "Class",
    "Engine.Controller.PlayerReplicationInfo": "ObjectProperty",
    "GearGame.GearPRI.SquadName": "NameProperty",
    "GearGame.GearGame.TeamHasAnyChanceOfSurvival.DBNOPawn": "ObjectProperty",
    "Engine.Pawn.PlayerReplicationInfo": "ObjectProperty",
    "GearGame.GearGame.TeamHasAnyChanceOfSurvival.bFoundReviver": "BoolProperty",
}

EXPECTED_TAILS = {
    "S2A-1": "14 19 00 [GearGame.GearGameSP_Base.SpawnDefaultPawnFor.ResultPawn] 00 06 00 04 2D 01 [Engine.Actor.bKillDuringLevelTransition] 28 06 00 95 53",
    "S2A-2": "07 00 67 82 82 77 01 [GearGame.GearPC.DefaultPawnClass] 2A 16 18 00 0D 77 01 [GearGame.GearPC.DefaultPawnClass] 20 [GearGame.GearPawn_COGMarcus] 16 16 18 00 0D 77 01 [GearGame.GearPC.DefaultPawnClass] 20 [GearGame.GearPawn_COGDom] 16 16 07 00 65 82 81 2D 01 [Engine.Actor.bPendingDelete] 16 18 00 09 72 01 [Engine.Controller.Pawn] 2A 16 16 19 2E [GearGame.GearGameSP_Base] 19 01 [Engine.Actor.WorldInfo] 00 05 00 04 01 [Engine.WorldInfo.Game] 00 07 00 00 1C [GearGame.GearGameSP_Base.RestartPlayer] 17 16 04 0B 1C [GearGame.GearPC.PlayAnyaCoopDeathSound] 16 2F 19 01 [Engine.Actor.WorldInfo] 00 23 00 00 1C [Engine.WorldInfo.AllControllers] 20 [GearGame.GearPC] 00 [GearGame.GearPC.ProcessGameOver.PC] 16 00 9B 19 00 [GearGame.GearPC.ProcessGameOver.PC] 00 06 00 00 1C [GearGame.GearPC.ClientGameOver] 16 31 30 04 0B 53",
    "S2A-3": "07 00 AD 82 82 82 82 77 01 [GearGame.GearPC.DefaultPawnClass] 2A 16 18 00 0D 77 01 [GearGame.GearPC.DefaultPawnClass] 20 [GearGame.GearPawn_COGMarcus] 16 16 18 00 0D 77 01 [GearGame.GearPC.DefaultPawnClass] 20 [GearGame.GearPawn_COGDom] 16 16 18 00 17 77 19 00 [GearGame.GearPC.Destroyed.Gear] 00 05 00 04 01 [Core.Object.Class] 20 [GearGame.GearPawn_COGMarcus] 16 16 18 00 17 77 19 00 [GearGame.GearPC.Destroyed.Gear] 00 05 00 04 01 [Core.Object.Class] 20 [GearGame.GearPawn_COGDom] 16 16 07 04 F7 77 2E [GearGame.GearPawn] 01 [Engine.Controller.Pawn] 2A 16 07 04 D9 84 19 00 [GearGame.GearPC.Destroyed.Gear] 00 08 00 04 1C [GearGame.GearPawn.IsDoingSpecialMove] 24 2A 16 18 00 13 19 00 [GearGame.GearPC.Destroyed.Gear] 00 08 00 04 1C [GearGame.GearPawn.IsDoingSpecialMove] 24 2B 16 16 06 00 AD 0F 00 [GearGame.GearPC.Destroyed.OldPawn] 01 [Engine.Controller.Pawn] 1C [GearGame.GearPC.UnPossess] 16 19 00 [GearGame.GearPC.Destroyed.OldPawn] 00 03 00 04 61 17 16 06 00 A4 53",
    "S2A-5": "07 01 EC 82 82 82 77 19 00 [GearGame.GearPC.SaveCheckpoint.PC] 00 05 00 04 01 [GearGame.GearPC.DefaultPawnClass] 2A 16 18 00 17 77 19 00 [GearGame.GearPC.SaveCheckpoint.PC] 00 05 00 04 01 [GearGame.GearPC.DefaultPawnClass] 20 [GearGame.GearPawn_COGMarcus] 16 16 18 00 17 77 19 00 [GearGame.GearPC.SaveCheckpoint.PC] 00 05 00 04 01 [GearGame.GearPC.DefaultPawnClass] 20 [GearGame.GearPawn_COGDom] 16 16 18 00 13 72 19 00 [GearGame.GearPC.SaveCheckpoint.PC] 00 05 00 04 01 [Engine.Controller.Pawn] 2A 16 16 06 00 DA 30 04 0B 53",
    "S2A-4": "07 02 88 82 82 82 82 77 00 [GearGame.GearGame.TeamHasAnyChanceOfSurvival.PC] 2A 16 18 00 09 81 2D 01 [GearGame.GearGame.bInSoloSplit] 16 16 18 00 07 2D 01 [GearGame.GearGame.bInCoopSplit] 16 18 00 49 82 82 77 19 00 [GearGame.GearGame.TeamHasAnyChanceOfSurvival.PC] 00 05 00 04 01 [GearGame.GearPC.DefaultPawnClass] 2A 16 18 00 17 77 19 00 [GearGame.GearGame.TeamHasAnyChanceOfSurvival.PC] 00 05 00 04 01 [GearGame.GearPC.DefaultPawnClass] 20 [GearGame.GearPawn_COGMarcus] 16 16 18 00 17 77 19 00 [GearGame.GearGame.TeamHasAnyChanceOfSurvival.PC] 00 05 00 04 01 [GearGame.GearPC.DefaultPawnClass] 20 [GearGame.GearPawn_COGDom] 16 16 16 18 00 3F FE 19 2E [GearGame.GearPRI] 19 00 [GearGame.GearGame.TeamHasAnyChanceOfSurvival.PC] 00 05 00 04 01 [Engine.Controller.PlayerReplicationInfo] 00 05 00 08 01 [GearGame.GearPRI.SquadName] 19 2E [GearGame.GearPRI] 19 00 [GearGame.GearGame.TeamHasAnyChanceOfSurvival.DBNOPawn] 00 05 00 04 01 [Engine.Pawn.PlayerReplicationInfo] 00 05 00 08 01 [GearGame.GearPRI.SquadName] 16 16 14 2D 00 [GearGame.GearGame.TeamHasAnyChanceOfSurvival.bFoundReviver] 27 06 02 89 53",
}

ROW_IDS = ("S2A-1", "S2A-2", "S2A-3", "S2A-5", "S2A-4")


def register(model, ref):
    handles = model.objects.get(ref)
    if handles:
        return handles[0]
    return model.add(ref)


def expected_tail(model, row_id):
    out = bytearray()
    for token in EXPECTED_TAILS[row_id].split():
        if token.startswith("[") and token.endswith("]"):
            path = token[1:-1]
            handle = register(model, sp.Ref(path, PATH_CLASS[path]))
            out += model.operand_value(handle).to_bytes(4, "big")
        else:
            out.append(int(token, 16))
    return bytes(out)


def build_original(model, row, corrupt=None):
    data = bytearray([0x0B] * row.script_size)
    for check in row.checks:
        data[check.offset:check.offset + len(check.data)] = check.data
    for operand in row.operands:
        handle = register(model, operand.ref)
        data[operand.offset:operand.offset + 4] = model.operand_value(handle).to_bytes(4, "big")
    for edit in row.byte_edits:
        data[edit.offset] = edit.old
    if corrupt is not None:
        corrupt(data)
    for piece in row.grow.tail:
        if isinstance(piece, sp.Ref):
            register(model, piece)
    n = row.script_size
    model.add(row.function, script=sp.ScriptArray(bytes(data), n, n))
    return bytes(data)


def planned_bytes_for(row_id):
    row = sv.ROWS[row_id]
    model = FakeModel()
    original = build_original(model, row)
    plan = sp.plan_group(model, [row], 4)
    return model, row, original, plan.entries[0].new


@pytest.mark.parametrize("row_id", ROW_IDS)
def test_plan_grows_with_expected_tail(row_id):
    model, row, original, _ = planned_bytes_for(row_id)
    plan = sp.plan_group(model, [row], 4)
    entry = plan.entries[0]
    prefix = bytearray(original[:row.grow.keep])
    for edit in row.byte_edits:
        prefix[edit.offset] = edit.new
    assert entry.new == bytes(prefix) + expected_tail(model, row_id)
    assert len(entry.new) == row.grow.new_size
    line = f"{row.id} {row.function.path}: script grown to {row.grow.new_size} bytes"
    if row.byte_edits:
        line += f", with {len(row.byte_edits)} write(s)"
    assert sp.group_lines(plan) == [line]


@pytest.mark.parametrize("row_id", ROW_IDS)
def test_grown_script_reports_already_applied(row_id):
    model, row, _, planned = planned_bytes_for(row_id)
    handle = model.objects[row.function][0]
    model.scripts[handle] = sp.ScriptArray(planned, row.grow.new_size, row.grow.new_size)
    plan = sp.plan_group(model, [row], 4)
    assert sp.group_lines(plan)[0].endswith("already applied")


@pytest.mark.parametrize("row_id", ROW_IDS)
def test_grown_tail_differs_skips(row_id):
    model, row, _, planned = planned_bytes_for(row_id)
    tampered = bytearray(planned)
    tampered[-2] ^= 0xFF
    handle = model.objects[row.function][0]
    model.scripts[handle] = sp.ScriptArray(bytes(tampered), row.grow.new_size, row.grow.new_size)
    with pytest.raises(sp.PatchSkip) as exc:
        sp.plan_group(model, [row], 4)
    assert "tail differs" in exc.value.reason


@pytest.mark.parametrize("row_id", ROW_IDS)
def test_checks_do_not_overlap_edits_or_operands(row_id):
    row = sv.ROWS[row_id]
    for check in row.checks:
        check_offsets = set(range(check.offset, check.offset + len(check.data)))
        for edit in row.byte_edits:
            assert edit.offset not in check_offsets
        for operand in row.operands:
            assert check_offsets.isdisjoint(range(operand.offset, operand.offset + 4))
    for edit in row.byte_edits:
        for operand in row.operands:
            assert edit.offset not in range(operand.offset, operand.offset + 4)


def corrupt_s2a3_byte(data):
    data[0x0066] = 0x83


def test_survival_group_wrong_byte_skips_without_mutation():
    model = FakeModel()
    rows = sv.survival_rows()
    for row in rows:
        if row.id == "S2A-3":
            build_original(model, row, corrupt=corrupt_s2a3_byte)
        else:
            build_original(model, row)
    before = dict(model.scripts)
    with pytest.raises(sp.PatchSkip) as exc:
        sp.plan_group(model, rows, 4)
    assert exc.value.row == "S2A-3"
    assert model.scripts == before


def test_s2a2_operand_and_max_rejected():
    row = sv.ROWS["S2A-2"]
    model = FakeModel()
    original = build_original(model, row)
    other = register(model, sp.Ref("GearGame.GearPC", "Class"))
    tampered = bytearray(original)
    tampered[0x0020:0x0024] = model.operand_value(other).to_bytes(4, "big")
    handle = model.objects[row.function][0]
    model.scripts[handle] = sp.ScriptArray(bytes(tampered), row.script_size, row.script_size)
    with pytest.raises(sp.PatchSkip) as exc:
        sp.plan_group(model, [row], 4)
    assert exc.value.row == "S2A-2"
    model2 = FakeModel()
    original2 = build_original(model2, row)
    handle2 = model2.objects[row.function][0]
    model2.scripts[handle2] = sp.ScriptArray(original2, 64, 80)
    with pytest.raises(sp.PatchSkip) as exc2:
        sp.plan_group(model2, [row], 4)
    assert "Script.Max" in exc2.value.reason


def test_append_structure_and_redirects():
    for row_id in ("S2A-1", "S2A-3", "S2A-5", "S2A-4"):
        _, row, _, planned = planned_bytes_for(row_id)
        assert row.grow.keep == row.script_size - 1
        assert planned[-1] == 0x53
    assert sv.ROWS["S2A-2"].grow.keep == 0
    _, _, _, p1 = planned_bytes_for("S2A-1")
    assert p1[0x8B:0x8D] == bytes([0x00, 0xA1])
    _, _, _, p3 = planned_bytes_for("S2A-3")
    assert p3[0x64:0x66] == bytes([0x04, 0x37])
    _, _, _, p5 = planned_bytes_for("S2A-5")
    assert p5[0xD7:0xDA] == bytes([0x06, 0x01, 0x87])
    _, _, _, p4 = planned_bytes_for("S2A-4")
    assert p4[0x259:0x25B] == bytes([0x02, 0x97])


def test_s2a3_tail_has_no_checkpoint_dummy_term():
    model, row, _, planned = planned_bytes_for("S2A-3")
    dummy = sp.Ref("GearGame.GearPC.bCheckpointDummy", "BoolProperty")
    operand = model.operand_value(register(model, dummy)).to_bytes(4, "big")
    assert planned[0x006A:0x006E] == operand
    assert operand not in planned[row.grow.keep:]


def test_render_cpp_matches_file():
    root = Path(__file__).resolve().parents[2]
    with open(root / "src" / "gow2_script_patch_survival.cpp", "r", encoding="utf-8", newline="") as f:
        # A core.autocrlf checkout turns the committed LF file into CRLF.
        assert sv.render_cpp() == f.read().replace("\r\n", "\n")


def test_render_cpp_contains_groups():
    text = sv.render_cpp()
    assert "PatchGroup CoopSurvivalGroup(bool extra_players_end_game)" in text
    assert "if (!extra_players_end_game) group.scripts.push_back(RowS2A_2());" in text
    assert "PatchGroup CoopSplitReviveGroup()" in text


@pytest.mark.parametrize("flag, want", [
    (False, ["S2A-1", "S2A-2", "S2A-3", "S2A-5"]),
    (True, ["S2A-1", "S2A-3", "S2A-5"]),
])
def test_survival_rows_flag_leaves_out_s2a2(flag, want):
    assert [row.id for row in sv.survival_rows(flag)] == want
