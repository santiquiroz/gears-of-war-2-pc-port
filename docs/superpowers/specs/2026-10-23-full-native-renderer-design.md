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
