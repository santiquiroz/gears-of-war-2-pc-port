# Native renderer, phase 1: skeleton and measurements

Phase 1 of [NATIVE_RENDERER_STUDY.md](NATIVE_RENDERER_STUDY.md) (issue
[#7](https://github.com/santiquiroz/gears-of-war-2-pc-port/issues/7)), after
[phase 0](NATIVE_RENDERER_PHASE0.md). Branch `native-renderer`; nothing here is in a release.

## Design change: a mode of the `xenos` plugin, not a new plugin

The study planned a separate `plume` plugin that hooks device creation and Swap and never runs the
command processor. Skipping the command processor means doing its side effects by hand: the game's
Direct3D waits on fences written by `EVENT_WRITE_SHD`/`MEM_WRITE` packets, on the ring buffer's read
pointer write-back and on interrupts. From the hooks that is a small command processor written again.

So phase 1 is SDK patch `0031-gpu-native-skeleton.patch`: `gpu_native` (off by default) keeps the
command processor reading every packet (fences, interrupts, swaps keep working) and skips `IssueDraw`,
which also runs the resolves. Nothing of the frame is emulated, and the screen is not drawn. A native
renderer then draws from the game's Direct3D calls (phase 0 named them) inside this mode. `plume` is not
needed while the SDK's own Direct3D 12 and Vulkan providers and presenter are reused.

## The ceiling (2026-10-21, RX 7800 XT, chapter 2 hospital, frame rate unlocked, no VSync)

| Mode | fps | p50 |
|---|---|---|
| Emulation, default path | 92-99 | 8.4-9.1 ms |
| `gpu_native=true` (no draws, no resolves) | 200-253 | 3.7-4.6 ms |

On this PC the GPU emulation takes about 60 % of the frame; the game itself runs at about 240 fps. The
Android numbers are still to measure (the phone was not connected; the APK with patch 0031 builds): set
`gpu_native = true` and `gow2_unlock_fps = true` in the phone's `gow2.toml`, travel to chapter 2, read
the `FRAMESTATS` lines.

## Where the emulation cost is: EDRAM ownership transfers

`tools/native/experiment-edram-transfers.patch` (applied by hand, not in the series) adds two switches
to the host render target path (`render_target_path_d3d12 = "rtv"`, the AMD and Android path):

| Run | fps | Picture |
|---|---|---|
| Emulation, `rtv` | 85-100 | The game (with the teal cast described below) |
| `gpu_edram_transfers=false`: no copies between render targets that share EDRAM | 150-153 | **Black** |
| `gpu_edram_skip_internal=true`: no copies only for draws with Direct3D's internal shaders | 106-108 | **Black** |

The copies Xenia makes when an EDRAM range changes owner cost about a third of the frame (99 to 152 fps
with the same draws), but the game needs some of them: without any, or without the ones before the
library's internal draws, the frame comes out black. Some internal draws only partly cover their
target or read what the copy brings (partial clears, restores).

## Phase 2, revised

Skipping a copy is only safe when the draw that follows overwrites the whole range without reading it:
it covers the render target, writes every channel, blends nothing, does not test against the old depth,
and its pixel shader does not discard pixels. The register state shows all of that except coverage,
which depends on the vertices. Direct3D knows it at call level: `Clear` (`sub_82981ED8`) receives the
rectangles, and phase 0 named the restore and resolve functions. Next steps:

1. Hook `Clear` and the internal draws at call level, log their rectangles and flags against the
   render target size, and mark full-surface overwrites.
2. Pass that mark to the render target cache with the draw (a packet the hook adds to the stream, or a
   per-draw side table keyed by the segment address), and skip the copy only for marked draws.
3. Compare screenshots with emulation in the hospital, Act 1 and a menu before measuring.

Each step stays behind a switch that is off by default.

## Also found: AMD's default path tints the hospital teal

With the default host render target path on AMD (`rtv`), the hospital has a strong teal cast at 16:9
too (also in the README screenshot from 2026-10-14); the ROV path does not. Which one matches the
console is not known here (it may be the colour precision of the host render target formats). The
widescreen test notes said `rtv` at 16:9 looked right; that was not checked against the screenshot and
is corrected in `docs/WIDESCREEN_AMD_TESTING.md`. A capture from a real Xbox 360 would settle it.
