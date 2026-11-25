# Optional replacement-texture pre-cache

`gow2_adaptive_precache` prepares previously observed replacement textures before gameplay on the Direct3D 12 backend. It is disabled by default. It does not compile shaders, predict unseen effects, or upload every texture to the GPU in advance.

## Configuration

Add these settings to your own `gow2.toml`, then restart:

```toml
gow2_adaptive_precache = true
gow2_precache_budget_mb = 3072
gow2_precache_startup_ms = 6000
```

`texture_replace` must be enabled and a replacement pack must be active. The implementation is restricted to Gears of War 2 (`4D53082D`). Vulkan behavior is unchanged.

| Setting | Default | Bounds | Meaning |
|---|---:|---|---|
| `gow2_adaptive_precache` | `false` | Boolean | Enable learned replacement-texture preparation. |
| `gow2_precache_budget_mb` | `3072` | 64вЂ“3072 MiB | Maximum retained prepared-image payload, separate from VRAM. |
| `gow2_precache_startup_ms` | `6000` | 100вЂ“10000 ms | Preparation deadline checked between DDS files. |

On Windows, the payload budget is also limited to one third of available physical RAM. Preparation is skipped below 1 GiB available. The deadline is not a hard timeout: an individual file read cannot be interrupted, and upload-buffer preparation happens afterward. Memory limits describe prepared payloads, not the process working set, temporary allocations, map overhead, or the existing replacement-image cache.

## How it works

1. Inspect at most 12 recent `logs/gow2_*.log` files and select up to three sessions mentioning an active pack path. Skip logs larger than 2 MiB; total log contents read are limited to 6 MiB. Directory enumeration itself is not bounded by a fixed entry count.
2. Parse successful replacement entries; prioritize the newest requests in the newest matching sessions. Deduplicate requests and cap the plan at 4096 hashes.
3. Resolve hashes through the current replacement map. Read DDS images within the payload/time budgets, skipping individual files larger than 128 MiB.
4. Keep prepared images separate from the validated replacement cache. On demand, check file size and modification time, then run the existing `ValidateReplacement` against the actual guest texture before reuse. Move accepted payloads rather than copying them into a second image allocation.
5. Prepare and touch two reusable 64 MiB upload pages before gameplay. Tight-pitch mip staging uses a bulk copy; padded rows preserve the existing row-copy layout.

Missing, changed, or unsuitable resources use ordinary loading or the existing rejection path. There is no background compiler, network access, external cache download, or additional game installation. Texture-pack keyboard/controller variants continue to use their existing loading path.

## Diagnostics

- `GOW2PRECACHE ready`: retained images, payload, startup time, history sessions and candidates.
- `GOW2PRECACHE upload buffers ready`: initial staging-page preparation.
- `GOW2PRECACHE use`: prepared hits, on-demand reads and remaining prepared payload, reported when counts change.
- Slow-read and slow-staging messages are bounded diagnostic samples, not a complete frame-time trace.

Logs are a best-effort history source. Removing them, changing their format, using logs above the size bound, or moving a pack can reduce preparation coverage. With no usable history, ordinary loading remains available. Logs from stacked packs do not describe an exact per-entry originating pack; current replacement resolution and guest validation still decide what can be used.

## Validation and limits

`pwsh -File scripts/test-host.ps1` includes 34 standalone checks for hash parsing, pack matching, truncated input, refusal filtering, request priority, deduplication, count/memory bounds, low-memory behavior, and byte-equivalent mip staging including padding and overrun guards.

Live observations on Windows, RX 9070 XT, Ryzen 7 9800X3D, 32 GiB RAM, and a Samsung 860 EVO SATA SSD, with a 4x replacement pack:

- One startup prepared 1076 images / 2235 MiB in 814 ms, plus 128 MiB of staging pages in 15 ms.
- A subsequent 0.4.7 startup prepared 1554 images / 3071 MiB in 1221 ms, plus staging pages in 72 ms.
- The player reported improved smoothness and confirmed the managed 0.4.7 build worked well.

These are startup observations and user feedback, not a controlled enabled/disabled frame-time benchmark. No numerical FPS uplift, universal stutter elimination, comparison with COGBox, or validation on other GPUs is claimed. The live builds also included unrelated local fixes; their gameplay feedback is not an isolated measurement of this feature.

Before recommending a default change, repeat the same checkpoint with the same pack, resolution, driver and existing shader cache, alternating enabled/disabled runs. Record frame-time distributions using `gow2_frame_log` / `d3d12_stats_log`, startup time, process memory, prepared hits and on-demand reads. Test file changes between preparation and use, pack switching, no history, constrained memory, and a long session.

Existing shader/pipeline warmup is unchanged. First GPU resource creation, new shaders, unseen textures, and unrelated game work can still cause stalls. Disable the feature by setting `gow2_adaptive_precache = false` and restarting.

## Controlled managed-runtime measurements

See TEXTURE_PRECACHE_INITIAL_BENCHMARK.md and TEXTURE_PRECACHE_STEADY_BENCHMARK.md for controlled OFF/ON observations, and TEXTURE_PRECACHE_MEASUREMENTS.json for aggregate data. These were taken on a managed runtime with other changes held constant, not a clean build of this isolated proposal. Early-window comparisons retain loading boundaries and are confounded by unreset OS/driver caches; no universal speedup or default-enable recommendation is made.
