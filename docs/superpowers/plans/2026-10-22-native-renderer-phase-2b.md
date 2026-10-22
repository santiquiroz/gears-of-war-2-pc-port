# Native renderer phase 2b (native resolves) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** With `gpu_native_resolves` (requires `gpu_native_surfaces`), a resolve also writes its result straight from the host render target into every texture cache texture that views the destination memory compatibly, and those textures stay up to date, so the texture cache skips the reload (today ~16 loads a frame, ~1.1 ms at 1440p on an RX 7800 XT).

**Architecture:** Hook in `D3D12RenderTargetCache::ResolveImpl` after `texture_cache.MarkRangeAsResolved(...)` (render_target_cache.cpp ~1520), which fires the watches and marks overlapping textures outdated. For each texture whose base level views the destination with a supported (render target format → texture format) pair, and which was up to date before this resolve, copy the resolved rectangle from the source render target into the texture's resource (bit-compatible pairs with `CopyTextureRegion`; the others with a small compute shader), then `MakeUpToDateAndWatch` under the global lock. The guest-memory write of the resolve stays as it is.

**Tech Stack:** C++20 in `rexgpu-xenos`, HLSL compiled with `fxc.exe` (Windows SDK 10.0.26100.0, `/T cs_5_1`) into a bytecode header like the existing `src/graphics/shaders/bytecode/d3d12_5_1/*.h`; GLSL with the NDK's `glslc` for Vulkan.

**Spec:** `docs/superpowers/specs/2026-10-22-native-renderer-design.md`

## Global Constraints

- Same as phase 2a (`docs/superpowers/plans/2026-10-22-native-renderer-phase-2a.md`): numbered SDK patches (next free: `0037`), switches off by default, drops and fallbacks counted and logged, idle-gated game runs, cool-gated alternated phone runs, no ABI change to `ReXApp`/`ImGuiDialog`.
- The resolve's own write to guest memory (or to the scaled resolve buffer) is not removed in this phase.

## Pairs seen in the chapter 2 hospital (from the phase 1 spike)

| Source RT (resource) | Dest texture (resource) | Resolve | Method |
|---|---|---|---|
| C7 `k_16_16_16_16_FLOAT` (`R16G16B16A16_TYPELESS`) | 32 `k_16_16_16_16_FLOAT` (`R16G16B16A16_FLOAT`) | bias 0 | `CopyTextureRegion` |
| C6 `k_16_16_FLOAT` (`R16G16_TYPELESS`) | 31 `k_16_16_FLOAT` (`R16G16_FLOAT`) | bias 0 | `CopyTextureRegion` |
| C3 `k_2_10_10_10_FLOAT` (`R16G16B16A16_FLOAT`) | 32 | bias -3 | compute: value × 2^bias, write `R16G16B16A16_FLOAT` |
| C0 `k_8_8_8_8` (`R8G8B8A8_UNORM`) | 6 `k_8_8_8_8` (`R8G8B8A8_TYPELESS`) | swap 1 | compute: swap R and B |
| C4 `k_16_16` (`R16G16_TYPELESS`, -32..32 range) | 25 `k_16_16` (`R16G16_TYPELESS`) | bias 0 | compute: the range conversion the full resolve applies (to be measured against the normal path) |
| D0 D24S8 (`R24_UNORM_X8` SRV) | 22 `k_24_8` (`R32_FLOAT`) | — | compute: depth to float |
| D1 D24FS8 (`R32_FLOAT_X8X24` SRV) | 23 `k_24_8_FLOAT` (`R32_FLOAT`) | — | compute: depth to float |

Pairs not in this table, MSAA sources with sample averaging, mipmapped, packed-mip, 3D, array, linear (non-tiled) or signed-separate textures fall back to the normal reload and are counted.

## Review Focus

1. A texture that was already outdated before the resolve (a CPU write, another resolve's partial cover) must not be marked up to date by a partial native write: it reloads.
2. Several texture keys share one base address with different pitch or format (the hospital reuses `0x108C0000` for 800x800 depth, 640x352 and 322x178 views): only compatible ones are written; the rest reload.
3. Resolution scale 1 and 2: the texture and the render target are both scaled; the rectangle must be scaled the same way.
4. Texture replacement packs (`MaybeUpdateReplacement`) must not be applied to a natively written texture (they skip scaled and GPU-written textures already; keep it that way).
5. The render target's resource state after the copy must be what the next draw expects (`DumpRenderTargets` leaves RTs in `NON_PIXEL_SHADER_RESOURCE`; follow the same transitions it uses).

---

### Task 1: Find textures by base address

**Files:** `include/rex/graphics/pipeline/texture/cache.h`, `src/graphics/pipeline/texture/cache.cpp`

**Interfaces:**
- Produces: `void TextureCache::ForEachTextureAtBase(uint32_t base_address, const std::function<void(Texture&)>& visit)` (protected; base address of the base level), backed by a `std::unordered_multimap<uint32_t, Texture*>` keyed by `key.base_page`, maintained where textures are inserted into and erased from `textures_`; and `bool Texture::IsBaseUpToDateBeforeResolve() const` returning the base outdated state captured by a new `void TextureCache::CaptureUpToDateBeforeResolve(uint32_t start, uint32_t length)` called just before `MarkRangeAsResolved` fires the watches.
- [ ] Test: a header-free unit test is not practical here; verify by the build and by Task 2's counters.

### Task 2: Native resolve for bit-compatible pairs (D3D12)

**Files:** `src/graphics/d3d12/render_target_cache.cpp` (`ResolveImpl`), `include/rex/graphics/d3d12/render_target_cache.h`, `src/graphics/pipeline/render_target/cache.cpp` (cvar), `include/rex/graphics/flags.h`

**Interfaces:**
- Consumes: Task 1.
- Produces: cvar `gpu_native_resolves` (bool, false, "GPU", requires restart; effective only with `gpu_native_surfaces`); `NATIVERESOLVE written N fallback <reason counts>` log line every 5 s.

- [ ] For each texture at the destination base with key `tiled`, 2D, one array slice, `mip_max_level == 0`, not `signed_separate`, `pitch` and format matching the resolve's destination, `scaled_resolve` matching the current scale, and up to date before the resolve: copy the resolve rectangle (scaled) from the source render target with `CopyTextureRegion` for the pairs C7→32 and C6→31, then mark it up to date (`MakeUpToDateAndWatch` under the shared memory global lock).
- [ ] Build, run the hospital at 720p and 1440p with the mode on; expected: `NATIVERESOLVE written` > 0, `texture_loads` per frame lower than with `gpu_native_surfaces` alone, same picture (diff tool, title/menu < 0.02).

### Task 3: Conversion shader for the other pairs (D3D12)

**Files:** `src/graphics/shaders/native_resolve.cs.hlsl` (new), its bytecode header `src/graphics/shaders/bytecode/d3d12_5_1/native_resolve_cs.h` (generated with `fxc /T cs_5_1 /E main /Fh ... /Vn native_resolve_cs`), `src/graphics/d3d12/render_target_cache.cpp`, `src/graphics/d3d12/texture_cache.cpp` (textures of formats 32, 6, 25, 22, 23 created with `D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS` when `gpu_native_resolves` is on).

- [ ] One compute shader with root constants: mode (scale, swap R/B, depth unorm, depth float, 16_16 range), exp-bias scale, source and destination offsets, size; reads the RT SRV, writes the texture UAV.
- [ ] Verify each pair against the normal path: run with the mode on and off, compare screenshots of the hospital and chapter 4 (diff tool), and log per pair how many textures were written.

### Task 4: Vulkan

- [ ] The same hook in `VulkanRenderTargetCache::Resolve` (`MarkRangeAsResolved` ~1292): `vkCmdCopyImage` for the bit-compatible pairs, the shader (GLSL compiled with `glslc`) for the rest; S25 measured cool-gated and alternated, with `vulkan_android_stats_log`.

### Task 5: Parity and numbers

- [ ] Chapters 1, 2, 4, 8 against the ROV reference; soak; mixed co-op run; numbers into `docs/NATIVE_RENDERER_PHASE2.md`.
