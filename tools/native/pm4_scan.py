"""Finds the recompiled functions that build Xenos PM4 packet headers (native renderer, phase 0).

The generated C++ keeps every original PowerPC instruction as a comment. A type-3 packet header is
0xC0000000 | (count - 1) << 16 | opcode << 8; the Direct3D library builds it with `lis` (upper half)
and `ori` (opcode byte), or `oris`/`addi`. This scans each function for those pairs and reports the
opcodes it emits, so the draw, constant and state setters of the XDK's Direct3D can be named without
symbols. Usage:

  python tools/native/pm4_scan.py [generated/default] [--json out.json]
"""
import argparse
import glob
import json
import os
import re
import sys
from collections import defaultdict

FUNC_RE = re.compile(r"^DEFINE_REX_FUNC\((sub_[0-9A-F]{8})\)")
ASM_RE = re.compile(r"^\s*//\s*(\w+\.?)\s+(.*)$")

OPCODES = {
    0x10: "NOP", 0x21: "REG_RMW", 0x22: "DRAW_INDX", 0x23: "VIZ_QUERY", 0x25: "SET_STATE",
    0x27: "IM_LOAD", 0x2B: "IM_LOAD_IMMEDIATE", 0x2D: "SET_CONSTANT", 0x2F: "LOAD_ALU_CONSTANT",
    0x36: "DRAW_INDX_2", 0x37: "CONTEXT_UPDATE", 0x3B: "INVALIDATE_STATE", 0x3C: "WAIT_REG_MEM",
    0x3D: "MEM_WRITE", 0x3E: "REG_TO_MEM", 0x3F: "INDIRECT_BUFFER", 0x44: "WAIT_FOR_IDLE",
    0x45: "COND_WRITE", 0x46: "EVENT_WRITE", 0x48: "ME_INIT", 0x4A: "FIX_2_FLT_REG",
    0x55: "SET_CONSTANT2", 0x56: "SET_SHADER_CONSTANTS", 0x58: "EVENT_WRITE_SHD",
    0x59: "EVENT_WRITE_EXT", 0x5A: "EVENT_WRITE_ZPD", 0x60: "SET_BIN_MASK_LO",
    0x61: "SET_BIN_MASK_HI", 0x62: "SET_BIN_SELECT_LO", 0x63: "SET_BIN_SELECT_HI",
}


def parse_imm(text):
    text = text.strip()
    return int(text, 16) if text.lower().startswith(("0x", "-0x")) else int(text)


def u32(value):
    return value & 0xFFFFFFFF


def header_opcode(value):
    value = u32(value)
    if value >> 30 != 3:
        return None
    opcode = (value >> 8) & 0x7F
    return opcode if opcode in OPCODES else None


def scan_function(lines):
    """Returns {opcode: count} for the type-3 headers this function builds."""
    upper = {}
    found = defaultdict(int)
    for line in lines:
        m = ASM_RE.match(line)
        if not m:
            continue
        op, args = m.group(1), [a.strip() for a in m.group(2).split(",")]
        if op == "lis" and len(args) == 2:
            upper[args[0]] = u32(parse_imm(args[1]) << 16)
            opcode = header_opcode(upper[args[0]])
            if opcode is not None:
                found[opcode] += 1
        elif op == "li" and len(args) == 2:
            # The low half first, the 0xC0.. half added by a later oris.
            upper[args[0]] = u32(parse_imm(args[1]))
        elif op in ("ori", "addi", "oris") and len(args) == 3 and args[1] in upper:
            imm = parse_imm(args[2])
            if op == "ori":
                value = upper[args[1]] | (imm & 0xFFFF)
            elif op == "oris":
                value = upper[args[1]] | ((imm & 0xFFFF) << 16)
            else:
                value = u32(upper[args[1]] + imm)
            upper[args[0]] = value
            opcode = header_opcode(value)
            if opcode is not None:
                found[opcode] += 1
        elif args and args[0] in upper and op not in ("stw", "stwx", "stwu", "cmplw", "cmpw"):
            upper.pop(args[0], None)
    return found


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
                            result[name] = dict(hits)
                    name, body = m.group(1), []
                elif name:
                    body.append(line)
        if name:
            hits = scan_function(body)
            if hits:
                result[name] = dict(hits)
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("folder", nargs="?", default=os.path.join("generated", "default"))
    ap.add_argument("--json", help="write {function: {opcode name: count}} here")
    args = ap.parse_args()
    found = scan(args.folder)
    by_opcode = defaultdict(list)
    for func, hits in found.items():
        for opcode in hits:
            by_opcode[OPCODES[opcode]].append(func)
    for name in sorted(by_opcode, key=lambda n: -len(by_opcode[n])):
        funcs = sorted(by_opcode[name])
        print(f"{name:22} {len(funcs):4} functions: {' '.join(funcs[:12])}{' ...' if len(funcs) > 12 else ''}")
    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump({k: {OPCODES[o]: c for o, c in v.items()} for k, v in sorted(found.items())}, f, indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
