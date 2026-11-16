"""Offline twin of the unlockchapters rows in src/gow2_unlock_chapters.cpp.

It owns the patch rows, prints the C++ table and checks the rows against the
player's own decompressed packages.

python -I tools/unlock_chapters_patch.py --check <dir with GearGame.upk>
python -I tools/unlock_chapters_patch.py --cpp
python -I tools/unlock_chapters_patch.py --cpp --write
"""
import argparse
import sys
import types
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import script_patch as sp

UNL_1 = sp.ScriptRow("UNL-1", "HasActBeenUnlockedForAccess -> return true (act list: every act can be picked)",
    sp.Ref("GearGame.GearProfileSettings.HasActBeenUnlockedForAccess", "Function"), 98,
    checks=(sp.Check(0x06, b"\x00\x14\x00\x04\x1b"),
            sp.Check(0x1E, b"\x0f\x00"),
            sp.Check(0x35, b"\x07\x00\x4f\x1b"),
            sp.Check(0x56, b"\x06\x00\x25"),
            sp.Check(0x61, b"\x53")),
    operands=(sp.Operand(0x02, sp.Ref("GearGame.GearUIDataStore_GameResource", "Class")),
              sp.Operand(0x30, sp.Ref("GearGame.GearProfileSettings.HasActBeenUnlockedForAccess.ChapterList", "ArrayProperty")),
              sp.Operand(0x5D, sp.Ref("GearGame.GearProfileSettings.HasActBeenUnlockedForAccess.ReturnValue", "BoolProperty"))),
    byte_edits=(sp.ByteEdit(0x00, 0x12, 0x04), sp.ByteEdit(0x01, 0x20, 0x27)))

UNL_2 = sp.ScriptRow("UNL-2", "CanAccessChapter -> return true (every chapter can be picked)",
    sp.Ref("GearGame.GearUISceneFE_SelectChapter.CanAccessChapter", "Function"), 145,
    checks=(sp.Check(0x0C, b"\x0f\x00"),
            sp.Check(0x23, b"\x19\x10\x00"),
            sp.Check(0x42, b"\x07\x00\x88\x82\x77"),
            sp.Check(0x64, b"\x04\x19\x19\x00"),
            sp.Check(0x90, b"\x53")),
    operands=(sp.Operand(0x07, sp.Ref("Engine.UIScreenObject.GetBestPlayerIndex", "Function")),
              sp.Operand(0x35, sp.Ref("GearGame.GearCampaignActData.ChapterProviders", "ArrayProperty")),
              sp.Operand(0x8C, sp.Ref("GearGame.GearUISceneFE_SelectChapter.CanAccessChapter.ReturnValue", "BoolProperty"))),
    byte_edits=(sp.ByteEdit(0x00, 0x0F, 0x04), sp.ByteEdit(0x01, 0x00, 0x27)))

UNL_3 = sp.ScriptRow("UNL-3", "ShouldDisableElementChapter -> return false (no chapter is greyed out in the co-op lobby)",
    sp.Ref("GearGame.GearUISceneFELobby_Campaign.ShouldDisableElementChapter", "Function"), 297,
    checks=(sp.Check(0x02, b"\x20\x2d\x01"),
            sp.Check(0x35, b"\x07\x01\x20\x77\x00"),
            sp.Check(0x71, b"\x07\x01\x20\x96\x00"),
            sp.Check(0xCE, b"\x07\x01\x20\x82\x77"),
            sp.Check(0xF0, b"\x04\x81\x19\x19\x00"),
            sp.Check(0x128, b"\x53")),
    operands=(sp.Operand(0x05, sp.Ref("GearGame.GearUISceneFELobby_Campaign.bConstrainChapterToProfile", "BoolProperty")),
              sp.Operand(0xB8, sp.Ref("Engine.UIScreenObject.GetBestPlayerIndex", "Function")),
              sp.Operand(0x124, sp.Ref("GearGame.GearUISceneFELobby_Campaign.ShouldDisableElementChapter.ReturnValue", "BoolProperty"))),
    byte_edits=(sp.ByteEdit(0x00, 0x07, 0x04), sp.ByteEdit(0x01, 0x01, 0x28)))

UNL_4 = sp.ScriptRow("UNL-4", "InitializeWidgetReferences -> skip the Insane lock (Insane can be picked without finishing the campaign)",
    sp.Ref("GearGame.GearUISceneFE_Difficulty.InitializeWidgetReferences", "Function"), 889,
    checks=(sp.Check(0x9F, b"\x06\x00\x1b"),
            sp.Check(0xB5, b"\x01\x27\x82\x82\x77"),
            sp.Check(0xEF, b"\x1b"),
            sp.Check(0xF8, b"\x24\x00\x16\x16\x16\x0f"),
            sp.Check(0x127, b"\x19")),
    operands=(sp.Operand(0xAE, sp.Ref("Engine.UIScreenObject.GetBestPlayerIndex", "Function")),
              sp.Operand(0xBB, sp.Ref("GearGame.GearUISceneFE_Difficulty.InitializeWidgetReferences.PC", "ObjectProperty"))),
    byte_edits=(sp.ByteEdit(0xB4, 0x07, 0x06),))

ROWS = (UNL_1, UNL_2, UNL_3, UNL_4)

WANT = {"UNL-1": (0x00, "return true"), "UNL-2": (0x00, "return true"), "UNL-3": (0x00, "return false"), "UNL-4": (0xB4, "goto 0x127")}


def render_cpp():
    lines = [
        "// gow2 - ReXGlue Recompiled Project",
        "//",
        "// The unlockchapters rows: every campaign act and chapter can be picked in the",
        "// chapter-select and co-op lobby menus, and Insane can be picked without",
        "// finishing the campaign. Printed by",
        "// `python -I tools/unlock_chapters_patch.py --cpp`; edit the rows there, not here. Check them",
        "// against your own decompressed packages with `python -I tools/unlock_chapters_patch.py --check <folder>`.",
        "",
        '#include "gow2_unlock_chapters.h"',
        "",
        "namespace rex::glue::script_patch {",
        "",
        "namespace {",
        "",
    ]
    for row in ROWS:
        lines.extend(sp._script_lines(row))
        lines.append("")
    lines.extend([
        "}  // namespace",
        "",
        "PatchGroup UnlockChaptersGroup() {",
        "  return {",
        '      .name = "unlockchapters",',
        "      .scripts = {RowUNL_1(), RowUNL_2(), RowUNL_3(), RowUNL_4()},",
        "      .defaults = {},",
        "  };",
        "}",
        "",
        "}  // namespace rex::glue::script_patch",
    ])
    return "\n".join(lines) + "\n"


def _line_at(ue3script, model, handle, path, new_script, offset):
    f = types.SimpleNamespace(pkg=model.pkg, index=handle, path=path,
                              code_start=0, code_end=len(new_script))
    d = ue3script.Decoder.__new__(ue3script.Decoder)
    d.w = model.world
    d.f = f
    d.p = model.pkg
    d.b = new_script
    d.pos = offset
    d.base = 0
    return d.expr()


def _check_script_row(ue3script, model, entry, new_script):
    row = entry.row
    print(f"== {row.id} {row.function.path} ScriptSize={row.script_size}")
    offset, want = WANT[row.id]
    try:
        before = model.world.read_function(model.pkg, entry.handle)
        old_script = bytes(model.pkg.b[before.code_start:before.code_end])
        was = _line_at(ue3script, model, entry.handle, before.path, old_script, offset)
    except Exception as e:
        print(f"   original decode failed: {e}")
        return False
    try:
        now = _line_at(ue3script, model, entry.handle, before.path, new_script, offset)
    except Exception as e:
        print(f"   patched decode failed: {e}")
        return False
    print(f"   line at 0x{offset:02x} was: {was}")
    print(f"   line at 0x{offset:02x} now: {now}")
    if now != want:
        print(f"   expected the patched script at 0x{offset:02x} to be {want!r}")
        return False
    return True


def check_package(pkg_dir):
    ue3script, _ = sp._ue3_modules()
    print(f"== unlock_chapters_patch --check {pkg_dir}")
    try:
        model = sp.PackageModel(pkg_dir)
    except OSError as e:
        print(f"error: cannot load packages from {pkg_dir}: {e}", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"error: cannot load packages from {pkg_dir}: {type(e).__name__}({e})", file=sys.stderr)
        return 1
    try:
        plan = sp.plan_group(model, list(ROWS), 4)
    except sp.PatchSkip as e:
        print(f"SKIP {e.row}: {e.reason}")
        return 1
    patched = sp.apply_plan(model, plan)
    failed = False
    for entry in plan.entries:
        if _check_script_row(ue3script, model, entry, patched[entry.handle]):
            print(f"OK {entry.row.id} {entry.row.function.path}")
        else:
            print(f"FAIL {entry.row.id} {entry.row.function.path}")
            failed = True
    for line in sp.group_lines(plan):
        print(line)
    print("== C++ table")
    print(render_cpp(), end="")
    return 1 if failed else 0


def parse_args(argv=None):
    p = argparse.ArgumentParser(description="Offline twin of the unlockchapters script patch.")
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
            out = Path(__file__).resolve().parent.parent / "src" / "gow2_unlock_chapters_table.cpp"
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
