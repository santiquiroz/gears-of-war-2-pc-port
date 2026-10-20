# Adapted from gears-judgement-recomp (BSD-3-Clause).
"""Post-codegen port of the xenia-canary game patches for Gears of War 2.

Source: xenia-canary/game-patches, patches/4D53082D base file
(title 4D53082D, module hash BDB97753374D520A = this exact retail XEX).
Verified that the file targets this exact image: every patched word below
matches the original instruction at that address in the decrypted image
(file offset == address - 0x82220000, big-endian) and the generated
code's comment there.

A patch in a recompile has to land on the C++ that implements the
instruction, not on bytes. Each site keeps the original behaviour behind a
host switch, `Gow2GamePatch(id)` (src/gow2_game_patches.cpp), so every
patch is a cvar and can be A/B'd without a rebuild:

  id 1 Black Shading Fix      82806A44 stw r11,11940(r6) -> li r11,0
                              (GUseTilingMode=FALSE; GameFixes/gow2_black_shading_fix=true)
  id 2 Texture Streaming Fix  8280D534 lwz r4,80(r1)     -> li r4,196 (PoolSize=196)
                              8280DDC8 lis r10,-31980    -> li r3,0 \
                              8280DDCC mr r11,r3         -> blr     / early "return 0"
                              (RHIGetTextureMemoryStats=FALSE; .../gow2_texture_streaming_fix=true)
  id 3 16x Anisotropic Filtering 8280E7FC lwz r5,408(r30) -> li r5,16
                              (GameFixes/gow2_aniso16=true)
  id 4 Unlock FPS           824A2E94 lfs f1,1284(r3)  -> nop (skipped when on)
                            8298CFB0 li r11,2         -> li r11,1
                             (GameFixes/gow2_unlock_fps=false)
  id 5 AO Upscaling Fix     82806B10 li r11,720       -> li r11,704 (height 704)
                            8272E00C lwz r10,15568(r17) -> li r10,1
                            8272FF54 lwz r11,15568(r11) -> li r11,1
                            8272B1F8 mflr r12 (function entry) -> blr (early return)
                            8252D608 beq cr6,0x8252d69c -> b 0x8252d69c (unconditional when on)
                             (GameFixes/gow2_ao_upscaling_fix=true)
  id 6 DoF Upscaling Fix    827296BC fsel f2,f4,f31,f28 -> fsubs f1,f1,f1 (f1=0)
                            82729700 bl 0x827291f8    -> fsubs f1,f1,f1 (skip call, f1=0)
                             (GameFixes/gow2_dof_upscaling_fix=true)
  id 7 Widescreen           82806B04 lfs f13,2856(r11) -> f13 = (Gow2GamePatch(7) + 0.25) / 720 when set
                             (sub_82806A80 builds the back buffer: width = aspect * 720, height 720 or
                             704 with id 5; the aspect is the 16:9 constant for any widescreen video
                             mode. Display/gow2_widescreen_aspect, 0 = off)
                            82806E64 blt cr6,0x82806b40 -> always taken when set, so the width is not
                             clamped to 1280 (the game clamps it to 900..1280)
                            82634F44, 82634CC8 fmuls f1,f1,f0 -> the half FOV is widened by W/1280, so
                             the vertical view stays what it was at 16:9 (Gow2GamePatch(7) is the
                             back buffer width in pixels, 0 = off; Gow2WidescreenHalfFov does the maths)

The id 2 blr part is portable only because 0x8280DDC8/0x8280DDCC are the
first two guest instructions of sub_8280DDC8 (no prologue); the patch is
therefore "return 0 immediately", emitted as an early return.
Likewise the id 5 blr at 0x8272B1F8 is portable only because it is the
FIRST guest instruction of sub_8272B1F8; the patch is "return
immediately", emitted as an early return.

The id 5 blr at 0x8280759C (mr r30,r3, 6th instruction of
sub_82807588) is REFUSED and has no entry below: a bl in the prologue
clobbers LR (lr=0x82807590 at that site), so the hardware blr could not
return to the caller, and the function's exits need explicit epilogue
(addi r1,r1,128 + __restgprlr_28) instead of a plain return. The paired
0x82807594 li r6,0 -> li r3,1 site is refused with it: on its own it would leave
r6 unset for the first sub_829783A8 call and keep running a function the patch
meant to leave.

    python tools/patch_game_patches.py generated/default/*.cpp

Idempotent (marker GOW2 GAMEPATCH). Fails closed if any site's instruction text or
C++ body differs from what is asserted below.
"""
from __future__ import annotations

import glob
import os
import re
import sys

MARK = "GOW2 GAMEPATCH"
DECL = (f'extern "C" int Gow2GamePatch(int id);  // {MARK}\n'
        f'extern "C" double Gow2WidescreenHalfFov(double half_fov, int site);  // {MARK}\n')

# (address, function start, expected comment, expected C++ body, patched C++ body, id)
SITES = [
    (0x82806A44, 0x828069F8, "stw r11,11940(r6)",
     ["REX_STORE_U32(ctx.r6.u32 + 11940, ctx.r11.u32);"],
     "if (Gow2GamePatch(1)) { ctx.r11.s64 = 0; } else { "
     "REX_STORE_U32(ctx.r6.u32 + 11940, ctx.r11.u32); }", 1),
    (0x8280D534, 0x8280D4C8, "lwz r4,80(r1)",
     ["ctx.r4.u64 = REX_LOAD_U32(ctx.r1.u32 + 80);"],
     "ctx.r4.u64 = Gow2GamePatch(2) ? 196 : REX_LOAD_U32(ctx.r1.u32 + 80);", 2),
    (0x8280DDC8, 0x8280DDC8, "lis r10,-31980",
     ["ctx.r10.s64 = -2095841280;"],
     "if (Gow2GamePatch(2)) { ctx.r3.s64 = 0; return; } ctx.r10.s64 = -2095841280;", 2),
    (0x8280DDCC, 0x8280DDC8, "mr r11,r3",
     ["ctx.r11.u64 = ctx.r3.u64;"],
     "if (!Gow2GamePatch(2)) { ctx.r11.u64 = ctx.r3.u64; }", 2),
    (0x8280E7FC, 0x8280E740, "lwz r5,408(r30)",
     ["ctx.r5.u64 = REX_LOAD_U32(ctx.r30.u32 + 408);"],
     "ctx.r5.u64 = Gow2GamePatch(3) ? 16 : REX_LOAD_U32(ctx.r30.u32 + 408);", 3),
    (0x824A2E94, 0x824A2E78, "lfs f1,1284(r3)",
     ["temp.u32 = REX_LOAD_U32(ctx.r3.u32 + 1284);",
      "ctx.f1.f64 = double(temp.f32);"],
     "if (!Gow2GamePatch(4)) { temp.u32 = REX_LOAD_U32(ctx.r3.u32 + 1284); "
     "ctx.f1.f64 = double(temp.f32); }", 4),
    (0x8298CFB0, 0x8298CEC0, "li r11,2",
     ["ctx.r11.s64 = 2;"],
     "ctx.r11.s64 = Gow2GamePatch(4) ? 1 : 2;", 4),
    (0x82806B10, 0x82806A80, "li r11,720",
     ["ctx.r11.s64 = 720;"],
     "ctx.r11.s64 = Gow2GamePatch(5) ? 704 : 720;", 5),
    (0x82806B04, 0x82806A80, "lfs f13,2856(r11)",
     ["ctx.fpscr.disableFlushMode();",
      "temp.u32 = REX_LOAD_U32(ctx.r11.u32 + 2856);",
      "ctx.f13.f64 = double(temp.f32);"],
     "ctx.fpscr.disableFlushMode(); temp.u32 = REX_LOAD_U32(ctx.r11.u32 + 2856); "
     "ctx.f13.f64 = Gow2GamePatch(7) ? (double(Gow2GamePatch(7)) + 0.25) / 720.0 : double(temp.f32);", 7),
    # The FOV (degrees) becomes a half angle in radians here; the widescreen patch widens it so the
    # vertical view stays what it was at 16:9 (both functions build a perspective matrix).
    (0x82634F44, 0x82634F00, "fmuls f1,f1,f0",
     ["ctx.f1.f64 = double(float(ctx.f1.f64 * ctx.f0.f64));"],
     "ctx.f1.f64 = double(float(ctx.f1.f64 * ctx.f0.f64)); "
     "if (Gow2GamePatch(7)) ctx.f1.f64 = double(float(Gow2WidescreenHalfFov(ctx.f1.f64, 1)));", 7),
    (0x82634CC8, 0x82634CA8, "fmuls f1,f1,f0",
     ["ctx.f1.f64 = double(float(ctx.f1.f64 * ctx.f0.f64));"],
     "ctx.f1.f64 = double(float(ctx.f1.f64 * ctx.f0.f64)); "
     "if (Gow2GamePatch(7)) ctx.f1.f64 = double(float(Gow2WidescreenHalfFov(ctx.f1.f64, 2)));", 7),
    (0x8252D644, 0x8252D1B8, "fmuls f1,f30,f0",
     ["ctx.f1.f64 = double(float(ctx.f30.f64 * ctx.f0.f64));"],
     "ctx.f1.f64 = double(float(ctx.f30.f64 * ctx.f0.f64)); "
     "if (Gow2GamePatch(7)) ctx.f1.f64 = double(float(Gow2WidescreenHalfFov(ctx.f1.f64, 3)));", 7),
    (0x8252D750, 0x8252D1B8, "fmuls f1,f30,f0",
     ["ctx.f1.f64 = double(float(ctx.f30.f64 * ctx.f0.f64));"],
     "ctx.f1.f64 = double(float(ctx.f30.f64 * ctx.f0.f64)); "
     "if (Gow2GamePatch(7)) ctx.f1.f64 = double(float(Gow2WidescreenHalfFov(ctx.f1.f64, 4)));", 7),
    (0x822D4A60, 0x822D4A20, "fmuls f31,f13,f0",
     ["ctx.f31.f64 = double(float(ctx.f13.f64 * ctx.f0.f64));"],
     "ctx.f31.f64 = double(float(ctx.f13.f64 * ctx.f0.f64)); "
     "if (Gow2GamePatch(7)) ctx.f31.f64 = double(float(Gow2WidescreenHalfFov(ctx.f31.f64, 5)));", 7),
    (0x8254127C, 0x825411A8, "fmuls f1,f13,f0",
     ["ctx.f1.f64 = double(float(ctx.f13.f64 * ctx.f0.f64));"],
     "ctx.f1.f64 = double(float(ctx.f13.f64 * ctx.f0.f64)); "
     "if (Gow2GamePatch(7)) ctx.f1.f64 = double(float(Gow2WidescreenHalfFov(ctx.f1.f64, 6)));", 7),
    (0x8254130C, 0x825411A8, "fmuls f1,f13,f0",
     ["ctx.f1.f64 = double(float(ctx.f13.f64 * ctx.f0.f64));"],
     "ctx.f1.f64 = double(float(ctx.f13.f64 * ctx.f0.f64)); "
     "if (Gow2GamePatch(7)) ctx.f1.f64 = double(float(Gow2WidescreenHalfFov(ctx.f1.f64, 7)));", 7),
    (0x82806E64, 0x82806A80, "blt cr6,0x82806b40",
     ["if (ctx.cr6.lt) goto loc_82806B40;"],
     "if (ctx.cr6.lt || Gow2GamePatch(7)) goto loc_82806B40;", 7),
    (0x8272E00C, 0x8272DF48, "lwz r10,15568(r17)",
     ["ctx.r10.u64 = REX_LOAD_U32(ctx.r17.u32 + 15568);"],
     "ctx.r10.u64 = Gow2GamePatch(5) ? 1 : REX_LOAD_U32(ctx.r17.u32 + 15568);", 5),
    (0x8272FF54, 0x8272FD20, "lwz r11,15568(r11)",
     ["ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 15568);"],
     "ctx.r11.u64 = Gow2GamePatch(5) ? 1 : REX_LOAD_U32(ctx.r11.u32 + 15568);", 5),
    (0x8272B1F8, 0x8272B1F8, "mflr r12",
     ["ctx.r12.u64 = ctx.lr;"],
     "if (Gow2GamePatch(5)) { return; } ctx.r12.u64 = ctx.lr;", 5),
    (0x8252D608, 0x8252D1B8, "beq cr6,0x8252d69c",
     ["if (ctx.cr6.eq) goto loc_8252D69C;"],
     "if (Gow2GamePatch(5) || ctx.cr6.eq) goto loc_8252D69C;", 5),
    (0x827296BC, 0x82729610, "fsel f2,f4,f31,f28",
     ["ctx.f2.f64 = ctx.f4.f64 >= 0.0 ? ctx.f31.f64 : ctx.f28.f64;"],
     "if (Gow2GamePatch(6)) { ctx.f1.f64 = double(float(ctx.f1.f64 - ctx.f1.f64)); } else { "
     "ctx.f2.f64 = ctx.f4.f64 >= 0.0 ? ctx.f31.f64 : ctx.f28.f64; }", 6),
    (0x82729700, 0x82729610, "bl 0x827291f8",
     ["ctx.lr = 0x82729704;",
      "sub_827291F8(ctx, base);"],
     "if (Gow2GamePatch(6)) { ctx.f1.f64 = double(float(ctx.f1.f64 - ctx.f1.f64)); } else { "
     "ctx.lr = 0x82729704; sub_827291F8(ctx, base); }", 6),
]

INSN = re.compile(r"^\s*// [a-z]")


def main(argv: list[str]) -> int:
    files: list[str] = []
    for a in argv:
        files.extend(glob.glob(a) if any(c in a for c in "*?[") else [a])
    by_func: dict[int, str] = {}
    head = re.compile(r"^DEFINE_REX_FUNC\(sub_([0-9A-F]{8})\)")
    wanted = {s[1] for s in SITES}
    for f in files:
        with open(f, errors="replace") as fh:
            for line in fh:
                m = head.match(line)
                if m and int(m.group(1), 16) in wanted:
                    by_func[int(m.group(1), 16)] = f
    missing = wanted - by_func.keys()
    if missing:
        print(f"patch_game_patches: functions not found: {sorted(hex(x) for x in missing)}")
        return 1
    edits: dict[str, list] = {}
    for site in SITES:
        edits.setdefault(by_func[site[1]], []).append(site)
    total = 0
    for f, sites in edits.items():
        with open(f, errors="replace", newline="") as fh:
            lines = fh.readlines()
        changed = False
        for addr, fstart, want, body, new, pid in sites:
            start = next(i for i, l in enumerate(lines)
                         if l.startswith(f"DEFINE_REX_FUNC(sub_{fstart:08X})"))
            k = (addr - fstart) // 4
            seen = -1
            for j in range(start + 1, len(lines)):
                if lines[j].startswith("DEFINE_REX_FUNC"):
                    raise SystemExit(f"{addr:08X}: ran off sub_{fstart:08X}")
                if INSN.match(lines[j]):
                    seen += 1
                    if seen == k:
                        break
            comment = lines[j].strip()
            if MARK in lines[j + 1]:
                continue  # already patched
            if comment != f"// {want}":
                raise SystemExit(f"{addr:08X}: expected '// {want}', found '{comment}'")
            got = [lines[j + 1 + n].strip() for n in range(len(body))]
            if got != body:
                raise SystemExit(f"{addr:08X}: body mismatch {got}")
            indent = re.match(r"\s*", lines[j + 1]).group(0)
            nl = "\r\n" if lines[j + 1].endswith("\r\n") else "\n"
            lines[j + 1:j + 1 + len(body)] = [f"{indent}{new}  // {MARK} {pid}{nl}"]
            changed = True
            total += 1
        if changed:
            text = "".join(lines)
            if DECL not in text:
                text = DECL + text
            tmp = f + ".gp.tmp"
            with open(tmp, "w", errors="replace", newline="") as fh:
                fh.write(text)
            os.replace(tmp, f)
    print(f"patch_game_patches: {total} site(s) patched")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
