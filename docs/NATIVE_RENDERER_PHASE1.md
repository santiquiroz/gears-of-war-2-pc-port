# Native renderer, phase 1: skeleton and measurements

Phase 1 of [NATIVE_RENDERER_STUDY.md](NATIVE_RENDERER_STUDY.md) (issue
[#7](https://github.com/santiquiroz/gears-of-war-2-pc-port/issues/7)), after
[phase 0](NATIVE_RENDERER_PHASE0.md). Branch `native-renderer`; nothing here is in a release.

## Design change: a mode of the `xenos` plugin, not a new plugin

The study planned a separate `plume` plugin that hooks device creation and Swap and never runs the
command processor. Skipping the command processor means doing its side effects by hand: the game's
Direct3D waits on fences written by `EVENT_WRITE_SHD`/`MEM_WRITE` packets, on the ring buffer's read
pointer write-back and on interrupts. From the hooks that is a small command processor written again.

So phase 1 is SDK patch `0032-gpu-native-skeleton.patch`: `gpu_native` (off by default) keeps the
command processor reading every packet (fences, interrupts, swaps keep working) and skips `IssueDraw`,
which also runs the resolves. Nothing of the frame is emulated, and the screen is not drawn. A native
renderer then draws from the game's Direct3D calls (phase 0 named them) inside this mode. `plume` is not
needed while the SDK's own Direct3D 12 and Vulkan providers and presenter are reused.

## The ceiling (2026-10-21, RX 7800 XT, chapter 2 hospital, frame rate unlocked, no VSync)

| Mode | fps | p50 |
|---|---|---|
| Emulation, default path | 92-99 | 8.4-9.1 ms |
| `gpu_native=true` (no draws, no resolves) | 200-253 | 3.7-4.6 ms |

On this PC the GPU emulation takes about 60 % of the frame; the game itself runs at about 240 fps.

Galaxy S25 Ultra (Adreno 830, Vulkan), same scene, `gow2_unlock_fps = true`, 100 seconds after the
travel, phone cooled between runs:

| Mode | fps | p50 | Battery temperature after |
|---|---|---|---|
| Emulation | 38-40 | 24.3-27.0 ms | 35.7 °C |
| `gpu_native=true` | 60, the phone's frame-rate limit | 16.6 ms | 36.5 °C |

On the phone the ceiling is at least 1.5x: without the GPU emulation the game reaches the 60 fps the
Android frame-rate hint allows (`present_android_frame_rate`), so the real ceiling is higher.

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

### Clears are not where the cost is

A hook on `sub_82981ED8` (`D3DDevice_ClearF`: flags in r4, rectangle in r5) shows 18 clears a frame in
the hospital, every one with the whole surface as its rectangle: colour (`0xF`) 4, depth (`0x10`) 5,
stencil only (`0x20`) 7, depth and stencil (`0x30`) 2. A stencil-only clear keeps the depth, which is why
skipping transfers for every internal draw lost the frame. The experiment's second version skips a
transfer only for an internal draw that replaces the whole target (all colour channels without blending;
an ALWAYS depth write with a full stencil REPLACE): about 90 fps against 90 without it. The transfers
before clears are cheap; the third of the frame that transfers cost is in other ownership changes (the
same EDRAM read with another format or pixel size). Counting the transfers by kind is the next step.

## The transfers came from a wrong estimate: patch 0033

`tools/native/spike-transfers.patch` (a temporary patch, not in the series) counts the transfers by
kind (`SPIKEXFER`: source and destination depth or colour, format, MSAA, tiles a second) and logs every
ownership change of a few frames in order (`SPIKESEQ`, with the depth control, colour mask, clip
control, window scissor and MSAA of the draw). Before the fix, in the hospital: 78,000 tiles of copies a
frame, 38 times the whole EDRAM (2,048 tiles).

The sequence showed why. Direct3D clears at 4x MSAA (a quarter of the pixels for the same memory), in
one draw without clipping and with the 8192 scissor. For such draws the EDRAM range comes from the
vertices only if `execute_unclipped_draw_vs_on_cpu` is on; the port had it off (Xenia's default is on),
so each clear claimed the whole EDRAM from its base, and the host render target path copied every other
target in there into the clear's target and back.

SDK patch `0033-unclipped-draw-extent-on-cpu.patch` (on master) restores Xenia's default:

| Run (hospital, unlocked) | Copies a frame | fps |
|---|---|---|
| RX 7800 XT, `rtv`, before | 78,000 tiles | 95 |
| RX 7800 XT, `rtv`, 0033 | 13,000 tiles | 114-116 |
| RX 7800 XT, `rov` (reference picture) | none | 50-55 |
| S25 Ultra, Vulkan, before | | 34-36 |
| S25 Ultra, Vulkan, 0033 | | 39-42 |

The copies were also what tinted the hospital teal on `rtv`: with 0033 the picture matches ROV at 16:9.
At 21:9 `rtv` keeps the pale rectangles over the light shafts (it emulates 2,048 tiles, ROV 4,096 since
patch 0026), so `auto` widescreen still picks ROV on AMD. The phone picture does not change.

## Direct3D's 4x clears drawn at 1x: patch 0034

What is left after 0033 is mostly the clear trick itself: per light, a depth clear at 4x then the
shadow map at 1x in the same EDRAM, and a stencil clear at 4x on the scene depth then the light volume
at 1x. Each is a copy into a 4x render target and a copy back. Ceiling tests with the same build
(temporary switches in the spike patch, wrong picture allowed), hospital, `rtv`:

| Run | fps |
|---|---|
| Emulation | 116-120 |
| No copies between targets that differ only in MSAA (black picture: the clears never reach the 1x targets) | 125-147 |
| No resolves | 137-141 |
| No draws (resolves still run) | 246-280 |
| The GPU's 3D engine during emulation | 89-95 % busy; no CPU thread above 84 % |

So the emulated frame is GPU-bound and the draws are most of it. (A first ceiling run reported "no gain"
for the MSAA copies; it ran with the previous DLLs, because `build-sdk.ps1` installs the DLLs in
`out/sdk-nodlss` and only `build.ps1` copies them next to `gow2.exe`.)

SDK patch `0034-msaa-fill-at-1x.patch` (branch only, `gpu_msaa_fill_at_1x`, off by default) removes
those copies correctly. A 4x pixel covers a 2x2 block of samples, the same EDRAM a 1x draw at twice the
size covers, and a fill with a constant value writes the same bits to every sample either way. So when
the vertex shader came embedded in the packet stream (`IM_LOAD_IMMEDIATE`, Direct3D's own clears), uses
no textures or memexport, and the draw is a fill (no blending on the colour it writes, no alpha to
coverage, no polygon offset, triangles or rectangles; in `kColorDepth` mode the pixel shader must be
embedded too, without textures, depth output or kill), the command processor rewrites the surface to 1x
at twice the pitch, doubles the viewport (plus half a pixel with Direct3D's pixel centres), the window
offset and the scissors, issues the draw, and restores the registers. The 1x render target that owns
the EDRAM keeps it; nothing moves to a 4x target.

| Run (hospital, RX 7800 XT, `rtv`, same build) | Copies a frame | fps |
|---|---|---|
| `gpu_msaa_fill_at_1x = false` | 13,000 tiles | 117-119 |
| `gpu_msaa_fill_at_1x = true` | 3,500 tiles, none between MSAA modes | 122 |

The picture matches. What is left are copies between colour and depth targets that reuse the same EDRAM
(for example a depth clear over the previous colour target), once per frame each.

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

## The rest of the gap: a buffer barrier pair per draw (master, SDK 0035)

With the copies gone the frame was still at half the `gpu_native` ceiling and GPU-bound. Two more
subtraction tests in the spike patch, same build, hospital, 720p:

| Run | fps |
|---|---|
| Emulation | 117-122 |
| Every draw clipped to one pixel (`SPIKE_TINY_SCISSOR`) | 166 |
| Guest memory uploads stopped once in the hospital (`SPIKE_SKIP_UPLOADS`, stale geometry) | 249 in the last window |

Pixels were only about 2 of the 8 ms. The rest followed the uploads: about 400 uploads and 740 barrier
batches a frame. Each draw whose vertex data changed moved the whole shared-memory buffer to
`COPY_DEST`, copied, and moved it back for reading, and each such pair waits for the GPU to drain. The
uploads were the SDK's `clear_memory_page_state`, on by default on the desktop: at every frame end it
marks every page the CPU uploaded as stale, so the whole working set went up again each frame (1 GB/s).
Android has had it off since patch 0007 (it halved the frame rate there too), and Xenia has it off.

The game meant to turn it off on integrated GPUs and to pick `resolution_scale` and
`anisotropic_override` per GPU in `OnPreSetup`, but none of that applied on Windows: those cvars
belong to `rexgpu-xenos.dll`, which loads after `OnPreSetup`, and `SetFlagByName` rejected the unknown
names silently. SDK patch `0035-runtime-defaults-for-late-cvars.patch` keeps such a value and applies it
at registration as the default (gow2.toml, `REX_*` and the command line still win, and `GetFlagSource`
reports their pending source), and the game now turns `clear_memory_page_state` off on every GPU.
Batching each upload with the rest of its 256 KB block also helped (122 to 198 fps) but loses to
turning the re-upload off (240, and 213 with both).

| Hospital, RX 7800 XT, unlocked (master a906596) | fps |
|---|---|
| 720p, before | 116 |
| 720p, `clear_memory_page_state = false` | 217-223 |
| 1440p (`resolution_scale = 2`, the dedicated-GPU default that now applies), before | 77 |
| 1440p, after | 126 |
| Guest uploads | 1 GB/s before, 37 MB/s after |

Chapters 1 (the opening cinematic), 4 and 8 look the same with the old and the new setting. On the
branch, `gpu_msaa_fill_at_1x` adds to it: 213 to 240 fps at 720p, 123 to 144 at 1440p.

Ten-minute soaks with the branch and `gpu_msaa_fill_at_1x = true` (scripted moving, aiming and firing, 720p, unlocked): chapter 2 averaged 254 fps, chapter 4 (the Derrick ride, outdoors) 171 fps; no errors after startup, no hangs, and the screenshots along the way look right.

Phone measurements need the order alternated: two identical runs back to back on the S25 gave 41-48
and then 35-40 fps, the second one warmer (37.1 against 39.5 °C), so a single A-then-B pair is noise.
Waiting for the battery to cool to 34.5 °C before each run and alternating the order (on, off, on, off), `gpu_msaa_fill_at_1x` averaged 43.2 and 40.8 fps against 41.4 and 40.0 off: about 3 %, always in its favour but inside the run-to-run spread. The phone is not limited by these copies; its frame is still about 40 fps against the 60 fps cap.

## Also found: AMD's default path tinted the hospital teal (fixed by 0033)

With the host render target path (`rtv`, the default on AMD) the hospital had a strong teal cast that
the ROV path did not. It was not colour precision: it was the copies described above bringing other
targets' contents into the clears' ranges. With 0033 `rtv` matches ROV at 16:9.
