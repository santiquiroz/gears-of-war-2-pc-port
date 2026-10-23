# Native renderer, phase 2: native surfaces and native resolves

Phase 2 of the [design](superpowers/specs/2026-10-22-native-renderer-design.md), following the plans
[2a](superpowers/plans/2026-10-22-native-renderer-phase-2a.md) and
[2b](superpowers/plans/2026-10-22-native-renderer-phase-2b.md). Branch `native-renderer`; everything is
off by default.

## Switches

| Switch | Patch | What it does |
|---|---|---|
| `gpu_native_surfaces` | 0036 | Render targets are host surfaces: EDRAM ownership copies between them are dropped except depth to depth, each drop counted (`NATIVESURF dropped ...` every 5 s). Forces host render targets (D3D12 and Vulkan) and Direct3D's 4x clears at 1x (0034). |
| `gpu_native_resolves` | 0037, 0038 | With `gpu_native_surfaces`: after a resolve, the textures that view its destination compatibly and were up to date before it get the resolved rectangle straight from the host render target (a copy for bit-compatible pairs, `native_resolve.cs.hlsl` for biased, swapped and depth pairs) and stay up to date, so the texture cache does not reload them. Fallbacks counted (`NATIVERESOLVE ...`). |

## Which copies the game needs (2026-10-22, RX 7800 XT)

Dropping every ownership copy (with the clears at 1x) leaves the chapter 2 hospital right, but chapter 4
shows ghost geometry with a grid pattern: depth read across the two depth formats at the same base.
Keeping only depth-to-depth copies removes it. In the hospital the mode then drops, per frame, about
3,000 tiles of colour-to-depth, depth-to-colour and colour-to-colour copies (ten kinds, the largest
`C0->D0`, `C3->C0` and `D0->C3`, about 700 tiles each), with the same picture.

## Numbers (hospital, 720p internal, unlocked, same build, other jobs running on the PC)

| Mode | fps | Texture loads / 5 s |
|---|---|---|
| Emulation with the 1x clears (`gpu_msaa_fill_at_1x`) | 190 | 17,400 |
| `gpu_native_surfaces` | 191 | 17,950 |
| `gpu_native_surfaces` + `gpu_native_resolves` | 211 | 14,460 |

Native resolves write three textures a frame directly (16_16 float, the 8888 front buffer with red and
blue swapped, and the D24FS8 depth); the others fall back: other views of the same scratch address
(`texture_incompatible`, the expected case), one resolve with two source render targets, and one
`k_16_16` resolve. The picture matches (title and hospital compared by eye; the title is animated, so
its numeric difference is not meaningful).

## Widescreen on host surfaces (phase 3, patch 0039 and the 0036 rule)

At 21:9 the game places render targets past EDRAM tile 2048 (`color_base_bit_11` / `depth_base_bit_11`);
the host render target path read 11-bit bases, so a target at 2048+N landed on N. With
`gpu_native_surfaces`, 0039 reads 12-bit bases and sizes the ownership map at 4096 tiles. That alone
left a hard edge over the light shafts; keeping every ownership copy removed it, and bisecting the
dropped kinds found the one the light shafts read: `k_16_16` colour into depth at the same base
(`C4x1->D0x1`). Keeping that kind too (0036's `KeepTransfer`, with a test) and dropping the rest
(keeping `C6x1->C4x1` instead smears the motion blur):

| Hospital, 21:9 (1680x704), RX 7800 XT | Picture | fps |
|---|---|---|
| ROV (the reference, today's AMD default for widescreen) | Right | 129 |
| Host render targets, emulation | Pale rectangles over the light shafts | 91 (earlier build) |
| Native surfaces + native resolves | Matches ROV | 192 |
| Native surfaces + native resolves, 16:9 | Matches | 243 |

## Depth resolves and chapter 4 (patch 0043)

Chapter 4 with native resolves showed white distant trees and ground and striped motion blur; with
native surfaces alone it matched. The game reads its D24FS8 depth back from a `k_24_8_FLOAT` texture
(distance fog, motion blur), and the host render target keeps guest float24 depth [0, 2) as [0, 1), so
the native write handed the game half the depth. 0043 doubles it (`write.scale = 2` for D24FS8, the
shader's depth mode now applies the scale) on D3D12 and Vulkan. Chapter 4 then matches ROV, with smooth
motion blur. Chapters 1 and 8 matched before the fix.

## On by default on AMD (game cvar `gow2_native_renderer`)

`gow2_native_renderer` (`auto` by default, `on`, `off`) sets `gpu_native_surfaces` and
`gpu_native_resolves` unless the player set them. `auto` turns the native renderer on for AMD GPUs on
Windows when the player has not chosen `render_target_path_d3d12`; with it on, automatic widescreen
keeps the host render targets instead of switching AMD to ROV. NVIDIA and Intel stay on their current
paths until someone measures them. Checked 2026-10-22 on the RX 7800 XT at the shipped
`resolution_scale` 2: the log says `gow2 native renderer on`, the hospital matches ROV, and native
resolves are written.

Shipped settings (`resolution_scale` 2, unlocked, hospital, RX 7800 XT, 2026-10-22, all patches through
0045):

| Mode | 16:9 fps | 21:9 fps | Picture |
|---|---|---|---|
| Native renderer (the default on AMD now) | 224 | 211 | Matches ROV at both aspects |
| Host render targets, emulation (`gow2_native_renderer = "off"`, `rtv`) | 202 | — | Artifacts above 1280 wide |
| ROV (the AMD widescreen default before) | 86 | 80 | The reference |

Soaks with the native renderer (10 minutes of scripted movement, aiming and firing, 1600x900,
`resolution_scale` 1, unlocked): chapter 4 mean 186 fps, chapter 2 mean 211 fps, no faults or GPU errors.

## Android (Vulkan): on by default

Galaxy S25 Ultra, v0.3.0 APK, chapter 2 hospital, frame rate unlocked, runs alternated and started below 34.5 °C
(2026-10-23):

| Run | Native | Emulated |
|---|---|---|
| Round 1 | 46.3 fps (p95 22-28 ms) | 39.3 fps (p95 24-32 ms) |
| Round 2 | 43.6 fps (p95 24-25 ms) | 40.9 fps (p95 25-27 ms) |

The native path draws the hospital correctly and is about 12 % faster with a lower p95, so
`gow2_native_renderer = "auto"` now turns it on on Android too (unless the player sets `render_target_path_vulkan`).
Checked on the phone with a `gow2.toml` holding only `net_link_active`: the log says `gow2 native renderer on`.

## Next


- Resolves that still reload (counted, correct): `k_16_16` colour (once a frame), resolves that span
  two render targets (`multiple_sources`, about once a frame), and the other views of a resolved address
  (`texture_incompatible`, expected). Scaled resolves (0041), the HDR scene colour with packed mips
  (0044) and `k_16_16_FLOAT` at `resolution_scale` 2 (0045) are written natively.
- Vulkan (phase 4): 0040, 0041, 0043 and 0045 compile in the Android build but have not run on a phone
  yet; the S25 run (cool-gated, alternated, `vulkan_android_stats_log`) decides whether Android turns it
  on by default.

## Phase 5 sketch: hooks for mods (decided, not built)

Native resolves give the frame natural pass boundaries with host textures behind them: in the hospital,
the HDR scene colour (`k_2_10_10_10_FLOAT` render target resolved with a -3 exponent bias into a
`k_16_16_16_16_FLOAT` texture), the 16_16 float buffers of the post-processing chain, and the final
8888 front buffer. The hook is a small SDK interface called after each native resolve with the
destination address, format, size and the host texture; a registered effect may run its own pass on
that texture before the game samples it. A first example effect runs on the HDR scene texture (so the
HUD, drawn later, is untouched). Everything stays behind a switch and off by default.
