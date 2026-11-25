# Twelve-run pre-cache comparison including initial loading

Date: 2026-11-25. Twelve sequential managed-runtime tests completed: three pre-cache OFF and three ON with the 4x world pack, and the same six conditions with original world textures plus the current UI assets only. Initial loading/first-input reporting windows were retained this time.

## Setup and controls

- Windows 11 Pro 10.0.26300; AMD RX 9070 XT, display driver 32.0.32029.2010; Ryzen 7 9800X3D; 32 GB RAM; Samsung 860 EVO 1TB SATA SSD (D:).
- Same installed managed v0.4.8 (source dd103fe8fb10c36b41827086a32104fdfe68e11f / SDK 923c1a5), 1440p internal resolution, 60 FPS cap, VSync off, D3D12 RTV, borderless fullscreen. Blood-mask/input/fence/diagnostic fixes stayed constant. This is not a clean build of the independent PR branch.
- Pre-cache budget 3072 MiB, startup deadline 6000 ms. Both arms retain texture replacement: 4x uses the current world/UI pack; original-world controls use only the current UI replacement files (two base entries plus keyboard/controller variants). No 4x world files exist in the UI-only test root.
- Each fresh process received an independent copy of the same warmed shader seed and user profile. Existing OS file/driver caches were not cleared. This is not a cold-boot/cold-SSD benchmark.
- Fixed local observed history was seeded before the suite. The UI-only historical session was relocated to the copied UI pack path; requests themselves were not invented. Trial logs were outside the scanned executable logs folder, so later trials did not train subsequent ones. Temporary history entries were removed afterward.
- Order: original/OFF, 4x/OFF, original/ON, 4x/ON; then the reverse; then the first order again. No concurrent game instances. A closed/invalid instance would stop the suite.
- Chapter index 1 (human chapter 2, hospital). A live Marcus pawn in PlayerWalking was required. Polling for readiness was reduced to 250 ms plus command acknowledgement, and aiming/firing began immediately after the readiness mark; the previous extra settling delay was removed.
- Five approximately 14-second cycles: aim/fire 4 s, reload/neutral 1 s, forward 2 s, turn 1 s, aim/fire 3 s, turn back 1 s, backward 2 s. State checks after every cycle passed. Input is time-based, not deterministic frame replay.

## Measurement boundaries

Both requested logs were enabled: gow2_frame_log and d3d12_stats_log. Startup wall time is process launch to the command pump; travel time is the marked travel request to the first confirmed live pawn. These are polling-based readiness measures, not exact visual loading-screen durations.

FRAMESTATS reports five-second aggregates. Every reporting window overlapping travel through the first ten seconds after readiness is retained as an early-window sample. These typically cover about fifteen seconds, including boundary portions of menu/loading or slightly later gameplay. The figures below must not be called exact counts in the first ten seconds, pure gameplay stutters, or GPU-present latency. They measure the upstream input-step frame-timing counter during a transition-inclusive region. Windows were not silently dropped.

Later steady-state statistics use only whole windows after the first ten seconds and before the end mark. Startup/menu and final neutral-tail boundary windows remain available in the numerical data with phase labels. Region subsets overlap and must not be added together. Thresholds are strictly >33, >50 and >100 ms. Window percentiles are not whole-run percentiles.

## Summary (three repeats per row)

| World textures | Pre-cache | Early >33 ms, median | Early >50 ms, median | Early >100 ms, median | Later FPS, median | Startup, median (s) | Private memory, sampled median maximum (MiB) |
|---|---|---:|---:|---:|---:|---:|---:|
| Original + UI only | OFF | 3 | 3 | 1 | 59.988 | 8.559 | 3774 |
| Original + UI only | ON | 3 | 1 | 1 | 59.987 | 8.558 | 3834 |
| 4x | OFF | 8 | 6 | 3 | 59.980 | 9.049 | 6894 |
| 4x | ON | 5 | 4 | 2 | 59.988 | 10.069 | 8708 |

## Individual runs

| Run | World | Pre-cache | Early sampled seconds | Early >33 / >50 / >100 ms | Later FPS | Later >33 ms | Startup / travel readiness (s) |
|---|---|---|---:|---|---:|---:|---|
| 1 | Original | OFF | 15.005 | 3 / 3 / 1 | 59.988 | 0 | 9.051 / 0.975 |
| 2 | 4x | OFF | 15.145 | 13 / 12 / 8 | 59.927 | 1 | 9.049 / 1.024 |
| 3 | Original | ON | 15.013 | 4 / 1 / 1 | 59.980 | 0 | 8.561 / 0.924 |
| 4 | 4x | ON | 15.014 | 7 / 4 / 3 | 59.991 | 0 | 14.780 / 1.025 |
| 5 | 4x | ON | 15.025 | 5 / 3 / 2 | 59.988 | 0 | 10.066 / 1.178 |
| 6 | Original | ON | 15.024 | 3 / 1 / 1 | 59.987 | 0 | 8.551 / 0.975 |
| 7 | 4x | OFF | 15.006 | 8 / 4 / 3 | 59.980 | 0 | 8.560 / 0.976 |
| 8 | Original | OFF | 15.016 | 4 / 3 / 1 | 59.987 | 0 | 8.556 / 0.975 |
| 9 | Original | OFF | 15.008 | 3 / 2 / 1 | 59.988 | 0 | 8.559 / 1.077 |
| 10 | 4x | OFF | 15.023 | 8 / 6 / 3 | 59.986 | 0 | 9.056 / 0.976 |
| 11 | Original | ON | 15.016 | 2 / 1 / 1 | 59.988 | 0 | 8.558 / 1.078 |
| 12 | 4x | ON | 15.025 | 5 / 4 / 2 | 59.980 | 0 | 10.069 / 0.976 |

## Whole post-readiness early windows

For a less loading-contaminated check, these are only the complete five-second windows wholly inside the first ten seconds after readiness. They do not cover the entire first ten seconds; the excluded boundary portions remain in the transition-inclusive table and raw numerical windows above.

| Run | World | Pre-cache | Whole early sampled seconds | >33 / >50 / >100 ms |
|---|---|---|---:|---|
| 1 | Original | OFF | 5.000 | 1 / 1 / 1 |
| 2 | 4x | OFF | 5.135 | 5 / 5 / 3 |
| 3 | Original | ON | 5.003 | 2 / 1 / 1 |
| 4 | 4x | ON | 5.011 | 4 / 3 / 2 |
| 5 | 4x | ON | 5.010 | 4 / 2 / 2 |
| 6 | Original | ON | 5.010 | 1 / 1 / 1 |
| 7 | 4x | OFF | 5.000 | 6 / 3 / 2 |
| 8 | Original | OFF | 5.007 | 1 / 1 / 1 |
| 9 | Original | OFF | 5.007 | 1 / 1 / 1 |
| 10 | 4x | OFF | 5.007 | 6 / 5 / 2 |
| 11 | Original | ON | 5.003 | 1 / 1 / 1 |
| 12 | 4x | ON | 5.012 | 3 / 3 / 1 |

## Pre-cache preparation and coverage

| Run | World | Prepared images / reported MB / ms | Staging pages | Last periodic prepared hits / demand reads |
|---|---|---|---|---|
| 3 | Original/UI | 2 / 10 / 44 | 2 pages (128 MB), 11 ms | 1 / 0 |
| 4 | 4x | 1777 / 3049 / 5533 | 2 pages (128 MB), 85 ms | 608 / 3 |
| 5 | 4x | 1777 / 3049 / 1169 | 2 pages (128 MB), 16 ms | 606 / 3 |
| 6 | Original/UI | 2 / 10 / 5 | 2 pages (128 MB), 14 ms | 1 / 0 |
| 11 | Original/UI | 2 / 10 / 6 | 2 pages (128 MB), 13 ms | 1 / 0 |
| 12 | 4x | 1777 / 3049 / 1177 | 2 pages (128 MB), 16 ms | 602 / 3 |

Preparation times and retained payload can vary under the same deadline because available RAM, file-cache residency and I/O vary. Periodic use counters are cumulative samples, not guaranteed exact final totals. Memory values are maxima of periodic diagnostic samples, not absolute peak commit or residency.

## Interpretation

With 4x textures, early-window >50 ms counts were [12, 4, 6] OFF versus [4, 3, 4] ON; summed counts decreased by 50.0%. This is a descriptive result for this three-repeat scenario, not a statistical-confidence estimate or a universal performance claim. See the full per-run counts and sampled durations above.

Order/cache-residency confound: the chronological 4x OFF counts above are [12, 4, 6], compared with ON [4, 3, 4]. The first OFF sample and the later OFF samples must be compared explicitly: if the baseline improves over the sequence, the aggregate reduction can include OS/driver-cache warming. Global caches were not reset, so the aggregate percentage cannot be attributed entirely to pre-cache. This suite alone does not prove that the feature is necessary.

The earlier six-run report (TEXTURE_PRECACHE_STEADY_BENCHMARK.md) excluded transition/boundary windows and showed equivalent steady capped FPS with no measured hitches. The new run deliberately retains the region in which first-use texture work is expected; the two reports answer different questions and the earlier neutral result is preserved.

A benefit, if repeatable here, supports an optional first-use-work tradeoff rather than higher sustained FPS or a requirement for every player. We do not claim exact attribution of each hitch to a DDS read: transition-window timing also includes engine/streaming work, and GPU-present frame times were not traced.

The original-world/UI-only conditions provide a control where very few replacement payloads need preparation. Graphics and prompt fixes are not evaluated by this performance test. There is no no-UI-pack, other-GPU, long-session, GPU-crash-fix or deterministic replay claim.

Before changing the default or claiming broad benefit, benchmark a clean build of the packaged PR, additional scenes/hardware and controlled cold/warm file-cache conditions; record finer per-frame boundaries if exact first-ten-second attribution is required. The managed runtime and all unrelated fixes were identical between OFF/ON arms, but the independently packaged branch itself was not rebuilt for these measurements.

Personal saves/config/runtime files remained hash-identical. No game files, DDS assets, Coalesced bundles, shader caches, raw logs, private paths, screenshots or binaries are included in the submission. Numerical performance measurements and English documentation only were added.
