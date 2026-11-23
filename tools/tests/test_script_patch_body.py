import sys
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import pytest

import script_patch as sp
import script_patch_body as spb


class FakeModel:
    def __init__(self):
        self.objects = {}
        self.values = {}
        self.scripts = {}
        self.fields = {}

    def add(self, ref, script=None):
        handle = len(self.values)
        self.objects.setdefault(ref, []).append(handle)
        self.values[handle] = 0x80001000 + handle
        if script is not None:
            self.scripts[handle] = script
        return handle

    def find(self, ref):
        return list(self.objects.get(ref, []))

    def operand_value(self, handle):
        return self.values[handle]

    def script(self, handle):
        return self.scripts[handle]

    def read_field(self, handle, offset, prop):
        return self.fields.get((handle, offset), 0)


def refs_of_script_row(row):
    refs = [row.function]
    refs.extend(o.ref for o in row.operands)
    for edit in row.operand_edits:
        refs.append(edit.old)
        refs.append(edit.new)
    if row.grow is not None:
        refs.extend(p for p in row.grow.tail if isinstance(p, sp.Ref))
    return refs


def model_with_refs(refs):
    model = FakeModel()
    for ref in dict.fromkeys(refs):
        model.add(ref)
    return model


def filled_script(model, row):
    data = bytearray(row.script_size)
    for check in row.checks:
        data[check.offset:check.offset + len(check.data)] = check.data
    for operand in row.operands:
        value = model.operand_value(model.find(operand.ref)[0])
        data[operand.offset:operand.offset + 4] = value.to_bytes(4, "big")
    for edit in row.byte_edits:
        data[edit.offset] = edit.old
    for edit in row.operand_edits:
        value = model.operand_value(model.find(edit.old)[0])
        data[edit.offset:edit.offset + 4] = value.to_bytes(4, "big")
    return bytes(data)


def plan_on_filled(row):
    model = model_with_refs(refs_of_script_row(row))
    handle = model.find(row.function)[0]
    data = filled_script(model, row)
    model.scripts[handle] = sp.ScriptArray(data, len(data), len(data))
    return model, handle, spb.plan_rows(model, [row], 4)[0]


def value_of(model, ref):
    return model.operand_value(model.find(ref)[0])


def test_render_cpp_matches_file():
    root = Path(__file__).resolve().parents[2]
    with open(root / "src" / "gow2_script_patch_body_table.cpp", "r", encoding="utf-8", newline="") as f:
        # a Windows checkout with core.autocrlf has CRLF; the rows are what must match
        assert spb.render_cpp() == f.read().replace("\r\n", "\n")


def test_check_table_real_rows():
    spb.check_table()


def test_cb3_needs_the_grown_script():
    model = FakeModel()
    model.add(spb.SUGGEST, script=sp.ScriptArray(bytes(145), 145, 145))
    with pytest.raises(sp.PatchSkip) as exc:
        sp._plan_script(model, spb.ROWS["CB-3"], 4)
    assert exc.value.reason == "Script.Num is 145, expected 318"


def test_cb3_edits():
    row = spb.ROWS["CB-3"]
    model, handle, plan = plan_on_filled(row)
    new = plan.new
    assert new[0x84] == 0x06
    assert new[0x101] == 0x20
    assert new[0x102:0x106] == value_of(model, spb.COG_GEAR).to_bytes(4, "big")
    assert spb.row_lines([plan]) == [f"CB-3 {spb.SUGGEST.path}: 3 write(s)"]
    model.scripts[handle] = sp.ScriptArray(new, len(new), len(new))
    again = spb.plan_rows(model, [row], 4)[0]
    assert again.writes == 0
    assert spb.row_lines([again]) == [f"CB-3 {spb.SUGGEST.path}: already applied"]


def test_cb3c_keeps_the_carmine_branch():
    row = spb.ROWS["CB-3c"]
    _, _, plan = plan_on_filled(row)
    assert plan.new[0x84] == 0x07
    assert plan.new[0x101] == 0x20


def test_cb4_tail():
    row = spb.ROWS["CB-4"]
    model = model_with_refs(refs_of_script_row(row))
    tail = sp._assemble_tail(model, row.grow, row.id)
    assert len(tail) == 274
    assert tail[0:3] == bytes.fromhex("07 02 8F")
    assert tail[0x83:0x86] == bytes.fromhex("07 02 49")
    assert tail[0xC9:0xCC] == bytes.fromhex("07 02 8F")
    assert tail[-5:] == bytes.fromhex("27 16 04 0B 53")
    model, handle, plan = plan_on_filled(row)
    new = plan.new
    assert len(new) == 658
    assert new[:0x180] == model.scripts[handle].data[:0x180]
    assert spb.row_lines([plan]) == [f"CB-4 {spb.RESTART.path}: script grown to 658 bytes"]


def test_cb4_tail_guards_extras():
    tail = spb.CB4_TAIL
    first_find = next(i for i, p in enumerate(tail) if p == spb.FIND_INVENTORY)
    before_refs = [p for p in tail[:first_find] if isinstance(p, sp.Ref)]
    assert spb.GEAR_PC in before_refs
    assert spb.DEFAULT_PAWN_CLASS in before_refs
    assert spb.M in before_refs
    assert spb.D in before_refs
    raw_before = b"".join(p.data for p in tail[:first_find] if isinstance(p, sp.Raw))
    assert b"\x2e" in raw_before
    assert b"\x2a" in raw_before


def test_cb5_edits():
    row = spb.ROWS["CB-5"]
    model, handle, plan = plan_on_filled(row)
    new = plan.new
    assert new[0x3B3:0x3B5] == bytes.fromhex("72 13")
    assert new[0x3B5:0x3B9] == value_of(model, spb.D).to_bytes(4, "big")
    assert new[0x3C4:0x3C8] == value_of(model, spb.DEFAULT_PAWN_CLASS).to_bytes(4, "big")
    model.scripts[handle] = sp.ScriptArray(new, len(new), len(new))
    again = spb.plan_rows(model, [row], 4)[0]
    assert again.writes == 0
    assert spb.row_lines([again]) == [f"CB-5 {spb.POST_LOGIN.path}: already applied"]


def test_cr1_tail_layout():
    _, _, plan = plan_on_filled(spb.ROWS["CR-1"])
    new = plan.new
    assert len(new) == 369
    assert new[0xA6:0xA9] == bytes.fromhex("07 01 6E")
    assert new[0xEB] == 0x2F
    assert new[0xF2:0xF4] == bytes.fromhex("00 77")
    assert new[0x106:0x108] == bytes.fromhex("01 6D")
    assert new[0x108:0x10B] == bytes.fromhex("07 01 6C")
    assert new[0x16B:0x171] == bytes.fromhex("27 31 30 04 0B 53")


def test_cr2_tail_layout():
    _, _, plan = plan_on_filled(spb.ROWS["CR-2"])
    new = plan.new
    assert len(new) == 451
    assert new[0xD5:0xD8] == bytes.fromhex("07 01 C0")
    assert new[0x102] == 0x2F
    assert new[0x109:0x10B] == bytes.fromhex("00 B2")
    assert new[0x11D:0x11F] == bytes.fromhex("01 BF")
    assert new[0x11F:0x122] == bytes.fromhex("07 01 BE")
    assert new[0x1BB:0x1C3] == bytes.fromhex("4A 4A 16 31 30 04 0B 53")


def test_cr3_tail_layout():
    _, _, plan = plan_on_filled(spb.ROWS["CR-3"])
    new = plan.new
    assert len(new) == 532
    assert new[0x1D] == 0x06
    assert new[0x1F] == 0xCB
    assert new[0x1CB:0x1CE] == bytes.fromhex("07 01 C7")
    assert new[0x210:0x213] == bytes.fromhex("06 00 2C")
    assert new[0x213] == 0x53


def cb1_setup():
    row = spb.ROWS["CB-1"]
    model = FakeModel()
    coggear = model.add(spb.COGGEAR_MESH)
    dom_skel = model.add(spb.DOM_SKEL)
    dom_mesh = model.add(spb.DOM_MESH)
    marcus_mesh = model.add(spb.MARCUS_MESH)
    marcus_skel = model.add(spb.MARCUS_SKEL)
    model.fields[(dom_mesh, spb.SKELETAL_MESH_OFFSET)] = model.operand_value(dom_skel)
    model.fields[(marcus_mesh, spb.SKELETAL_MESH_OFFSET)] = model.operand_value(marcus_skel)
    return model, row, coggear


def test_cb1_none_writes():
    model, row, _ = cb1_setup()
    plan = spb.plan_object_field(model, row)
    assert plan.writes == 1
    assert spb.row_lines([plan]) == [f"CB-1 {spb.COGGEAR_MESH.path}: 1 write(s)"]


def test_cb1_already_applied():
    model, row, coggear = cb1_setup()
    model.fields[(coggear, spb.SKELETAL_MESH_OFFSET)] = value_of(model, spb.DOM_SKEL)
    plan = spb.plan_object_field(model, row)
    assert plan.writes == 0
    assert spb.row_lines([plan]) == [f"CB-1 {spb.COGGEAR_MESH.path}: already applied"]


def test_cb1_wrong_value():
    model, row, coggear = cb1_setup()
    model.fields[(coggear, spb.SKELETAL_MESH_OFFSET)] = 0x12345678
    with pytest.raises(sp.PatchSkip) as exc:
        spb.plan_object_field(model, row)
    assert exc.value.reason == ("+0x01E0 points at 0x12345678, expected None or "
                                 "COG_Dom.Game.COG_Dom_Game_CamSkel (SkeletalMesh)")


def test_cb1_bad_anchor():
    model, row, _ = cb1_setup()
    model.fields[(model.find(spb.MARCUS_MESH)[0], spb.SKELETAL_MESH_OFFSET)] = 0
    with pytest.raises(sp.PatchSkip) as exc:
        spb.plan_object_field(model, row)
    assert exc.value.reason.startswith(
        "anchor GearGame.Default__GearPawn_COGMarcus.GearPawnMesh (SkeletalMeshComponent) "
        "+0x01E0 points at 0x00000000")


def test_cpp_has_coop_client_group():
    text = spb.render_cpp()
    assert "PatchGroup CoopClientGroup()" in text
    assert '.name = "coop-client"' in text


def test_group_composition():
    assert [r.id for r in spb.coop_client_rows()] == ["CB-1", "CB-2"]
    assert [r.id for r in spb.coop_body_rows()] == ["CB-3", "CB-4", "CB-5"]
    assert [r.id for r in spb.coop_body_rows(carmine=True)] == ["CB-3c", "CB-4", "CB-5"]
    assert [r.id for r in spb.coop_body_rows(marcus_clones=True)] == ["CB-4", "CB-5"]
    assert [r.id for r in spb.coop_ride_rows()] == ["CR-1", "CR-2", "CR-3"]


def test_forced_export_path():
    pkg = SimpleNamespace(
        exports=[{"outer": 0, "export_flags": 1},
                 {"outer": 1, "export_flags": 0},
                 {"outer": 0, "export_flags": 0}],
        obj_path={1: "COG_Dom", 2: "COG_Dom.Skel", 3: "Default__GearPawn_COGGear"}.__getitem__,
        obj_class={1: "Package", 2: "SkeletalMesh", 3: "GearPawn_COGGear"}.__getitem__,
    )
    model = object.__new__(spb.BodyModel)
    model.pkg = pkg
    model.pkg_name = "GearGame"
    model._paths = {}
    assert model._full_path(2) == "COG_Dom.Skel"
    assert model._full_path(3) == "GearGame.Default__GearPawn_COGGear"


def test_cli_usage():
    assert spb.main(["--write"]) == 2
    assert spb.main([]) == 2
