# Tests for tools/prepare_android_sources.py. All inputs are synthetic; no game code needed.
import os
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))

import prepare_android_sources as pas

APPEND_FENCE = (
    "import glob, sys\n"
    "for path in glob.glob(sys.argv[1]):\n"
    "    with open(path, 'a', encoding='utf-8') as handle:\n"
    "        handle.write('// fenced\\n')\n"
)


def make_codegen(folder: Path, files: dict[str, str]) -> Path:
    folder.mkdir(parents=True)
    (folder / "sources.cmake").write_text("set(GENERATED_SOURCES)\n")
    for name, text in files.items():
        (folder / name).write_text(text)
    return folder


def fake_patchers(tmp_path: Path):
    script = tmp_path / "fake_patcher.py"
    script.write_text(APPEND_FENCE)
    return lambda staging: [[sys.executable, str(script), str(staging / "*.cpp")]]


def set_old_mtime(path: Path) -> float:
    old = 1_000_000_000.0
    os.utime(path, (old, old))
    return old


def test_copy_is_patched_and_source_untouched(tmp_path):
    source = make_codegen(tmp_path / "generated" / "default", {"a.cpp": "int a;\n", "a.h": "#pragma once\n"})
    dest = tmp_path / "out" / "generated" / "default"
    pas.prepare(source, dest, commands=fake_patchers(tmp_path))
    assert (dest / "a.cpp").read_text() == "int a;\n// fenced\n"
    assert (dest / "a.h").read_text() == "#pragma once\n"
    assert (source / "a.cpp").read_text() == "int a;\n"


def test_unchanged_files_keep_their_timestamp(tmp_path):
    source = make_codegen(tmp_path / "gen", {"a.cpp": "int a;\n", "b.cpp": "int b;\n"})
    dest = tmp_path / "dest"
    pas.prepare(source, dest, commands=fake_patchers(tmp_path))
    old = set_old_mtime(dest / "a.cpp")
    set_old_mtime(dest / "b.cpp")
    (source / "b.cpp").write_text("int b2;\n")
    replaced, kept, removed = pas.prepare(source, dest, commands=fake_patchers(tmp_path))
    assert (dest / "a.cpp").stat().st_mtime == old
    assert (dest / "b.cpp").stat().st_mtime > old
    assert (dest / "b.cpp").read_text() == "int b2;\n// fenced\n"
    assert (replaced, kept, removed) == (1, 2, 0)


def test_files_gone_from_the_source_are_removed(tmp_path):
    source = make_codegen(tmp_path / "gen", {"a.cpp": "int a;\n", "b.cpp": "int b;\n"})
    dest = tmp_path / "dest"
    pas.prepare(source, dest, commands=fake_patchers(tmp_path))
    (source / "b.cpp").unlink()
    _, _, removed = pas.prepare(source, dest, commands=fake_patchers(tmp_path))
    assert removed == 1
    assert not (dest / "b.cpp").exists()


def test_staging_folder_is_cleaned_up(tmp_path):
    source = make_codegen(tmp_path / "gen", {"a.cpp": "int a;\n"})
    dest = tmp_path / "dest"
    pas.prepare(source, dest, commands=fake_patchers(tmp_path))
    assert not (tmp_path / "dest.staging").exists()


def test_refuses_to_patch_the_codegen_output_itself(tmp_path):
    source = make_codegen(tmp_path / "gen", {"a.cpp": "int a;\n"})
    with pytest.raises(RuntimeError, match="separate copy"):
        pas.prepare(source, source, commands=fake_patchers(tmp_path))
    assert (source / "a.cpp").read_text() == "int a;\n"


def test_missing_codegen_is_reported(tmp_path):
    with pytest.raises(RuntimeError, match="sources.cmake"):
        pas.prepare(tmp_path / "nothing", tmp_path / "dest", commands=fake_patchers(tmp_path))


def test_failing_patcher_leaves_destination_alone(tmp_path):
    source = make_codegen(tmp_path / "gen", {"a.cpp": "int a;\n"})
    dest = tmp_path / "dest"
    failing = lambda staging: [[sys.executable, "-c", "import sys; sys.exit(3)", "x"]]
    with pytest.raises(RuntimeError, match="exit code 3"):
        pas.prepare(source, dest, commands=failing)
    assert not dest.exists()


def test_real_patchers_run_in_order():
    names = [Path(command[1]).name for command in pas.patcher_commands(Path("staging"))]
    assert names == ["patch_switch_traps.py", "patch_game_patches.py", "localize_fix.py", "patch_memory_barriers.py", "patch_spin_hints.py"]
