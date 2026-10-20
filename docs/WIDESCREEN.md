# Ultrawide (Hor+) rendering

## TL;DR

It is on by itself (`gow2_widescreen = "auto"`) when the main display is wider than 16:9. Set `"off"`, or an aspect such as `"21:9"` or `"2.39"`, in `gow2.toml` and restart to change that. The
game then draws a really wider picture: you see more to the left and right, nothing is stretched, and the
black bars of a 16:9 picture on a 21:9 monitor are gone. On a 16:9 or narrower display nothing changes.

```toml
gow2_widescreen = "auto"     # auto (default) | off | 21:9 | 16:10 | 2.39 ...
```

| | |
|---|---|
| What you get | A back buffer up to 1680x704 (21:9). The vertical view stays the one of the 16:9 game; the horizontal view grows. |
| Cost | Frame time unchanged (60 fps, p99 19.3-19.5 ms in two test scenes). GPU time per frame about 13-15 % higher. |
| Cap | 1680 pixels wide. A 32:9 screen gets 21:9 with side bars. |
| Side effect | The shadow map is made smaller (800 -> 480 px) so it fits the console's 10 MB EDRAM next to the wider picture. The ROV render target path (NVIDIA, Intel; AMD with `render_target_path_d3d12 = "rov"`) emulates 20 MB for the game's half-resolution passes. |
| Not covered | Videos (stay 16:9), HUD and menu placement were only partly checked (see "What does not work"). |

## How it works

The game was built for 16:9 at 1280x720 (704 lines with the AO upscaling fix). Three things fix it there:

1. **Back buffer size.** `sub_82806A80` computes the width as `720 x aspect` and clamps it to 1280 (the `cr6.lt`
   branch at `0x82806E64`). Patch id 7 turns the aspect into `(W + 0.25) / 720` for the wanted width W and lifts
   the clamp. W is a multiple of 80, because an EDRAM tile is 80 pixels wide at 32 bits per pixel.
2. **Field of view.** The camera code multiplies a field of view by a constant; the per-frame main scene one is
   at `0x8252D750` (site 4). The patch widens the half angle as `atan(tan(half) * W / 1280)`, which keeps the
   vertical angle of the 16:9 game (Hor+). Other sites (cutscene cameras at `0x82634F44`, `0x82634CC8`) use the
   same function. Sites 3, 5, 6, 7 are patched too but never fired in the scenes tested; they are harmless.
3. **Presenter.** `PrepareWidescreen` (called from `OnPreSetup`) sets `video_mode_width/height` to the new
   buffer size unless you set them yourself, so the window shows the frame at the right aspect.

### The EDRAM limit

The Xbox 360 draws into 10 MB of EDRAM (2048 tiles of 80x16 samples) and the recompilation emulates that. Scene colour plus depth use
`8 x W x H` bytes: 9.5 MB at 1680x704. The game's 800x800 shadow map (32-bit depth, 2.4 MB) then overlapped
the scene and left dark rectangles on characters. `gow2_system_settings` edits in the Coalesced bundle
(`ResX`, `ResY`, `MaxShadowResolution`) now size the shadow map to what is left: 480 px at 1680x704.

**The half-resolution passes need more than that.** After the lighting the game draws its smoke and particles at half size into three more
surfaces (a depth buffer and two colour targets, each half the scene's size) placed behind the scene colour and depth, and
it sets their render target bases as 12-bit tile numbers: at 1680x704 the last one starts at tile 2332 and the layout ends at tile 2574 (`EdramTilesNeeded` in
`src/gow2_widescreen_math.h`). The console's 11-bit addressing wraps those past tile 2048 onto tile 0, which is where the *scene depth* lives, so
everything that tests against it afterwards read garbage: the translucent light shafts and haze of the lighting pass came out as hard-edged
rectangles (hospital, chapter 2) or were cut off in horizontal bands. The layout needs 1936 tiles at 1280 (it fits), 2090 at 1360, 2178 at 1440, 2332 at 1520,
2420 at 1600 and 2574 at 1680, so the tiles past 2048 landed on tile 0 onwards: tiles 0-41 at 1360 (the top two or three rows of the picture), 0-129 at 1440
(the top seven rows: the haze above y = 112 was missing), 0-283 at 1520, 0-371 at 1600 and 0-525 at 1680 (the top 25 rows, 400 px). Skipping the half-resolution
draws, or turning the depth test of the haze draw off, removed the artifact at 1440 and 1680.

The SDK now honours bit 11 of those bases (patch `sdk/patches/0021-edram-virtual-tiles.patch`): the pixel-shader-interlock (ROV) render
target path, which NVIDIA and Intel GPUs use by default, emulates 4096 tiles (20 MB, times the resolution scale squared). Draws address all of
them, and a resolve of a target in the second 2048 tiles reads them through extra views of the second half (the resolve shaders only
address 2048 tiles from the start of their view). 4096 tiles hold up to 2640x704.

**Host render targets (`render_target_path_d3d12 = "rtv"`, the default on AMD) still emulate 2048 tiles** and show the artifact above 1280. How to test it on AMD and how to patch it: [WIDESCREEN_AMD_TESTING.md](WIDESCREEN_AMD_TESTING.md). On AMD set
`render_target_path_d3d12 = "rov"` in `gow2.toml` for widescreen, or `gow2_widescreen = "off"`.

Wider than 1680 is still held at 1680: the width limit is now the game's own layout (2640 at most), not the EDRAM, but nothing wider
was tested, and the shadow map has to keep shrinking as the scene grows.

## Settings

| Setting | Meaning |
|---|---|
| `"auto"` (default) | The aspect of the main display (Windows only) when it is wider than 16:9; otherwise the game's own 16:9. |
| `"off"` | The game's own 16:9. |
| `"21:9"`, `"16:10"`, `"2.39"` | A fixed aspect, accepted between 1.2 and 4. |

Widths are rounded to a multiple of 80 and held between 960 and 1680. Needs a restart. `video_mode_width` and
`video_mode_height` set by you win.

## Performance (hospital_walk and outdoor_assault, 2 runs each, 21:9 vs the 16:9 ROV baselines)

| Scene | GPU p50 16:9 | GPU p50 21:9 | Frame p99 |
|---|---|---|---|
| hospital_walk | 6.45 ms | 7.40 ms (+15 %) | 19.3 ms (unchanged) |
| outdoor_assault | 10.75 ms | 12.15 ms (+13 %) | 19.5 ms (unchanged) |

The pixel count grows by 31 %, the GPU time by about 14 %, and the game stays at 60 fps. The perf tool flags
`gpu_ms.p50` as WORSE (its threshold is 10 %) and 0.5-1.0 hitches per minute in outdoor_assault; the frame pacing
itself did not change.

## Menus and the unlock toast (found after the first release)

**Menus were drawn at (1280 / width) squared of their size** (0.58 at 1680 wide) in the main menu, the pause menu and the
title screen. The game's UI scene client (`GearGameUISceneClient`) keeps a canvas-to-screen matrix at offset 112:
`m00 = 1.8182 / aspect`, `m11 = -1.8182`, and a perspective distance D in the last word of its translation row (+172).
For a canvas pixel to be one screen pixel D must be `|m11| * height / 2` (640 at 704 lines). The game computes
`D = (width / 2) / m00`, which is that only at 1280 wide, so at 1680 wide everything shrank by the square of 1280 / 1680.
`src/gow2_widescreen.cpp` (`FixUiProjectionTick`, an engine tick listener) puts D back, and the matching depth word,
whenever the game rewrites them. `gow2_ui_projection_fix = false` turns it off. Measured with the pause menu's line
pitch at five widths (47, 42, 37 and 27 px at 1280, 1360, 1440 and 1680, matching `47 * (1280 / width)^2`).

**The unlock toast stayed on screen.** The in-game "NEW ITEM UNLOCKED" panel (`GearUIScene_Toast`) plays a slide-in, hold and
slide-out animation whose positions are pixels of the 1280-wide design; at 1680 wide the slide-out ends at x = 1280, still
on screen, so the panel stays until the next toast. `src/gow2_toast.cpp` keeps it for `gow2_toast_seconds` (default 3, a
0.4 s fade at the end) and then hides the scene through its opacity until the next toast; 0 keeps the game's own timing.
At 16:9 it also shortens the toast (about 7 seconds before). A 0.25 s fade-in hides the slide-in, which also starts at
x = 1280, in the middle of an ultrawide picture. The panel's resting place is a percentage of the UI's safe region, so it
is where the game puts it at 16:9 (about half way across, not at the right edge).

**Full-width bars and the pause dimming covered only the first 1280 pixels.** The title and pause bars, the pause menu's grey
dimming and the main menu backdrop are `UIImage` widgets authored 0 .. 1280 pixels wide (`Position.Value[2]`, at +68 of the
widget, is 1280.0; Left, at +60, is 0). `WidenUiImagesTick` in `src/gow2_widescreen.cpp` sets Right to the back buffer width
and Position.bInvalidated[2] (+82) so the engine resolves it again. It walks the object table when it grew, and every 120
ticks. Found by dumping the vertex constants of the title's solid-fill quads (the matrix maps 1280 canvas pixels to 76 % of
1680) and then listing the UI objects holding a 1280.0 float. `gow2_ui_widen = false` turns it off. Not checked: other
aspect ratios than 21:9, lobby and war-journal scenes.

## What does not work, or was not checked

- **Videos** (Games for Windows logos, pre-rendered cutscenes) are 16:9 and are shown as they are.
- **HUD and menus.** The title screen, main menu and pause menu are checked (see above). The in-game HUD (ammo, health) did not show in my
  captures, and the pause menu was not captured at 21:9, so their placement is **not verified**. I have not looked at where they land.
- **Wider than 1680.** Held at 1680 (see above); not tested.
- **Shadows** are lower resolution (480 px instead of 800 px) while widescreen is on.
- **DLSS / upscaler** combinations were not tested with widescreen on. AMD and Intel GPUs untested.
- `auto` was checked on this 3440x1440 monitor (same picture as `21:9`).
- **Other aspects** (16:10, 4:3) are covered by the arithmetic test only, not by screenshots.

## Tests

`tools/tests/widescreen_math_test.cpp` (header `src/gow2_widescreen_math.h`) checks the aspect parser, the
width rounding and caps, the shadow size and the field-of-view arithmetic. It runs with `scripts\test-host.ps1`.
