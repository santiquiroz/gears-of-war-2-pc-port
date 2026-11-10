import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import patch_game_patches as pgp


def test_current_marked_line_is_unchanged():
    line = "\tctx.r11.s64 = Gow2GamePatch(8);  // GOW2 GAMEPATCH 5\r\n"

    assert pgp.rewrite_marked_line(line, "ctx.r11.s64 = Gow2GamePatch(8);", 5) is None


def test_stale_marked_line_rewrite_keeps_tab_and_crlf():
    line = "\tctx.r11.s64 = Gow2GamePatch(5) ? 704 : 720;  // GOW2 GAMEPATCH 5\r\n"

    assert pgp.rewrite_marked_line(line, "ctx.r11.s64 = Gow2GamePatch(8);", 5) == (
        "\tctx.r11.s64 = Gow2GamePatch(8);  // GOW2 GAMEPATCH 5\r\n"
    )


def test_stale_marked_line_rewrite_keeps_tab_and_lf():
    line = "\tctx.r11.s64 = Gow2GamePatch(5) ? 704 : 720;  // GOW2 GAMEPATCH 5\n"

    assert pgp.rewrite_marked_line(line, "ctx.r11.s64 = Gow2GamePatch(8);", 5) == (
        "\tctx.r11.s64 = Gow2GamePatch(8);  // GOW2 GAMEPATCH 5\n"
    )
