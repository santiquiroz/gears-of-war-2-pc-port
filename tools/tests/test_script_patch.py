import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import pytest

import script_patch as sp


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


FUNC_A = sp.Ref("Pkg.ClassA.FuncA", "Function")
FUNC_B = sp.Ref("Pkg.ClassB.FuncB", "Function")
TARGET = sp.Ref("Pkg.ClassC.Target", "IntProperty")
FUNC_G = sp.Ref("Pkg.ClassG.FuncG", "Function")
GROW_TARGET = sp.Ref("Pkg.ClassC.GrowTarget", "Class")
FUNC_E = sp.Ref("Pkg.ClassE.FuncE", "Function")
OLD_REF = sp.Ref("Pkg.ClassC.Old", "Class")
NEW_REF = sp.Ref("Pkg.ClassC.New", "Class")
DEF_OBJ = sp.Ref("Pkg.Default__Thing", "Thing")


def two_row_setup(byte_b=0x02):
    model = FakeModel()
    target = model.add(TARGET)
    value = model.operand_value(target)
    data_a = bytearray(16)
    data_a[0] = 0x04
    data_a[2] = 0x9A
    data_a[4:8] = value.to_bytes(4, "big")
    handle_a = model.add(FUNC_A, script=sp.ScriptArray(bytes(data_a), 16, 16))
    data_b = bytearray(12)
    data_b[0] = 0x0F
    data_b[5] = byte_b
    handle_b = model.add(FUNC_B, script=sp.ScriptArray(bytes(data_b), 12, 12))
    rows = [
        sp.ScriptRow("T-1", "test row one", FUNC_A, 16,
                     checks=(sp.Check(0x00, b"\x04"),),
                     operands=(sp.Operand(0x04, TARGET),),
                     byte_edits=(sp.ByteEdit(0x02, 0x9A, sp.PLAYERS),)),
        sp.ScriptRow("T-2", "test row two", FUNC_B, 12,
                     checks=(sp.Check(0x00, b"\x0F"),),
                     byte_edits=(sp.ByteEdit(0x05, 0x02, 0x07),)),
    ]
    return model, rows, handle_a, handle_b


def test_plan_applies_two_rows():
    model, rows, handle_a, handle_b = two_row_setup()
    before_a = model.scripts[handle_a].data
    before_b = model.scripts[handle_b].data
    plan = sp.plan_group(model, rows, 4)
    out = sp.apply_plan(model, plan)
    assert out[handle_a] == before_a[:2] + b"\x04" + before_a[3:]
    assert out[handle_b] == before_b[:5] + b"\x07" + before_b[6:]
    assert model.scripts[handle_a].data == before_a
    assert model.scripts[handle_b].data == before_b
    assert sp.group_lines(plan) == [
        f"T-1 {FUNC_A.path}: 1 write(s)",
        f"T-2 {FUNC_B.path}: 1 write(s)",
    ]


def test_wrong_old_byte_skips_group():
    model, rows, handle_a, handle_b = two_row_setup(byte_b=0x03)
    before_a = model.scripts[handle_a].data
    before_b = model.scripts[handle_b].data
    with pytest.raises(sp.PatchSkip) as exc:
        sp.plan_group(model, rows, 4)
    assert exc.value.row == "T-2"
    assert "+0x0005" in exc.value.reason
    assert model.scripts[handle_a].data == before_a
    assert model.scripts[handle_b].data == before_b


def test_wrong_num():
    model, rows, handle_a, _ = two_row_setup()
    bad = model.scripts[handle_a]
    model.scripts[handle_a] = sp.ScriptArray(bad.data, 15, 15)
    with pytest.raises(sp.PatchSkip) as exc:
        sp.plan_group(model, rows, 4)
    assert exc.value.row == "T-1"
    assert "Script.Num" in exc.value.reason


def test_wrong_operand():
    model, rows, handle_a, _ = two_row_setup()
    bad = bytearray(model.scripts[handle_a].data)
    bad[4:8] = (0xDEADBEEF).to_bytes(4, "big")
    model.scripts[handle_a] = sp.ScriptArray(bytes(bad), 16, 16)
    with pytest.raises(sp.PatchSkip) as exc:
        sp.plan_group(model, rows, 4)
    assert exc.value.row == "T-1"
    assert "points at" in exc.value.reason


def test_function_found_twice():
    model, rows, _, _ = two_row_setup()
    model.add(FUNC_A)
    with pytest.raises(sp.PatchSkip) as exc:
        sp.plan_group(model, rows, 4)
    assert exc.value.row == "T-1"
    assert "found 2 times" in exc.value.reason


def test_already_applied():
    model, rows, handle_a, _ = two_row_setup()
    patched = bytearray(model.scripts[handle_a].data)
    patched[2] = 4
    model.scripts[handle_a] = sp.ScriptArray(bytes(patched), 16, 16)
    plan = sp.plan_group(model, rows, 4)
    assert plan.entries[0].writes == 0
    assert sp.group_lines(plan)[0] == f"T-1 {FUNC_A.path}: already applied"


def grow_parts(model):
    target = model.add(GROW_TARGET)
    value = model.operand_value(target)
    row = sp.ScriptRow("G-1", "test grow", FUNC_G, 10,
                       grow=sp.Grow(6, 14, (sp.Raw(b"\xAA"), sp.Text("hi"), GROW_TARGET)))
    return row, value


def test_grow_success():
    model, rows, _, _ = two_row_setup()
    row, value = grow_parts(model)
    prefix = bytes(range(10))
    handle_g = model.add(FUNC_G, script=sp.ScriptArray(prefix, 10, 10))
    group = [rows[0], row, rows[1]]
    plan = sp.plan_group(model, group, 4)
    out = sp.apply_plan(model, plan)
    assert out[handle_g] == prefix[:6] + b"\xAAhi\x00" + value.to_bytes(4, "big")
    assert sp.group_lines(plan)[1] == f"G-1 {FUNC_G.path}: script grown to 14 bytes"
    assert model.scripts[handle_g].data == prefix


def test_grow_max_mismatch():
    model = FakeModel()
    row, _ = grow_parts(model)
    prefix = bytes(range(10))
    model.add(FUNC_G, script=sp.ScriptArray(prefix, 10, 12))
    with pytest.raises(sp.PatchSkip) as exc:
        sp.plan_group(model, [row], 4)
    assert exc.value.row == "G-1"
    assert "Script.Max" in exc.value.reason


def test_grow_already_applied():
    model = FakeModel()
    row, value = grow_parts(model)
    full = bytes(range(6)) + b"\xAAhi\x00" + value.to_bytes(4, "big")
    model.add(FUNC_G, script=sp.ScriptArray(full, 14, 14))
    plan = sp.plan_group(model, [row], 4)
    assert plan.entries[0].writes == 0
    assert sp.group_lines(plan) == [f"G-1 {FUNC_G.path}: already applied"]


def test_grow_tail_differs():
    model = FakeModel()
    row, _ = grow_parts(model)
    model.add(FUNC_G, script=sp.ScriptArray(bytes(14), 14, 14))
    with pytest.raises(sp.PatchSkip) as exc:
        sp.plan_group(model, [row], 4)
    assert exc.value.row == "G-1"
    assert "tail differs" in exc.value.reason


def edit_setup(found):
    model = FakeModel()
    old = model.add(OLD_REF)
    new = model.add(NEW_REF)
    old_value = model.operand_value(old)
    new_value = model.operand_value(new)
    data = bytearray(8)
    data[0] = 0x04
    data[4:8] = found.to_bytes(4, "big")
    handle = model.add(FUNC_E, script=sp.ScriptArray(bytes(data), 8, 8))
    row = sp.ScriptRow("E-1", "test operand edit", FUNC_E, 8,
                       checks=(sp.Check(0x00, b"\x04"),),
                       operand_edits=(sp.OperandEdit(0x04, OLD_REF, NEW_REF),))
    return model, row, handle, old_value, new_value


def test_operand_edit_swaps():
    model, row, handle, old_value, new_value = edit_setup(0x80001000)
    assert (old_value, new_value) == (0x80001000, 0x80001001)
    plan = sp.plan_group(model, [row], 4)
    out = sp.apply_plan(model, plan)
    assert out[handle][4:8] == new_value.to_bytes(4, "big")
    assert sp.group_lines(plan) == [f"E-1 {FUNC_E.path}: 1 write(s)"]


def test_operand_edit_third_value_fails():
    model, row, _, _, _ = edit_setup(0x12345678)
    with pytest.raises(sp.PatchSkip) as exc:
        sp.plan_group(model, [row], 4)
    assert exc.value.row == "E-1"
    assert "points at" in exc.value.reason


def test_operand_edit_already_applied():
    model, row, _, _, _ = edit_setup(0x80001001)
    plan = sp.plan_group(model, [row], 4)
    assert plan.entries[0].writes == 0
    assert sp.group_lines(plan) == [f"E-1 {FUNC_E.path}: already applied"]


def default_setup(int_value):
    model = FakeModel()
    handle = model.add(DEF_OBJ, int_value=int_value)
    row = sp.DefaultRow("F-1", "test default", DEF_OBJ, 132, "NumPublicConnections", 2, sp.PLAYERS)
    return model, row, handle


def test_default_row_write():
    model, row, handle = default_setup(2)
    plan = sp.plan_group(model, [row], 4)
    assert sp.apply_plan(model, plan) == {handle: 4}
    assert sp.group_lines(plan) == [f"F-1 {DEF_OBJ.path}: 1 write(s)"]
    assert model.ints[handle] == 2


def test_default_row_wrong_value():
    model, row, _ = default_setup(3)
    with pytest.raises(sp.PatchSkip) as exc:
        sp.plan_group(model, [row], 4)
    assert exc.value.row == "F-1"
    assert "holds 3, expected 2" in exc.value.reason


def test_players_out_of_range():
    model, rows, _, _ = two_row_setup()
    with pytest.raises(sp.PatchSkip) as exc:
        sp.plan_group(model, rows, 256)
    assert exc.value.row == "T-1"
    assert "out of range" in exc.value.reason


@pytest.mark.parametrize("players", [8, 10])
def test_players_value_written_for_large_counts(players):
    model, rows, handle_a, _ = two_row_setup()
    plan = sp.plan_group(model, rows, players)
    out = sp.apply_plan(model, plan)
    assert out[handle_a][2] == players
    assert sp.group_lines(plan) == [
        f"T-1 {FUNC_A.path}: 1 write(s)",
        f"T-2 {FUNC_B.path}: 1 write(s)",
    ]


@pytest.mark.parametrize("players", [8, 10])
def test_main_accepts_large_counts(monkeypatch, players):
    seen = []
    monkeypatch.setattr(sp, "check_package", lambda pkg, n: seen.append(("stage1", pkg, n)) or 0)
    monkeypatch.setattr(sp, "check_stage2_stack", lambda pkg, n: seen.append(("stage2", pkg, n)) or 0)
    assert sp.main(["--check", "x", "--players", str(players)]) == 0
    assert seen == [("stage1", "x", players), ("stage2", "x", players)]


@pytest.mark.parametrize("players", [8, 10])
def test_real_packages_check_at_large_counts(players):
    pkg = Path(__file__).resolve().parents[2] / "out" / "re-mouse" / "pkg"
    if not (pkg / "GearGame.upk").exists():
        pytest.skip("decompressed game packages are not in out/re-mouse/pkg")
    assert sp.main(["--check", str(pkg), "--players", str(players)]) == 0


def test_render_cpp_matches_file():
    root = Path(__file__).resolve().parents[2]
    with open(root / "src" / "gow2_script_patch_table.cpp", "r", encoding="utf-8", newline="") as f:
        # a Windows checkout with core.autocrlf has CRLF; the rows are what must match
        assert sp.render_cpp() == f.read().replace("\r\n", "\n")


class AnyRefModel(FakeModel):
    def find(self, ref):
        return ["any"]


def grow_edit_row(row_id="G-2"):
    return sp.ScriptRow(row_id, "test grow with edit", FUNC_G, 10,
                        byte_edits=(sp.ByteEdit(0x02, 0x9A, sp.PLAYERS),),
                        grow=sp.Grow(6, 14, (sp.Raw(b"\xAA"), sp.Text("hi"), GROW_TARGET)))


def grow_edit_setup(full_data, num):
    model = FakeModel()
    target = model.add(GROW_TARGET)
    value = model.operand_value(target)
    handle = model.add(FUNC_G, script=sp.ScriptArray(full_data, num, num))
    return model, handle, value


def test_grow_with_prefix_edit():
    prefix = bytearray(range(10))
    prefix[2] = 0x9A
    model, handle, value = grow_edit_setup(bytes(prefix), 10)
    plan = sp.plan_group(model, [grow_edit_row()], 4)
    out = sp.apply_plan(model, plan)
    expected = bytes(prefix[:2]) + b"\x04" + bytes(prefix[3:6]) + b"\xAAhi\x00" + value.to_bytes(4, "big")
    assert out[handle] == expected
    assert plan.entries[0].writes == 1
    assert sp.group_lines(plan) == [f"G-2 {FUNC_G.path}: script grown to 14 bytes, with 1 write(s)"]
    assert model.scripts[handle].data == bytes(prefix)


def grown_with_byte_2(byte_2):
    prefix = bytearray(range(6))
    prefix[2] = byte_2
    model = FakeModel()
    value = model.operand_value(model.add(GROW_TARGET))
    full = bytes(prefix) + b"\xAAhi\x00" + value.to_bytes(4, "big")
    handle = model.add(FUNC_G, script=sp.ScriptArray(full, 14, 14))
    return model, handle, full


def test_grow_with_edit_already_applied():
    model, _, _ = grown_with_byte_2(0x04)
    plan = sp.plan_group(model, [grow_edit_row()], 4)
    assert plan.entries[0].writes == 0
    assert sp.group_lines(plan) == [f"G-2 {FUNC_G.path}: already applied"]


def test_grow_with_edit_pending_in_grown_script():
    model, handle, full = grown_with_byte_2(0x9A)
    plan = sp.plan_group(model, [grow_edit_row()], 4)
    out = sp.apply_plan(model, plan)
    expected = full[:2] + b"\x04" + full[3:]
    assert plan.entries[0].writes == 1
    assert sp.group_lines(plan) == [f"G-2 {FUNC_G.path}: 1 write(s)"]
    assert out[handle] == expected


def test_grow_byte_edit_past_keep():
    model = FakeModel()
    model.add(GROW_TARGET)
    model.add(FUNC_G, script=sp.ScriptArray(bytes(range(10)), 10, 10))
    row = sp.ScriptRow("G-3", "test grow past keep", FUNC_G, 10,
                       byte_edits=(sp.ByteEdit(0x07, 0x07, 0x00),),
                       grow=sp.Grow(6, 14, (sp.Raw(b"\xAA"), sp.Text("hi"), GROW_TARGET)))
    with pytest.raises(sp.PatchSkip) as exc:
        sp.plan_group(model, [row], 4)
    assert exc.value.row == "G-3"
    assert exc.value.reason == "+0x0007 is past the kept prefix +0x0006"


def test_grow_operand_edit_past_keep():
    model = FakeModel()
    model.add(GROW_TARGET)
    old = model.add(OLD_REF)
    model.add(NEW_REF)
    data = bytearray(10)
    data[4:8] = model.operand_value(old).to_bytes(4, "big")
    model.add(FUNC_G, script=sp.ScriptArray(bytes(data), 10, 10))
    row = sp.ScriptRow("G-3", "test grow past keep", FUNC_G, 10,
                       operand_edits=(sp.OperandEdit(0x04, OLD_REF, NEW_REF),),
                       grow=sp.Grow(6, 14, (sp.Raw(b"\xAA"), sp.Text("hi"), GROW_TARGET)))
    with pytest.raises(sp.PatchSkip) as exc:
        sp.plan_group(model, [row], 4)
    assert exc.value.row == "G-3"
    assert "is past the kept prefix" in exc.value.reason


def test_check_table_real_rows():
    sp.check_table()


def test_check_table_bad_row(monkeypatch):
    bad = sp.ScriptRow("X-1", "test bad row", FUNC_G, 10,
                       byte_edits=(sp.ByteEdit(0x07, 0x07, 0x00),),
                       grow=sp.Grow(6, 14, (sp.Raw(b"\xAA"),)))
    monkeypatch.setitem(sp.ROWS, "X-1", bad)
    with pytest.raises(ValueError) as exc:
        sp.check_table()
    assert str(exc.value).startswith("X-1: ")
    with pytest.raises(ValueError) as exc:
        sp.render_cpp()
    assert str(exc.value).startswith("X-1: ")


def test_real_suggest_tail_assembles():
    model = AnyRefModel()
    model.values["any"] = 0x80001000
    row = sp.ROWS["S1-2"]
    data = bytearray(145)
    for check in row.checks:
        data[check.offset:check.offset + len(check.data)] = check.data
    for operand in row.operands:
        data[operand.offset:operand.offset + 4] = (0x80001000).to_bytes(4, "big")
    model.scripts["any"] = sp.ScriptArray(bytes(data), 145, 145)
    plan = sp.plan_group(model, [row], 4)
    new = sp.apply_plan(model, plan)["any"]
    assert len(new) == 318
    tail = new[0x84:]
    assert len(tail) == 318 - 0x84
    assert tail[:4] == b"\x07\x01\x00\x77"
    assert new[0xC2:0xC4] == b"\x04\x13" and new[0x100:0x102] == b"\x04\x13"
    assert new[0x137:0x139] == b"\x04\x3A" and new[-1:] == b"\x53"
    assert b"GearGameContent.GearPawn_COGBenjaminCarmine\x00" in tail
    assert (0x80001000).to_bytes(4, "big") in tail
