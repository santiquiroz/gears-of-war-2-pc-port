import subprocess
import sys
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent / "localize_fix.py"

# sub_A keeps r30 as a local and calls sub_C, which never names r30 but calls sub_B, which
# reads r30 before setting it: sub_A must hand its r30 to sub_B through ctx.
SOURCE = """\
DEFINE_REX_FUNC(sub_B) {
\tREX_FUNC_PROLOGUE();
\tPPCRegister r30{};
\t// lwz r3,0(r30)
\tctx.r3.u64 = REX_LOAD_U32(r30.u32 + 0);
\treturn;
}

DEFINE_REX_FUNC(sub_C) {
\tREX_FUNC_PROLOGUE();
\tctx.lr = 0x82000010;
\tsub_B(ctx, base);
\treturn;
}

DEFINE_REX_FUNC(sub_A) {
\tREX_FUNC_PROLOGUE();
\tPPCRegister r30{};
\tr30.u64 = ctx.r4.u64;
\tctx.lr = 0x82000020;
\tsub_C(ctx, base);
\treturn;
}
"""


# Without the *_as_local options every register lives in ctx and the helpers still save them.
PLAIN = """\
DEFINE_REX_FUNC(sub_D) {
\tREX_FUNC_PROLOGUE();
\t// bl 0x82a930dc
\tctx.lr = 0x82000030;
\t__savegprlr_29(ctx, base);
\tctx.r31.u64 = ctx.r3.u64;
\tctx.r1.s64 = ctx.r1.s64 + 112;
\t__restgprlr_29(ctx, base);
\treturn;
}
"""

INIT = "{ 0x82A930DC, __savegprlr_29 },\n"


def run(folder):
    return subprocess.run([sys.executable, str(SCRIPT), str(folder / "*.cpp")], capture_output=True, text=True, check=True)


def function(text, name):
    start = text.index(f"DEFINE_REX_FUNC({name})")
    return text[start:text.index("\n}", start)]


def test_shared_register_goes_through_ctx_and_second_run_changes_nothing(tmp_path):
    path = tmp_path / "gow2_recomp.0.cpp"
    path.write_text(SOURCE)
    run(tmp_path)
    text = path.read_text()
    b = function(text, "sub_B")
    assert "PPCRegister r30{}" not in b
    assert "REX_LOAD_U32(ctx.r30.u32 + 0)" in b
    a = function(text, "sub_A").splitlines()
    call = next(i for i, line in enumerate(a) if "sub_C(ctx, base);" in line)
    assert [line.split("  //")[0].strip() for line in a[call - 2:call + 3]] == [
        "lf_call_r30 = ctx.r30;",
        "ctx.r30 = r30;",
        "sub_C(ctx, base);",
        "r30 = ctx.r30;",
        "ctx.r30 = lf_call_r30;",
    ]
    assert "PPCRegister r30{}" in "\n".join(a)
    run(tmp_path)
    assert path.read_text() == text


# The codegen copies r31 to ctx before a funclet call, so sub_E must give its caller's r31 back;
# it ends in a call, without a return statement.
FALLS_OFF_END = """\
DEFINE_REX_FUNC(sub_E) {
\tREX_FUNC_PROLOGUE();
\tPPCRegister r31{};
\tr31.u64 = ctx.r3.u64;
\tctx.r31 = r31;
\tctx.lr = 0x82000040;
\tsub_F(ctx, base);
}
"""


def test_restore_at_the_end_of_a_function_is_written_once(tmp_path):
    path = tmp_path / "gow2_recomp.0.cpp"
    path.write_text(FALLS_OFF_END)
    run(tmp_path)
    text = path.read_text()
    assert text.count("lf_in_r31 = ctx.r31;") == 1
    assert text.count("ctx.r31 = lf_in_r31;") == 1
    assert text.index("sub_F(ctx, base);") < text.index("ctx.r31 = lf_in_r31;")
    run(tmp_path)
    assert path.read_text() == text


def test_code_without_register_locals_is_left_alone(tmp_path):
    (tmp_path / "gow2_init.cpp").write_text(INIT)
    path = tmp_path / "gow2_recomp.0.cpp"
    path.write_text(PLAIN)
    run(tmp_path)
    assert path.read_text() == PLAIN
