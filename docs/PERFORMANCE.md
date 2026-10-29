# Performance: where the frame goes, and the plan

Status: 2026-10-28 (v0.3.8). This page answers two questions players ask (issue
[#24](https://github.com/santiquiroz/gears-of-war-2-pc-port/issues/24)): why the port needs more GPU
than "Gears of War 2: Hollow" on the same PC, and what can still be cut on PC and on Android. Every
number here was measured on the hardware named next to it; every cause cites the code it comes from.
The code study behind sections 3 and 4 read the SDK with all of this repo's patches applied; its line
numbers refer to that tree (`out\sdk-build\rexglue-sdk` after `scripts\build-sdk.ps1`).

## 1. Short answer

- **The game code is not the problem.** It is the retail Xbox 360 code compiled to native x64 (or
  arm64). With the GPU work switched off (`gpu_native=true`, which draws nothing), the chapter 2 hospital
  runs at 200-253 fps on a Ryzen 9 7900X3D, and a Galaxy S25 Ultra reaches its 60 fps display limit
  ([NATIVE_RENDERER_PHASE1.md](NATIVE_RENDERER_PHASE1.md)).
- **The cost is the GPU side.** Hollow is Unreal Engine 3's PC renderer: shaders compiled for PC by
  Epic, drawn straight through Direct3D. This port runs the console game's own renderer, so every draw
  still passes through a model of the Xbox 360 GPU (its command stream, registers and formats), and
  every shader is Xbox 360 GPU code translated to DXIL or SPIR-V, carrying extra work to behave like
  that GPU.
- **Most of the old emulation overhead is already gone.** The native renderer (0.3.0, 0.3.4) keeps the
  game's render targets as host textures, writes resolves straight into textures, and takes each draw
  from the game's Direct3D calls instead of the console's command stream.
- **The biggest single cost on PC was the native front end, and 0.3.8 turns it off there.** Turned on by
  default in 0.3.3, it snapshots the game's Direct3D state for every draw while the command processor
  still reads the console's command stream, so today it adds work instead of replacing it: 151 fps with it,
  263 without, same picture (hospital, 1440p internal, RX 7800 XT). With it off, the full frame on that PC
  runs at the level the game code reaches with no drawing at all. 0.3.10 turns it off on Android too:
  players on a Mali-G615 phone report far fewer stutters with it off (#27).
- **What is left:** CPU time per draw in the command processor (busy scenes, slower CPUs), render passes
  and memory traffic (phones), and heavier translated shaders (both). Section 5 ranks the fixes. Shaders
  specialised per draw and fewer render-pass breaks are the two big ones on Android; the cost per draw is
  the big one on PC.
- **Reaching Hollow's GPU load is not realistic** without replacing the game's shaders (which a
  translation cannot do). Halving today's cost per draw on PC and making phones hold their cool-start
  frame rate are realistic goals.

## 2. Measured today

### PC

| Hardware | Scene | Setting | Result |
|---|---|---|---|
| Ryzen 9 7900X3D + RX 7800 XT | Chapter 2 hospital, 1440p internal, unlocked | 0.3.8, native renderer, front end off (PC default) | 262-263 fps, Direct3D 12; Vulkan 248 fps |
| Same | Same | 0.3.3-0.3.7 default (native front end on) | 150-165 fps (0.3.8 with the front end on: 151) |
| Same | Same | 0.3.2 | 250-261 fps |
| Same | Same, 0.3.0 (October 22) | Native renderer | 224 fps (ROV path: 86; emulated host targets: 202) |
| Same | Same, 720p internal, 16:9 | Native renderer | 243 fps; the game alone (`gpu_native`, no draws) 200-253 |
| i7-4771 + GTX 1070 (player report, #24) | Hospital, 60 fps limit | Emulated (EDRAM) | 57 fps, GPU 100 % |
| Same | Same | Native | 60 fps, GPU 55 % (Hollow on the same PC: about 10 %) |
| RX 7900 XT (player report, #26) | Act 1, near the gas canisters, about 4,300 draws a frame | Native | 117 fps in calm areas, about 50 there |

The 7900 XT drop is about 20 ms for 4,300 draws, roughly 4.6 µs per draw: a CPU cost per draw on the
GPU command thread (section 3.1), not pixels. That report was on 0.3.7, with the native front end on.

### Android (Galaxy S25 Ultra, Adreno 830, 720p internal, frame rate unlocked)

| Build | Result |
|---|---|
| v0.3.1, native renderer | 52.9 fps from a cool start (32 °C battery), about 37 fps warm (38 °C); 1,100 draws and 53 render passes a frame; GPU 95-99 % busy |
| v0.3.4 (0054: uploads by 256 KB block) | Render passes 55 to 26 a frame, GPU work about 7 % lower; 46.7 fps cool |
| Before the native renderer (October 15) | 26-29 fps for the first 40 s from cool, then down with the thermal cap to 9-13 fps at 222 MHz |

Two things limit the phone: GPU work per frame, and heat. The GPU clock falls from 1,200 MHz to
342-525 MHz within minutes ([ANDROID_EXPERIMENTS.md](ANDROID_EXPERIMENTS.md)), so a change that only
raises the cool-start peak does not raise the warm frame rate. Halving the scene resolution in each
direction (a quarter of the pixels) cut only about 25 % of the GPU work: most of the cost is per draw and
per pass, not per pixel.

## 3. Where the time goes

### 3.1 PC: CPU time per draw on the GPU command thread

With the native front end on (the PC default from 0.3.3 to 0.3.7, the Android default until 0.3.9), for each
draw the game-side hook (`src/gow2_native_frontend.cpp`) snapshots the guest Direct3D device
state, about 2.5 KB, plus the constant blocks the draw changed, and the SDK's command processor turns that
snapshot back into Xbox 360 GPU registers:

- **2,415 dword compares per draw** against the register file, 2,048 of them the shader constants reached
  through 128 block pointers (`src/graphics/command_processor.cpp:1341-1364`), and a virtual
  `WriteRegister` call for each changed dword. The game side already shares unchanged constant blocks
  between snapshots (copy-on-write), so an unchanged pointer means unchanged contents, and most of that
  work could be skipped.
- **Constant uploads:** when a used constant changes, the whole used set is gathered and uploaded again;
  a shader that indexes its constants dynamically (UE3's skinning does) uploads all 256 (4 KB per stage per
  draw).
- **Vulkan only:** the texture and sampler descriptor sets are written again for every draw
  (`src/graphics/vulkan/command_processor.cpp:7692-7695`, with a TODO in the SDK).
- **Game side,** about 1-2 µs per draw: two 2.5 KB copies of the snapshot (one could be avoided), 366
  byte-swapped loads of the device state, and 2-3 lock acquisitions.

### 3.2 PC: occlusion queries that empty the GPU

The game asks the GPU how many pixels of an object passed the depth test, to skip drawing hidden
objects. On Direct3D 12 the end of every such query closes the command list and waits until the GPU has
finished all of its work (`src/graphics/d3d12/command_processor.cpp:5084-5092`, `CheckSubmissionFence`),
so the CPU and the GPU take turns instead of overlapping. Android already runs with
`occlusion_query_enable = false` (it cut GPU work by 6-12 % on the S25); the PC kept it on.

Measured on PC in 0.3.8 (hospital, 1440p internal, RX 7800 XT, two alternated rounds, front end on): 123
and 149 fps with the queries, 143 and 147 without, the same picture. That is inside the run-to-run spread,
so in this scene the drains do not cost a measurable amount, and the PC keeps the queries on. A scene with
many more queries could still show it (item 14).

### 3.3 Android: render passes and memory traffic

A phone GPU renders the screen in small tiles kept on chip; every render pass end writes the tiles to
memory and every start reads them back. In this renderer:

- **every render target is loaded and stored at every pass** (`LOAD_OP_LOAD` and `STORE_OP_STORE`,
  stencil included); there is no `CLEAR` or `DONT_CARE` anywhere for the game's targets
  (`src/graphics/vulkan/render_target_cache.cpp:1570-1660`). Clears are drawn inside a pass that has just
  loaded the target;
- **the render area is always the whole target**, even for a pass that touches a corner;
- **every guest memory upload, texture load and resolve ends the pass** (26 passes a frame in the
  hospital after 0054). At 720p one pass break moves up to about 15 MB (store and reload of colour and
  depth), less with Adreno's compression. On a phone held back by heat, memory traffic is power.

### 3.4 Both: translated shaders carry the console's rules

The translator emits generic code that reads the draw's state at run time instead of being built for it
(no specialisation constants for guest state anywhere,
`src/graphics/pipeline/shader/spirv_translator*.cpp`):

- each texture fetch tests, per component, whether the format is unsigned, signed, biased or gamma, may
  sample twice (signed and unsigned views), and applies the console's piecewise gamma curve;
- the LOD bias and exponent bias are unpacked from the fetch constant on every fetch;
- an alpha-test `discard` is compiled into most colour shaders even when the test is off, which turns off
  the early depth rejection (LRZ) Adreno relies on;
- bool and loop constants are read at run time, and shaders with dynamic register indexing keep their
  registers in indexed memory, which is slow on mobile GPUs.

### 3.5 Both: resolves still write guest memory

A native resolve writes the rectangle straight into the textures that read it, but the SDK still does
the console-style resolve into guest memory first: 2-3 GPU passes per resolve where a PC renderer has one
copy (`render_target_cache.cpp:1157-1413`).

## 4. Why Hollow is lighter

| | Hollow (UE3 PC renderer, 2008 development build) | This port (retail Xbox 360 renderer) |
|---|---|---|
| Shaders | HLSL compiled for PC | Xbox 360 GPU microcode translated at run time, with the console's format, gamma and constant rules |
| Draw submission | Direct3D calls | Game's Direct3D calls → snapshot → Xbox 360 GPU registers → Direct3D 12 or Vulkan |
| Render targets and resolves | PC textures | Host textures (native renderer), plus the console-style resolve into guest memory |
| Visibility | PC occlusion path | Xbox 360 occlusion queries, one GPU drain each on Direct3D 12 (section 3.2) |
| Content | A leaked, unfinished 2008 build | The complete retail game |

## 5. The plan, ranked

Gain ÷ effort, with the platform each one helps. "Status" says what is done.

| # | Change | Helps | Expected gain | Effort | Status |
|---|---|---|---|---|---|
| 1 | **Native front end off by default on PC** (`gow2_native_frontend`): until the command processor stops reading the console's command stream, the per-draw snapshots are extra work | PC | 151 to 263 fps in the hospital (RX 7800 XT) | done | 0.3.8 |
| 2 | **Write changed register ranges in bulk** instead of one virtual `WriteRegister` call per dword, on the command-stream path that PC uses again | PC, Android CPU | medium in busy scenes | low | planned (skipping unchanged constant blocks by pointer was tried on the front end path: no gain, section 6) |
| 3 | **Draw buffer of the native front end** 4,096 to 8,192 draws a frame, so busy scenes stay on it where it is on | Android, PC when chosen | removes the fallback seen in #26 | done | 0.3.8 |
| 4 | **Native front end off by default on Android too**: players on a Mali-G615 phone report far fewer stutters with it off (#27); on PC it cost 42 % of the frame | Android | fewer stutters on Mali (player report); Adreno not measured yet | low | done (0.3.10) |
| 5 | **Fewer render pass breaks on phones**: gather the uploads and texture loads a pass needs before it starts, instead of breaking it for each | Android | large (most of the 26 passes end at an upload) | medium-high | planned |
| 6 | **Load and store only what is needed**: `CLEAR` for full clears, `DONT_CARE` for depth and stencil not read again and for fully overwritten targets, a render area that fits the pass | Android | medium-large (memory traffic is power) | medium | planned |
| 7 | **Shaders specialised per draw**: texture sign and gamma, bool constants and the alpha test as specialisation constants, so off features compile out and LRZ stays on | Android, PC | medium-large GPU time | medium-high | planned |
| 8 | **Vulkan descriptor sets written only when they change** | Android, PC Vulkan | medium CPU | medium | planned |
| 9 | **Smaller constant uploads** for shaders with dynamic indexing | both | medium CPU and bus | medium | planned |
| 10 | **Skip the guest-memory resolve** when every reader was written natively (keep the guest copy stale until something reads it) | both | 1-2 GPU passes and a copy per resolve | medium-high, coherency risk | planned |
| 11 | **Shaders translated ahead of time** (with @NeloRojo's XenosRecomp pipeline, #22) and a pipeline warm-up from the game's own cache on first start | both | no stutters the first time an effect appears; no Vulkan translation on the command thread | medium | in discussion (#22) |
| 12 | **Heat-aware frame rate on Android**: keep the game's 30 fps cap by default and lower the internal resolution when Android reports little thermal headroom (ADPF), instead of racing to 50 fps and throttling | Android | a steadier warm frame rate | medium | idea |
| 13 | **Idle command thread waits instead of yielding** (500 yields before each wait) | Android | CPU power, heat | low | idea |
| 14 | **Occlusion queries without GPU drains on PC**: read each result when the GPU has it, or a frame late | PC | none measurable in the hospital (on and off inside the spread); profile a scene with more queries first | medium | parked |

Before each change: profile it in the scene it targets. The first numbers to take are the command
processor's time per draw stage at 4,300 draws (PC), the reason each of the 26 Android passes ends
(`vulkan_android_stats_log`, PassTiming), the occlusion queries per frame, and the bytes of each texture
load.

## 6. Already measured and dropped

So nobody tries them again without a new reason:

| Change | Result |
|---|---|
| Skip every EDRAM ownership copy (`tools/native/experiment-edram-transfers.patch`) | 99 to 152 fps, but a black picture: the game needs some of them; the native renderer keeps only those |
| Index buffers copied on the CPU (0037), host-visible guest memory (0038) | No sustained gain on the S25; out of the series ([NATIVE_RENDERER_PHASE1.md](NATIVE_RENDERER_PHASE1.md)) |
| Four pipeline creation threads instead of the default | No effect |
| Half scene resolution on the phone (`ScreenPercentage` 50) | About 25 % less GPU work for 75 % fewer pixels, and a very blurry picture |
| Occlusion queries off on PC (`occlusion_query_enable = false`) | Inside the run-to-run spread in the hospital (RX 7800 XT, section 3.2); kept on |
| Skip unchanged constant blocks by pointer on the front end path (prototype SDK patch 0064) | 133 and 151 fps against 143 and 148 without, inside the spread; not shipped |

## 7. How to measure

- PC: `gow2_frame_log = true` logs `FRAMESTATS` (frame times) every 5 s; `d3d12_stats_log = true` the
  Direct3D 12 counters; PresentMon for presented frames.
- Android: `vulkan_android_stats_log = true` logs `GPUSTATS` (draws, render passes and why they ended,
  uploads) every 5 s; `FRAMES` lines carry the GPU clock and thermal state. Start every run from a cool
  phone (battery at or below 34.5 °C) and alternate A and B (ABAB): two identical runs in a row can
  differ by 15 %.
- PC A/B runs: nothing else may use the GPU. A local LLM server answering in the background halved some runs on the 7800 XT; pause it, and alternate A and B (ABAB) as on the phone.
- Bug reports: attach the whole `gow2.log` (PC) or Launch options → Share last log (Android).
