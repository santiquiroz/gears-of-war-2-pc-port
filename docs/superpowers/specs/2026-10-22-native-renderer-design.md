# Native renderer: design

Date: 2026-10-22. Branch `native-renderer`. Builds on [NATIVE_RENDERER_STUDY.md](../../NATIVE_RENDERER_STUDY.md),
[phase 0](../../NATIVE_RENDERER_PHASE0.md) and [phase 1](../../NATIVE_RENDERER_PHASE1.md).

## Intent

The port draws Gears of War 2 by emulating the Xbox 360 GPU: the command processor reads the game's PM4
packets and the render target cache reproduces the console's 10 MB EDRAM, with ownership transfers
between host render targets that alias the same EDRAM, and resolves that go from a render target to the
EDRAM buffer, to guest memory, and back into a texture. Phase 1 removed most of the waste of that model
(v0.2.11), but the model itself stays: three passes per resolve, copies whenever EDRAM changes owner, a
2048-tile limit that breaks widescreen on host render targets, and a large surface of Vulkan features
that some mobile drivers get wrong.

The owner wants the native renderer for every target at once:

- **Weak hardware runs well**: integrated GPUs and handhelds (a Legion Go), phones older than the
  Galaxy S25 Ultra.
- **No corruption on troublesome GPUs**: issue #1, an Adreno 630 whose Vulkan 1.3 drivers corrupt the
  picture of every Xenos-emulating port. (Guess, not established: part of that comes from the
  emulation's render target copies and format reinterpretation; a renderer that does only what a PC
  game does exercises less of the driver.)
- **Better picture**: any resolution and aspect without EDRAM artifacts, and room for modern
  antialiasing and upscaling on real render targets.
- **Mods and new effects**: render targets, textures and passes as host objects that can be hooked.

## Goals

1. Draw the whole game without EDRAM emulation: render targets are host textures, clears are host
   clears, resolves are host copies into host textures, no ownership transfers, no predicated-tiling
   replays.
2. The same picture as the emulation in every scene compared (screenshots side by side), the emulation
   staying selectable as the reference and fallback.
3. Direct3D 12 on Windows and Vulkan on Android, both.
4. At least as fast as the emulation everywhere; faster sustained on the S25 and on integrated GPUs.
5. A small, documented place to hook passes and render targets for mods (after parity).

## Non-goals

- Changing the game's logic, audio, input or networking.
- A new shader translator. The Xenos microcode translators the SDK already has (DXBC and SPIR-V) stay;
  XenosRecomp ahead-of-time translation is a later option, not part of this design.
- New texture decoding. Loading textures that come from guest memory (untiling, format conversion)
  reuses the texture cache as it is.
- Other games or a generic Xbox 360 renderer: Gears of War 2's Direct3D usage is the target, and
  game-specific knowledge is allowed.

## Anti-goals (failure even if it works)

- A visual regression shipped as the default. The native path becomes a default only per platform, and
  only after parity on that platform.
- A switch that half-works silently: anything the native path cannot handle must be counted and logged,
  never dropped without a trace.
- Breaking co-op or System Link (lockstep, per-instance rendering).
- Making Android or PC slower than the current release in the scenes measured.

## Constraints

- **The command processor keeps running every packet** (fences, interrupts, swaps), as phase 1 found;
  the native renderer replaces what happens at draws, clears, resolves and swaps, not the packet loop.
  `gpu_native` (patch 0032) is the switch; the skeleton skips `IssueDraw`, and the native renderer takes
  that place.
- **SDK changes are numbered patches in `sdk/patches`**, built with `scripts/build-sdk.ps1` and then
  `scripts/build.ps1` (only the latter copies the DLLs next to `gow2.exe`).
- **ABI**: the game is compiled against the installed SDK headers on Windows, so no change to the layout or
  virtual interface of classes the game subclasses or touches (`ReXApp`, `ImGuiDialog`). The renderer
  lives inside `rexgpu-xenos`.
- **Measurement rules** learned in phase 1: compare against a baseline from the same build; on the
  phone, wait for the battery to cool to 34.5 °C before each run and alternate the order.
- Licences: the repository is BSD-3; the SDK and Xenia code are BSD.

## Where the renderer gets its input (my call)

At the command processor, per draw, clear, resolve and swap, from the register state the game's
Direct3D library has written, plus Direct3D-level hooks only for meaning the registers do not carry.

Why not draw from the Direct3D calls alone, as the study first planned: phase 0 found the draw, Clear,
Resolve and Swap entry points, but render state reaches the GPU through the library's own flush
(`sub_8298F600`) and setters that LTCG inlined into the game, so the registers are the only complete
record of the state of each draw. The hooks named in phase 0 still help: `sub_82981ED8` (Clear: flags
and rectangle), `sub_8298A128` (Resolve), `sub_8298D430` (Swap), and the tiling calls, to tell intent
apart from the console's EDRAM mechanics.

## Components

1. **Surface cache** (replaces the render target cache in native mode). A surface is a host render
   target identified by what the game binds: EDRAM base, pitch, format, sample count and the
   resolution scale. Surfaces are sized by the render target's real extent, not by EDRAM, so there is no
   2048-tile limit. Binding a surface never copies another surface's contents into it. The few cases
   where the game relies on EDRAM aliasing (one surface's pixels read as another format at the same
   base) are measured first (phase 1 counted about ten such ownership changes a frame in the hospital,
   colour to depth and back) and handled one by one with an explicit, logged rule; the default for an
   unknown case is a fresh surface plus a log line.
2. **Clears** become host clears of the surface (colour, depth, stencil separately), from the Clear hook
   or from Direct3D's embedded clear shaders (the detection patch 0034 already does).
3. **Predicated tiling** runs once: the native renderer draws each surface whole and ignores the
   per-tile replays and their predicates.
4. **Resolves** become a host copy from the surface region into a host texture identified by the guest
   destination (address, pitch, size, format), with the conversions the resolve asks for (sample
   averaging, exponent bias, channel swap, format packing) done in a shader. When the texture cache
   later fetches a texture at that address with a compatible description, it takes the host texture
   directly. Guest memory still receives the resolved data (the emulation's copy path) until it is shown
   that nothing else reads it; making that write lazy is a later optimisation, behind its own switch.
5. **Swap** presents the surface or texture the game swaps.
6. **Unchanged and reused**: shader translation, the pipeline cache, constants, vertex and index fetch from
   shared memory, textures loaded from guest memory, the presenter and its effects.
7. **Comparison tooling**: the scene screenshots used in phase 1 (chapters 1, 2, 4, 8, title, menus) for
   both modes side by side, the frame statistics, and counters for every native fallback.

## Answers from the owner

- **Test hardware**: only the RX 7800 XT PC and the Galaxy S25 Ultra. Integrated GPUs, handhelds and older
  or Adreno 6xx phones are validated through external testers (issues), with no dates promised.
- **Stop rule**: none. The full renderer gets finished even if one platform gains little; the value is
  also in mods, picture and compatibility.

## Decisions made by the orchestrator (to check)

- **Resolves and guest memory (2b).** For correctness first, a resolve also dumps the source region into
  a scratch buffer in the EDRAM layout and runs the existing resolve copy into guest memory (or the
  scaled resolve buffer when `resolution_scale` > 1, as today). The first saving is the texture reload,
  which the host texture replaces. A lazy guest write (only when something other than the matching
  texture needs the range) comes later behind its own switch, after measuring whether the CPU reads any
  resolved data (exposure readback, for example).
- **Vulkan is measured early.** The S25 runs the native path right after 2a, not only in phase 4, so the
  phone's benefit is known before phase 3.
- **"The same picture".** The reference is the emulation's ROV path at 16:9 on the PC (it has neither the
  old teal cast nor the widescreen rectangles). Comparison by eye plus a numeric image difference with a
  tolerance (a small tool, part of 2a). Where the native path is better on purpose (widescreen without
  EDRAM wrap, for example), the native picture wins and the difference is written down.
- **Platforms.** A platform is an OS and backend (Windows D3D12, Android Vulkan); per-vendor defaults
  only if a vendor needs a different one, as ROV does today.
- **Co-op and System Link.** Mixed modes are allowed (one instance native, another emulated). Each
  phase's soak includes a two-instance run with one of each, watching for divergence; occlusion query
  and memexport results keep the emulation's behaviour.

## Phases and exit criteria

| Phase | Work | Exit criterion |
|---|---|---|
| 2a | Surface cache, clears, present, tiling once (D3D12 first) | Title screen and main menu look as in emulation |
| 2b | Resolves to host textures, the texture cache taking them | Chapter 2 hospital looks as in emulation |
| 3 | Parity across Act 1 and the scenes compared in phase 1; the game fixes (AO, depth of field, black shading); widescreen on host render targets | Same pictures as emulation; 21:9 without the light-shaft rectangles on AMD |
| 4 | Vulkan / Android parity, then performance | S25 at least as fast sustained, same pictures; an Adreno 6xx tester if one can be found |
| 5 | Mod hooks: named passes and render targets exposed to an injection point | Documented, with one example effect |

Each phase ends with the comparison screenshots, a 10-minute soak, and the frame statistics against the
emulation from the same build. The native path stays off by default until a platform reaches parity;
then it becomes that platform's default in a release, with the emulation one switch away.

## Delegation

The owner wants the work delegated. The orchestrator writes each task's contract (files, functions,
expected behaviour, how it is verified) and reviews and verifies every diff; implementation goes to the
external lanes through bipolar-code's broker (`mcp__bipolar__delegate`, including the Anthropic API
account the owner added), Muse and Codex first when they have quota.

## Left to the builder

Data structures and class layout inside `rexgpu-xenos`, file names, how a surface is keyed and looked up,
the shader that converts resolves, how fallbacks are counted, and the order of tasks inside a phase.
