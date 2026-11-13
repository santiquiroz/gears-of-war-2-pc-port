"""Android copy of the recompiled game code, with the patches the Android build needs.

The Windows build compiles generated/default in place. ARM additionally needs real memory
barriers (patch_memory_barriers.py), and those must never reach the Windows tree, so the Android
build compiles a copy:

  1. copies <source> into <dest>.staging,
  2. runs patch_switch_traps.py, patch_game_patches.py, localize_fix.py,
     patch_memory_barriers.py and patch_spin_hints.py on that copy (all are idempotent, so a
     source the Windows build already patched is fine),
  3. moves into <dest> only the files whose bytes changed, so ninja recompiles only those, and
     deletes files that are no longer in <source>.

    python tools/prepare_android_sources.py [--source generated/default] [--dest out/android/generated/default]

The source folder is only read.
"""
import argparse
import filecmp
import os
import shutil
import subprocess
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent
PATCHERS = (
    "patch_switch_traps.py",
    "patch_game_patches.py",
    "localize_fix.py",
    "patch_memory_barriers.py",
    "patch_spin_hints.py",
)
SKIPPED_SUFFIXES = (".tmp",)


def patcher_commands(staging: Path) -> list[list[str]]:
    sources = str(staging / "*.cpp")
    return [[sys.executable, str(TOOLS / name), sources] for name in PATCHERS]


def is_copied(name: str) -> bool:
    return not name.endswith(SKIPPED_SUFFIXES)


def copy_tree(source: Path, staging: Path) -> None:
    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir(parents=True)
    for entry in sorted(source.iterdir()):
        if entry.is_file() and is_copied(entry.name):
            shutil.copyfile(entry, staging / entry.name)


def run_patchers(commands: list[list[str]]) -> None:
    for command in commands:
        result = subprocess.run(command, cwd=ROOT)
        if result.returncode != 0:
            raise RuntimeError(f"{Path(command[1]).name} failed with exit code {result.returncode}")


def sync_changed(staging: Path, dest: Path) -> tuple[int, int, int]:
    dest.mkdir(parents=True, exist_ok=True)
    staged = {entry.name for entry in staging.iterdir() if entry.is_file()}
    replaced = kept = 0
    for name in sorted(staged):
        target = dest / name
        if target.is_file() and filecmp.cmp(staging / name, target, shallow=False):
            kept += 1
            continue
        os.replace(staging / name, target)
        replaced += 1
    removed = 0
    for entry in dest.iterdir():
        if entry.is_file() and entry.name not in staged:
            entry.unlink()
            removed += 1
    return replaced, kept, removed


def same_folder(a: Path, b: Path) -> bool:
    return a.resolve() == b.resolve()


def prepare(source: Path, dest: Path, commands=patcher_commands) -> tuple[int, int, int]:
    if not (source / "sources.cmake").is_file():
        raise RuntimeError(f"{source} has no sources.cmake; run rexglue codegen first")
    if same_folder(source, dest):
        raise RuntimeError("the destination must be a separate copy, never the codegen output itself")
    staging = dest.with_name(dest.name + ".staging")
    copy_tree(source, staging)
    run_patchers(commands(staging))
    counts = sync_changed(staging, dest)
    shutil.rmtree(staging)
    return counts


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(prog="prepare_android_sources")
    parser.add_argument("--source", default=str(ROOT / "generated" / "default"))
    parser.add_argument("--dest", default=str(ROOT / "out" / "android" / "generated" / "default"))
    options = parser.parse_args(argv)
    try:
        replaced, kept, removed = prepare(Path(options.source), Path(options.dest))
    except RuntimeError as error:
        print(f"prepare_android_sources: {error}", file=sys.stderr)
        return 1
    print(f"prepare_android_sources: {replaced} file(s) updated, {kept} unchanged, {removed} removed in {options.dest}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
