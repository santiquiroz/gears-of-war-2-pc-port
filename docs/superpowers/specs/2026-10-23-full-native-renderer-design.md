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
