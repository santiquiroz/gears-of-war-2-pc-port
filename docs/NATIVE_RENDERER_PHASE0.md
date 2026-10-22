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
| Non-predicated draws, colour | 168, the same every frame | not yet: see below |
| Non-predicated draws, copy (resolves) | 17 | the Resolve function, not yet named |
| Indirect buffers run | 41 (26 distinct, 4 run more than once) | 20 from `sub_8298BAB0` (`INDIRECT_BUFFER`) |

The predicated draws match the hooked draw-function calls exactly (1008 = 1008): those four functions
draw the scene, once per call. Every draw packet arrives through an indirect buffer (the ring buffer
only holds indirect-buffer packets).

## Open: the 185 non-predicated draws

- **168 colour draws a frame, constant**, 144 of them in indirect buffers that run several times in a
  frame. This looks like **command buffers recorded once and replayed** (the XDK's
  `BeginCommandBuffer`/`RunCommandBuffer`; UE3 uses them for fixed passes), replayed through
  `sub_8298BAB0`. Their draw functions do not run per frame, so per-call hooks cannot see them. A native
  renderer must either hook the recording (and keep what was recorded) or interpret the recorded PM4
  when `sub_8298BAB0` replays it. Next: log `sub_8298BAB0`'s arguments (buffer address and size) and
  the draws inside each replayed buffer, then find who records them.
- **17 resolves a frame** (EDRAM copies). Their headers are not built with `lis`/`ori` in a way the
  scan catches; next: find the Resolve function (it writes `RB_COPY_*` registers and an
  `EVENT_WRITE`), for example by hooking the `EVENT_WRITE` builders (`sub_8298DC90`, 528 calls a frame).

## Status

Phase 0 is not finished. The scene draws are fully accounted for by four hookable functions, which
retires most of the "no symbols" risk; the replayed command buffers and the resolves are the known
gaps, each with a concrete next step above.
