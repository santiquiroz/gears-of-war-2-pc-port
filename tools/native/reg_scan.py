"""Finds recompiled functions that mention Xenos GPU register indices (native renderer, phase 0).

Each function starts with DEFINE_REX_FUNC(sub_XXXXXXXX). Original PowerPC instructions are kept
as comment lines like `	// li r27,8984`. This scanner checks `li`, `ori`, `addi`, `cmplwi`
for 16-bit immediates that match the configured register indices.
Usage:
  python tools/native/reg_scan.py [folder] [--json out.json]
"""
import argparse
import glob
import json
import os
import re
import sys
import tempfile
from collections import defaultdict

FUNC_RE = re.compile(r"^DEFINE_REX_FUNC\((sub_[0-9A-F]{8})\)")
ASM_RE = re.compile(r"^\s*//\s*(\w+)\s+(.*)$")

REGISTERS = {
    "RB_SURFACE_INFO": 0x2000, "RB_COLOR_INFO": 0x2001, "RB_DEPTH_INFO": 0x2002,
    "RB_COLOR1_INFO": 0x2003, "RB_COLOR2_INFO": 0x2004, "RB_COLOR3_INFO": 0x2005,
    "RB_COPY_CONTROL": 0x2318, "RB_COPY_DEST_BASE": 0x2319, "RB_COPY_DEST_PITCH": 0x231A,
    "RB_COPY_DEST_INFO": 0x231B, "SQ_PROGRAM_CNTL": 0x2180, "SQ_CONTEXT_MISC": 0x2181,
    "PA_SC_WINDOW_SCISSOR_TL": 0x2081, "PA_CL_VPORT_XSCALE": 0x210F,
    "RB_MODECONTROL": 0x2208, "RB_BLENDCONTROL0": 0x2201, "RB_DEPTHCONTROL": 0x2200,
    "VGT_DRAW_INITIATOR": 0x21FC,
}


def parse_imm(text):
    text = text.strip()
    return int(text, 16) if text.lower().startswith(("0x", "-0x")) else int(text)


def u32(value):
    return value & 0xFFFFFFFF


def scan_function(lines):
    hits = defaultdict(int)
    for line in lines:
        m = ASM_RE.match(line)
        if not m:
            continue
        op, args = m.group(1), [a.strip() for a in m.group(2).split(",")]
        if op not in ("li", "ori", "addi", "cmplwi") or len(args) < 2:
            continue
        try:
            masked = u32(parse_imm(args[-1])) & 0xFFFF
        except ValueError:
            continue
        for reg_name, reg_idx in REGISTERS.items():
            if masked == reg_idx:
                hits[reg_name] += 1
    return dict(hits)


def scan(folder):
    result = {}
    for path in sorted(glob.glob(os.path.join(folder, "gow2_recomp.*.cpp"))):
        name, body = None, []
        with open(path, encoding="utf-8", errors="replace") as f:
            for line in f:
                m = FUNC_RE.match(line)
                if m:
                    if name:
                        hits = scan_function(body)
                        if hits:
                            result[name] = hits
                    name, body = m.group(1), []
                elif name:
                    body.append(line)
        if name:
            hits = scan_function(body)
            if hits:
                result[name] = hits
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("folder", nargs="?", default=os.path.join("generated", "default"))
    ap.add_argument("--json", help="write scan dict here")
    ap.add_argument("--self-test", action="store_true", help="run self-test")
    args = ap.parse_args()

    if args.self_test:

        with tempfile.TemporaryDirectory() as tmpdir:
            cpp_path = os.path.join(tmpdir, "gow2_recomp.0.cpp")
            with open(cpp_path, "w", encoding="utf-8") as f:
                f.write("DEFINE_REX_FUNC(sub_82000000) {\n")
                f.write("\t// li r27,8984\n")
                f.write("\t// ori r11,r11,8193\n")
                f.write("}\n")
                f.write("DEFINE_REX_FUNC(sub_82000010) {\n")
                f.write("\t// li r3,5\n")
                f.write("}\n")
                f.write("DEFINE_REX_FUNC(sub_82000020) {\n")
                f.write("\t// li r4,-1\n")
                f.write("}\n")
            result = scan(tmpdir)
            assert result == {"sub_82000000": {"RB_COPY_CONTROL": 1, "RB_COLOR_INFO": 1}}, f"Unexpected result: {result}"
            assert "sub_82000020" not in result
        print("self-test ok")
        return 0

    found = scan(args.folder)
    # Aggregate by register: register -> set of functions
    by_reg = defaultdict(lambda: defaultdict(int))
    for func, regs in found.items():
        for reg_name, count in regs.items():
            by_reg[reg_name][func] += count

    reg_funcs = {}
    for reg in sorted(by_reg):
        funcs = sorted(by_reg[reg])
        reg_funcs[reg] = funcs

    for reg_name in sorted(reg_funcs, key=lambda r: -len(reg_funcs[r])):
        funcs = reg_funcs[reg_name]
        func_list = sorted(funcs)
        display = " ".join(func_list[:12])
        suffix = " ..." if len(func_list) > 12 else ""
        print(f"{reg_name:24} {len(func_list):>4} functions: {display}{suffix}")

    if args.json:
        # Output sorted scan dict
        out = {k: dict(sorted(v.items())) for k, v in sorted(found.items())}
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump(out, f, indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
