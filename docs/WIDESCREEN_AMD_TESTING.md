# Widescreen on AMD: how to test it, and how to patch it

For whoever has AMD hardware (an RX 7800 XT and a Ryzen iGPU were offered). Written by the author of the widescreen PR, who
only has NVIDIA, so every statement about AMD below is a hypothesis to confirm or refute, and every statement about
the code is something to check against the tree.

## 1. The problem in one paragraph

Widescreen (`docs/WIDESCREEN.md`) widens the game's back buffer from 1280x720 (704 lines with the AO fix) up to 1680x704. Above 1280 wide
the game's own layout of the Xbox 360's 10 MB EDRAM reaches past tile 2048: it sets 12-bit render target bases for its half-resolution
passes (smoke, particles, the light shafts of the lighting pass), up to tile 2574 at 1680. The console's 11-bit addressing wraps those onto
tile 0, which is where the scene depth lives, so everything that depth-tests afterwards reads garbage: hard-edged pale rectangles and horizontal bands
in lit scenes (the hospital, chapter 2, shows it clearly), a thin strip at the top at 1360 and 1440.

SDK patch 0026 fixes it **only on the ROV path** (`render_target_path_d3d12 = "rov"`, the default on NVIDIA and Intel): that path now emulates
4096 tiles. The host render target path (`"rtv"`, the SDK's default on AMD) still emulates 2048 and shows the artifact above 1280 wide. It was reproduced on v0.2.4 plus these PRs with `render_target_path_d3d12 = "rtv"` forced on NVIDIA (hospital, chapter 2, 21:9: a hard-edged pale
rectangle where the soft light shafts should be); it has **not** been seen on AMD hardware. The widescreen setting defaults to `auto`, which turns it on for any display
wider than 16:9, so an AMD user on an ultrawide monitor gets the artifact until this is decided.

Why `rov` is not simply made AMD's default: the original authors recorded in 2021 that AMD's shader compiler crashed on ROV pixel shaders. Nobody
on this PR has tested a recent driver. That is the first thing to find out.

## 2. What to find out (in this order)

| # | Question | Why it matters |
|---|---|---|
| A | Does the game run on AMD with `render_target_path_d3d12 = "rov"` at the normal 16:9 (`gow2_widescreen = "off"`)? Crash, hang, black or wrong picture, compile time? | Separates "ROV on AMD" problems from widescreen problems. If A fails, nothing else below is possible with ROV |
| B | With `rov` and widescreen on, is the picture right at 1360, 1440, 1520, 1600 and 1680 wide? | Confirms patch 0026 on a second vendor |
| C | With the default (`rtv`) and widescreen on, what exactly is wrong at each width? | Defines what a fix to the host-render-target path has to repair |
| D | Cost: GPU time per frame and hitches of `rov` against `rtv` at 16:9, same scene | On NVIDIA ROV halved the GPU time at `resolution_scale = 2`; on AMD it could be the opposite (ROV is slow on some hardware) |

## 3. Setup

* Build or download v0.2.4 plus the widescreen PR (and, for D, the performance PR). Use `gow2.toml` next to `gow2.exe`; change one setting per run.
* Settings for every run (add to `gow2.toml`):

```toml
gow2_frame_log = true        # FRAMES lines: frame time, and GPU clock on Android
d3d12_stats_log = true       # D3D12STATS lines every five seconds: draws, draws skipped, pipelines and shaders built
log_file = "run_<tag>.log"   # one log per run, named after the tag in the table below
gow2_widescreen = "1.93"     # see the width table
render_target_path_d3d12 = "rov"   # or "rtv"; leave unset to get the vendor default
```

* The back buffer width is `round(aspect x 704 / 80) x 80` (704 lines because `gow2_ao_upscaling_fix` is on by default), held between 960 and 1680:

| `gow2_widescreen` | Width | EDRAM tiles the game's layout needs (2048 on the console) |
|---|---|---|
| `off` | 1280 | 1936 (fits: control run) |
| `1.93` | 1360 | 2090 |
| `2.05` | 1440 | 2178 |
| `2.16` | 1520 | 2332 |
| `2.27` | 1600 | 2420 |
| `2.39` (or `21:9`) | 1680 | 2574 |

  The window can be any size; `gow2_widescreen` with an explicit aspect does not depend on the monitor. Set `video_mode_width` / `video_mode_height` yourself only
  if you want to override the window.
* Keep `resolution_scale` at its default for A to C, and report it. Do the performance runs (D) at the default and at `resolution_scale = 2`.
* Start each run from a fresh `cache_root` or note that the driver shader cache was warm: the first visit to an area compiles pipelines and can freeze for a fraction of a second.

## 4. Scenes and what to look for

1. **Chapter 2, the hospital entrance (first area after the checkpoint).** The lighting pass draws soft light shafts and haze from the ceiling windows. Correct: soft,
   gradual, no straight edges. The bug: a pale rectangle with hard edges, or horizontal bands cut off partway, and at 1360/1440 a strip along the top rows.
2. **Chapter 4, the outdoor assault.** Smoke and particles are the half-resolution passes; look for flat blocks or missing smoke.
3. **Any in-engine cutscene** (for example chapter 1): the same passes at a different camera.
4. **Pause menu, title screen and main menu:** the bars and the grey dimming must span the whole picture (a separate UI fix); report if not.
5. For D: walk the same route for 60 seconds in the hospital and in chapter 5 (Landown, large outdoor), with the log on, once per setting.

Take a screenshot per scene and width (Windows Snipping tool is enough; the Print Screen key of the game window is fine) and name it `<scene>_<width>_<path>.png`.
Do not attach screenshots of the game to a public issue if you are unsure about content rules; describe the artifact instead, or send them privately.

## 5. Run matrix

| Tag | `gow2_widescreen` | `render_target_path_d3d12` | Answers |
|---|---|---|---|
| a1 | `off` | `rov` | A |
| a2 | `off` | `rtv` | baseline for A and D |
| b1 | `1.93` | `rov` | B |
| b2 | `2.05` | `rov` | B |
| b3 | `2.39` | `rov` | B |
| c1 | `1.93` | `rtv` | C |
| c2 | `2.05` | `rtv` | C |
| c3 | `2.39` | `rtv` | C |
| d1 | `off` | `rov`, `resolution_scale = 2` | D |
| d2 | `off` | `rtv`, `resolution_scale = 2` | D |

Skip the b and c rows that depend on a failed row (if a1 crashes, say so and stop with ROV).

## 6. What to report

Per run: tag, GPU model and driver version, Windows version, `resolution_scale`, whether the driver cache was warm, and

* **Crash or hang:** when (shader compile at start, entering a scene, a particular effect), the last 30 lines of the log, and the AMD driver event if there is one.
* **Picture:** which scene, which width, a one-sentence description or a screenshot, and whether the artifact moves with the camera.
* **Performance (D):** from the `FRAMES` lines the median and 99th-percentile frame time and the number of frames over 40 ms; from `D3D12STATS` the pipelines built and draws skipped.
  If a pipeline compile freezes the game, the `SLOWPIPE` lines (when the performance PR is in) give the time and the shader hashes.
* A one-line verdict per question A to D.

A table like this is enough:

```text
tag  GPU / driver        scale  scene       width  result
a1   RX 7800 XT 25.x.x   1      ch2 start   1280   ok / crash at start / artifact ...
```

## 7. Suggestions for patching, from cheapest to most work

**1. Policy only (a few lines, no SDK change).** Make `auto` turn widescreen on only when the render target path is ROV (explicit `gow2_widescreen = "21:9"` still works, with a
log line saying the artifact is expected on `rtv`). `PrepareWidescreen` runs from `OnPreSetup`; the effective path is `render_target_path_d3d12` when set, otherwise the SDK's vendor choice (patch
0024 makes that NVIDIA and Intel, AMD stays `rtv`), so the app can compute it from the cvar and the GPU it picks (`PickRenderGpu` already knows the vendor). Documents the limit honestly and costs nothing.
Use this if A or B fail on AMD.

**2. Make AMD use ROV (one condition in `render_target_cache.cpp`, as patch 0024 does for NVIDIA), if A, B and D pass.** Keep the existing fallback to host render targets when the GPU reports no ROV support,
and keep `render_target_path_d3d12 = "rtv"` as the escape hatch. If only some drivers crash, key the choice on the driver version rather than the vendor.

**3. Teach the host render target path about 4096 tiles (the real fix for `rtv`; large).** Patch 0026 is the template: read it as the list of places that assume 2048 tiles. For the host path the
equivalent places should be:
* the tile count and the 11-bit field widths of the render target key and of the base used for ownership tracking (the ownership ranges are sized by `xenos::kEdramTileCount`; patch 0026 adds
  `xenos::kEdramVirtualTileCount`);
* the EDRAM buffer that the transfer and resolve paths use as intermediate storage (twice as large: 20 MB times the resolution scale squared), and its UAV/SRV descriptors;
* the transfer shaders that copy between host render targets when two targets overlap in EDRAM, and the resolve shaders, which address 2048 tiles from the start of the view they are given
  (patch 0026 gives them an extra view of the second half of the buffer; a resolve that also clears such a target swaps tiles through scratch buffers because the clear shaders write through the first half's view);
* every place the base register is masked to 11 bits (`RB_COLOR[0-3]_INFO`, `RB_DEPTH_INFO`; the registers carry 12 bits).
Test it against the ROV path, which is now correct on NVIDIA: render the same frame with both and compare. Put it behind a cvar until it matches.

**4. Things that do not work, to save time.**
* *Remapping the half-resolution passes below tile 2048.* The scene depth must stay alive for their depth tests, and the layout needs 2574 tiles at 1680 (2090 at 1360), so there is
  no free range to remap to.
* *Skipping the half-resolution draws or the haze draw's depth test.* This removes the artifact at 1440 and 1680 (it is how the cause was found) but also removes the smoke, particles and shafts.
* *Capping the width at what 2048 tiles hold.* That is 1280, i.e. no widescreen at all; a horizontal-FOV-only widening at 1280 wide would stretch.

## 8. Contact

Open an issue or comment on the widescreen PR with the table from section 6. Mention the commit or release you tested.
