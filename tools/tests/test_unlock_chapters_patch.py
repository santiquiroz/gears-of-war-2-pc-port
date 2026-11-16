import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import pytest

import script_patch as sp
import unlock_chapters_patch as ucp


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


def make_model():
    model = FakeModel()
    handles = {}
    for row in ucp.ROWS:
        for op in row.operands:
            if op.ref not in handles:
                handles[op.ref] = model.add(op.ref)
    scripts = {}
    for row in ucp.ROWS:
        data = bytearray(row.script_size)
        for check in row.checks:
            data[check.offset:check.offset + len(check.data)] = check.data
        for op in row.operands:
            data[op.offset:op.offset + 4] = model.operand_value(handles[op.ref]).to_bytes(4, "big")
        for edit in row.byte_edits:
            data[edit.offset] = edit.old
        before = bytes(data)
        handle = model.add(row.function, script=sp.ScriptArray(before, row.script_size, row.script_size))
        scripts[row.id] = (row, handle, before)
    return model, scripts


def test_plan_applies_two_byte_changes():
    model, scripts = make_model()
    plan = sp.plan_group(model, list(ucp.ROWS), 4)
    out = sp.apply_plan(model, plan)
    for row_id, (row, handle, before) in scripts.items():
        after = out[handle]
        edited = set()
        for edit in row.byte_edits:
            assert after[edit.offset] == edit.new
            edited.add(edit.offset)
        for i in range(row.script_size):
            if i not in edited:
                assert after[i] == before[i]
        assert model.scripts[handle].data == before
    assert sp.group_lines(plan) == [
        "UNL-1 GearGame.GearProfileSettings.HasActBeenUnlockedForAccess: 2 write(s)",
        "UNL-2 GearGame.GearUISceneFE_SelectChapter.CanAccessChapter: 2 write(s)",
        "UNL-3 GearGame.GearUISceneFELobby_Campaign.ShouldDisableElementChapter: 2 write(s)",
        "UNL-4 GearGame.GearUISceneFE_Difficulty.InitializeWidgetReferences: 1 write(s)",
    ]


def test_already_applied():
    model, scripts = make_model()
    for row, handle, before in scripts.values():
        patched = bytearray(before)
        for edit in row.byte_edits:
            patched[edit.offset] = edit.new
        model.scripts[handle] = sp.ScriptArray(bytes(patched), row.script_size, row.script_size)
    plan = sp.plan_group(model, list(ucp.ROWS), 4)
    assert sp.group_lines(plan) == [
        "UNL-1 GearGame.GearProfileSettings.HasActBeenUnlockedForAccess: already applied",
        "UNL-2 GearGame.GearUISceneFE_SelectChapter.CanAccessChapter: already applied",
        "UNL-3 GearGame.GearUISceneFELobby_Campaign.ShouldDisableElementChapter: already applied",
        "UNL-4 GearGame.GearUISceneFE_Difficulty.InitializeWidgetReferences: already applied",
    ]


def test_wrong_old_byte():
    model, scripts = make_model()
    row, handle, before = scripts["UNL-2"]
    bad = bytearray(before)
    bad[row.byte_edits[0].offset] = 0x00
    model.scripts[handle] = sp.ScriptArray(bytes(bad), row.script_size, row.script_size)
    with pytest.raises(sp.PatchSkip) as exc:
        sp.plan_group(model, list(ucp.ROWS), 4)
    assert exc.value.row == "UNL-2"


def test_wrong_operand():
    model, scripts = make_model()
    row, handle, before = scripts["UNL-3"]
    bad = bytearray(before)
    op = row.operands[0]
    bad[op.offset:op.offset + 4] = (0xDEADBEEF).to_bytes(4, "big")
    model.scripts[handle] = sp.ScriptArray(bytes(bad), row.script_size, row.script_size)
    with pytest.raises(sp.PatchSkip):
        sp.plan_group(model, list(ucp.ROWS), 4)


def test_wrong_num():
    model, scripts = make_model()
    row, handle, before = scripts["UNL-1"]
    model.scripts[handle] = sp.ScriptArray(before, row.script_size - 1, row.script_size - 1)
    with pytest.raises(sp.PatchSkip):
        sp.plan_group(model, list(ucp.ROWS), 4)


def test_function_found_twice():
    model, _ = make_model()
    model.add(ucp.UNL_1.function)
    with pytest.raises(sp.PatchSkip):
        sp.plan_group(model, list(ucp.ROWS), 4)


def test_want_covers_every_row():
    assert set(ucp.WANT) == {row.id for row in ucp.ROWS}


def test_unl_4_single_byte_edit():
    model, scripts = make_model()
    plan = sp.plan_group(model, list(ucp.ROWS), 4)
    out = sp.apply_plan(model, plan)
    row, handle, before = scripts["UNL-4"]
    after = out[handle]
    assert after[0xB4] == 0x06
    for i in range(row.script_size):
        if i != 0xB4:
            assert after[i] == before[i]


def test_render_cpp_matches_file():
    root = Path(__file__).resolve().parents[2]
    with open(root / "src" / "gow2_unlock_chapters_table.cpp", "r", encoding="utf-8", newline="") as f:
        assert ucp.render_cpp() == f.read().replace("\r\n", "\n")


def test_write_without_cpp():
    assert ucp.main(["--write"]) == 2
