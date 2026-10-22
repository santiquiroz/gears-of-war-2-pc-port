# Native renderer: feasibility study

Status: study, 2026-10-21. Tracking issue: [#7](https://github.com/santiquiroz/gears-of-war-2-pc-port/issues/7).

## Goal

Draw from the game's Direct3D calls instead of emulating the Xenos GPU command processor, with
shaders translated ahead of time and the game's render targets kept as real host textures. Android
is the main reason: on a Galaxy S25 Ultra the GPU sits at 99 % emulating the EDRAM (render target
ownership transfers, resolves, a render pass ended for most barriers), the phone heats up and falls
from 30 to about 10 fps within minutes, and Adreno 6xx phones show corrupted images on newer Vulkan
drivers (#1). Desktop GPUs gain too.

## What already proves it can work

[Xerenge](https://github.com/shipa-2/Xerenge) (Burnout Revenge, also on ReXGlue) draws by default
through its own `plume` GPU plugin and keeps Xenia-style emulation only as a slower reference:

- It hooks about 25 functions of the XDK's Direct3D library by address
  (`Direct3D_CreateDevice`, `D3DDevice_Swap`, `SetRenderTarget`, `Resolve`, `SetTexture`,
  `SetStreamSource`, `SetIndices`, `SetVertexShader`/`SetPixelShader`, `SetVertexDeclaration`,
  `DrawVertices`/`DrawIndexedVertices`/`DrawVerticesUP`, `BeginVertices`/`EndVertices`,
  `BeginTiling`/`EndTiling`, `ClearF`, the ring-buffer and fence functions), plus offsets into the
  guest `D3DDevice` structure (render-state and sampler-state tables, fetch and float constants,
  viewport).
- It replaces the ring-buffer submission, so the command processor never runs.
- Its addresses came from **a beta build with symbols**, matched against the retail code
  (`tools/name_retail_d3d.py`); its notes record that guessed addresses landed on shared epilogues.
- It has no license, so it is reference only. Unleashed Recompiled (the same idea for Sonic) is
  GPL-3: reference only too. Usable building blocks are MIT: [plume](https://github.com/renderbag/plume)
  (one RHI over Vulkan, D3D12 and Metal) and [XenosRecomp](https://github.com/hedge-dev/XenosRecomp)
  (Xenos shader microcode to HLSL/SPIR-V ahead of time).

## What Gears of War 2 gives us

- **XDK 2.0.7645**, Direct3D linked as **`D3D9LTCG` v2.0.7645.1** (XexTool `-l`). LTCG
  (link-time code generation) lets the linker inline Direct3D functions into Unreal Engine 3's
  renderer, so some entry points do not exist as functions of their own. That is the same problem
  Xerenge met ("absorbed into larger functions").
- **No symbol source is known.** The leaked 2008 development build (used by "Hollow") might carry
  symbols or a map; not checked, and we must not depend on leaked material.
- **Anchors found without symbols**, through the named kernel imports the library calls:
  - `sub_8298D430` calls `VdSwap` and `VdGetSystemCommandBuffer`: `D3DDevice_Swap`.
  - `sub_8298C760` calls `VdInitializeRingBuffer`, `VdEnableRingBufferRPtrWriteBack` and
    `VdSetSystemCommandBufferGpuIdentifierAddress`: the ring-buffer setup of device creation.
  - `0x8298C5B8` is the graphics interrupt callback the runtime already logs
    (`SetInterruptCallback(8298C5B8, ...)`); `sub_82990B80` and `sub_82990DE8` register it.
  - So the library sits around `0x8297xxxx`-`0x8299xxxx` (a few hundred of the 48,830 recompiled
    functions). Device creation gives the `D3DDevice` pointer; setters are then recognisable by the
    device offsets they write and the PM4 packets they emit (draws are `DRAW_INDX` type-3 packets).
- **Shaders are already collected.** The runtime persists the game's shader microcode
  (`<cache>/shaders/*.xsh`); XenosRecomp can translate that set ahead of time.
- **Unreal Engine 3 sits on top.** Where LTCG inlined Direct3D into UE3, the hook point moves up to
  UE3's own renderer functions (its RHI), which are fewer and have stable meanings.

## Verdict

Feasible, with one real risk: identifying the hook points without symbols. Everything else has a
working precedent on the same SDK (Xerenge) or MIT building blocks (plume, XenosRecomp). The risk
is retired or confirmed by a short spike (phase 0), before any renderer code is written.

## Plan

| Phase | Work | Exit criterion |
|---|---|---|
| 0. Identification spike | From the anchors, name the device-creation, swap, render-target, resolve, texture, stream, shader, state and draw functions; mark the ones LTCG inlined and find the UE3 functions above them. Count calls per frame with temporary hooks against the PM4 draws the command processor sees. | Every draw and state change of a frame is accounted for by a hooked function, or the gap is known. |
| 1. Plugin skeleton | A `plume` GPU plugin beside `xenos`, selectable with `gpu_plugin`; hooks `Direct3D_CreateDevice` and `Swap`; clears and presents on Vulkan (Android, Linux) and D3D12. | The title screen's frame pacing runs with a cleared screen, no command processor. |
| 2. Resources | Textures (untiling, formats), vertex/index buffers, render targets as host textures, resolves as copies, shaders from XenosRecomp with a runtime fallback for unseen ones. | Title screen and menus look right. |
| 3. Gameplay parity | Act 1 at parity with `xenos` (screenshots side by side), the game's post-processing, the fixes we already carry (AO, depth of field, black shading). | Act 1 playable on PC and Android; `xenos` stays as the reference. |
| 4. Android first | Performance and thermals on the S25 and an Adreno 6xx phone; then make `plume` the default where it wins. | Sustained frame rate clearly above today's 10-12 fps when warm. |

Effort: phase 0 is days; phases 1-3 are weeks each. Phase 0 is the decision point.

## Alternatives if phase 0 fails

- Keep the command processor but cut the EDRAM work on Android (fewer render passes and transfers,
  host render targets reused across frames). Smaller gains, no hook risk.
- Hook only at the UE3 RHI level, not Direct3D: fewer functions, but every engine renderer path
  must be covered.

## Next step

Phase 0 started on 2026-10-21; its measurements are in
[NATIVE_RENDERER_PHASE0.md](NATIVE_RENDERER_PHASE0.md): the scene draws are accounted for by four
hookable functions, Clear and Resolve are named too, and phase 0 met its exit criterion: phase 1 is next.
