import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import pytest

import script_patch as sp
import script_patch_stage2 as st


class FakeModel:
    def __init__(self, script):
        self.ref = sp.Ref("Fake.Pkg.Func", "Function")
        self.handle = 7
        self.data = script

    def find(self, ref):
        return [self.handle] if ref == self.ref else []

    def operand_value(self, handle):
        return handle & 0xFFFFFFFF

    def script(self, handle):
        return sp.ScriptArray(self.data, len(self.data), len(self.data))

    def read_int(self, handle, row):
        raise AssertionError("unused")


def function_body(text, signature):
    start = text.index(signature)
    open_brace = text.index("{", start)
    depth = 0
    for i in range(open_brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[open_brace:i + 1]
    raise AssertionError(f"unbalanced braces after {signature}")


def test_stack_groups_follow_the_hook_order():
    names = [g.name for g in st.stack_groups()]
    assert names == ["coop", "survival", "coop-body", "coop-ride", "coop-scenes"]
    names = [g.name for g in st.stack_groups(split_revive=True)]
    assert names == ["coop", "survival", "split-revive", "coop-body", "coop-ride", "coop-scenes"]


def test_variants_pass_their_settings():
    by_name = {g.name: g for g in st.stack_groups(marcus_clones=True)}
    coop_ids = [r.id for r in by_name["coop"].rows]
    assert "D-1" in coop_ids
    assert "S1-2" not in coop_ids
    body_ids = [r.id for r in by_name["coop-body"].rows]
    assert "CB-3" not in body_ids
    assert "CB-3c" not in body_ids
    by_name = {g.name: g for g in st.stack_groups(carmine=True)}
    body_ids = [r.id for r in by_name["coop-body"].rows]
    assert "CB-3c" in body_ids
    assert "CB-3" not in body_ids


def test_hook_applies_groups_in_stack_order():
    hook = Path(__file__).resolve().parents[2] / "src" / "gow2_script_patch_hook.cpp"
    text = hook.read_text(encoding="utf-8")
    body = function_body(text, "void ApplyStartupScriptPatches")
    markers = ("ApplyAndLogGroup(base, group", "ApplySurvivalGroups(",
               "ApplyCoopFollowUps(", "ApplyRegisteredGroups(")
    positions = [body.index(m) for m in markers]
    assert positions == sorted(positions)
    follow_ups = function_body(text, "void ApplyCoopFollowUps")
    assert follow_ups.index("ApplyCoopBody") < follow_ups.index("CoopRideGroup")


def first_second_groups(check_byte):
    ref = sp.Ref("Fake.Pkg.Func", "Function")
    first = sp.ScriptRow("T-1", "first", ref, 4,
                         byte_edits=(sp.ByteEdit(1, 0x20, 0x21),))
    second = sp.ScriptRow("T-2", "second", ref, 4,
                          checks=(sp.Check(1, bytes([check_byte])),),
                          byte_edits=(sp.ByteEdit(2, 0x30, 0x31),))
    return [st.StackGroup("first", (first,)), st.StackGroup("second", (second,))]


def test_later_group_sees_earlier_bytes():
    model = FakeModel(bytes([0x10, 0x20, 0x30, 0x40]))
    grouped, overlay, changed = st.plan_stack(model, first_second_groups(0x21), 4)
    assert [g.name for g, _ in grouped] == ["first", "second"]
    assert overlay[model.handle] == bytes([0x10, 0x21, 0x31, 0x40])
    assert changed[model.handle] == ["first", "second"]


def test_skip_names_the_group():
    model = FakeModel(bytes([0x10, 0x20, 0x30, 0x40]))
    with pytest.raises(st.StackSkip) as exc:
        st.plan_stack(model, first_second_groups(0x99), 4)
    assert exc.value.group == "second"


def test_main_rejects_bad_arguments():
    assert st.main([]) == 2
    assert st.main(["--check", "x", "--players", "2"]) == 2


@pytest.mark.parametrize("players", [8, 10])
def test_main_accepts_large_counts(monkeypatch, players):
    seen = []
    monkeypatch.setattr(st, "check_stack", lambda pkg, n: seen.append((pkg, n)) or 0)
    assert st.main(["--check", "x", "--players", str(players)]) == 0
    assert seen == [("x", players)]


@pytest.mark.parametrize("players", [8, 10])
def test_stack_plans_for_large_counts(players):
    model = FakeModel(bytes([0x10, 0x20, 0x30, 0x40]))
    grouped, overlay, changed = st.plan_stack(model, first_second_groups(0x21), players)
    assert [g.name for g, _ in grouped] == ["first", "second"]
    assert overlay[model.handle] == bytes([0x10, 0x21, 0x31, 0x40])
