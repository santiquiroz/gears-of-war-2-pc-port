import subprocess
import sys
from pathlib import Path

import pytest
from PIL import Image

from image_diff import diff_ratio

SCRIPT = Path(__file__).with_name("image_diff.py")


def _solid(path: Path, color, size=(100, 100)) -> str:
    Image.new("RGB", size, color).save(path)
    return str(path)


def _with_block(path: Path, base, block_color, origin=(0, 0), block=(10, 10)) -> str:
    img = Image.new("RGB", (100, 100), base)
    patch = Image.new("RGB", block, block_color)
    img.paste(patch, origin)
    img.save(path)
    return str(path)


def test_identical_images_have_zero_diff(tmp_path):
    a = _solid(tmp_path / "a.png", (10, 20, 30))
    b = _solid(tmp_path / "b.png", (10, 20, 30))
    assert diff_ratio(a, b) == 0.0


def test_block_difference_of_100_is_one_percent(tmp_path):
    a = _solid(tmp_path / "a.png", (50, 50, 50))
    b = _with_block(tmp_path / "b.png", (50, 50, 50), (150, 50, 50))
    assert diff_ratio(a, b) == pytest.approx(0.01)


def test_difference_within_tolerance_is_ignored(tmp_path):
    a = _solid(tmp_path / "a.png", (100, 100, 100))
    b = _solid(tmp_path / "b.png", (110, 110, 110))
    assert diff_ratio(a, b, tolerance=24) == 0.0


def test_crop_isolates_changed_block(tmp_path):
    a = _solid(tmp_path / "a.png", (50, 50, 50))
    b = _with_block(tmp_path / "b.png", (50, 50, 50), (150, 50, 50))
    assert diff_ratio(a, b, crop=(0, 0, 10, 10)) == pytest.approx(1.0)


def test_mismatched_sizes_raise(tmp_path):
    a = _solid(tmp_path / "a.png", (0, 0, 0), size=(100, 100))
    b = _solid(tmp_path / "b.png", (0, 0, 0), size=(50, 50))
    with pytest.raises(ValueError):
        diff_ratio(a, b)


def test_cli_exits_1_above_max_and_prints_diff(tmp_path):
    a = _solid(tmp_path / "a.png", (50, 50, 50))
    b = _with_block(tmp_path / "b.png", (50, 50, 50), (150, 50, 50))
    result = subprocess.run(
        [sys.executable, str(SCRIPT), a, b, "--max", "0.005"],
        capture_output=True,
        text=True,
    )
    assert result.returncode == 1
    assert result.stdout.strip() == "diff 0.0100"
