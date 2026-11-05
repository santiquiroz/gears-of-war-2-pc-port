# Direct draws for low-end PCs: design

Date: 2026-11-03. Status: approved (the owner delegated the open calls on 2026-11-03).

## Intent

Players with old or 4-core CPUs (issue #37: i5-3570, 4 cores and 4 threads, GTX 970) run the port at
20-30 fps with the CPU at 100 % and the GPU at about 10 %. The GPU is not the limit; CPU time is. The
owner wants the port playable on cheap PCs first, and expects the "native GPU call" (the game's Direct3D
draws reaching the host GPU without the Xbox 360 command emulation in between) to be what fixes it.

Success: on a 4-core, 4-thread CPU, the chapter 2 hospital (the scene of the PC numbers in
`docs/PERFORMANCE.md`) holds 30 fps, Windows, Direct3D 12, same picture as today.

How it is measured:

- Day to day, on the 7900X3D: 4 physical cores without SMT (affinity mask 0x55) plus the power
  plan's maximum processor state lowered until one core is about as fast as an i5-3570's. Masking
  cores alone is not a proxy: on 2026-11-03 the 4-core run still gave about 145 fps in the hospital.
- Every step also reports CPU milliseconds per frame on the game render thread and the GPU command
  thread, at full clock and on the proxy. A step counts only if it cuts them.
- At the end, test builds go to the #37 reporter (i5-3570) to confirm on the real machine.

## Goals

- Cut the CPU time per frame on 4-core CPUs until the success target holds.
- The draw path: the game's Direct3D state reaches the host backend in bulk, without per-dword
  compares and virtual register writes, and without the game's Direct3D library building console
  packets that the port then throws away.
- Threads that wait by spinning stop stealing cores from the threads that work when the CPU has fewer
  than 6 logical processors.

## Non-goals

- Phones (Android stays as it is; the front end default by GPU does not change).
- Higher frame rates on fast PCs (they may improve; not a target).
- Shaders compiled ahead of time (XenosRecomp, #22): later, separate design.
- Removing the Xbox 360 register file from the SDK: every backend subsystem (pipeline cache, render
  target cache, texture cache, primitive processor, `draw_util`) reads `const RegisterFile&`; rewriting
  about six classes is out of scope. The register file stays the backing store; it is filled in bulk.
- Proton: measured and reported, not a target (vkd3d-proton adds its own CPU cost; Graphics API →
  Vulkan avoids it).

## Anti-goals (a failure even if fps goes up)

- Any change in the picture (image diff above 0.02 against the current default path in the hospital
  and chapters 1, 4, 8).
- Adreno phones losing their current 60 fps or getting GPU hangs back (#33): the front end is on there.
- An option hidden or forced by vendor in the menu: Automatic picks the best default, every machine can
  choose. Automatic may decide by the number of logical processors (the owner agreed on 2026-11-03).
- System Link / co-op desyncs or saves that stop loading.

## Constraints

- SDK changes stay numbered patches in `sdk/patches`; no layout change of exported classes and no new
  virtual functions (the exe is built against unpatched headers). Use the existing virtuals
  (`WriteRegistersFromMem` on D3D12 and Vulkan) and file-local code.
- Game runs are idle-gated (`out/idle-gate`, `scripts/chapter-sweep.ps1`) or explicitly approved.
- Every step keeps the current path selectable (a cvar) until it passes its exit check, then becomes
  the default where it measured better.
- Delegation: implementation goes to external lanes under contract; Claude reviews, verifies, commits.

## What exists (map from 2026-11-01, read-only research)

- Front end (`src/gow2_native_frontend.cpp`): hooks `sub_82980B20`, `sub_82980210`, `sub_8297FD30`,
  Clear `sub_82981ED8`; per draw snapshots the guest `D3DDevice` (struct `NativeDrawSnapshot`, 2,560
  bytes, `src/gow2_native_frontend_snapshot.h`) with 366 byte-swapped loads, two copies of the state,
  and walks, NOPs and compacts the packets the library already built (`sub_8298F600` flush). The draw's
  packets become a type-3 NOP marker in the ring pointing at the snapshot.
- SDK consumer (patches 0056-0058, `CommandProcessor::ExecutePacketType3_NOP`): compares 2,415 dwords
  against the register file and calls the virtual `WriteRegister` per changed dword, then runs
  `ExecutePacketType3Draw` and `IssueDraw`.
- With the front end on, the PC frame drops from 263 to 151 fps (RX 7800 XT, hospital); 0.3.8 turned it
  off on PC. The research infers most of that cost is on the game's render thread (the hook plus the
  library still building every packet), not on the command processor: prototype 0064 skipped unchanged
  constant blocks on the command-processor side with no gain. Not measured yet.
- Spin waits: the GPU command thread yields 500 times before each 5 ms wait
  (`command_processor.cpp:599`); `WAIT_REG_MEM` yields when vsync is off; the frame cap spins its last
  0.5 ms (`src/gow2_fps_cap.cpp:66`). With fewer than 6 logical processors the SDK skips thread
  affinities (`xthread.cpp:897-903`).

## The plan, decided

Step 0 decides the order of the rest; each later step has its own exit check.

0. **Measure on 4 cores.** Process affinity to 4 physical cores without SMT (mask 0x55 on the 7900X3D)
   plus `--ignore_thread_affinities=true`, frame rate unlocked, hospital: fps, and CPU time per thread
   (game render thread, GPU command thread, the other guest threads, audio) with the front end off
   (today's PC default) and on. Output: which threads saturate the 4 cores, and how far from 30 fps.
   First A/B, front end off, full clock (`out/perf/four-core/`): all cores 230-250 fps, p99 frame
   time 8-11 ms; 4 cores 145 fps, p99 20-27 ms. The SDK's "Too few processor cores" check counted the
   system's processors, not the mask, so its affinity skip did not trigger; the mask was set by a
   watcher after launch.

   Result of step 0 (2026-11-03, `out/perf/step0/`, hospital, CPU % of one core per host thread):

   | Run | fps | busiest guest thread | render thread | GPU command thread |
   |---|---|---|---|---|
   | engine cap (30) | 30 | 105 % | 21 % (7.0 ms/frame) | 12 % (3.9 ms/frame) |
   | gow2 cap 60 | 60 | 105 % | 38 % (6.5 ms/frame) | 26 % (4.3 ms/frame) |
   | unlocked, front end off | 180-200 | 106 % | 97 % (5.4 ms/frame) | 58 % (3.2 ms/frame) |
   | unlocked, front end on | 153 | 106 % | 86 % | 64 % |
   | 4 cores, clock limit 45 % (perf counter 67 %) | 142-150 | 103 % | 80 % | 59 % |

   - One guest thread (handle F80000C8) uses a whole core at every frame rate. Stack samples (cdb,
     non-invasive) put it in `sub_823348E0`, called from `sub_82334BA8`: a loop that polls a queue
     and the guest clock (`sub_82AA7B78` = QueryPerformanceCounter) and never blocks. On a 4-thread
     CPU that is a quarter of the machine gone before the game draws anything.
   - At 30 fps the render thread needs 7 ms and the command thread 4 ms per frame: the draw path is
     not what saturates. The front end on costs fps on PC (153 vs 180-200) without cutting either.
   - The power plan cannot slow this CPU below 67 % of its rated clock, so the day-to-day proxy cannot
     reach the i5-3570's speed; CPU time per thread is the working measure, and the #37 reporter's
     test builds the real one.

   Step 4, first part, done on 2026-11-03 (136d18c, `out/perf/step4/`): the worker sleeps 1 ms when
   its queue is empty (`gow2_idle_worker_sleep`, on everywhere). At 30 fps the game's CPU drops from
   166 % to 69 % of one core; on 4 cores unlocked, 150 to 184 fps (p99 22.9 to 20.5 ms). A 12-minute
   hospital soak held 30 fps with no errors. Shipped in 0.3.13 without the Adreno run (no Adreno phone
   available); Android reports from players stand in for it.

   So the order is: step 4 first, aimed at the guest thread above (not only the SDK's waits); then
   step 1 and step 2 only if the reporter's numbers still show the render thread saturated.

   Front end cost, 2026-11-04 night (hospital, unlocked; other programs were busy on the PC, so fps
   swing between runs and only consecutive runs compare; `out/perf/step2/`):

   - Step 2, first part, SDK patch 0066 (c45d120): bulk apply as above, except that ALU blocks are
     compared rather than skipped by pointer (a kept packet can write ALU registers between two native
     draws, so a pointer match alone is not safe). Front end on, A/B/A/B: 112 and 160 fps without it,
     211 and 185 with it.
   - Front end on against off after 0066, consecutive runs: all cores 194 on, 235 off; 4 cores (mask
     0x55, clock at the 67 % floor) 177 on, 182 and 189 off. Still slower on PC, so the PC default stays
     off (exit check not met).
   - Where the on-path time goes (`gpu_native_frontend_timing`, SDK patch 0067, and
     `gow2_native_frontend_timing`, both off by default), about 330 draws a frame:

     | Thread | Part | Per draw call | Per frame |
     |---|---|---|---|
     | game render | hook after each draw call (snapshot, packet walk, ring rewrite) | 3.0-3.3 us | about 1.0 ms |
     | game render | constant mirror | 0.15 us per constant load | 0.13 ms |
     | GPU command | snapshot apply (after 0066) | 1.2 us | 0.4 ms |
     | GPU command | shader binding | 0.2 us | 0.06 ms |
     | GPU command | draw issue (same as the console path) | 2.4 us | |

     The hook splits into read 0.35, state 0.68, plan 1.0, alloc 0.85 and store 0.35 us. A register slot
     table (one load per register instead of a range test) took plan to 0.8 us; verification without
     suppression showed 0 mismatches in every group over about 870,000 draws. Most of what is left in
     state and alloc is writing about 3.5 KB a draw into arena memory that is never in cache; shrinking
     it needs a new snapshot format (state and ALU tables shared between draws), and fetch and ALU
     change on most draws, so the gain is small.
   - Step 3 is not built: a shader binding costs about 0.2 us (the XXH3 of a few KB of microcode and a
     map lookup), 0.06 ms a frame on a thread that is not the bottleneck.
   - What remains between on and off is the game-side hook, about 1 ms a frame on the game render
     thread. Step 1 (the library no longer building the packets the front end replaces) is the change
     that removes both the packet building and most of the hook; it is the next step for the front end.
1. **Game side: no packets for hooked draws.** For draws the front end takes, the library's flush does
   not build console packets; the device's dirty-mask bookkeeping is kept; the ring carries only the
   marker and the packets the snapshot does not cover (the S2b plan in
   `2026-10-23-full-native-renderer-design.md`). The snapshot is written once, straight into the arena,
   in raw guest byte order (`memcpy` instead of 366 swapped loads), with bitmasks of the fetch and bool
   slots that changed.
2. **SDK side: bulk apply.** In the NOP handler, `memcpy` the non-constant groups into the register
   file (no side effects there), compare fetch per 6-dword slot and notify only changed slots, skip ALU
   blocks whose pointer did not change and push the rest through `WriteRegistersFromMem`, then build the
   index buffer info from the draw words and call `IssueDraw` directly (keeping the viz-query check,
   `MsaaFillAt1x` and `gpu_native`).
3. **Shader binding cache** keyed by guest address and size, invalidated on packet shader loads and on
   the memory watch, so alternating shaders skip the XXH3 of the whole microcode.
4. **Spin waits on small CPUs.** The guest polling loop `sub_823348E0` (thread F80000C8) blocks or
   sleeps briefly when it has no work, on every machine if it costs nothing measurable, else on
   fewer than 6 logical processors. With fewer than 6: the GPU command thread and `WAIT_REG_MEM`
   block on their event after a short spin instead of 500 yields; the frame cap sleeps instead of
   spinning. Kept as is on 6 or more.

Order after step 0: if the render thread saturates, 1 then 2; if the command thread does, 2 then 1; if
spinning threads do, 4 first. Step 3 goes with whichever of 1 or 2 lands second.

## Exit checks

- Picture: image diff at most 0.02 against the default path in the hospital and chapters 1, 4, 8.
- Stability: 10-minute soak in the hospital; one System Link session between two instances.
- Performance: the 4-core hospital fps from step 0, after each step. Target 30 fps.
- Android: one Adreno run (front end on) does not regress; Mali unaffected (front end off).
- Default: the front end becomes the PC default again only if, on both 4 cores and all cores, it is at
  least as fast as the console-command path.

## Calls made in the design (not the owner's; check them)

- If step 0 shows a bottleneck outside the three cases (game simulation threads, audio), or the target
  already holds, the work stops after step 0 and goes back to the owner with the numbers.
- If one step reaches the target, the remaining steps are still built only where they cut CPU time per
  frame measurably on 4 cores; otherwise they stop there.
- Step 1 patches the guest flush so it skips only the emitters the snapshot covers (the uncovered
  packets, about 1,070 a frame, keep coming from the library as today). Reimplementing the flush in C++
  is out unless that measures too small.
- "Fewer than 6 logical processors" (step 4) counts the processors in the process affinity mask, so a
  masked test run triggers it.

## Left to the builder

- Names of files, functions, cvars and patch numbers (next free after the last in `sdk/patches`).
- How the per-thread CPU time is taken in step 0 (ETW, `GetThreadTimes` sampling, or the SDK's
  counters).
- The exact spin length before blocking in step 4.
- Whether steps 1 and 2 share one snapshot format bump (NDS4) or two.
