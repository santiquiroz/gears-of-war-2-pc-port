# Native renderer phase 2a (native surfaces) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A `gpu_native_surfaces` mode in which render targets are host surfaces with no EDRAM ownership copies (except the depth copies the game needs), Direct3D's 4x clears run at 1x, and every dropped copy is counted; title, main menu and the compared chapters look as in the ROV reference.

**Architecture:** The mode lives in the SDK's common render target cache (`rexgpu-xenos`), so D3D12 and Vulkan both get it. A pure, header-only policy decides which ownership transfers survive; the cache filters `last_update_transfers_` through it after `ChangeOwnership` and records what it dropped. The mode forces the host render target path and turns on the 1x fills of patch 0034. Validation is screenshots against the ROV reference with a small image-difference tool.

**Tech Stack:** C++20 (SDK, Catch2 header tests run by `scripts/test-host.ps1`), PowerShell build scripts, Python 3 + Pillow for the image tool (pytest, run by `test-host.ps1`).

**Spec:** `docs/superpowers/specs/2026-10-22-native-renderer-design.md`

## Global Constraints

- SDK changes are numbered patches in `sdk/patches` (next free number on the branch: `0036`), generated against the series 0031-0035 applied in `C:\personal\gow2-sdk-wt` on top of commit `3a02db2` (`0030`); `scripts/build-sdk.ps1 -NoDlss` then `scripts/build.ps1 -Sdk 'C:\personal\tools\rexglue\0.10.0.9\win-amd64' -SkipCodegen` (only the second copies the DLLs next to `gow2.exe`).
- No layout or virtual-interface change to classes the game subclasses or touches (`ReXApp`, `ImGuiDialog`).
- Every new behaviour behind a switch that is off by default; anything dropped is counted and logged, never silent.
- Game runs on the PC only after `out/idle-gate/wait-idle.ps1` reports 180 s without user input; screenshots taken while the user was active are deleted.
- Phone A/B: battery at or below 34.5 °C before each run, order alternated.
- Commit messages in English, conventional type prefix, no `Co-Authored-By` line.

## Review Focus

1. A dropped transfer that a later draw actually reads (a surface reused as another format at the same base): expected to show as stale content, so the drop counter must name the kinds per frame and the screenshots must cover chapters 1, 2, 4 and 8.
2. Resolve clears (`PerformTransfersAndResolveClears` with clear values) must keep working in the mode: the policy applies only to draw-time ownership changes.
3. The ROV path on NVIDIA/Intel: the mode must force host render targets, or it does nothing on those GPUs.
4. Co-op with one instance native and one emulated: no divergence in a 10-minute two-instance run.
5. Vulkan: the same switch on the S25 must not crash or break the picture (measured after the PC parity).

---

### Task 1: Transfer policy and drop counters (header-only, tested)

**Files:**
- Create: `include/rex/graphics/pipeline/render_target/native_surfaces.h` (SDK worktree)
- Create: `tests/unit/graphics/native_surfaces_test.cpp` (SDK worktree)
- Modify: `scripts/test-host.ps1:193-196` (gow2-recomp; add the test to the `$tests` list)

**Interfaces:**
- Produces, in namespace `rex::graphics::native_surfaces`:
  - `struct SurfaceKind { bool is_depth; uint32_t format; uint32_t msaa_samples_log2; };`
  - `bool KeepTransfer(const SurfaceKind& source, const SurfaceKind& dest)` — true only when both are depth.
  - `class DropCounters { public: void Record(const SurfaceKind& source, const SurfaceKind& dest, uint32_t tiles); std::string ReportAndReset(double seconds); bool empty() const; };` — one line per distinct (source, dest) pair, sorted by tiles descending, format `"C3x1->D0x1 12.0/s 8400 tiles/s"` (`C`/`D`, format, `x` sample count), joined with `"; "`.

- [ ] **Step 1: Write the failing tests** in `native_surfaces_test.cpp`: `KeepTransfer` true for depth→depth (formats 0→1 and 1→0, any MSAA), false for colour→depth, depth→colour, colour→colour; `DropCounters` reports two pairs sorted by tiles with the exact format above for `seconds = 2.0`, and `empty()` is true after `ReportAndReset`.
- [ ] **Step 2: Run** `scripts/test-host.ps1`; expected: the new test fails to compile (header missing).
- [ ] **Step 3: Implement** `native_surfaces.h` (header-only, `<map>`, `<string>`, `fmt::format` or `std::to_string`, no SDK runtime dependencies).
- [ ] **Step 4: Run** `scripts/test-host.ps1`; expected: `All 5 steps passed.` with the new file listed.
- [ ] **Step 5: Commit** the patch `0036-native-surfaces.patch` (SDK files of this task) and the `test-host.ps1` change.

### Task 2: The `gpu_native_surfaces` mode in the render target cache

**Files:**
- Modify: `src/graphics/pipeline/render_target/cache.cpp` (around the `ChangeOwnership` loop, `RenderTargetCache::Update`, lines ~686-691)
- Modify: `include/rex/graphics/pipeline/render_target/cache.h` (private members only: a `native_surfaces::DropCounters` and a report timer)
- Modify: `src/graphics/d3d12/render_target_cache.cpp:179-211` (path selection)
- Modify: the Vulkan render target path selection (same rule)
- Modify: `src/graphics/command_processor.cpp` (patch 0034's `IsMsaaFillAt1xEligible` call site: also on when the mode is on)

**Interfaces:**
- Consumes: Task 1's `KeepTransfer`, `DropCounters`.
- Produces: cvar `gpu_native_surfaces` (bool, false, category `"GPU"`, lifecycle requires restart); log line `NATIVESURF dropped <ReportAndReset text>` every 5 s while drops happened.

- [ ] **Step 1: Define** `REXCVAR_DEFINE_BOOL(gpu_native_surfaces, false, "GPU", "Native renderer, phase 2a: render targets are host surfaces; EDRAM ownership copies between them are dropped (depth to depth kept) and counted; forces host render targets and draws Direct3D's 4x clears at 1x")` in `cache.cpp`, declared in `include/rex/graphics/flags.h`.
- [ ] **Step 2: After each `ChangeOwnership`** in `Update` (not in the interlock path), when the mode is on, remove from `last_update_transfers_[rt_bit_index]` every transfer whose `KeepTransfer(source kind, dest kind)` is false and `Record` it (tiles = `end_tiles - start_tiles`); report every 5 s.
- [ ] **Step 3: Force** `Path::kHostRenderTargets` in D3D12 and the Vulkan equivalent when the mode is on, over the vendor default and over an explicit `render_target_path_d3d12` / Vulkan path cvar, logging one line (`NATIVESURF host render targets forced`) when it overrides an explicit choice.
- [ ] **Step 4: Make** the 1x fill condition `REXCVAR_GET(gpu_msaa_fill_at_1x) || REXCVAR_GET(gpu_native_surfaces)`.
- [ ] **Step 5: Build** (`build-sdk.ps1 -NoDlss`, `build.ps1`) and run `scripts/test-host.ps1`; expected: builds, `All 5 steps passed.`
- [ ] **Step 6: Run** the hospital with `--gpu_native_surfaces=true` (idle-gated); expected: `NATIVESURF dropped` lines naming only colour↔depth and colour↔colour kinds, frame rate at least that of `--gpu_msaa_fill_at_1x=true`.
- [ ] **Step 7: Commit** the regenerated `0036-native-surfaces.patch`.

### Task 3: Image difference tool

**Files:**
- Create: `tools/native/image_diff.py`
- Create: `tools/native/test_image_diff.py`

**Interfaces:**
- Produces: `diff_ratio(a_path: str, b_path: str, tolerance: int = 24, crop: tuple[int, int, int, int] | None = None) -> float` (fraction of pixels whose max channel difference exceeds `tolerance`) and a CLI `python tools/native/image_diff.py A.png B.png [--tolerance N] [--crop x,y,w,h]` printing `diff 0.0123` and exiting 1 above `--max` (default 0.05).

- [ ] **Step 1: Write the failing tests**: identical images → 0.0; one image with a 10x10 block changed by 100 in a 100x100 image → 0.01; a change of 10 with tolerance 24 → 0.0; crop limits the comparison.
- [ ] **Step 2: Run** `python -m pytest tools/native/test_image_diff.py -q`; expected: fails (module missing).
- [ ] **Step 3: Implement** with Pillow (`ImageChops.difference`, per-pixel max channel).
- [ ] **Step 4: Run** the tests; expected: pass.
- [ ] **Step 5: Commit.**

### Task 4: Parity check on the PC

**Files:**
- Create: `docs/NATIVE_RENDERER_PHASE2.md`

- [ ] **Step 1: Capture** (idle-gated, `--gow2_unlock_fps=false`) title, main menu, chapters 1, 2, 4, 8 in three modes: ROV reference (`--render_target_path_d3d12=rov`), current host RT (`rtv`), native surfaces.
- [ ] **Step 2: Compare** native against ROV with `image_diff.py` (title and menu must be under 0.02; chapters are compared by eye because the moment differs) and record `NATIVESURF dropped` per scene.
- [ ] **Step 3: For every artifact**, find the dropped kind responsible (re-run keeping that kind through a temporary environment switch), and add it to `KeepTransfer` with a test (Task 1's file) and a one-line reason.
- [ ] **Step 4: Soak** 10 minutes in chapters 2 and 4 with the mode on, and a two-instance co-op run with one instance native and one emulated; expected: no errors, no divergence, same pictures along the way.
- [ ] **Step 5: Write** `NATIVE_RENDERER_PHASE2.md` (scenes, diff numbers, kinds kept and why, frame rates against `rtv` and ROV) and commit.

### Task 5: The same mode on the S25 (Vulkan)

- [ ] **Step 1: Build** the APK (`scripts/build-android.ps1 -LowPriority`) and install it when the phone is connected.
- [ ] **Step 2: Run** the hospital, cool-gated, alternating `gpu_native_surfaces` on and off twice, with `vulkan_android_stats_log = true`; expected: same picture, `passes` per frame not higher, frame rate not lower.
- [ ] **Step 3: Record** the numbers in `NATIVE_RENDERER_PHASE2.md` and commit.

Phase 2a ends here; phase 2b (resolves into host textures) gets its own plan from what 2a measured.
