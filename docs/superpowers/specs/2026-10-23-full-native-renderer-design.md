# Full native renderer (Direct3D-call level): design

Owner request (2026-10-23): "hagamos todo lo posible para llegar a native rendering" — go from today's hybrid
(Xenos command processor with native surfaces and resolves, v0.3.x) to a renderer that draws the game from its
Direct3D calls, with the command processor out of the frame. Android is the target that needs it (GPU bound, hot,
Adreno driver bugs, issue #1); the PC is already near the no-draw ceiling and must not regress.

## Where we start

- Phase 0 ([NATIVE_RENDERER_PHASE0.md](../../NATIVE_RENDERER_PHASE0.md)) named, without symbols: the four draw functions
  (`sub_82980B20` 900 calls a frame in the hospital, `sub_82980210` 77, `sub_829813F0` 25 with an embedded shader,
  `sub_8297FD30` 5.7), `Swap` (`sub_8298D430`), `Resolve` (`sub_8298A128`, 17), `Clear` (`sub_82981ED8` +
  `sub_82981970`/`sub_829816E0`, 18), the state flush before draws (`sub_8298F600`, 743), shader constants
  (`sub_8298F378`, 1211), command segment kick-off (`sub_8298BAB0`) and device creation (`sub_8298C760`).
  `REX_HOOK_RAW` hooks them (phase 0 counted them with `tools/native/native_probe.cpp`).
- The game links Direct3D with LTCG: some state setters are inlined into Unreal Engine 3 code, so the API surface is
  not a clean list of functions. Six functions outside the library build PM4 packets themselves (phase 0, "to be
  named").
- Prior art (concept only, no code: no license): Xerenge hooks ~25 Direct3D functions and reads the guest `D3DDevice`
  (render-state, sampler, fetch and float constant tables, viewport) at each draw, replacing the ring buffer. MIT
  building blocks we may use: XenosRecomp (Xenos microcode to HLSL/SPIR-V ahead of time) and plume (RHI).

## Architecture (decided)

1. **State source: the guest `D3DDevice` at each hooked draw, verified against the command processor.** At a draw the
   device holds the state the library is about to flush (`sub_8298F600` writes it into PM4 right before). We read
   it there instead of from PM4. Where LTCG inlined a setter, the device tables are still the source of truth
   because the inlined code writes them too; where a function writes PM4 directly without the device, it gets its
   own hook.
2. **Renderer: the SDK's existing Vulkan and D3D12 backends**, driven by a new front end ("native front end") instead
   of the command processor's packet parser. It reuses the pieces that already work natively in v0.3: host render
   targets as surfaces (0036, 0039), native resolves (0037-0045), the texture cache, the shader translator with its
   pipeline storage. No new RHI in the first phases.
3. **Shaders: translated once, from the shader pointers the device holds**, through the existing translator and
   pipeline cache (already ahead of time from storage). XenosRecomp is the later option if translated shaders turn
   out to be the GPU cost on the phone (gate G2).
4. **Ring buffer: kept alive but empty of draws** while the front end draws. The game still writes it and waits on its
   fences; the front end acknowledges them. Removing the ring buffer completely is the last phase, only if the
   CPU-side cost of building packets shows up in the measurements.
5. **Switch:** `gow2_native_frontend` (off by default) with per-draw-class fallback to the command processor, so the
   picture never breaks while coverage grows. Every fallback is counted and logged, like the v0.3 native paths.

## Phases and gates

| Phase | Goal | Exit criterion |
|---|---|---|
| S0 measure | Upper bound on the phone: frame with no draws, render passes a frame and why they end, GPU time split (`vulkan_android_stats_log`, `gpu_native`) with v0.3.1 | Numbers in the doc; if the no-draw ceiling is within 10 % of today, stop and say so |
| S1 state parity | Read the `D3DDevice` at the four draw hooks (shaders, vertex streams and declaration, index buffer, fetch constants, ALU constants, render targets, viewport, render states) and compare with the command processor's state at the same draw | ≥ 99 % of hospital draws match on every field; the mismatches are named |
| S2 first native draws | The front end issues the HUD/UI draws (`sub_829813F0`, embedded shader) and then the indexed draws of one pass through the backend, the command processor skipping those draws | Same picture (image diff ≤ 0.02 against v0.3.1), counted fallbacks |
| S3 whole frame | All four draw functions, clears and resolves through the front end; command processor only fences | Chapters 1, 2, 4, 8 match; soak 10 minutes; phone A/B against v0.3.1 |
| S4 phone tuning | Render pass merging, fewer barriers, shader cost (XenosRecomp if G2 says so) | Sustained fps on the S25 above v0.3.1 by a margin worth the change |
| S5 ring buffer out | Only if CPU packet building is measurable | Measured gain |

Gates: G1 after S1 (state readable from the device: yes or no), G2 after S3 (where the phone GPU time goes). A failed
gate stops the project with the measurements documented, as the hybrid's study did.

## Constraints

- SDK changes stay numbered patches in `sdk/patches`; no layout change of exported classes (members only at the end,
  `feedback`: the exe is built against unpatched headers).
- Off by default until S3 passes; then default per platform after measurements, as v0.3.x did.
- Game runs are idle-gated or explicitly approved; phone runs cool-gated and alternated.
- No code from projects without a license (Xerenge, Unleashed Recompiled): concepts only.

## S0 results (2026-10-23, Galaxy S25 Ultra, v0.3.1 with native surfaces and resolves on, chapter 2 hospital, unlocked)

| Run | fps | Notes |
|---|---|---|
| Hybrid (today's default) | 52.9 from 32.4 °C, falling to 37.2 at 38.0 °C | 1,100 draws a frame (365 without a pixel shader), 53.5 render passes a frame, 1 submission a frame, no pipeline stalls |
| `gpu_native = true` (no draws, no resolves) | 60.0 steady, still 60 at 36.7 °C | The display's limit, so the real ceiling is higher |

Gate S0 passes: the frame without draws runs at least 33 % faster than today's warm frame and 60 % faster at the
warm end. The GPU, not the CPU, is the limit (one submission a frame, no stalls), so the gain has to come from
cheaper GPU work per draw: fewer render passes and barriers, and lighter shaders (gate G2).

## S1 results so far (2026-10-23, RX 7800 XT, chapter 2 hospital)

`tools/native/native_probe.cpp` (`GOW2_NATIVE_PROBE`, `GOW2_PARITY=1`) hashes the device shadows of
[NATIVE_RENDERER_DEVICE_LAYOUT.md](../../NATIVE_RENDERER_DEVICE_LAYOUT.md) after each of the four draw functions
returns (the flush inside the draw updates the shadows it uses), and a temporary SDK spike (`gpu_parity_log`, not in
`sdk/patches`) hashes the command processor's registers at each draw packet; `tools/native/parity_compare.py` pairs
them. Every game draw pairs with a command processor draw; the command processor's 185 extra draws a frame are the
library's clears, resolves and embedded-shader draws.

| Function (draws a frame) | surf | prog | rs | vp | scis | fetch |
|---|---|---|---|---|---|---|
| `sub_82980B20` DrawIndexed (~900) | 100 % | 100 % | 100 % | 100 % | 100 % | 100 % |
| `sub_82980210` (~77) | 100 % | 100 % | 100 % | 100 % | 100 % | 0 % |
| `sub_829813F0` user vertices, embedded shader (~25) | special path, not from the shadows | | | | | |

Open before S2: the vertex fetch of the non-indexed draws (written outside the fetch shadow), the ALU constants (they
come through LOAD_ALU_CONSTANT descriptors, not the 1920/6016 shadows: 16-18 % match, as expected), and shader
identity (A/B microcode addresses against the vertex and pixel shaders the command processor loads).

S1c (same scene, `out/native/parity3`): the probe now also rebuilds the ALU constants (shadows overlaid with the
LOAD_ALU_CONSTANT records of A at A+40 and B at B+872), the microcode address and length of A and B, and the fetch the
non-indexed draws write straight into the ring (header `0x000548BA`, fetch slot 31).

| `sub_82980B20` (gameplay frames) | match |
|---|---|
| vertex constants (rebuilt) | 98.5 % (was 20.7 %) |
| pixel constants (rebuilt) | 38 % (was 23 %) |
| shader identity, when A != 0 | 100 %: B is the vertex shader (IM_LOAD at `Baddr`, `Blen` dwords), A the pixel shader |
| shader identity, when A == 0 (~25 % of draws) | the CP pixel shader is an IM_LOAD_IMMEDIATE of 9 dwords the probe does not locate yet |

`sub_82980210` keeps 0 % on rebuilt constants and fetch, and `sub_829813F0` differs in every group. S1d dumps the
full register blocks of one frame on both sides to name the differing registers.

S1d (`GOW2_PARITY_DUMP_FRAME`/`GOW2_PARITY_DUMP_FIRST` and `gpu_parity_dump_frame`/`gpu_parity_dump_first`, diffed by
`tools/native/parity_dump_diff.py`; details in `tools/native/S1D_FINDINGS.md`). Frame 1500, 2,240 dwords per draw
(1,024 vertex constants, 1,024 pixel constants, 192 fetch):

- Draws 0-63 (`sub_82980B20`): all 63 identical, every dword.
- From draw ~229 on, every block that differs does so only in **pixel constants c249-c255 and vertex c255**. The
  command processor holds values such as 5.0, 1/3, 0.99, −π and 1e-4 there, the game's tables hold 0. Something
  outside the device shadows and the A/B constant records writes them (shader literals is the guess), and the values
  stay in the register file for later draws. Being traced (`PARITY CPSRC`).
- `sub_82980210` fetch: only fetch slot 31 (dwords 186-191), which it writes straight into the ring.
- `sub_829813F0`: writes its fetch, surface and depth state straight into the ring and emits its own DRAW_INDX_2
  (one per call, prim 8, 3 vertices per rectangle). It never flushes the shadows.
- Draws with A == 0 are depth-only (`sub_8298F600` sets RB_MODECONTROL to 5 and loads no pixel shader); the command
  processor keeps a stale pixel shader there that does not run.

The c249-c255 values came from LOAD_ALU_CONSTANT packets with other descriptors (the spike's `PARITY CPSRC` names the
packet kind and source): `sub_8298F378` loads constants from many descriptors, not only A and B, and they persist in
the register file. The probe now keeps a **constant mirror** fed, in call order, by `sub_8298F378` (descriptor r4,
data r5) and the shadow flush `sub_8298E5B8` (64-bit dirty mask r4, register base r5, shadow r6).

### Gate G1: passed (`out/native/parity10`, gameplay frames)

| Function | surf | prog | rs | vp | scis | fetch | vertex constants | pixel constants | shaders |
|---|---|---|---|---|---|---|---|---|---|
| `sub_82980B20` (5,774 draws) | 100 % | 100 % | 100 % | 100 % | 100 % | 100 % | 100 % | 100 % | 100 % (A != 0); A == 0 is depth-only |
| `sub_82980210` (84) | 100 % | 100 % | 100 % | 100 % | 100 % | slot 31 only, its own vertex copy | 100 % | 100 % | 100 % |
| `sub_829813F0` (154) | writes its own state into the ring; native version is built from its arguments | | | | | | 100 % | 100 % | embedded |

Every mismatch is named: fetch slot 31 of the non-indexed draws (`{0, 0, 0, 0, Phys(ring copy) | 3, 0x10000002 |
size}`, built from the draw's own vertices) and the rectangle path of `sub_829813F0`. The device plus the constant
mirror is a sufficient state source.

Known issue of the probe build: 2 of 11 runs hung at startup with one guest thread spinning; not seen in release
builds, not investigated (diagnostic build only).

## After G1: where the phone's GPU time goes (2026-10-23, S25 Ultra, chapter 1 checkpoint)

G1 shows the device state equals what the command processor already draws, so swapping the front end (S2-S3) would
not change the GPU work; the owner chose to measure the GPU cost first. Method of `docs/ANDROID_EXPERIMENTS.md`:
cool-gated runs (battery ≤ 34.5 °C), 60 s of gameplay sampled with `scripts/android-gpu-clocks.sh`, compared as GPU
megacycles per game frame (effective clock x busy / fps), so thermal clock drops cancel out. Logs and clock samples
in `out/native/gpucost`.

| Run | fps | GPU busy | Mcycles/frame |
|---|---|---|---|
| Baseline (first) | 32.4 | 95 % | 16.7 |
| `vulkan_android_empty_fragment_shader` | 34.6 | 99 % | 15.9 |
| Game effects off (AO, motion blur, depth of field, high-quality bloom, distortion) | 40.1 | 99 % | 13.0 |
| Baseline (last) | 39.4 | 99 % | 15.9 |

- The GPU is the limit (95-99 % busy).
- Fragment shader work is not the cost: empty fragment shaders save about 2.5 %, inside the run-to-run spread.
- The game's full-screen effects cost about 20 %. Since their shading is cheap, that is bandwidth and pass overhead
  on the tiler: per frame about 53 render passes (48 ended by a barrier), 180 geometry-shader draws and 35 uploads.
- A Perfetto trace with `gpu.renderstages` came back empty: this phone exposes no render-stage producer to the shell.

Round 2 (same method, one switch at a time; the three baselines so far span 15.7-16.7 Mcycles/frame):

| Run | Mcycles/frame | Against the 15.7 baseline of the same session |
|---|---|---|
| Geometry shaders replaced (`vulkan_force_expand_*`, quad lists as triangles) | 16.1 | none: geometry shaders are not the cost |
| `AmbientOcclusion=False` | 14.6 | −7 % |
| `MotionBlur=False` | 15.0 | −4 % |
| `DepthOfField=False` | 14.3 | −9 % |
| `UseHighQualityBloom=False` | 15.5 | noise |
| `Distortion=False` | 15.6 | noise |

Ambient occlusion, depth of field and motion blur together make up the 20 % of round 1; nothing dominates, and
about 80 % of the frame is the scene passes themselves. Next: GPU time per render pass with timestamps at pass
boundaries (valid on a tiler, unlike inside a pass), `vulkan_android_pass_timing`.

Round 3: GPU time per render pass (patch 0052, `vulkan_android_pass_timing`; 14.4 Mcycles/frame, 40 fps, so the
timing itself costs nothing visible). About 16.7 ms of GPU work a frame, 53 passes:

| Pass kind (color, depth, width) | ms/frame | Passes/frame | Draws/frame | Ended by |
|---|---|---|---|---|
| Scene: `2_10_10_10_FLOAT` + `D24FS8`, 1280 | 6.7 | 5.8-6.7 | 633 | barrier |
| Between passes (barriers, copies, uploads) | 3.4 | | | |
| `8_8_8_8`, no depth, 1280 | 2.5 | 25 | 82 | barrier |
| `16_16_FLOAT` + `D24FS8`, 640 | 1.1 | 5 | 60 | barrier |
| `16_16_16_16_FLOAT`, no depth, 400 | 1.0 | 4 | 28 | barrier |
| `2_10_10_10_FLOAT`, no depth, 1280 | 0.43 | 1 | 25 | switch |
| Depth only, `D24FS8`, 1280 | 0.42 | 2 | 211 | switch |
| `16_16` + `D24FS8`, 640 | 0.39 | 1 | 10 | barrier |
| Rest | ~0.6 | | | |

On a tiler every pass end stores the tiles and the next pass loads them again. The scene is cut into about six
passes and the 8888 passes average three draws each (about 100 µs apiece, mostly that overhead); 48 of the 53 pass
ends are barriers, and the gaps between passes take 20 % of the frame. Next: name what pushes each barrier (upload,
resolve, render target transfer, texture layout) and remove the avoidable ones.

Round 4: what ends the passes (patch 0053; 14.1 Mcycles/frame, 39.5 fps):

| Pass kind | Ended by | Passes/frame | ms/frame |
|---|---|---|---|
| Scene, last part | `barrier:rt` | 1 | 5.1 |
| `8_8_8_8`, 1280 | `barrier:upload` | 24 | 2.3 |
| Scene, earlier parts | `barrier:upload` | 5.8 | 1.2 |
| `16_16_FLOAT`, 640 | `barrier:rt` | 5 | 1.1 |
| `16_16_16_16_FLOAT`, 400 | `barrier:rt` | 2 | 0.9 |

Between passes per frame: 36 buffer copies, 9 image copies, 32 compute dispatches and 127 barrier batches; barriers
pushed per frame by source: upload 90, rt 48, texture 33, other 6. **About 30 of the 53 passes end only because guest
memory was uploaded in the middle of the frame** (each upload is a copy, which cannot run inside a render pass). The
render-target ones are real dependencies (the next pass samples what was drawn). First fix to measure: patch 0054
(`gpu_upload_whole_blocks`), which batches the uploads of a block into one copy.

Round 5: patch 0054 `gpu_upload_whole_blocks`, A/B/B/A from cool starts:

| Run | Mcycles/frame | Passes/frame | Buffer copies/frame |
|---|---|---|---|
| A (off) | 14.3, 14.3 | 55, 54 | 36 |
| B (on) | 13.3, 13.4 | 26, 26 | 2.7 |

7 % fewer GPU megacycles per frame, both B runs below both A runs; the picture matches the earlier capture of the
same view (the black lines on the floor are there with and without it). Now on by default on Android.

Round 6 (0054 on): 13.2 Mcycles/frame at native resolution, 29.4 at `draw_resolution_scale` 2x2 (four times the
pixels). Cost ≈ 7.8 + 5.4 × (pixels / native), so about 40 % of today's frame grows with the pixels and 60 % does not
(per-draw and per-pass work, vertex shaders). Empty fragment shaders saved nothing, so the pixel part is render
target bandwidth (tile loads and stores, blending), not shading.

## S2 design (2026-10-24): draws from the device, in ring order

Owner decision: build the full native renderer, no more measurement rounds. S2 moves `sub_82980B20` (DrawIndexed,
~90 % of draws) to the device state; the other draw functions, Clear and Resolve follow in S3.

**Ordering without new SDK API.** The game and the command processor run in one process. After the original
`sub_82980B20` returns, the game hook finds the DRAW_INDX packet it just wrote (header `0xC0032201`, the last packet
before the write pointer `dev+48`, inline path) and rewrites it in place into a type-3 NOP of the same length
(`0xC0031000`): words `'NATV'` (0x5654414E), the low and high halves of a host pointer to a `NativeDrawSnapshot`, and 0.
The command processor reads it in ring order, exactly where the draw was, so clears, resolves and fences keep their
places. A draw that is not on the inline path (segment record at 13256) is left alone and counted.

**Snapshot** (host memory, host byte order, layout shared by both sides and checked by `magic` and `size`):

| Field | Registers | Source on the game side |
|---|---|---|
| `magic` `'NDS1'`, `size`, `sequence` | | |
| `draw_words[4]` | DRAW_INDX words 1-4 | the packet before the rewrite |
| `surf[6]` | 0x2000-0x2005 | shadow 10368 |
| `prog[2]` | 0x2180-0x2181 | shadow 10528 |
| `rs[12]` | 0x2200-0x220B | shadow 10548 |
| `vp[21]` | 0x2100-0x2114 | shadow 10444 |
| `scissor[2]` | 0x2081-0x2082 | 10436 / 10440 |
| `fetch[192]` | 0x4800-0x48BF | shadow 1152 |
| `alu[128]` pointers to 16 dwords | 0x4000-0x47FF | constant mirror (S1), copy-on-write blocks |

The game keeps the constant mirror of S1 (fed by `sub_8298F378` and `sub_8298E5B8`) as 128 blocks of 16 dwords;
a block written since the last snapshot gets a new copy in a per-frame arena, so a snapshot copies about 1 KB of
fixed registers plus the changed blocks. Arenas rotate over 4 frames (the command processor runs at most 2 behind).

**Command processor side (SDK patch).** The NOP handler recognises `'NATV'` and, with `gpu_native_frontend`
(default on; it only acts on rewritten packets), writes the snapshot's registers into the register file and issues
the draw from `draw_words` through the normal `IssueDraw` path. `gpu_native_frontend_verify` compares the snapshot
with the register file first and counts the mismatching groups (online G1). Counters every 300 frames:
`NATIVEFRONT draws N verified N mismatches {group: n} bad_snapshot N`.

**Steps.** S2a: rewrite + snapshot + register write + draw, with the library's state packets still emitted (the
register write is then a no-op: the picture cannot change). S2b: stop emitting the library's state packets for
those draws (skip the flush inside the hook, keep the device's dirty-mask bookkeeping), so state reaches the GPU
only from the device. S2c: shaders from A/B microcode (load them on the snapshot instead of IM_LOAD).

**Switch.** `gow2_native_frontend` (auto/on/off) in Graphics → Renderer group; auto turns it on after S3 passes.

### S2a result (2026-10-24, RX 7800 XT, chapter 2 hospital)

`gow2_native_frontend = on` with `gpu_native_frontend_verify`: 270,030 DrawIndexed calls per 300 frames (about 900
a frame) rewritten by the game and drawn by the command processor from their snapshots; 0 mismatches in every group
(surf, prog, rs, vp, scissor, fetch, ALU constants), 0 bad snapshots, 0 draws off the inline path, 13 MB of arenas.
The picture is the same as the emulated path. Note from the implementation: `dev+48` holds the address of the last
dword written (`stwu`), so the DRAW_INDX header sits at write pointer − 16.

### S2b+c, game side (2026-10-24, not run in game yet)

**Snapshot `'NDS2'`** (2552 bytes on 64-bit) appends after `alu[128]`: `window_offset` (0x2080, always 0),
`g2280[21]` (shadow 10596), `g2300[38]` (10680), `g2380[8]` (10832), `g2388[24]` (0x2388-0x239F, 10272),
`bool_consts[40]` (0x4900-0x4927: BOOL and LOOP, 10112), `vs_address/vs_dwords` (B, `MicrocodeOfB`),
`ps_address/ps_dwords` (A, `MicrocodeOfA`; 0 when the object is 0), `flags`, `reserved`. Two counts differ from the
first plan, both read in the generated code: `sub_8298E2E0` writes 4 registers per mask bit (6 bits → 24 registers,
not 6), and the BOOL write's mask `lis r4,-256` is 64-bit 0xFFFFFFFFFF000000, so it writes 40 registers (BOOL and the
32 LOOP constants), not 8. The SDK's copy of the struct must follow.

**Suppression** (`gow2_native_frontend_suppress`, default true, read once). The hook keeps `dev+48` from before the
call; after it, `PlanSuppression` walks the packets from there to the DRAW_INDX header and NOPs (header only,
`0xC0001000 | (count−1) << 16`) the type-0 writes that touch only snapshot registers, LOAD_ALU_CONSTANT, IM_LOAD,
IM_LOAD_IMMEDIATE and SET_CONSTANT into covered registers. Everything else stays (the scissor packet's own registers
are covered; WAIT_REG_MEM, INVALIDATE_STATE, `sub_8298E2E0`'s writes to 0x2007 and 0x0A2F-0x0A31 stay). A packet
that runs past the header, a region over 16384 dwords, or a write pointer that went backwards aborts: nothing is
NOPed and `flags` bit 0 stays clear. Bit 0 set means the command processor must load the shaders and all covered
registers from the snapshot. Counters in `NATIVEFRONT game ... suppressed_packets N kept_packets N aborted N`.

### S2b+c result (2026-10-24, RX 7800 XT, chapter 2 hospital)

With `gow2_native_frontend_suppress = false` and verification, every group of the NDS2 snapshot (including 0x2280,
0x2300, 0x2380-0x239F, BOOL/LOOP 0x4900-0x4927 and the window offset) matches the register file on all 270,028 draws
per 300 frames. With suppression on (default), per frame about 8,700 library packets (register groups, ALU constant
loads, shader loads) are turned into NOPs and DrawIndexed state and shaders reach the GPU only from the device;
about 1,070 packets a frame are outside the snapshot and kept, and about 7 draws a frame keep their packets because
the region could not be walked (`aborted`). The picture is the same as the emulated path.

### S3 result (2026-10-24): every draw path and Clear

- `sub_82980210` and `sub_8297FD30` write their state and draw into the ring but leave the new write pointer in
  `dev+13444` (the caller commits it to `dev+48` after copying the vertices): the hook reads it there, and only when the
  function succeeded (`sub_82980210` returns 0, `sub_8297FD30` the vertex pointer). Details:
  `tools/native/S3_SEGMENT_FINDINGS.md`.
- Every draw of a hooked call gets its own snapshot: the device shadows plus the covered register writes of the call,
  folded in order up to that draw. Segments are compacted (kept packets first, one marker over the rest), so short
  draws (DRAW_INDX_2 of the rectangles, the 3-dword DRAW_INDX of `sub_8297FD30`) get a marker too.
- Clear (`sub_82981ED8`) is hooked as a whole: its rectangles (`sub_829813F0`) use the state Clear writes in its own
  packets, which the per-draw folding now carries; Clear's two inline shaders (IM_LOAD_IMMEDIATE) stay in the ring.
  `sub_829813F0` called outside Clear stays on the console path. Details: `tools/native/S3_CLEAR_FINDINGS.md`.
- A call during which `sub_8298C1A0` ran (the ring went past its high-water mark and may have been handed to the GPU)
  is left as the library wrote it (`ring_moved`, about 3 calls a frame).
- Hospital, RX 7800 XT: DrawIndexed, both non-indexed functions and Clear all through the front end; `not_inline`,
  `short_draw` and `segments_kept` 0; same picture as the emulated path. Galaxy S25 Ultra (DrawIndexed stage): same
  picture, 0 bad snapshots, 46.7 fps from a cool start.

Still on the command processor by design: the library's resolves (native resolves since 0037-0045), swaps, fences and
the packets the snapshot does not cover (kept in their place).
