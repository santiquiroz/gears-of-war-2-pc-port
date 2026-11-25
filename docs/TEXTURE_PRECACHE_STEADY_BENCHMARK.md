# Controlled pre-cache comparison  -  2026-11-25

Six completed runs (three disabled, three enabled) found no measurable hitch-rate improvement in this warm-cache, 60 FPS-limited hospital scenario: every measured arm stayed near 60 FPS and recorded zero frames over 33 ms. The enabled path did prepare and reuse textures, at additional startup and CPU-memory cost. This is neutral performance evidence, not proof of a speedup or a reason to enable the feature by default.

## Platform and build

- Windows 11 Pro, build 26300 (10.0.26300).
- AMD Radeon RX 9070 XT; Windows display-driver version 32.0.32029.2010.
- AMD Ryzen 7 9800X3D; 32 GB installed memory (30.9 GiB reported usable).
- Game, texture pack and test cache on D:, Samsung SSD 860 EVO 1TB, SATA.
- Managed port v0.4.8, source dd103fe8fb10c36b41827086a32104fdfe68e11f, SDK 923c1a5.
- Same installed EXE and SDK DLLs in both arms; only gow2_adaptive_precache was toggled. The runtime also contained the same blood-mask, menu-Escape, DRED diagnostics and fence-failure changes in both arms. This was not a clean build of the independently packaged PR branch. Its extra planner null-guard test was not part of this installed build.
- 2560x1440 internal rendering (resolution_scale=2), 60 FPS cap, VSync off, D3D12 RTV, borderless fullscreen on monitor 1, 4x replacement pack with keyboard/controller UI variants. Existing graphical fixes and settings were retained.
- Pre-cache enabled settings: budget 3072 MiB, startup deadline 6000 ms.

## Procedure

1. Ran a training pilot in chapter index 1 (human chapter 2, hospital). The earlier pilots that remained at the title screen or were closed by the player were excluded.
2. Froze the trained shader-cache seed, copied user profile and session-history input. Every measured run had its own new user/cache directories copied from those same seeds. Trial logs were outside the executable history folder, so later trials could not replace the history plan. A temporary training-history entry was removed after the suite.
3. Ran sequentially in order OFF, ON, OFF, ON, OFF, ON, with a new game process each time. No concurrent game instances. Intro-logo skipping and diagnostic logging were enabled in both arms.
4. Travelled using servertravel geargame_p?loadcheckpoint=0?chapter=1. Required a live Marcus pawn and PlayerWalking state before beginning. Rechecked player validity after every cycle; all six state samples per run passed.
5. Repeated five 14-second input cycles: forward 2 s; aim/fire 4 s; reload 0.15 s and neutral 0.85 s; turn right 1 s; aim/fire 3 s; turn left 1 s; backward 2 s. File polling and validation overhead made each marked scenario approximately 72 seconds.
6. Captured gow2_frame_log=true and d3d12_stats_log=true. Included only whole five-second reporting windows contained between BENCH_BEGIN and BENCH_END marks. Each run supplied 13 complete FRAMESTATS windows (approximately 65 seconds). Loading, menus and boundary-straddling windows were excluded from the gameplay table and startup/chapter times were reported separately.

## Gameplay results

| Run | Pre-cache | Measured seconds | Frames | FPS | Median window p99 (ms) | Worst window p99 (ms) | Frames >33 / >50 / >100 ms |
|---|---|---:|---:|---:|---:|---:|---|
| 1 | Off | 65.021 | 3900 | 59.981 | 17.003 | 23.066 | 0 / 0 / 0 |
| 2 | On | 65.012 | 3900 | 59.989 | 16.991 | 17.080 | 0 / 0 / 0 |
| 3 | Off | 65.011 | 3900 | 59.990 | 16.981 | 17.157 | 0 / 0 / 0 |
| 4 | On | 65.016 | 3900 | 59.985 | 17.007 | 17.189 | 0 / 0 / 0 |
| 5 | Off | 65.041 | 3901 | 59.978 | 17.987 | 18.429 | 0 / 0 / 0 |
| 6 | On | 65.015 | 3900 | 59.986 | 16.940 | 17.195 | 0 / 0 / 0 |

The runtime counts thresholds at strictly greater than 33.000, 50.000 and 100.000 ms (not 33.333 ms). Percentiles above are statistics of the individual five-second windows. They are not whole-run p99 values and must not be averaged or presented as a global p99. FPS is frames divided by the summed log-window durations.

Across three runs per arm: OFF recorded 11,701 frames over 195.073 seconds; ON recorded 11,700 frames over 195.043 seconds. Median per-run FPS: OFF 59.981, ON 59.986. That difference is negligible at the 60 FPS cap. Neither arm recorded a measured >33 ms hitch. All included D3D12 windows reported zero pipelines created, zero synchronous/deferred shader builds and zero pending-pipeline skipped draws, consistent with the warmed shader condition.

## Startup, preparation and memory

| Run | Pre-cache | Process start to command pump (s) | Travel to first valid pawn (s) | Sampled max private / working / GPU-local (MiB) |
|---|---|---:|---:|---|
| 1 | Off | 9.083 | 2.117 | 6815 / 4092 / 3144 |
| 2 | On | 15.436 | 2.118 | 8274 / 5505 / 3215 |
| 3 | Off | 9.082 | 2.117 | 6926 / 4207 / 3168 |
| 4 | On | 13.416 | 2.119 | 8657 / 5914 / 3183 |
| 5 | Off | 9.087 | 2.219 | 6752 / 4088 / 3097 |
| 6 | On | 10.097 | 2.119 | 8654 / 5915 / 3163 |

Median startup-to-command-pump time: OFF 9.083 s, ON 13.416 s. This includes startup work and polling granularity; it is not a direct time-to-interactive UI measurement. Median chapter travel time was approximately 2.12 s for both arms. Median sampled maximum private memory increased by 1839 MiB; working set increased by 1822 MiB. Memory values are maxima of periodic diagnostic samples, not high-frequency absolute peaks.

| Enabled run | Prepared images | Prepared payload (reported MB) | Preparation time (ms) | Staging pages (MiB / ms) | Last reported hits / demand reads |
|---|---:|---:|---:|---|---|
| 2 | 1359 | 2548 | 6003 | 128 / 61 | 620 / 5 |
| 4 | 1777 | 3049 | 3756 | 128 / 21 | 627 / 0 |
| 6 | 1777 | 3049 | 1102 | 128 / 11 | 625 / 0 |

The first enabled run reached the 6000 ms preparation deadline and retained a smaller subset. Successive enabled runs benefited from warming of the operating-system file cache; this explains why equal settings did not imply equal preparation time or coverage. Hit/read counters above are last periodic cumulative reports, not guaranteed final totals.

## Interpretation and limitations

- The six automated runs and valid gameplay checks completed without a crash. Thirteen protected installed/runtime/config/personal-save files remained hash-identical afterward.
- This scenario did not reproduce the previously reported stutter without pre-cache. It cannot establish stutter reduction, a meaningful FPS uplift, or superiority over another project.
- Shader seeds were identical and warmed. The operating-system file cache and driver cache were not purged; background system load and thermal conditions were not strictly controlled. This is not a cold-boot, cold-SSD or first-encounter test.
- The input script is time-based, not frame-perfect replay. AI and draw/upload counts varied, so the runs are comparable scenarios rather than bit-identical scenes.
- The 60 FPS cap limits any throughput comparison. No no-4x, other-GPU, long-session or packaged-PR-build gameplay claim is made.
- Follow-up before claiming a performance benefit: a scene with reproducible baseline texture-read stalls, balanced order and controlled cache state, plus measurements on a clean build of the submitted change. Keep the proposal opt-in/draft.
- Personal saves, raw session logs, game data, cache seeds, generated bundles, textures, binaries and captures are excluded from this public report and package. Only the numerical report and anonymized measurements are included.
