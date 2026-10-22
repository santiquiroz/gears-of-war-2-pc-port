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

## Next

- More resolve pairs (the scaled resolves, which fall back while
  `resolve_resolution_scale_fill_half_pixel_offset` fills the edge; `k_16_16`; resolves from two render
  targets).
- Vulkan (phase 4), then the parity runs of phase 3.
