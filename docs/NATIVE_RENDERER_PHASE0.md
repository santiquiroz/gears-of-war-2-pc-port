# Native renderer, phase 0: identification log

Phase 0 of [NATIVE_RENDERER_STUDY.md](NATIVE_RENDERER_STUDY.md) (issue
[#7](https://github.com/santiquiroz/gears-of-war-2-pc-port/issues/7)): name the XDK Direct3D functions
without symbols and account for every draw of a frame. This file records what has been measured; the
tools are in `tools/native/`.

## Method

1. **Static scan** (`tools/native/pm4_scan.py`). The generated C++ keeps each PowerPC instruction as a
   comment. A PM4 type-3 header is `0xC0000000 | (count - 1) << 16 | opcode << 8`; the Direct3D
   library builds it with `lis` + `ori` (or `addi`/`oris`). The scan lists, per recompiled function, the
   opcodes it builds headers for. 66 functions build at least one.
2. **Counting hooks** (`tools/native/native_probe.cpp`, CMake option `GOW2_NATIVE_PROBE`, off by
   default and never in a release). `tools/native/gen_probe.py` turns the scan into one `REX_HOOK_RAW`
   per function; every 150 frames the game logs `NATIVEPROBE <function> <calls>/frame [opcodes]`.
3. **Command processor counters** (`tools/native/spike-cp-draw-sources.patch`, applied temporarily to
   the SDK, not part of `sdk/patches`): per 150 frames, the draw packets executed, how many are
   predicated, how many indirect buffers ran (and how many ran more than once in a frame), and the
   draws by EDRAM mode (`RB_MODECONTROL.edram_mode`: 4 colour, 5 depth only, 6 copy/resolve).
   It also tells draws whose shader came embedded in the packet (`IM_LOAD_IMMEDIATE`) from the game's.
4. **Register scan** (`tools/native/reg_scan.py`): which functions mention given GPU registers.

Reproduce: `python tools/native/pm4_scan.py --json out/native/pm4_scan.json`,
`python tools/native/gen_probe.py out/native/pm4_scan.json`, configure with `-DGOW2_NATIVE_PROBE=ON`,
optionally add the spike patch to `sdk/patches` and run `scripts/build-sdk.ps1 -NoDlss`, then
`scripts/coop-instances.ps1 start` + `chapter -Chapter 2` with `--d3d12_stats_log=true`.

## Static scan (2026-10-21)

- `ME_INIT` is built only in `sub_8298C760`, the ring-buffer setup the study already anchored
  (`VdInitializeRingBuffer`): the scan agrees with the anchors.
- `DRAW_INDX`: `sub_82977760`, `sub_8297FD30`, `sub_82980210`, `sub_82980730`, `sub_82980B20`,
  `sub_82980F70`. `DRAW_INDX_2`: `sub_82976D88`, `sub_82976EE0`, `sub_829813F0`, `sub_82982A08`,
  `sub_82993F88`, `sub_82994830`, `sub_82998CC0`.
- Almost every PM4-building function lies in `0x82976600`-`0x829A7C40`, the Direct3D library. A few
  outside it (`sub_8227A648`, `sub_82529CC0`, `sub_82C67850`/`A28`/`C08`, `sub_82EB7700`,
  `sub_824C97C0`) are LTCG copies or game code writing packets directly; to be named.

## Counts in the chapter 2 hospital (RX 7800 XT, 30 fps)

Per frame:

| What | Count | Accounted for by |
|---|---|---|
| Draw packets the command processor executed | 1193 | |
| Predicated draws, colour | 782 | the hooked draw functions |
| Predicated draws, depth only | 226 | the hooked draw functions |
| Hooked draw-function calls | 1008 (`sub_82980B20` 900, `sub_82980210` 77, `sub_829813F0` 25, `sub_8297FD30` 5.7) | |
| Non-predicated draws, colour | 168 = 24 unique x 7 tiling passes | Direct3D's internal draws (clears): see below |
| Non-predicated draws, copy (resolves) | 17 | `sub_8298A128`, Resolve (17 calls a frame) |
| Indirect buffers run | 41 (26 distinct, 4 run more than once) | about 28 from `sub_8298BAB0` |

The predicated draws match the hooked draw-function calls exactly (1008 = 1008): those four functions
draw the scene, once per call. Every draw packet arrives through an indirect buffer (the ring buffer
only holds indirect-buffer packets).

## The 185 non-predicated draws

- **They are not recorded command buffers.** `sub_8298BAB0` builds the `INDIRECT_BUFFER` packet of each
  command segment Direct3D submits: every call comes from one caller inside the library (`8298C190`),
  about 28 a frame, and the buffers (alternating 11-dword and ~16,000-dword segments) are new every
  frame. It is the library's segment kick-off, not a replay of recorded buffers.
- **168 colour draws = 24 unique draws x 7 tiling passes.** All of them run with a shader loaded by
  `IM_LOAD_IMMEDIATE`, that is, with microcode embedded in the packet: Direct3D's internal shaders,
  not the game's. 24 of them run in the first pass of their buffer; the others are the same draws
  replayed by predicated tiling (they carry no predicate, so they run in every tile). Their packets are
  copied from templates in the executable's data (`sub_82981ED8` copies a 108-byte PM4 block with
  `memcpy`), which is why the header scan does not see a draw there.
  - `sub_82981ED8` (18 calls a frame) writes `SQ_PROGRAM_CNTL`, depth, blend and an embedded shader:
    **Clear** (`reg_scan.py`). With `sub_82981970` (18 a frame, which splits the clear rectangle into
    EDRAM tiles through `sub_829816E0`) it accounts for most of the 24; the rest are the library's
    other internal draws.
  - The predicated draws of `sub_829813F0` (`DRAW_INDX_2`, 25 a frame) also use an embedded shader:
    a draw with user-supplied vertices (`DrawVerticesUP`-style).
- **17 resolves a frame: `sub_8298A128`** (17 calls a frame), the only Direct3D function writing
  `RB_COPY_CONTROL` and `RB_COPY_DEST_INFO`.

## Named so far

| Function | Calls a frame | Role |
|---|---|---|
| `sub_8298C760` | at start | Device creation, ring-buffer setup (`ME_INIT`) |
| `sub_8298D430` | 1 | `Swap` (`VdSwap`) |
| `sub_82980B20`, `sub_82980210`, `sub_8297FD30` | 900, 77, 5.7 | Indexed draws (`DRAW_INDX`) |
| `sub_829813F0` | 25 | Draw with user vertices and an embedded shader (`DRAW_INDX_2`) |
| `sub_8298A128` | 17 | Resolve |
| `sub_82981ED8` (+ `sub_82981970`, `sub_829816E0`) | 18 | Clear |
| `sub_8298F600` | 743 | Writes `RB_COLOR_INFO`, `RB_MODECONTROL` and constants: render-target and state flush before draws (to confirm in phase 1) |
| `sub_8298F378` | 1211 | `LOAD_ALU_CONSTANT`: shader constants |
| `sub_8298BAB0` | ~28 | Command segment kick-off (`INDIRECT_BUFFER`) |

`tools/native/reg_scan.py` lists which functions mention a given set of GPU registers;
`RB_SURFACE_INFO` (`0x2000` = 8192) also matches unrelated constants.

## Status

**Phase 0 met its exit criterion on 2026-10-21**: every draw of a frame in the chapter 2 hospital is
accounted for by a named, hookable function: 1008 scene draws by four draw functions, 24 internal draws
by Clear and the library's internal draws (repeated by tiling, which a native renderer does not need),
and 17 resolves by Resolve. LTCG did not inline the draw, clear or resolve entry points into the game.
Phase 1 (a plugin skeleton that hooks device creation and Swap, then clears and presents) can start;
the internal draws are the one place where a native renderer must handle Direct3D's own embedded shaders.
