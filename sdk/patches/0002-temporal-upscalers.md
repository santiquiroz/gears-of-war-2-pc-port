# SDK patch 0002: temporal upscalers (DLSS / FSR 3.1 / FSR 4)

Source: `gears-judgement-recomp`, `gpu\patches\0001-upscaler-and-overrides.patch`
plus `gpu\files\*` (BSD-3-Clause, credited in `THIRD_PARTY_NOTICES.md`),
made against this same SDK commit `923c1a5`. Reference build script:
`gears-judgement-recomp\scripts\build-gpu.ps1`.

What it adds: temporal upscaling (AMD FSR 4 / FSR 3.1 through the FidelityFX
API loader DLL loaded at runtime, NVIDIA DLSS) by capturing the
view-projection matrix from vertex shader constants, depth from resolved depth
textures, Halton jitter, and camera-only motion vectors, applied at swap time.
All identifiers renamed `gowj`/`GOWJ` -> `gow2`/`GOW2` (namespace, cvars,
macros, CMake options, env vars, log prefixes, file names). Each new file
carries an `Adapted from gears-judgement-recomp (BSD-3-Clause).` line.

## Hunk classification (reference patch hunks -> our patch)

Legend: (a) required by the upscalers, (b) Judgment-specific override,
(g) generic fix/dev-tool kept although not strictly upscaler-related.

| # | File | Hunk (ref @@) | What it does | Verdict |
|---|------|---------------|--------------|---------|
| 1 | include/.../d3d12/command_processor.h | @67 | `InvalidateBindingsAfterExternalCommands()`: re-bind everything after a deferred-list callback | KEEP (a): used after the upscale `RunJob` callback |
| 2-4 | include/.../d3d12/deferred_command_list.h | @442/@494/@556 | `Callback(fn,user)` + `kCallback` + `CallbackArguments` | KEEP (a): upscale pass and dump captures record through it |
| 5 | include/.../d3d12/texture_cache.h | @113 | `GetActiveTextureResource()` declaration | KEEP (a): depth-texture lookup for the upscaler |
| 6 | include/.../pipeline/texture/cache.h | @450 | `GetActiveTexture()` accessor | KEEP (a): backs #5 |
| 7-9 | src/filesystem/devices/host_path_device.cpp | @12/@21/@116 | `GOWJ_VFS_OVERRIDES` env file substitution (includes, table, PopulateEntry hook) | DROP (b): Judgment content workflow (coalesced experiments), unrelated to upscaling; not a generic fix |
| 10 | src/graphics/CMakeLists.txt | @82 | Build the 4 new `gow2_*.cpp` files | KEEP (a), renamed |
| 11 | src/graphics/CMakeLists.txt | @112 | `GOW2_DEV_TOOLS` / `GOW2_FFX_SDK` / `GOW2_DLSS_SDK` options | KEEP (a), renamed |
| 12 | src/graphics/d3d12/command_processor.cpp | @20 | Includes (`gow2_temporal.h`, `gow2_trace.h`, `<cstdlib>` for getenv) | KEEP (a) |
| 13 | .../command_processor.cpp | @1896 | `BeginFrame()` at swap + `GOW2_DEV_TOOLS` SWAP trace | KEEP: BeginFrame (a); SWAP trace (g, one log line/frame) |
| 14 | .../command_processor.cpp | @1980 | `UpscaleFinal()` at swap: show the upscaled image instead of the guest frame | KEEP (a) |
| 15 | .../command_processor.cpp | @2246 | `CaptureOutputIfRequested()` headless screenshot (`gow2_shot.now`) | KEEP (g, dev tool, works for any title) |
| 16a | .../command_processor.cpp | @2327 pt 1 | `NoteSceneDraw()` camera capture per draw | KEEP (a) |
| 16b | .../command_processor.cpp | @2327 pt 2 | `GOW2_DEV_TOOLS` per-draw DRAW trace | KEEP (g, generic) |
| 16c | .../command_processor.cpp | @2327 pt 3 | UBERPOST dump keyed on two hardcoded Judgment pixel-shader hashes | DROP (b): Judgment-only shader hashes |
| 17 | .../command_processor.cpp | @2408 | `OnDrawTextures()` depth-texture tracking per draw | KEEP (a) |
| 18 | .../command_processor.cpp | @2462 | Temporal jitter via a copied NDC viewport (+ render-scale shrink) | KEEP (a); `ShouldJitter` signature widened for the `gow2_vp_register` cvar |
| 19-21 | src/graphics/d3d12/deferred_command_list.cpp | @36/@229/@262 | Descriptor-heap tracking + `kCallback` replay (restores heaps after callbacks) | KEEP (a) |
| 22 | src/graphics/d3d12/render_target_cache.cpp | @25 | Includes | KEEP (a) |
| 23a | .../render_target_cache.cpp | @1180 pt 1 | `NoteResolve()` + draw-scale capture per resolve | KEEP (a) |
| 23b | .../render_target_cache.cpp | @1180 pt 2 | `GOW2_DEV_TOOLS` RESOLVE trace | KEEP (g, generic) |
| 24 | src/graphics/d3d12/texture_cache.cpp | @877 | `GetActiveTextureResource()` implementation | KEEP (a) |
| 25 | .../texture_cache.cpp | @1544 | `ALLOW_UNORDERED_ACCESS` on large R10G10B10A2 textures (scene-colour UAV target) | KEEP (a): added for the upscaler write-back path |
| 26a | src/ui/d3d12/d3d12_provider.cpp | @305 pt 1 | Prefer D3D feature level 12_1/12_0, fall back to 11_0 | KEEP (a): DLSS requires 12.x |
| 26b | .../d3d12_provider.cpp | @305 pt 2 | `GOW2_DEV_TOOLS` + `GOW2_D3D12_DEBUG=1` debug-layer switch with info-queue logging | KEEP (g, generic) |

Result: 10 modified files (host_path_device.cpp fully dropped) + 8 new files
(`gow2_temporal/dlss/fsr/trace` headers, `gow2_temporal/dlss/upscale/fsr` sources).

## GoW2 differences and validation notes (must-read before enabling)

The reference hardcodes Judgment's vertex-shader constant layout. Gears of War 2
(Unreal Engine 3, 2008) must be validated with `GOW2_DEV_TOOLS=ON` + `gow2_trace.now`:

- ViewProjection matrix at VS float constants c0..c3 -> now cvar
  `gow2_vp_register` (default 0, clamped 0..252 for jitter, 0..240 for capture).
  The `ShouldJitter` world-shader test (VS reads all 4 VP rows) follows the cvar.
- Camera world position at VS row c13 (translation delta for motion vectors) ->
  now cvar `gow2_campos_register` (default 13); indexed relative to the capture
  window. FoV/near/far derivation uses offsets *within* the captured VP matrix,
  so it follows `gow2_vp_register` automatically.
- Still hardcoded from Judgment measurements, kept as-is but must be confirmed
  for GoW2: scene-draw filter `color_fmt == 3 && index_count >= 30`
  (`NoteSceneDraw`), frame structure (one HDR scene target, depth resolve, colour
  resolve, single tonemap draw, HUD), teleport/cut threshold 5000 Unreal units.
- Kept valid for GoW2: Unreal centimetre units (`viewSpaceToMetersFactor 0.01`,
  near/far defaults), inverted depth, Halton jitter, camera-only motion vectors.
- NGX project ID replaced with a fresh GoW2 GUID
  (`b742a0d0-b3f9-4ebd-baa6-c3066954b994`).
- New cvars: `gow2_temporal`, `gow2_upscaler` (off/dlss/fsr3/fsr4/auto),
  `gow2_upscale_quality` (-1 auto, 0/1/2/3/4/5), `gow2_render_scale`,
  `gow2_dlss` (legacy bool), `gow2_dlss_hdr`, `gow2_mv_debug`,
  `gow2_jitter_scale`, `gow2_jitter_phases`, `gow2_vp_register`,
  `gow2_campos_register`, `gow2_upscale_typed_copy` (debug: FSR reads the typed
  copy DLSS gets), `gow2_upscale_status` (set by the plugin for the FPS
  overlay). New env vars: `GOW2_TARGET_W/H` (output size, required),
  `GOW2_HOME` (vendor-DLL folder for single-file layouts), `GOW2_D3D12_DEBUG`,
  `GOW2_FSR_DEBUG`; trigger files `gow2_trace.now`, `gow2_dump.now`, `gow2_shot.now`.

## Game-side driver in the reference (for our game to replicate; NOT implemented)

Reference wiring (`gears-judgement-recomp\src`):

- `gowj_quality.h`: `kTemporalUpscalers` (shipped `false`: DLSS flickered above
  ~2.5x upscale at Judgment's render sizes, so the release used SMAA/EASU;
  code kept behind the flag), `QualityPlan` (`upscaler`, `upscale_mode`,
  `render_w/h`, `upscaler_override`).
- `gowj_quality.cpp` (`ChooseQualityPlan`): reads `gowj_upscaler` /
  `gowj_upscale_quality` from the config; auto mode from tier (Low ultra-perf,
  Medium performance, High quality); render height 720 or 1440 (only sizes
  Judgment draws cleanly); disables the upscaler on small screens; forces
  `fsr3` via `upscaler_override` when DLSS would upscale > 2.5x.
- `gowj_app.h` (`EnsureQualityPlan`): publishes `REX_GOWJ_UPSCALER` env
  (the `REX_<NAME>` env convention overrides cvar `gowj_upscaler`; forced
  `off` while `kTemporalUpscalers == false`) and `GOWJ_TARGET_W/H` from the
  display mode; render size via `gowj_render_width/height` cvars
  (`gowj_render_scale.cpp`).
- `gowj_fps_overlay.cpp` / `gowj_fsr.cpp`: read `gowj_upscale_status` for the
  overlay and to skip SMAA while a temporal upscaler runs (`": render "`).
- `GOWJ_HOME`: set by the single-file launcher to the folder the player runs
  from; both vendor-DLL loaders search it first.

Our game (`C:\personal\gow2-recomp\src`) would need, none of it implemented
here: a `gow2_upscaler`/`gow2_upscale_quality` config surface, a quality plan
setting `REX_GOW2_UPSCALER` + `GOW2_TARGET_W/H` before GPU init, a `GOW2_HOME`
equivalent if we ship single-file, overlay status via `gow2_upscale_status`,
and measured GoW2 render sizes replacing Judgment's 720p/1440p assumptions.

## Build (`scripts\build-sdk.ps1`)

- Fetches `dlss-sdk` (`git clone --depth 1 https://github.com/NVIDIA/DLSS.git`)
  and `ffx-sdk` (FidelityFX-SDK **tag v2.3.0**, sparse: `api/include`,
  `upscalers/include`, `signedbin`) into `out\sdk-build\`.
- Configure adds `-DGOW2_DLSS_SDK=<dlss>`, `-DGOW2_FFX_SDK=<ffx>/Kits/FidelityFX`
  (alongside `-DREXGLUE_ENABLE_FIDELITYFX=ON`) and `-DGOW2_DEV_TOOLS=` from `-Dev`.
- Installs `nvngx_dlss.dll` (`lib\Windows_x86_64\rel`) plus
  `amd_fidelityfx_loader_dx12.dll` / `amd_fidelityfx_upscaler_dx12.dll`
  (`signedbin`) into `out\sdk\`. The patch is self-contained (new files are
  part of the diff, no separate files-copy step).
- `-NoDlss` builds the release DLLs: no DLSS SDK is fetched, `GOW2_DLSS_SDK` names a folder that
  does not exist (`DLSS (NGX) support: OFF`), `nvngx_dlss.dll` is not installed, and the DLLs go
  to `out\sdk-nodlss\`; the build fails if an installed DLL still holds `nvsdk_ngx` or `nvngx`.
  For that reason the DLSS failure status names no NVIDIA file:
  `DLSS: unavailable (NGX init failed or no DLSS runtime DLL)`.

## Verified in game (2026-10-14, Radeon RX 7800 XT, `resolution_scale = 1`)

- The FidelityFX SDK v2.3.0 loader lists its providers by bare version: `4.1.1`, `3.1.5`,
  `2.3.4`. The FSR 4 test looked for "FSR" in the name, so `gow2_upscaler = fsr4` ran FSR 3.1;
  the major version is now parsed from the first number. FSR 4.1.1 creates its context on
  RDNA 3 (`FSR: context created '4.1.1' render 1280x704 -> 2560x1408`); if it ever fails, the
  plugin falls back to FSR 3.1 instead of turning the upscaler off.
- Gears of War 2's finished frame is `R8G8B8A8_TYPELESS` (dxgi 27), not 10:10:10:2.
  `UpscalerColorFormat` is the one TYPELESS -> typed mapping both backends use: FidelityFX gets
  it as the view format. NGX's D3D12 evaluate takes bare resources and views them with their own
  format, so DLSS reads a typed copy of the render region instead (`CopyTextureRegion` into an
  `R8G8B8A8_UNORM` texture; log `the upscaler reads a typed copy of the frame (dxgi 27 -> 28,
  1280x704)`). DLSS runs on NVIDIA hardware through that copy: confirmed on an RTX 5060 Ti (driver
  32.0.16.1742, `nvngx_dlss.dll` 310.9.16, 1280x704 -> 2560x1408, issue #18). `gow2_upscale_typed_copy = true`
  gives FSR the same copy so that input path can be checked on any GPU: with it on, FSR 4's
  image matches the direct read (mean RGB within 0.2 levels, same frame-to-frame difference).
- `gow2_upscaler = auto` falls back to FSR 3.1 when DLSS is unavailable. One flag used to mean
  both "DLSS failed" and "stop upscaling", so on this AMD card `auto` logged `DLSS: NGX init
  failed 0xBAD00001` and then `Upscaler: failed` for good; each backend now has its own failure
  flag and the next frame creates `FSR: context created '3.1.5'`.
- Scene detection at scale 1: frame and depth are both 1280x704 (depth dxgi 41), the output is
  the monitor size at the frame's aspect (2560x1408 on 2560x1440). Colours match the
  upscaler-off image (mean RGB within one level) and the HUD stays sharp while turning.
