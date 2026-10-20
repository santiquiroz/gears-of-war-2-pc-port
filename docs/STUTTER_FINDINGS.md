# Stutter: what the hospital run showed, and the fixes

Two changes, both SDK patches, both in the Windows D3D12 backend: **0024** makes the pixel-shader-interlock ("ROV") path the
default way to emulate the Xbox 360 EDRAM on NVIDIA (the render target stage of the frame), and **0025** stops the command
thread waiting for pipeline compiles (the draw submission stage). AMD keeps host render targets, so 0024 changes nothing there.

## ROV default on NVIDIA (patch 0024)

Measured on one machine (RTX 5080, 1280x720 at `resolution_scale = 2`, vsync on, 2 to 3 runs per scene, runs agree within about
1 to 2 %). With host render targets the card needed 20 to 26 ms per frame while moving, so those frames arrived late and were
shown twice; with ROV the same picture costs about half the GPU time.

| Scene | Host render targets | ROV |
|---|---|---|
| Hospital, walking | 58 fps, 39 hitches/min, worst frames 33 ms | 60 fps, 0 hitches, worst frames 19 ms |
| Outdoor assault | 40 fps, worst frames 33 ms | 60 fps, worst frames 19 ms, about 1 hitch/min |
| Landown (large outdoor) | 50 fps, 235 hitches/min | 60 fps, 2 hitches/min |
| GPU time per frame | 13 to 26 ms | 6 to 11 ms |

Not tested: AMD and Intel GPUs, most chapters, co-op, long sessions. `render_target_path_d3d12 = "rtv"` goes back to the old path.
With `vsync = false` the hospital runs at about 135 fps.

## Pipeline compile stalls (patch 0025)

## TL;DR

A 12-minute play of the hospital level (21:9, ROV path, no upscaler) ran at the normal 16.6 ms a frame for 99.4 % of its
frames. The rest were freezes of 300 ms to 3.9 s, and **every one of them was a frame that built a new shader pipeline**
(125 of the 147 frames slower than 40 ms; the other 22 were a 20 fps video). Together they cost 42 seconds. The machine was
clean: no other program on the GPU, no throttling, 17 GB of memory free, CPU at 14 % median.

Cause: the renderer did compile the new pipelines in background threads, but **waited for all of them at the end of every
frame** and **built draws without a pixel shader (shadow maps, depth passes) on the spot**. A new heavy pipeline costs about 300
ms in the NVIDIA driver, so the frame froze for that long. SDK patch 0025 removes both waits. A draw whose pipeline is not
ready is skipped until it is, so a new effect can be missing for a few frames instead of the game freezing.

## What the run recorded

Setup: the game started normally (its own shader caches), with a per-frame log (wall clock, GPU span, pipelines and shaders
built, draws skipped, uploads), `log_file`, and a one-second sampler of GPU, CPU, memory, page faults, disk, and per-process
GPU use. The per-frame log was a local tool and is not part of these patches; the counters it recorded are the ones
`gow2_frame_log` and `d3d12_stats_log` report (frames, draws, draws skipped while their pipeline is pending, pipelines and
shaders built).

| | |
|---|---|
| Frames | 40,795 in 723 s (56 fps average, the boot and loads included) |
| Gameplay frames at normal pace | 99.4 % (p50 16.6 ms, p99 19.7 ms) |
| Frames over 40 ms | 147 |
| ...of which built a pipeline or shader | 125 |
| ...of which a 20 fps video near the start | 22 |
| Pipelines built during play | 530 (366 shaders) |
| Time lost to them | 42 s |
| Frames with 2 or more pipelines built | 300-450 ms each (median 306-364 ms); 1.1 s for 19, 3.9 s for 88 |
| Draws skipped while a pipeline was not ready | 1,875 in 226 frames |
| GPU | 2,925 MHz, 49 C, no throttling, no other process using it (Claude and the desktop, a few samples) |
| Input guard | 24 overlap warnings, never a timeout or a wait over 1 ms |

## Why short or repeated test runs did not see it

The hospital route used for the first tests had been played many times, so the game's own pipeline storage and, more to the point, **NVIDIA's
driver shader cache** (`%LOCALAPPDATA%\NVIDIA\DXCache`, shared by every program) already held everything: a pipeline found
there is created in about 0.2 ms, a new one costs 300 ms. A fresh game cache alone does not make a run cold; the driver cache
has to miss. To get a cold run, change something that changes the shader code, for example `resolution_scale` (it did here: the new\nscale compiled new pipelines): `resolution_scale=4` with a new `cache_root` reproduced the stalls (23 hitches, 6.3 s lost, one
`SLOWPIPE 276 ms` line for a pipeline without a pixel shader created on the command thread).

## The two waits (SDK patch 0025)

1. `PipelineCache::EndSubmission` created the queued pipelines on the command thread and then waited for every creation
   thread to finish. The frame therefore lasted as long as the slowest compile. Now only with
   `d3d12_pipeline_wait_at_submission_end = true`.
2. `ConfigurePipeline` created a pipeline **synchronously** when the draw had no pixel shader. Now it goes to the creation threads
   too.

Measured on the same scene with new shaders (hospital_walk, resolution scale 4 before, 5 after, so the after case compiled six
times as many pipelines):

| | Before | After |
|---|---|---|
| Hitches over 20 % off the pace | 23 | 7 |
| Time lost | 6,299 ms | 307 ms |
| Worst frame | 378 ms | 102 ms |
| Draws skipped | 41 (1.0 % of frames) | 7,662 (20.8 % of frames) |

The skipped draws are the price: while a pipeline compiles in the background (about 12 s for the 213 pipelines of that very cold
run, a fraction of a second for the one or two a normal game moment needs) the objects that use it are not drawn. Set
`async_shader_compilation=false` to have the old behaviour of waiting for every pipeline.

Also added: `SLOWPIPE` (a pipeline call over 15 ms on the command thread, with whether it was async, whether it had a pixel
shader, and the shader hashes) and `SUBMITWAIT` lines in the game log, 200 at most each.

## Measured on real play: the same hospital level, before and after

Both runs: a person playing at the keyboard, 21:9, ROV path, no upscaler, logging as above. The second run was on the fixed build
(patch 0025) and 7 minutes long, the first 12.

| | Before the fix | After |
|---|---|---|
| Gameplay frames over 40 ms | 126 | 4 |
| ...worst gameplay frame | 3,945 ms | 63 ms |
| Time lost to them | about 42 s | about 0.2 s |
| Gameplay frame time p99.9 | 321 ms | 20.5 ms |
| Pipelines built during play | 530 | 228 |
| Frames that built a pipeline | 226, stalling 300 ms or more | 130, median 16.9 ms |
| System (GPU clocks, other programs, memory) | clean | clean |

The second run was not on a cold cache: the first run had already put its 530 pipelines in the game's storage and the driver cache,
yet it still built 228 more (parts of the level the first run had not reached), and none of them stalled.

Cost of the fix, as seen in the second run: draws were skipped in 63 episodes (gaps under 0.3 s merged), 18.5 s in all of 420 s, the
longest 2 s, usually a handful of draws of the 1,700 a frame (up to 75 in one frame): a character, a prop or an effect missing for a
fraction of a second as it appears. The first run skipped as many draws, hidden inside frames that were frozen anyway.

What is left in the log: a 3 s frame at start-up (the title screen loading), 50-66 ms frames for 5 s at about 7 s (a 20 fps video:
the analysis tool calls them cpu/game hitches), and 3 frames of 42-49 ms during play with no pipeline built (large texture and
shared memory uploads at a streaming moment, 28-46 MB in the frame).

## Not done, and what is next

- The remaining 100 ms frames of the cold run are GPU-bound frames of the 5x scale and a few creations the command thread still does
  itself (root signatures); not investigated.
- Nothing hides the missing draws while a pipeline compiles. Options: a simpler stand-in pipeline for the first frames, or
  building the whole pipeline list ahead of time on a first run ("warming" the cache from the game's own storage, which only
  needs the driver to compile; it must stay a local cache, not something we upload).
