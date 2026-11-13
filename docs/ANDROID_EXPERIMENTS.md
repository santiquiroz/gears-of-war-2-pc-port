# Android experiments: black scene and frame rate

The phone runs of October 14-15, 2026 left three problems (see [ANDROID.md](ANDROID.md)): large
black regions in Act 1 gameplay, 2.6 to 6 game frames per second with the GPU thermally capped at
160-222 MHz, and an old checkpoint that crashes on load. This is the plan for the next session on
the Galaxy S25 Ultra, in order. Each experiment says what to set, what to capture and what counts as
a result. The switches it uses keep today's behaviour unless a step sets them.

## Results of the October 15 session

Run on a warm phone (thermal status 2-3, skin 41-45 C, GPU capped at 160-342 MHz), so frame rates
are compared as GPU megacycles per game frame (capped clock x busy / fps) at the same cap. The
table with every run, its screenshot and its log is in the session's `out\device\results.md`.

- **Frame rate: fixed half of it.** `GPUSTATS` showed about 1,600 render passes per game frame for
  about 3,000 draws, 98 % of them ended by a barrier, and about 1,850 guest memory uploads per
  frame (about 21 MB, 85 % of it vertex buffers). Only about 2 % of those pages had been
  invalidated by CPU writes: the SDK's `clear_memory_page_state` (on by default) drops the valid
  bit of every CPU-uploaded page at each frame end, so the whole working set (about 6,000 pages) is
  uploaded again every frame, each upload a transfer that ends the Vulkan render pass. With it off,
  at the same 222 MHz cap: 9.4 instead of 4.6 fps (23.4 instead of 47.8 Mcycles/frame), 155 render
  passes and about 46 uploads per frame. Patch 0007 now defaults it to off on Android; the memory
  watches still invalidate pages the CPU writes (on Windows it stays on). A walkthrough with both
  settings (the Epic logo movie, the main menu, the save slot screen, loading, gameplay, walking and
  turning) showed the same picture, the movie and the HUD updating, at 9.7-9.9 instead of 5.0 fps.
- **Baseline from a cool start (step 1, with the fix).** The phone stayed on USB power (it cannot
  be unplugged remotely), so the gate was not reached: the run started at thermal status 0,
  `thermal_pwrlevel` 0, skin 34.0 C and battery 33.9 C, brightness 19, Motion smoothness on
  (`refresh_rate_mode` 1). The first 40 s of gameplay ran at 26-29 fps with the GPU at 734 MHz
  (at times the game's own 30 fps cap; GPU busy 74-99 %), then 19-23 fps at 525-660 MHz for two
  minutes, 14-16 fps at 342-443 MHz, and 9-13 fps at 222 MHz from the fourth minute on, when the
  skin had reached 44 C (thermal status 2). GPU megacycles per game frame stayed at 23-28 down to
  342 MHz and 16-19 at 222 MHz (median 24.2 over 132 quiet windows). The October 14 code managed
  13.6-16.8 fps at up to 734 MHz in the same place. Repeat on other days before trusting the
  decay; the cost now sits close to the game's frame cap at the higher clocks, so the next limit
  may be the CPU side (`top -H` during play, step 1).
- **Black scene: read that night as not a GPU state problem (corrected below).** None of
  experiments 2, 3, 5, 7-10 changed it, nor did the PC's texture cache limits, invalid fetch
  constants allowed, or the four minutes waited in
  place. Experiment 4 does not apply (the game issues no occlusion queries in this view, `ends 0`)
  and experiment 6 is ruled out (the first gameplay with every pipeline built during play and the
  later runs with all of them loaded from storage look the same). The new diagnostics rule out the
  rest of the GPU state: depth test forced to pass for color draws, the depth prepass skipped,
  stencil off, a whole-target scissor, face culling off and user clip planes / vertex kill dropped
  (the game uses neither) all leave the same black shape. No needed texture is missing
  (`missing_textures` now counts only the signed or unsigned view a texture actually uses: 0) and no
  DXT texture is loaded as all zeros. Painting every draw that tests against the scene depth with a
  constant magenta fragment shader (`vulkan_android_depth_experiment = 16`) paints the whole scene
  except the black region: no scene draw that passes its depth test reaches those pixels (the
  first reading, that no scene draw reaches them at all, is corrected below). A one-frame draw
  trace (`vulkan_android_trace_frame_after_seconds`) shows plain float positions (format 57) and
  non-zero vertex data in every draw, and every mesh drawn in the depth prepass also has its
  base-pass draws (3,093 draws). The game data on the phone matches the PC copy file for file.
  Corrections 1-4 below reopen the depth path: the switches named "depth test forced to pass" and
  "depth prepass skipped" did not test what their names say. The conclusion drawn that night (the
  missing objects absent from both passes or rasterizing nowhere) therefore does not hold, and the
  depth path (prepass, mid-frame depth clears, render target transfers) is a suspect again. Next,
  as planned then: an occlusion query around every draw of the traced frame (with the depth test
  and culling off) to name the draws that produce no samples, then their vertex shaders and
  constants; or a frame capture (RenderDoc or AGI), and a comparison with the PC on the same
  checkpoint, to tell a CPU-side difference (culling, streaming) from a vertex shader that places
  those objects wrongly. The diagnostic switches added for the next session (bits 128-1024, the
  color depth bias, the shader and draw skip lists, the extended trace) test the depth path first.

### Corrections to these results

Found while planning the next session (`t1-frametrace-gow2_081.log` is the one-frame trace of
October 15; the code is patch 0007):

1. **Bit 16 (paint) proves less.** `IsPaintedPipeline` only swaps the fragment shader; the depth
   compare stays the guest's. A draw rejected by the depth test leaves no magenta either, so "no
   scene draw reaches the black region" is not established: the scene may be drawn there and fail
   the depth test.
2. **Bit 1 (depth test always) never touched the base pass.** It relaxes color draws *without*
   depth writes (`IsAndroidDepthTestRelaxed`: `!z_write_enable`); all 1,885 base-pass draws
   (#576-#2460) have a pixel shader, write color and test GEQUAL *with* depth writes, so they kept
   their depth test.
3. **Bit 2 (prepass skipped) also skipped the depth clears.** The clears are depth-only rectangle
   lists (#0, #575, #2731, #2936 and #2953: no pixel shader, prim 8, z ALWAYS with writes), so with
   bit 2 the depth was never cleared. `d4-no-prepass.png` shows the result: characters as solid
   black silhouettes and dotted black trails along the swinging cables, stale depth rejecting the
   base pass. d4 says nothing about the prepass.
4. **The six 36-vertex boxes are not prepass draws.** #2503, #2513, #2524, #2534, #2540 and #2547
   are light or shadow volume stencil boxes (no pixel shader, GEQUAL, stencil, after the base pass),
   each followed by a stencil-tested box with the depth test off (#2507, #2514, ...). Every one of
   the 103 prepass meshes of #1-#574 (71 vertex buffers) has base-pass draws.
5. **b3c did not turn ambient occlusion off.** It set `AmbientOcclusion=False`, but the default-on
   `gow2_ao_upscaling_fix` forces the AO enable flags (`src/gow2_game_patches.cpp`), so AO was
   probably still on in b3c and is not ruled out.
6. **Few pixel shaders write a sample mask.** `GetCurrentPixelShaderModification` picks the early
   depth test hint (`kEarlyHint`: early fragment tests, no sample mask) unless the shader forbids
   implicit early Z or its coverage depends on alpha; only alpha-test, alpha-to-coverage and kill
   shaders get `gl_SampleMask`. b9 (depth written by the pixel shader, late Z: 66.6 against 47.8
   Mcycles per frame) shows what early Z is worth here.

### PC reference at the same checkpoint (plan A1)

On 2026-10-15, the RX 7800 XT PC (Windows 11, D3D12) ran the phone's own save and profile at
`resolution_scale` 1 and 2, using separate user and cache copies and the phone's six game-patch
values, including the AO patch. The player's own user folder stayed unchanged. Each kept run
passed the 180 s idle gate and ended with no real input; runs interrupted by input were repeated.

| First gameplay view | Black share | Probe (952,234), mean RGB |
|---|---|---|
| Phone `b0-first-gameplay.png` | 37.0 % | (4,4,4) |
| PC 1x `g45` | 2.9 % | (41,48,50) |
| PC 2x `g45` | 3.1 % | (39,47,48) |

The phone and PC first views have the same framing; the PC draws the right building, street and
sky where the phone is flat black. The probe is a 9x9 guest-pixel box on that building.
The phone's white block behind the car is far buildings and a banner on the PC at 1x and 2x.
The lit facade is warm on the phone and teal on the PC; which matches the console is not established.

- The black regions and white blocks are Android-specific: the PC renders this checkpoint
  completely with the phone's save, profile and game-patch values at both scales.
- Ruled out as the whole cause here: the game, the six patches on their own, native 720p, and
  shared render-target logic as far as D3D12 exercises it. The PC also uses host render targets,
  not ROV; branches that depend on backend features can still differ.
- Vulkan backend code and the Adreno driver remain open: this Windows build has no Vulkan backend.
  A patch could still trigger a Vulkan-only fault, so A2's game-patch rows stay cheap tests.
- Next: A2's render-target and transfer rows, A3 validation, A4 RenderDoc pixel history and A5 depth
  switches. A PC Vulkan run would separate backend from driver; it needs `REXGLUE_USE_VULKAN`
  and a backend selector (`LoadGpuPlugin` gets no backend name; "any" picks D3D12 first).

The full record and repeat commands are in `out\pcref\results.md`, in the session's local folder,
like `out\device\results.md` above.

### Black scene found (October 15, second session)

The afternoon session ran on a warm phone with the GPU capped at 160-222 MHz. The evidence is in
`out\phone2\`, a local folder not in git, like `out\device\`.

The trace found the cause: 1,881 of the 1,885 base-pass draws (#576-#2460) test GEQUAL with depth
writes and no polygon offset against the depth the same meshes wrote in the prepass (#1-#574,
also without polygon offset). The game relies on the depth-only and base-pass vertex shaders
giving bit-identical depth. On the PC (D3D12) they do; on the Adreno 830 some meshes come out up to
about one float24 step farther, fail the test and leave the cleared value: the black regions.
The translated shaders already emit every multiply and add with NoContraction in a fixed order
(no OpDot) and mark the position Invariant. The prepass reads a position-only vertex stream
(stride 3), while the base pass reads the full vertex (stride 7).

| Run | Setting | Black share |
|---|---|---|
| Reference (`a2-base`) | Defaults | 36.2 % |
| `a5-128` | `vulkan_android_depth_experiment = 128`: skip prepass meshes, keep depth clears | 3.8 % |
| `a5-256` | `vulkan_android_depth_experiment = 256`: every float24 color draw passes depth | 4.5 % |
| `a5-512` | `vulkan_android_depth_experiment = 512`: skip mid-frame depth clear rectangles | 38.5 % |
| `a5-bias16` | `vulkan_android_color_depth_bias = 16` | 3.7 % |

Bit 256 leaves the scene mis-sorted. With bias 16, probe (952,234) reads the building (55,48,50)
instead of (4,4,4). No configuration-only switch changed the black regions: the A2 table in
`out\phone2\results.md` spans 35.5-38.5 %, while `gow2_black_shading_fix = false` gives 42.7 %.
At the same caps, gameplay cost was 23.9 Mcycles per game frame for the reference, 26.0 for bit
128, 24.8 for bias 16 and 33.8 for bit 256, with noise of about 10 %.

`vulkan_android_color_depth_bias` defaults to 16 on Qualcomm GPUs (Vulkan vendor ID `0x5143`)
unless set, and to 0 on other GPUs. Sixteen units are 16 float32 ULPs of the D32 host depth
(float24 is hosted as `D32_SFLOAT_S8_UINT`), about 2 float24 steps; the measured prepass/base-pass
mismatch is up to about one float24 step. The runtime logs
`Android: color depth bias 16 (GPU vendor 0x5143, default for this vendor)`.
`ResolveColorDepthBias` in `include/rex/graphics/vulkan/android_draw_experiments.h` is checked by
`tests/unit/graphics/android_draw_experiments_test.cpp`. It was chosen over skipping the prepass
because it keeps the game's frame structure: the prepass, early depth rejection and depth for
later passes.

Patch 0007 also fixes the Vulkan render target transfer shader's host depth source addressing.
It used the guest source's sample count (`key.source_msaa_samples`); D3D12 uses the host depth
source's own count. For a 2x MSAA source and a 1x host depth source, it read rows
`8 * tile_row + y` instead of `16 * tile_row + y`. This had no visible effect at this checkpoint.
The fix is not Android-specific and is not behind `REX_PLATFORM_ANDROID`: only the Android build
compiles the Vulkan backend (`REXGLUE_USE_VULKAN` is off on Windows). Also in 0007,
`ANativeWindow_setFrameRate` is looked up with `dlopen("libnativewindow.so")` then `dlsym`, falling
back to `libandroid.so`. `dlsym(RTLD_DEFAULT, ...)` found nothing in the app's namespace and logged
"not available (Android 10)" on Android 16. The new lookup succeeded on the phone (P8/C3 below).

The colour grade still differs from the PC (phone warm, PC teal); which matches the console is
not established. After walking forward, two lamp posts and a hanging banner render solid white
with a glow (`crop-white-bias16.png`, 0.81 % of the walked picture). These are the white blocks of
October 15. With bit 128 they are absent (0.01 %); with bit 256 they are present (0.53 %). A draw
that should fail its depth test on those objects passes once depth moves. The later shadow tests
isolate light environment shadows: F3's Android default `LightEnvironmentShadows=0` removes the
white objects while keeping the world's dynamic shadows. The cause in the Vulkan backend or
driver is still open.

Patch 0011 removed the old Launch log marker, so the helpers kept pressing Space into gameplay.
They now add `system_link_trace = true` to every `gow2.toml` and wait for `[netlink] bind`.
`gow2_system_settings` values that make a Coalesced bundle longer than its TOC size are not served
(`COALESCED: no bundle could be prepared (... longer than their TOC size ...)`): `True` -> `False`
grows each value, while `0` reads as false in UE3 and keeps the bundle shorter. The phone self-test
printed `runtime_selftest: 21 checks, 0 failed` (both FPSCR checks) and
`touch_model_test: 105 checks, 0 failed`.

### Frame rate, display and image quality (October 15, second session)

The full measurements are in `out\phone2\results.md`, including every section after "After the
fix". F2 (commit `9a93000`) defaults to depth bias 16. F3 (commit `373c1e8`) also defaults to
`gow2_system_settings = "LightEnvironmentShadows=0"`, `gpu_worker_spin_us = -1` and
`gow2_android_gpu_wait_yield = false` on Android. `present_effect` stays `fsr`, FIFO and the 30 Hz
request stay, and `vulkan_pipeline_cache_file` stays on. Sustained performance mode stays off.

The F2 cool start began at thermal status 0, skin and battery 35.8 C, on USB power. It reached
status 3, skin 44.7 C, about 90 s into gameplay. USB power could not keep the phone cool; the
warm-phone A/B comparisons therefore use Mcycles per game frame and fps at the same capped clock.

| Cool-start clock | Quiet windows | Median fps | Mcycles/frame |
|---|---|---|---|
| 734 MHz | 3 | 23.6 | 30.2 |
| 660 MHz | 1 | 23.5 | 27.8 |
| 525 MHz | 5 | 16.9 | 30.8 |
| 443 MHz | 20 | 15.8 | 27.8 |
| 389 MHz | 14 | 13.9 | 27.6 |
| 342 MHz | 17 | 12.8 | 26.5 |

The median over quiet gameplay windows was 27.0 Mcycles/frame, against 24.2 in the morning build
with black regions: the missing scene is now drawn. C0 sampled `top -H` at +60 s, 389 MHz and
status 3, with the GPU 99 % busy:

| Thread | CPU use (% of one core) |
|---|---|
| Guest thread | 97-99 % |
| Audio Worker | 67-70 % |
| GPU Commands | 54-57 % |
| Main XThread | 20-23 % |
| SDLThread | 13-17 % |
| Second guest thread | 13-17 % |

Total CPU use was about 3.1 cores. Audio Worker runs the guest audio callback after a semaphore
wait with a 500 ms timeout; it is not polling. No power rails were readable, so this is not a
power measurement.

C1 recorded only the installed app with `appCategory` game; the other Game Booster/category
states were not tested. The cap followed skin temperature, with thermal-service thresholds
38/40/42/45 C, even before Android changed its reported thermal status.

| C1 point | Clock or cap | Thermal observation |
|---|---|---|
| At rest | Cap 1200 MHz, pwrlevel 0 | Status 0 |
| After 54 s at title | Cap 525 MHz, pwrlevel 9 | Skin 39.4 C, still status 0 |
| Gameplay 14:15:54 to 14:15:58 | 900, 832, 734 MHz | Clock falling |
| 14:16:28-31 | 660-525 MHz | Then 525/443 MHz alternating for a minute |
| Later gameplay | 389 MHz, then 342 MHz from 14:19 | Status 3, skin 44.5 C |

C8 changed the game's scene resolution and the runtime's render scale separately. Costs below
are Mcycles per game frame, with fps in parentheses; the repeated baseline shows the noise.

| C8 setting | At 160 MHz | At 222 MHz | Edge strength |
|---|---|---|---|
| ScreenPercentage 100, bias 16 | 20.3 (7.8) | 27.5 (8.0) | 15.0 |
| Same baseline repeated | 20.6 (7.7) | 28.2 (7.8) | 15.0 |
| ScreenPercentage 50 | 18.7 (8.5) | 21.3 (10.3) | 4.7 |
| ScreenPercentage 75, later valid run | - | About 21.9 (9.8-10.2) | - |
| resolution_scale 2 | 75.4 (2.1) | 73.3 (3.0) | 37.7 |

The first ScreenPercentage 75 run was void because an incoming-call overlay covered the game;
the later run above was valid. These F2 measurements had light environment shadows on, before the
F3 default. ScreenPercentage 75 saved about 20 % in one run and gave about 25 % more frames than
100 at 222 MHz; 50 gave no further gain and blurred the image heavily. Cost fell far less than the
pixel count (75 % keeps 56 % of the pixels, 50 % keeps 25 %), so most cost does not depend on scene
resolution (the runtime's render-target work, shadows and full-resolution passes).

The white banner stayed white with bloom and lens flares off. Disabling dynamic lights or
shadows removed it; the narrower shadow comparison was:

| Shadow setting (bias 16) | Mcycles/frame at 222 MHz | fps at 222 MHz | Banner/lamps |
|---|---|---|---|
| Shadows on | 24.4 | 9.0 | White |
| DynamicShadows=0 | 18.2 | 12.1 | Normal (dark banner) |
| LightEnvironmentShadows=0 | 17.9 | 12.3 | Normal |
| Both off | 17.9 | 12.3 | Normal |

F3 defaulted to `LightEnvironmentShadows=0` on Android because it removes the white objects while
keeping the world's dynamic shadows. In that build, setting `gow2_system_settings` replaced the
default. The Vulkan backend or driver cause remains open.

Current behavior (2026-10-19): `gow2_gfx_light_environment_shadows = "auto"` keeps
object shadows off on Android; the raw `gow2_system_settings` default is now empty.
Raw pairs override the composed platform defaults and quality rows, without removing
unrelated defaults. Recognized booleans use short `0`/`1` values to avoid bundle growth.
See [SYSTEM_SETTINGS.md](SYSTEM_SETTINGS.md).

P8/C3 confirmed the panel at 30 Hz with FIFO and `present_android_frame_rate = 30`:
`ANativeWindow_setFrameRate(30 Hz) returned 0`, SurfaceFlinger reported `vsyncRate=30.00 Hz` and
`renderRate=30.00 Hz`, and display reported `mActiveRenderFrameRate=30.000002`. The panel left
120 Hz. This confirms the request, not a power saving measurement.

P9 compared the F2 waits (spin 50 microseconds plus ISB, game wait yield, FIFO and 30 Hz) with
spin -1, no game wait yield, MAILBOX and no rate request on a warm phone at the same 222 MHz cap.
Both cost 26.5 Mcycles/frame. Process CPU snapshots were 253/249/240 % against 250/238/238 %;
GPU Commands used 21-28 % against 21-23 %. No difference was measured, so F3 restores -1 and
false for the two wait switches. The build-time ISB delay hints stayed in both runs and were
not isolated. P11 was not measured: cool A/B/B/A starts were not available on USB power.

P10 timed the same 852 stored pipelines back to back on a warm phone: 27.5 s without the driver
cache, 2.7 s with it (`.vkpc` loaded successfully, about 13 MB of driver data). The cache stays on.

| Present effect | Edge strength (first view / walked) | Mcycles/game frame at 222 MHz |
|---|---|---|
| bilinear | 8.2 / 8.6 | 21.8, 22.0 (34.3 in the first window) |
| fsr (default) | 17.6 / 18.6 | 22.9-27.8, median 23.9 |
| sgsr | 18.7 / 19.6 | 20.5-31.0, about 28.5 |

P7's present-effect runs drew FSR 1 and SGSR 1 correctly on Adreno 830, with no artefacts seen;
both were much sharper than bilinear, SGSR a little crisper. At 9 fps with a 30 Hz panel the
presenter paints about 3.3 times per game frame, putting the FSR cost at most about 0.6 Mcycles
per paint in these measurements, within run-to-run noise. SGSR was not cheaper here. There was
only one run per effect, so this is not a precise cost comparison; `present_effect` stays `fsr`.

A3 loaded Vulkan-ValidationLayers 1.4.363.0 for an F2 gameplay run, then removed the layer (it is
not shipped). One presenter semaphore error, `VUID-vkQueueSubmit-pSignalSemaphores-00067`, was
reported six times at boot: a signaled semaphore could still be in use by the swapchain. The only
warnings were fragment outputs without color attachments, capped at 10. Gameplay had no image
layout, barrier, attachment or depth/stencil errors. Fixed later that day with one present semaphore
per swapchain image (see [Review fixes](#review-fixes-october-15-late-session)).

A6-light set the production and prerelease updatable-driver opt-in settings with bias 0. Both
runs still loaded stock driver `0x80320040`, build `f61dec9117`; neither alternative driver was
tested. Black shares of 35.1 % and 38.1 % were consistent with the stock driver at bias 0.

| Build | Black share (value 2-6) | fps / Mcycles at 222 MHz | fps / Mcycles at 607-660 MHz |
|---|---|---|---|
| October 15 morning (`apk-final.apk`, p0) | 38.0 % | 10.7 / 20.5 | 19.2-19.4 / 26.5-27.8 |
| F3 (`apk-f3.apk`) | 0.4 % | 12.5 / 17.6 | - |

At 160 MHz F3 ran at 10.1 fps and 15.8 Mcycles/frame. It draws the whole scene with about 14 %
less GPU cost per frame and 17 % more frames at the same 222 MHz cap than the morning build.

**F3 cool start.** `f3-cool` used the final defaults, starting at 16:50 with thermal status 0,
skin 37.1 C, battery 37.6 C, pwrlevel 0 and USB charging (1.3 C warmer than `fr-f2-cool`).
After the title it reached status 1 and pwrlevel 9 (525 MHz), and status 3 by 16:53.

| Clock | Windows | F3 fps | F3 Mcycles/frame | F2 fps (fr-f2-cool) |
|---|---|---|---|---|
| 734 MHz | 1 | 29.7 (game cap 30) | 21.3 | 23.6 |
| 607 MHz | 1 | 27.9 | 19.8 | - |
| 525 MHz | 5 | 25.0 | 20.5 | 16.9 |
| 443 MHz | 14 | 23.2 | 18.9 | 15.8 |
| 389 MHz | 7 | 16.9 | 21.2 | 13.9 |
| 342 MHz | 24 | 15.8 | 21.4 | 12.8 |
| 222 MHz | 9 | 12.7 | 17.3 | - |

The median of quiet gameplay windows was 19.5 Mcycles/frame (F2 27.0, morning build 24.2). The
thermal cap was still falling when the F3 run ended about six minutes after launch: 15.8 fps at
342 MHz, then 12.7 fps at 222 MHz in its last minute. Warm, the same build ran at 10.1 fps at
160 MHz and 12.5 at 222 MHz (F3 warm run). Frame generation (plan E) does not apply: warm, the
build runs at about 10-12.5 fps, below the roughly 28 fps sustained threshold.

## Review fixes (October 15, late session)

- Vulkan presenter: one present semaphore per swapchain image is signaled by that image's paint
  submission and awaited by its present; retired swapchain semaphores are destroyed with it. This
  fixed `VUID-vkQueueSubmit-pSignalSemaphores-00067`.
- Vulkan presenter: the final swapchain pipeline records the format it was built for, avoiding a
  wait for the previous paint submission and a pipeline rebuild on every paint.
- Android present effects: launch creates only the selected effect's shader modules and pipelines
  (bilinear is always available). A rejected module or pipeline makes that effect unavailable with
  one warning; painting falls back to bilinear with one warning. SGSR 1 is never an intermediate
  pass.
- `vulkan_android_color_depth_bias` defaults to 16 on Qualcomm GPUs (Vulkan vendor ID `0x5143`)
  unless set, and to 0 elsewhere. Sixteen units are 16 float32 ULPs of the D32 host depth, about
  2 float24 steps; the measured prepass/base-pass mismatch is up to about one float24 step. The
  runtime log and `ResolveColorDepthBias` unit test document the default.
- Pre-transform check (F3 build, before these fixes; `p3-sf-f3-title-sf.txt`,
  `p3-sf-f3-game-sf.txt`, `p3-sf-f3-game2-sf.txt`): at the title, during gameplay and after walking, SurfaceFlinger showed
  the game's SurfaceView as DEVICE with ROT_90 and no CLIENT layers. The display rotated the
  IDENTITY-preTransform swapchain without GPU composition, so swapchain pre-rotation was not
  needed on this phone; it may matter on devices whose display processor cannot rotate the layer.
- Validation (`r-validation-gow2_153.log`, Vulkan-ValidationLayers 1.4.363.0): the review-fix run
  had 0 validation errors, versus 3 `VUID-vkQueueSubmit-pSignalSemaphores-00067` error messages
  in A3. The 10 capped `Undefined-Value-ShaderOutputNotConsumed-DynamicRendering` warnings,
  `WARNING-Swapchain-PreTransform` warning and `WARNING-cache-file-error` info message remained.
- Title effects (`r-title-bilinear.png`, `r-title-sgsr.png`, `r-title-fsr.png`): bilinear, SGSR and
  FSR each drew the guest output, with no `unavailable` lines in their logs.
- Gameplay without the layer (`r-game-gow2_154.log`): the scene was complete (3.7 % black share,
  including the movement tutorial's black box). A short run measured 27.5-28.1 fps at 607-734 MHz
  (20.2-21.7 Mcycles/frame), with a quiet-window median of 27.9 fps and 20.2 Mcycles/frame.
  Thermal status reached 3; walking windows at 443-525 MHz then ran at 15.0-18.3 fps
  (22.6-26.9 Mcycles/frame). This was not a sustained-rate measurement.

## What the build adds

The APK from `scripts\build-android.ps1 -LowPriority` at this commit (or later) has:

- **The shader storage fix.** Bionic's `fopen(..., "a+")` starts reading at the end of the file, so
  the Vulkan pipeline and shader storage found no header and truncated itself on every launch; no
  phone log ever had a `Created N graphics pipelines from Vulkan storage` line, while the PC loads
  about 1300. From the second launch on, pipelines seen before are built at boot instead of in
  gameplay.
- **`FRAMES` lines with the clock.** `gow2_frame_log` lines end with `gpu <MHz> of <MHz> busy <%>
  pwrlevel <n> thermal <status> headroom <h>`; any value Android hides from the app reads `n/a`.
  Android 16 on the S25 Ultra hides `gpuclk` from the app and from `adb shell`, so the clock is read
  from `devfreq/cur_freq` (or `clock_mhz`) instead.
- **`GPUSTATS` lines** (`vulkan_android_stats_log = true`), every five seconds:
  `swaps` / `unpresented` (frames not shown because a pipeline was still being built), `draws` /
  `pending` (draws skipped for that reason), `no_ps` / `no_ps_color` (draws without a pixel shader,
  and those of them with color targets bound), `gs` (rectangle, point and quad lists expanded in a
  geometry shader), `d24s8` / `d24fs8` (depth format of the draws), `missing_textures` (valid
  texture fetches bound to no image, which read 0), `color_draw_zfunc` (depth test of draws that
  write color), `passes` / `submits` / `drains` (render passes, queue submissions, waits for the
  whole GPU), `occlusion ends` / `host` / `zero` / `samples`, and `pipeline_setup` (time the GPU
  command thread spent translating shaders and building pipelines, the slowest call, and how many
  calls took 1 ms or more). Added on October 15: `no_vertices`, `ucp` / `vertex_kill` (draws with
  user clip planes or a vertex kill flag), `pass_ends barrier` / `forced` / `switch` (why render
  passes ended), `uploads` / `upload_kb` and `upload_kb_by index` / `texture` / `vertex` /
  `memexport` (guest memory copied to the GPU, by the draw step that needed it),
  `cpu_invalidations` / `cpu_invalidated_kb` / `newly_invalid_pages` (CPU writes caught by the
  memory watches), and `texture_loads` / `gpu_written` / `zero_bc_base` / `zero_bc_mips` /
  `zero_other` / `reloaded_after_zero` / `invalidated` (texture loads; `gpu_written` counts those
  with a range a resolve or memexport wrote, whose data lives only in the GPU's copy of guest
  memory, so they are not checked for zero; the `zero_*` counts are the other loads whose guest
  data was all zero, with `TEXZERO` / `TEXRELOAD` lines for block-compressed ones; the earlier
  October 15 builds counted GPU-written loads in `zero_other` too, whatever the GPU had written).
  `missing_textures` counts only the signed or unsigned view a texture actually uses (every fetch
  declares both). `no_ps_d24fs8_no_rt0` counts depth-only draws on the float24 depth with no color
  target 0 bound: the ones bit 1024 cannot paint.
- **`TEXFMT`** lines (with `vulkan_android_stats_log`): each texture format the first time it is
  loaded, with its host format and load shader. **`TEXFAIL`** lines (always): a texture whose data
  could not be loaded, once per format; that used to fail without a word.
- **Present effects and display.** Android now has `bilinear`, `cas`, `fsr` (FSR 1, the default)
  and `sgsr` (Snapdragon GSR 1); FSR 2/3 are not available. The game still renders at 1280x720.
  At 2560x1440 output FSR 1 runs EASU into an intermediate image then RCAS into the swapchain;
  SGSR draws once into the swapchain without an intermediate image or dithering. The other final
  passes dither by default. These filters change image quality, not the game's GPU work (P7).
  The swapchain now defaults to plain FIFO, painting on the UI thread instead of the GPU command
  thread; the October 15 logs used MAILBOX. Android 11+ receives an
  `ANativeWindow_setFrameRate` request for 30 Hz; P8/C3 confirmed the panel left 120 Hz for 30 Hz.
  The request is not a forced rate. 0 makes no request; `gow2_unlock_fps` uses 0 for that launch
  unless `gow2.toml` sets `present_android_frame_rate` (`DISPLAY frame rate hint` in the log).
  The call logs `VulkanPresenter: ANativeWindow_setFrameRate(30 Hz) returned 0` on success.
- **Transfer shader and frame rate lookup.** Patch 0007 addresses the host depth source with its
  own sample count in the Vulkan render target transfer shader, with no visible change at this
  checkpoint. `ANativeWindow_setFrameRate` is resolved through `dlopen("libnativewindow.so")` then
  `dlsym`, falling back to `libandroid.so`; P8/C3 confirmed the lookup and request on the phone.
- **Driver pipeline cache.** `vulkan_pipeline_cache_file` keeps
  `cache/shaders/shareable/4D53082D.vkpc` beside `4D53082D.xsh` and `4D53082D.fbo.vk.xpso`.
  It loads at boot after checking vendor, device, driver version, pipeline cache UUID, size and
  an XXH3 hash; a different or damaged file is ignored and rewritten. The storage writer saves
  after the boot-time pipelines are built, every two minutes while new pipelines appear, and
  at shutdown, writing `.tmp` then renaming it. Logs name the path, load status and driver bytes:
  `VulkanPipelineCache: pipeline cache file <path>: <status> (<n> bytes of driver data)` and
  `VulkanPipelineCache: saved <n> bytes of driver pipeline cache to <path>`. Reusing driver data
  avoided repeating driver compilation in P10: the same 852 stored pipelines built in 2.7 s
  instead of 27.5 s on a warm phone.
- **CPU waits.** After P9 measured no difference, `gpu_worker_spin_us` defaults to -1 (the SDK's
  roughly 500 yield polls before sleeping) and `gow2_android_gpu_wait_yield` to false (the game's
  SDK wait behaviour, without the added yield). Its guest work and time-base timeout stay the same.
  November 13: `gow2_gpu_wait_sleep_us` replaced that yield (see the defaults table).
  At build time `tools/prepare_android_sources.py` runs `tools/patch_spin_hints.py` on the Android
  copy of the generated code, adding a guarded `__builtin_arm_isb(15)` (inside
  `#if defined(__aarch64__)`) after Xenon delay hints the recompiler otherwise omits. GoW2 has 12
  `db16cyc` sites: eight in `sub_82977688`, the rest in
  wait loops in `sub_829925B0`, `sub_82955768` and `sub_829531F8`. The original `generated/` and
  Windows build are unchanged. The window's sustained performance mode is opt-in where
  `PowerManager.isSustainedPerformanceModeSupported()` allows it; the runtime logs
  `SUSTAINED performance mode requested: ...`. P11 remains unmeasured; the USB-powered session
  could not provide cool A/B/B/A starts.
- **Switches** (all in `gow2.toml`; defaults below are Android defaults):

  | Switch | Default | Effect |
  |---|---|---|
  | `present_effect` | "fsr" | `bilinear`, `cas`, `fsr` (FSR 1 EASU then RCAS), `sgsr` (Snapdragon GSR 1, one pass); no FSR 2/3 (restart) |
  | `present_fsr_sharpness_reduction` | 0.3 | RCAS sharpness reduction in stops (0.2 elsewhere; restart) |
  | `present_dither` | true | Dither the final pass except SGSR (false elsewhere; restart) |
  | `present_cas_additional_sharpness` | 0 | Additional CAS sharpness in [0, 1], same default as elsewhere (restart) |
  | `present_fsr_max_upsampling_passes` | 4 | Maximum chained EASU passes, same default as elsewhere (restart) |
  | `vulkan_allow_present_mode_immediate` | false | Allow IMMEDIATE instead of FIFO |
  | `vulkan_allow_present_mode_mailbox` | false | Allow MAILBOX instead of FIFO |
  | `vulkan_allow_present_mode_fifo_relaxed` | false | Allow FIFO_RELAXED instead of plain FIFO |
  | `present_android_frame_rate` | 30 | Ask Android 11+ for this window frame rate in Hz; 0 makes no request; launch default 0 with `gow2_unlock_fps` unless configured |
  | `vulkan_pipeline_cache_file` | true | Load and save the driver's `.vkpc` cache beside shader storage (restart) |
  | `gpu_worker_spin_us` | -1 | -1 keeps roughly 500 SDK yield polls; 0 sleeps immediately; N polls with ISB pauses for N microseconds, then waits in 5 ms sleeps; 50 was measured with no difference |
  | `gow2_gpu_wait_sleep_us` | 0 | Microseconds the render thread sleeps between polls of its wait for ring buffer space, at most 1 ms per wait; 0 spins like the console. 200 took the process from 200 % to 155 % of a core for 1.5 fps less (November 13); replaces `gow2_android_gpu_wait_yield`, whose yield per poll P9 measured with no difference |
  | `gow2_android_sustained_performance` | false | Request supported window sustained performance mode (restart); opt-in until measured |
  | `vulkan_android_empty_fragment_shader` | false | Draws without a pixel shader that have color targets bound get an empty fragment shader instead of none (restart) |
  | `vulkan_android_invariant_position_member` | false | `gl_Position` also gets the `Invariant` member decoration glslang emits (restart) |
  | `vulkan_occlusion_query_precise` | false | Occlusion queries ask for exact sample counts (`VK_QUERY_CONTROL_PRECISE_BIT`) |
  | `vulkan_async_pipeline_placeholders` | true | false: new pipelines are queued without first building a placeholder on the GPU command thread, and pipelines without a pixel shader are built in the background too |
  | `gow2_system_settings` | "" | Advanced `Key=Value` pairs separated by `;` for `[SystemSettings]` in `Xe-GearEngine.ini`; raw pairs override the composed platform defaults and quality rows; recognized booleans become `0`/`1` (restart) |
  | `gow2_gfx_light_environment_shadows` | "auto" | Platform default: object shadows off on Android, retail on Windows; `on` / `off` explicitly override it (restart) |
  | `clear_memory_page_state` | false on Android | true re-uploads every CPU-written page at each frame end (the SDK's default elsewhere); halves the frame rate here |
  | `vulkan_android_depth_experiment` | 0 | Diagnostic bits, each drawing the scene wrongly: 1 color draws without depth writes pass the depth test, 2 skip every depth-only draw on the float24 depth (the prepass and its depth clears), 4 stencil off for color draws, 8 whole-target scissor, 16 scene draws that pass the depth test painted magenta, 32 face culling off, 64 no user clip planes or vertex kill, 128 skip the prepass meshes but keep the depth clears (rectangle lists), 256 every color draw with a pixel shader on the float24 depth passes the depth test (depth writers too), 512 skip the depth clear rectangles drawn after the frame's first prepass mesh (#575-class; #0 stays), 1024 depth-only draws on the float24 depth with color target 0 bound write magenta (restart) |
  | `vulkan_android_color_depth_bias` | 16 on Qualcomm, 0 otherwise | Unless set, 16 on Qualcomm GPUs (Vulkan vendor ID `0x5143`) and 0 on others, added to the depth bias of color draws with a pixel shader on float24 depth that test GREATER or GEQUAL; 16 units are 16 float32 ULPs of the D32 host depth (float24 is hosted as `D32_SFLOAT_S8_UINT`), about 2 float24 steps, against a measured prepass/base-pass mismatch of up to about one float24 step; fixes the black regions while keeping the prepass, early depth rejection and depth for later passes; 0 turns it off (restart) |
  | `vulkan_android_skip_shader_hashes` | "" | Comma-separated 16-hex ucode hashes (vertex or pixel shader, as `FRAMETRACE` prints them): their draws are skipped (restart) |
  | `vulkan_android_paint_shader_hashes` | "" | The same list format: their draws write magenta (a depth-only draw only where color target 0 is bound) (restart) |
  | `vulkan_android_skip_draw_range` | "" | `first-last` or one number: those draws of every frame are skipped, numbered as `FRAMETRACE` numbers them (restart) |
  | `vulkan_android_trace_frame_after_seconds` | 0 | Logs every draw of one frame this many seconds after the first swap (restart): `FRAMETRACE` (shaders, vertex formats, the first vertex words), `FRAMETRACE-STATE` (render pass key, surface pitch and MSAA, depth and written color targets as base/format, viewport, window scissor and offset, polygon offset, the color depth bias applied and why a switch skipped the draw), `FRAMETRACE-VERTS` (the first three vertices of rectangle lists) and `FRAMETRACE-RT @<next draw>` (render target ownership transfers with their source and tiles, resolves with RB_COPY_* and clear values, resolve clears) |

  Draws are numbered from 0 after every swap, as they reach the pipeline (a draw skipped while its
  pipeline is still being built gets no number); a skipped draw keeps its number and its
  `FRAMETRACE` lines, so a trace of a run with a skip switch still lines up with the plain one.
  Switches that change a pipeline's description (bits 1, 4, 32, 64, 256 and 1024, and the paint
  list for draws without a pixel shader) add those pipelines to the shader storage, and a later
  plain launch builds them at boot too: delete the storage and `4D53082D.vkpc` (experiment 6's
  `rm`) before measuring boot time or frame rate after such runs. A stale driver cache would hide
  boot-time compilation. Bit 16 and the paint list for draws with a pixel shader
  only swap the fragment shader when a pipeline is built.

- **Tools.** `tools\android_log_summary.py <log> [--clocks <file>]` prints one row per `FRAMES`
  window (fps, pipelines built in it, clock, GPU megacycles per game frame, `GPUSTATS` fields) and
  the median of the quiet windows (4 or fewer pipelines built). `tools\black_fraction.py <png>`
  gives the share of the 16:9 picture that is one flat dark value: 34.0% in tonight's screenshots
  at the start of the checkpoint (`fix-11-loaded.png`, `fix-23-gameplay.png`,
  `final-06-gameplay.png`). `scripts\android-gpu-clocks.sh` samples the GPU clock on the phone once
  a second for `--clocks`.

## Setup (every session)

PowerShell from the worktree (`C:\personal\gow2-recomp-droid`). Settings reach the app only
through `gow2.toml` in `Android/data/com.santiquiroz.gow2recomp/files`, beside the `logs` folder,
read at launch ([android/README.md](../android/README.md#frame-rate-log)). Each step below writes
a complete `gow2.toml`; the phone's own file is saved first and put back at the end. Keys are
sent with the keyboard driver (`mnk_mode`, touch controls off), which `adb shell input keyevent`
drives: Return is Start, Space is A.

```powershell
$s = 'R5CY82F28DM'
$pkg = 'com.santiquiroz.gow2recomp'
$files = "/sdcard/Android/data/$pkg/files"
$exp = 'out\android-probe\exp'
New-Item -ItemType Directory -Force $exp | Out-Null

function Set-GameConfig([string]$name, [string[]]$extra = @()) {
    $lines = @('gow2_frame_log = true', 'vulkan_android_stats_log = true',
               'gow2_touch = "off"', 'mnk_mode = true', 'system_link_trace = true') + $extra
    $toml = Join-Path $exp "$name.toml"
    Set-Content -Path $toml -Value $lines -Encoding ascii
    adb -s $s push $toml "$files/gow2.toml"
}
function Start-Game {
    adb -s $s shell am force-stop $pkg
    adb -s $s shell am start -W -n "$pkg/.Gow2Activity" --ez "$pkg.extra.SHOW_WHEN_LOCKED" true
}
# A plain keyevent is down and up within a few ms; the game polls once per frame and misses it.
function Send-Key([string]$key, [int]$seconds) {
    adb -s $s shell input keyevent --longpress $key
    Start-Sleep -Seconds $seconds
}
function Get-LogCount([string]$pattern) {
    $name = (adb -s $s shell ls -t "$files/logs" | Select-Object -First 1).Trim()
    [int]((adb -s $s shell "grep -c '$pattern' $files/logs/$name").Trim())
}
# Presses a key until the log shows the step happened (the title can take longer on a hot phone).
function Send-KeyUntil([string]$key, [string]$marker, [int]$want) {
    for ($try = 0; $try -lt 4; $try++) {
        Send-Key $key 0
        for ($waited = 0; $waited -lt 12; $waited += 2) {
            Start-Sleep -Seconds 2
            if ((Get-LogCount $marker) -ge $want) { return }
        }
    }
}
# Title (Start), Solo Campaign, save slot 1 (Tip of the Spear), Launch: the game probes the
# checkpoint saves when the main menu and the slot screen open and binds its socket at Launch.
function Open-Checkpoint {
    Start-Sleep -Seconds 45
    Send-KeyUntil KEYCODE_ENTER 'Gears2Checkpoint1.sav' 1
    Send-KeyUntil KEYCODE_SPACE 'Gears2Checkpoint1.sav' 2
    Send-KeyUntil KEYCODE_SPACE '\[netlink\] bind' 1
}
function Save-Shot([string]$name) {
    adb -s $s shell screencap -p /data/local/tmp/gow2-shot.png
    adb -s $s pull /data/local/tmp/gow2-shot.png (Join-Path $exp "$name.png")
}
function Save-Log([string]$name) {
    $newest = (adb -s $s shell ls -t "$files/logs" | Select-Object -First 1).Trim()
    adb -s $s pull "$files/logs/$newest" (Join-Path $exp "$name-$newest")
}
function Measure-Black([string]$name) {
    Start-Game; Open-Checkpoint; Start-Sleep -Seconds 75
    Save-Shot $name; Save-Log $name
    python tools\black_fraction.py (Join-Path $exp "$name.png")
}
```

Patch 0011 removed the old Launch marker, so `system_link_trace = true` and `[netlink] bind` keep
the helpers from pressing Space into gameplay. Use `0` rather than `False` for false UE3 settings:
`True` -> `False` grows Coalesced bundles beyond their TOC size and prevents them from being served.

The first time, take a `Save-Shot` after each key of `Open-Checkpoint` and lengthen its waits if a
screen was not ready (the boot logos, then the title). Screenshots go through a file on the phone:
redirecting `exec-out screencap` in Windows PowerShell corrupts the PNG.

## 0. Prepare (the phone may be warm)

```powershell
adb -s $s pull "$files/gow2.toml" (Join-Path $exp 'gow2.toml.orig')
adb -s $s install -r android\app\build\outputs\apk\debug\app-debug.apk
powershell -ExecutionPolicy Bypass -File scripts\test-android-device.ps1 -Serial $s -LowPriority
adb -s $s push scripts\android-gpu-clocks.sh /data/local/tmp/android-gpu-clocks.sh

Set-GameConfig 'storage'
Start-Game; Start-Sleep -Seconds 40
adb -s $s shell ls -l "$files/gow2/cache/shaders/shareable"
Start-Game; Start-Sleep -Seconds 40; Save-Log 'storage-second'
Select-String 'from Vulkan storage' (Join-Path $exp 'storage-second-*.log')

Measure-Black 'b0-first-gameplay'
```

`install -r` keeps the copied game, the saves and the config. If the `shareable` folder is not
there, `adb -s $s shell find /sdcard/Android/data/$pkg -name '*.xpso'` finds it.

Pass: the self-test prints `runtime_selftest: 21 checks, 0 failed` (the 19th is the fiber switch
keeping d8-d15; the 20th and 21st, added later on October 15, write each guest rounding mode, read
it back as `mffs` does and round 1/3 with it) and `touch_model_test: 105 checks, 0 failed`;
`4D53082D.fbo.vk.xpso` and `4D53082D.xsh` are larger than their 12- and 8-byte headers after the
first launch; the second launch logs `Created N graphics pipelines from Vulkan storage` with N
above 0. The
`b0-first-gameplay` run is the first gameplay since the install: it fills the storage with the
gameplay pipelines and is the reference picture for experiment 6. Keep its `GPUSTATS`, `TEXFMT`
and `TEXFAIL` lines; they say which of the black-scene experiments to run first (see there).

## 1. Cool-phone baseline (first measurement)

1. Move adb to Wi-Fi so charging does not heat the phone, then unplug USB:
   `adb -s $s tcpip 5555`, `adb -s $s shell ip -f inet addr show wlan0`,
   `adb connect <ip>:5555`, `$s = '<ip>:5555'`. If `connect` fails while the phone answers ping,
   pair again under Wireless debugging (the port changes; the mDNS serial also works).
2. `adb -s $s shell am force-stop $pkg`, `adb -s $s shell input keyevent KEYCODE_SLEEP`, phone out
   of its case on a hard surface, no builds or installs, room temperature noted. Check every five
   minutes until all hold (usually 20 minutes):
   ```powershell
   adb -s $s shell dumpsys thermalservice | Select-String 'Thermal Status|mType=3'
   adb -s $s shell dumpsys battery | Select-String 'temperature'
   adb -s $s shell cat /sys/class/kgsl/kgsl-3d0/thermal_pwrlevel
   ```
   Gate: thermal status 0, SKIN at 33 C or less, battery temperature 300 (30.0 C) or less,
   `thermal_pwrlevel` 0. Note `cat /sys/class/kgsl/kgsl-3d0/max_gpuclk`, the screen brightness, and
   the Game Booster profile and Motion smoothness setting; keep them for every later run.
3. Measure (storage warm from step 0, no input after the checkpoint loads):
   ```powershell
   Set-GameConfig 'p0-baseline'
   Start-Game; Open-Checkpoint; Start-Sleep -Seconds 20
   $clocks = Start-Job { param($s) adb -s $s shell sh /data/local/tmp/android-gpu-clocks.sh 600 /data/local/tmp/gpu-clocks.txt } -ArgumentList $s
   Start-Sleep -Seconds 120; Save-Shot 'p0-baseline-120s'
   Wait-Job $clocks | Out-Null
   adb -s $s pull /data/local/tmp/gpu-clocks.txt (Join-Path $exp 'p0-baseline-clocks.txt')
   Save-Log 'p0-baseline'
   python tools\android_log_summary.py (Get-Item "$exp\p0-baseline-gow2_*.log").FullName --clocks "$exp\p0-baseline-clocks.txt"
   ```
   The sampler runs 10 minutes, so the run also shows how long the phone holds its clock. During
   it, `adb -s $s shell dumpsys thermalservice | Select-String 'Thermal Status|mType=3'` every
   minute records the throttling as it comes.

Read: fps, clock, busy and Mcycles/frame of the quiet windows over the 10 minutes. GPU-bound (as
tonight): busy 95% or more and Mcycles/frame within about 15% while the clock falls; expect 15-28
fps at 680-1200 MHz and 35-50 Mcycles per frame, falling to 4-7 fps once the cap returns. If busy
stays under 85% while fps is low, the game is CPU- or sync-bound: record
`adb -s $s shell top -H -b -d 5 -n 6 -p <pid>` (`adb -s $s shell pidof $pkg`) to see which
thread holds a core. Repeat on two more days before trusting the numbers. Every later frame-rate
experiment compares Mcycles/frame of quiet windows, with the gate of step 2 before each run.

## 2-11. The black scene

The black regions do not depend on the clock: run these on a warm phone, one launch each, the
same checkpoint and no input. `Measure-Black <name>` after `Set-GameConfig <name> @(<keys>)`; it
prints the black share. Baseline: about 34% at this view (`p0-baseline-120s.png`). A cause found:
under 5% with the scene complete; partial: a drop of 5 points or more, or a visibly different
shape. Keep each log; `GPUSTATS` shows what the switch changed.

What `b0`/`p0` tell first: `no_ps_color` above 0 favours experiment 2; `TEXFAIL` lines or
`missing_textures` above 0 point at textures (go to 11 with that format); many `zero` occlusion
results favour 4; most `color_draw_zfunc` in `equal` favours 5; `d24fs8` above 0 makes 9 worth
running.

2. **Empty fragment shader.** `Set-GameConfig 'b2-empty-fs' @('vulkan_android_empty_fragment_shader = true')`,
   `Measure-Black 'b2-empty-fs'`; the log has `pipelines without a pixel shader get an empty
   fragment shader`. The black shapes in `fix-11`/`fix-23` have straight edges that cut jaggedly
   into the buildings, like boxes drawn with the depth test (occlusion boxes, shadow volumes and
   depth-only passes are drawn with color writes off and no pixel shader, while the scene's color
   targets stay bound); LibertyRecompAndroid documents garbage writes in exactly that case on
   Turnip. Pass: black share under 5%. Then the stock driver mishandles fragment-less pipelines
   with color targets bound, and the switch should become the Android default.
3. **The game's shadows.** `Set-GameConfig 'b3-noshadows' @('gow2_system_settings = "DynamicShadows=False"')`,
   `Measure-Black 'b3-noshadows'`; the log's `COALESCED:` line names `system-settings`. Modulated
   shadows draw their frustum into stencil without color and then darken what is inside it, so a
   shadow-depth texture that reads 0 would blacken boxy regions. If the black goes, narrow it down
   with `LightEnvironmentShadows=False`, `bAllowBetterModulatedShadows=False` and
   `bEnableBranchingPCFShadows=False` one at a time. Also try `DirectionalLightmaps=False` for the
   lightmap path. The section has 76 keys; the switches among them are `StaticDecals`,
   `DynamicDecals`, `DynamicLights`, `DynamicShadows`, `LightEnvironmentShadows`,
   `CompositeDynamicLights`, `DirectionalLightmaps`, `MotionBlur`, `DepthOfField`,
   `AmbientOcclusion`, `Bloom`, `UseHighQualityBloom`, `Distortion`, `LensFlares`, `FogVolumes`,
   `bAllowBetterModulatedShadows`, `bEnableBranchingPCFShadows` and `ScreenPercentage`.
4. **Occlusion queries.** `Set-GameConfig 'b4-noquery' @('occlusion_query_enable = false')`, then
   `Set-GameConfig 'b4-precise' @('vulkan_occlusion_query_precise = true')`, a `Measure-Black` each.
   The game still draws its query boxes either way, so a change here means the counts the Adreno
   driver returns (zero or too low) make the game cull meshes; `b4-noquery`'s `GPUSTATS` shows
   `host 0` and fewer `drains`.
5. **Position invariance.** `Set-GameConfig 'b5-invariant' @('vulkan_android_invariant_position_member = true')`,
   `Measure-Black 'b5-invariant'`. A depth pass followed by a color pass with an `equal` depth test
   fails everywhere if the two vertex shaders compute even slightly different depth; the switch adds
   the decoration drivers most often honour.
6. **Pipelines built late.** Compare `b0-first-gameplay.png` (first gameplay after the install, every
   pipeline built during play) with `p0-baseline-120s.png` (storage warm). A draw skipped while its
   pipeline was being built is gone for good when its result is kept across frames. If `b0` is black
   and `p0` is not, the fix is the storage; if both are black, run
   `Set-GameConfig 'b6-sync' @('async_shader_compilation = false')` once with the storage deleted
   (`adb -s $s shell rm "$files/gow2/cache/shaders/shareable/4D53082D.fbo.vk.xpso" "$files/gow2/cache/shaders/shareable/4D53082D.xsh" "$files/gow2/cache/shaders/shareable/4D53082D.vkpc"`)
   and `Measure-Black 'b6-sync'`. Delete the driver cache too: leaving it would hide boot-time
   compilation. Run step 0's two title launches again afterwards to refill the storage.
7. **Sparse shared memory.** `@('vulkan_sparse_shared_memory = false')`, `Measure-Black 'b7-nosparse'`;
   the log then says `Vulkan sparse binding is not used for shared memory emulation`. A change
   means sparse binds the driver accepted did not take effect.
8. **Render passes instead of dynamic rendering.** `@('vulkan_dynamic_rendering = false')`,
   `Measure-Black 'b8-renderpass'`. Set it only in `gow2.toml`, never in the overlay.
9. **Float24 depth in the shader** (only if `d24fs8` is above 0).
   `@('depth_float24_convert_in_pixel_shader = true')`, `Measure-Black 'b9-float24'`.
10. **No geometry shaders.** `@('vulkan_force_expand_rectangle_lists_in_vs = true', 'vulkan_force_expand_point_sprites_in_vs = true', 'vulkan_force_convert_quad_lists_to_triangle_lists = true')`,
    `Measure-Black 'b10-nogs'`; `GPUSTATS` then shows `gs 0`. Also a frame-rate experiment (P3).
11. **Name the draw (if 2-10 change nothing).** The debug APK is debuggable: capture one frame at
    this view with RenderDoc for Android (`gpu_debug_markers = true` in the config helps), and run
    pixel history on a black pixel in the column above the street in `fix-23` and on the jagged
    edge in `alt-07`. The draw that writes the black, or the base-pass draw that is missing, is
    named directly.

Restore the phone's own settings at the end of a session:
`adb -s $s push (Join-Path $exp 'gow2.toml.orig') "$files/gow2.toml"`.

## P1-P11. Frame rate (cool phone, A/B)

Each pair alternates A, B, B, A, two minutes each, with the gate of step 1 before every launch,
the clock sampler running (`android-gpu-clocks.sh 120`), and the comparison done on Mcycles/frame
of quiet windows (fps alone moves with the clock). A switch helps when Mcycles/frame falls by 10%
or more in both B runs.

- **P1 occlusion queries.** B: `occlusion_query_enable = false`. Each guest query ends the render
  pass twice and waits for the whole GPU (`drains`). Helps if the game issues many per frame
  (`ends` per swap in `GPUSTATS`); everything then counts as visible, so watch for extra geometry.
- **P2 fewer submissions.** B: `vulkan_submit_on_primary_buffer_end = false`; then B:
  `readback_memexport = false`. `submits` and `passes` per swap show the change.
- **P3 no geometry shaders.** B: the three keys of experiment 10.
- **P4 the game's effects.** B: `gow2_system_settings = "AmbientOcclusion=False;MotionBlur=False;DepthOfField=False;UseHighQualityBloom=False;Distortion=False"`,
  then one key at a time for those that matter, then `ScreenPercentage=75` (no change means the
  console build ignores it). Splits the 35-50 Mcycles into the game's content and the emulation's
  overhead.
- **P5 placeholders.** With the storage deleted before each launch (experiment 6's `rm`), A:
  default, B: `vulkan_async_pipeline_placeholders = false`. Compare `pipeline_setup` ms, the
  slowest call and fps of the compile windows. Pass: `pipeline_setup` halves or better, frames are
  still presented once building ends (`unpresented` back to 0), and the log has no `Vulkan
  IssueDraw failed` lines.
- **P6 trace.** If P1-P5 leave the cost unexplained, record ten seconds of quiet gameplay with
  Perfetto (`gpu.renderstages`, `gpu.counters`, `linux.ftrace` with `sched_switch` and
  `gpu_frequency`) or AGI's system profile: render passes and submissions per frame, GMEM load and
  store share, and GPU idle while the GPU command thread waits.

- **P7 present effect.** A: `present_effect = "bilinear"`; B: `present_effect = "fsr"`, then a
  separate A/B/B/A with B: `present_effect = "sgsr"`. Use a cool phone and quiet windows at the
  same view. Compare GPU Mcycles per presented frame: use `GPUSTATS` swaps minus unpresented
  per window with the sampled clock and busy fraction, rather than the game's `FRAMES` count.
  Require `unpresented = 0` in the quiet windows being compared. Keep screenshots of that same
  view for quality. The internal game resolution stays 1280x720; filter cost and look
  remain to be measured.
  Result (October 15): FSR and SGSR were sharper than bilinear; one run each leaves cost noisy ([measurements](#frame-rate-display-and-image-quality-october-15-second-session)).
- **P8 display pacing.** A: defaults; B: `vulkan_allow_present_mode_mailbox = true` and
  `present_android_frame_rate = 0`. Use A/B/B/A with the same cool-start gate and quiet windows.
  Read `adb -s $s shell dumpsys SurfaceFlinger` in each run for the refresh rate, and keep the
  present-mode and frame-rate request log lines. Compare Mcycles/frame, presented frame cadence
  and sustained clock; any panel or power effect is to be measured.
  Result (October 15): FIFO with the 30 Hz request put the panel at 30 Hz ([measurements](#frame-rate-display-and-image-quality-october-15-second-session)).
- **P9 CPU waits.** A: F2 settings (`gpu_worker_spin_us = 50` and
  `gow2_android_gpu_wait_yield = true`); B: `gpu_worker_spin_us = -1` and
  `gow2_android_gpu_wait_yield = false`. Use A/B/B/A, cool starts and quiet-window Mcycles/frame.
  Record `adb -s $s shell top -H -b -d 5 -n 6 -p <pid>` during the same view: CPU use of the GPU
  command thread and game thread, plus the sustained GPU clock from the sampler. This compares
  the configurable waits; the build-time ISB delay hints stay in both builds. CPU and power
  effects are to be measured.
  Result (October 15): no difference at the same cap; defaults reverted to -1 and false, build-time ISB hints kept ([measurements](#frame-rate-display-and-image-quality-october-15-second-session)).
- **P10 pipeline cache.** Delete both shader storage files and `4D53082D.vkpc` with experiment
  6's `rm`, then make a first launch through the same view to populate storage and let it save.
  Keep the next two launches without deleting anything: compare the time from
  `Initializing shader storage` to `Created N graphics pipelines from Vulkan storage` on the
  second and third launch, with the same pipeline count. The second builds the stored pipelines
  and saves driver data; the third can reuse it. Look for `pipeline cache file <path>: ok` and
  the saved-byte line. For a control, repeat the three-launch sequence with
  `vulkan_pipeline_cache_file = false`; alternate enabled/disabled/disabled/enabled (A/B/B/A),
  with a cool phone before every launch. Compare boot time and quiet-window Mcycles/frame
  separately. A stale driver cache would hide the compilation this test needs to time; savings
  remain to be measured.
  Result (October 15): 852 stored pipelines built in 2.7 s with the driver cache against 27.5 s without ([measurements](#frame-rate-display-and-image-quality-october-15-second-session)).
- **P11 sustained performance.** A: defaults; B: `gow2_android_sustained_performance = true`.
  Use A/B/B/A from cool starts, the same two-minute view and quiet-window Mcycles/frame. Keep
  `SUSTAINED performance mode requested: ...`, sustained CPU/GPU clocks, thermal status and game
  frame rate. If unsupported, record that result; any steadier clock or frame-rate effect is to
  be measured.
  Result (October 15): not measured; cool A/B/B/A starts were unavailable on USB power ([measurements](#frame-rate-display-and-image-quality-october-15-second-session)).

## Not covered

The old checkpoint that crashes on load (`sub_8255E4A8` recursing over object `42C20140`, which
lists itself as its only child) was written by a build before the network fixes; new campaigns and
their checkpoints load. Nothing here changes it.
