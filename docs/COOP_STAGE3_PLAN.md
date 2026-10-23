# Co-op Stage 3: 8-10 players, bandwidth and AI difficulty

Status: design only, nothing built or run (issue #15). Evidence marks as in [COOP_N_PLAYERS.md](COOP_N_PLAYERS.md):
[V] read in code or dump, [I] inferred. Line numbers are from this checkout. DE/DG = `out/re-mouse/decomp_Engine.txt` / `decomp_GearGame.txt`.
## 0. Summary

- The player count is not the limit. `gow2_coop_players` accepts 2..255 (`src/gow2_script_patch_hook.cpp:35-40`)
  and the rows store N in one byte, so 8 and 10 need no new join rows. Only the menu (2/3/4) blocks them.
- Limits to check above 4: the per-client bandwidth cap (section 3), a few unmeasured UI limits, and a
  discovery risk in the beacon reply copies (row 6), which is not proven to be a cap.
- Bandwidth: the 4-player peak (5.3 KB/s per client) is below every cap, so none is proven. If traffic grows
  linearly, 8 players need 7.6 KB/s: over the 7000 B/s LAN cap and the 4000 B/s internet cap (section 3).
- AI: no per-tier enemy multiplier exists, but Horde's wave mutators scale enemy health and accuracy. Health is
  the lever for the N-based curve (section 4), joining the hash through `GroupHash` / `StackText`.

## 1. Caps at 4 (or 2) today

| # | Where | Effect today | At 8 and 10 |
|---|---|---|---|
| 1 | `src/gow2_options_catalog.cpp:878-882` | Menu offers 2, 3, 4 | Add 8 and 10 |
| 2 | `src/gow2_script_patch_hook.cpp:35-40, 86-87, 281` | Range 2..255; groups apply for 3..255 | Nothing |
| 3 | `src/gow2_script_patch_table.cpp:13-35` (S1-1 `AtCapacity`) | `NumPlayers >= N` at join (byte +0x0008) | Nothing; `NumPlayers` grows per login |
| 4 | `src/gow2_script_patch_table.cpp:90-123` (S1-3, S1-4) | `NumPublicConnections` for beacon and menu | Check `slots_pub` shows 8/10 |
| 5 | `src/gow2_script_patch_table.cpp:125-165` (S1-5, S1-6) | `MaxPlayers` in lobby and SP `InitGame`; `GameInfo.AtCapacity` reads it (DE:20994-21002) | Nothing |
| 6 | `sdk/patches/0011-network-crossplay.patch:33` `kMaxReplyCopiesPerBroadcast = 4`, used at `:3205` | Each beacon broadcast is also copied to at most 4 senders heard in the last second (`:32`); 2 copies/s per sender (`:34`) | **Not a player cap.** `BroadcastFanOut` (`:1421-1431`) still sends every broadcast to the LAN broadcast addresses, every configured target and every peer; the 4 only limits the extra copies for NAT/firewalled senders. `coop-instances.ps1:117-123` lists all 127.0.0.k in `--net_broadcast_targets`, so local tests do not depend on it. Risk: more than 4 joiners searching in the same second without a target or peer entry. `COOP_N_PLAYERS.md:1221` calls it "the player cap"; correct that text. Change the constant only if step 4 shows a missed beacon. |
| 7 | `src/gow2_coop_leash.cpp:53, 75` | Leash starts at 3, no upper bound | Nothing; pairwise check is cheap at 10 |
| 8 | `src/gow2_dev_commands.cpp:420` (`num > 4`) | Local split-screen players | Not network; ignore |
| 9 | `docs/COOP_N_PLAYERS.md:1160-1165` | 4 downed-teammate coordinates, 2 spectator paths, 2 `VoiceTaccomIconPos` slots, 2 lobby rows | Game code not in `src/`; find in `decomp_GearGame.txt` (step 10) |
| 10 | `out/re-mouse/decomp_GearGame.txt:25262, 25270` | Party setup sets `MaxPlayers = 10` | Reference only: 10 is the game's own party limit |
| 11 | `scripts/coop-instances.ps1:57`, `scripts/coop-stage2.ps1:28` (`-Players 4`) | Script defaults | Pass `-Players 8` / `10` |
| 12 | `src/gow2_coop_scenes.cpp:31`, `src/gow2_coop_network.h:8`, `config/gow2.toml:139`, `docs/NETWORK_PLAY.md:14-28` | Help text says "3 or 4" | Update |

### 1.1 Stage 2 survival, respawn, checkpoints and ride (inventory)

All these rows tell an extra from a hero with the "hero test": `GearPC.DefaultPawnClass` is not None, `GearPawn_COGMarcus`
or `GearPawn_COGDom` (`docs/COOP_STAGE2_SURVIVAL.md:20-31`). That is a class compare, not a slot index, so none holds a
count. The groups apply for every N in 3..255: survival at `src/gow2_script_patch_hook.cpp:187-208` (called at `:294`),
coop-ride at `:267`, both only after the Stage 1 group. [V script; behaviour above 4 I]

| Row | Where | What it does | Why no change at 8/10 | Test at N = 8 and 10 |
|---|---|---|---|---|
| S2A-1 travel | `src/gow2_script_patch_survival.cpp:13-50` (`SpawnDefaultPawnFor`, tail `:42-46`) | Sets `bKillDuringLevelTransition = false` on each spawned human pawn | Runs per spawn, no loop or index | Chapter commit (survival test 4 transition B, `COOP_STAGE2_SURVIVAL.md:297-299`): census keeps 6 / 8 extras, no game over |
| S2A-2 respawn | `src/gow2_script_patch_survival.cpp:52-119` (`ProcessGameOver`; hero test `:79-88`, `RestartPlayer` `:100`, retail loop `:105-115`) | A dead extra respawns; a dead hero keeps the retail game over on every PC | Per-PC decision; the `AllControllers` loop is linear in N | Kill 3 extras within 2 s: all respawn near a teammate, no game over; then kill P1: retail game over and the reload brings all N back (test 4 S2A-2 (a), (c)) |
| S2A-3 leave | `src/gow2_script_patch_survival.cpp:121-205` (`GearPC.Destroyed`; class checks `:159-180`) | A leaving extra (or checkpoint dummy holding one) is unpossessed and destroyed; no AI Dom, no `SeqEvent_CoopPlayerLeft` | Class test on the leaver only | `stopone` 2 extras at once, rejoin both; then test 4 S2A-3 (f) (`COOP_STAGE2_SURVIVAL.md:309-313`): no orphan body, no `GearAI_Dom` on a `GearPawn_COGGear` |
| S2A-5 checkpoints | `src/gow2_script_patch_survival.cpp:207-263` (`SaveCheckpoint`; tail `:239-259`) | A dead extra waiting for respawn no longer blocks the save | `AllControllers` loop skips each dead extra; linear | 3 extras dead when a Kismet checkpoint fires: the save is written, a forced wipe reloads with all N |
| S2A-4 split revive | `src/gow2_script_patch_survival.cpp:265-346` | Off by default (`gow2_coop_split_revive`) | Squad name compare, no count | Only with the cvar on, in a split (T6) |
| CR-3 Kismet sync | `src/gow2_script_patch_body_table.cpp:426-480` (`SyncKismetSetProperties`; hero test `:460-476`) | A joiner copies cinematic / god-mode flags only from Marcus or Dom, never from an extra | More extras are more skipped sources; the source is still a hero | At 10, an extra rejoins during a god-mode scene: god=0 after it ends (as `COOP_N_PLAYERS.md:1125`) |
| CR-1/2, CB, SC, leash | `src/gow2_script_patch_body_table.cpp:493-498`; `src/gow2_script_patch_body.h:16`; `src/gow2_coop_scenes.cpp`; `src/gow2_coop_leash.cpp` | Ride protection, bodies, scenes, leash | Same hero test; SC pairs even extras with Marcus and odd with Dom (`docs/COOP_STAGE2_SCENES.md` section 4), so 8 extras split 4/4 | Section 2 and step 10 |

Open risk: the retail respawn timer is 5 s per death; at 8-10 several can be armed at once and S2A-5 must skip all of
them. Splits are untested (T6).

## 2. Extra pawns for players 5-10

- **Class.** S1-2 (`src/gow2_script_patch_table.cpp:38-88`) gives everyone from player 3 on the same class:
  `GearPawn_COGBenjaminCarmine` where loaded (chapters 2, 5-11), else `GearPawn_COGRedShirt`. Players 5-10 take the same path. [V]
- **Body.** coop-body (`src/gow2_script_patch_body.h`, CB-1..5) gives every extra Dom's mesh on `GearPawn_COGGear`,
  the pistol and Gnasher, and a spawn without Right Trigger. Up to 8 identical bodies; names differ
  (`--gow2_coop_extra_carmine=true` if the look matters).
- **Spawn points.** Players 3+ go through `SpawnDefaultPawnFor` (patched by S2A-1) and `FindPlayerStart`: a BFS within
  1024 uu of a teammate, skipping cover markers and occupied spots, then a `PlayerStart` fallback with
  `bNoCollisionFail` [V, `docs/COOP_N_PLAYERS.md` 1166-1168]. At 8-10 free spots run out sooner, and the
  fallback can stack pawns. The leash separates overlaps (`src/gow2_coop_leash.cpp:222-285`). [I] enough at 10.
- **Cinematics.** SC-1 mirrors Marcus's cinematic mode to each extra (linear, cheap). SC-6 holds a late Dom
  until the cinematic ends. Run T1 and T4 with 8 instances.
- **Vehicles.** CR-1 protects extras on foot while a hero rides; CR-2 brings them to the hero when he gets out.
  Seats belong to the two heroes, so 8 extras crowd the vehicle. Untested with more than 2 extras.
- **Scripted sequences.** Kismet teleports move heroes only; SC-4 moves paired extras beyond 1500 uu.
  `FindAvailableTeleportSpot` can find no spot (`docs/COOP_STAGE2_SCENES.md` item 4), so more get left at 8.
  `PawnList` is newest-first, so a Marcus clone would steal Kismet lookups (`docs/COOP_N_PLAYERS.md` 1.3): keep
  `gow2_coop_marcus_clones` off.
- **Risk order.** (1) spawn crowding and leash churn, (2) missed teleport spots, (3) vehicle crowding, (4) cinematic cost.

## 3. Bandwidth

Measured on loopback, scripted play, `--net_traffic_log_sec=10` [V, `docs/COOP_N_PLAYERS.md` 1092, Stage 1 row 12]:
host to each client (4 players) 3.0-3.5 KB/s average, **5.3 KB/s peak** (about 29 packets/s); host upload total
(3 clients) 15.8 KB/s peak; client to host 0.7 KB/s average, 1.6 KB/s peak. The per-client rate is the lowest of these:

- Client request: `ConfiguredInternetSpeed=7000` (`out/re-mouse/Coalesced_int_full.txt:4983`); clients use `open <ip>`.
- Host: `InitGame+0x00BC` sets `MaxDynamicBandwidth = 7000` (DG:46878), copied to `AdjustedNetSpeed` (DE:20464); a listen-server host clamps client requests to it (DE:4878-4882).
- Off LAN, `UpdateNetSpeeds` sets `AdjustedNetSpeed = Clamp(TotalNetBandwidth / max(NumPlayers,1), MinDynamicBandwidth,
  MaxDynamicBandwidth)` (DE:21289-21312); Coalesced `TotalNetBandwidth=32000`, `MaxDynamicBandwidth=6000` (overridden to 7000 by
  `InitGame`), `MinDynamicBandwidth=4000` (`Coalesced_int_full.txt:5557-5559`). Cap: 7000 at N=2-4, 6400 at 5, 5333 at 6, **4000 at 8 and 10**. LAN skips it (DE:21295).
- Native: `[IpDrv.TcpNetDriver]` `MaxClientRate=10000` (5002), `MaxInternetClientRate=7000` (5003). Their applying
  code is not in the dumps, so which one binds is unknown [I]. (`MaxClientRate=25000`, line 4851, is `DemoRecDriver`.)

The 7000 is configured, not measured: no run has reached it. Step 6b tests it.

Extrapolation [I], two models kept apart. **Flat**: each client's traffic stays at the measured 5.3 KB/s peak and
only the client count grows (host upload = 15.8 × (N-1)/3). **Linear**: each client's traffic grows with the other
humans' pawns, 5.3 × (N-1)/3. NPC replication is the same for every client, so the truth lies between them.
*Demand* is what the game would send with no cap; *limited* is (N-1) × min(per-client demand, cap).

| Players | Clients | Per-client peak demand, flat / linear | Host upload demand, flat / linear | Host upload limited, LAN 7000 B/s, flat / linear | Off LAN 4000 B/s, flat / linear |
|---|---|---|---|---|---|
| 4 (measured) | 3 | 5.3 / 5.3 KB/s | 15.8 / 15.8 KB/s | 15.8 / 15.8 KB/s (ceiling 21) | 15.8 / 15.8 (cap 7000 at N=4) |
| 8 | 7 | 5.3 / 12.4 KB/s | 36.9 / 86.8 KB/s | 36.9 / 49 KB/s (about 392 kbit/s) | 28 / 28 KB/s |
| 10 | 9 | 5.3 / 15.9 KB/s | 47.4 / 143.1 KB/s | 47.4 / 63 KB/s (about 504 kbit/s) | 36 / 36 KB/s |

Flat fits under the LAN cap; linear is clamped (each client starved by 5.4 KB/s at 8 and 8.9 KB/s at 10). Off LAN
both models clamp. Step 6 measures which model holds. Engine settings, in order:

1. `MaxDynamicBandwidth`: IntConst at `InitGame+0x00C3` (opcode 0x1D at +0x00C2, value 0x1B58). Same-size
   edit, so the script keeps its length. Put it in a new group, not S1-6 (which only checks those bytes).
   [V bytes, `src/gow2_script_patch_table.cpp:155`]
2. `ConfiguredInternetSpeed` (`Engine.Player`, Coalesced line 4983) via the bundle (`src/gow2_coalesced.cpp`) or
   the default object. Every peer must match.
3. `MaxInternetClientRate` (line 5003) and `MaxClientRate` (5002): raise both so either one allows the new value.
4. Off LAN only: `MinDynamicBandwidth=7000` in the bundle, so the clamp returns 7000 at any N (or
   `TotalNetBandwidth` ≥ 7000×N: 56000 at 8, 70000 at 10). Leave both alone on LAN.
5. Tick: `NetServerMaxTickRate=45`, `LanServerMaxTickRate=35` (5007-5008). Not changed by N. [V values, I semantics]

Packets stay at 512 bytes or less, so size is not a factor.

## 4. AI difficulty that scales with player count

### 4.1 Today [V unless marked]

- Difficulty is per player: `GearPRI.Difficulty` is one of the `DifficultySettings_*` objects, set by the URL
  (DG:24034-24047). A joiner copies the host's value (DG:24059; context [I]). Values: `Coalesced_int_full.txt:7317-7418`.

| Field | Casual | Normal | Hardcore | Insane | Read by |
|---|---|---|---|---|---|
| PlayerHealthMod | 2.35 | 1.5 | 1.0 | 0.35 | human pawn health, DG:9208 |
| PlayerDamageMod | 2.0 | 1.6 | 1.1 | 1.0 | damage dealt by a human, DG:104212 |
| FriendHealthMod | 3.5 | 3.0 | 3.0 | 2.5 | teammate AI health, DG:9217 |
| EnemyEvadeChanceScale | 0.0 | 0.5 | 1.0 | 1.0 | `GetLowestPlayerDifficultyLevel`, DG:16535 |
| bAllowLocustBlindFire | false | false | true | true | `GetLowestPlayerDifficultyLevel`, DG:17648 |

- AI follows the **lowest** human tier (`GetLowestPlayerDifficultyLevel`, DG:9232-9248), the right rule for a shared fight.
- **No per-tier enemy multiplier exists** (no `EnemyHealthMod`; the table has only human, friend and bot fields).
- **Horde has enemy multipliers** (`Coalesced_int_full.txt:5729-5732`, per wave, 2.0-2.5): `GearMut_ExtraLocustHealth`
  (`DefaultHealth *= Multiplier`, team 1, DG:55003-55010); `GearMut_LocustAccuracyMultiplier` (`AimErrorMultiplier = 1/Multiplier`, DG:55012-55017); `GearRules_COGDmgTakenMult` (team 0 damage, DG:79452-79463).
- **AI accuracy:** `GearAI.GetAccuracyRotationModifier` (DG:16159-16172) scales the AI cone by `AimErrorMultiplier` (higher = worse). Campaign default not in dumps.
- **The 46 matches** for `DamageScale|HealthScale|Accuracy`: none is `HealthScale`. `DamageScale=2.0` is
  `GSM_LadderClimbDown/Up`, `AIDamageScale` is grenade splash, `HeadShotDamageScale` is per weapon.

### 4.2 Formula

Let N be the human count and s = `gow2_coop_ai_scale` (0 = off, 1 = full curve): `m_hp(N) = 1 + s * (sqrt(N / 2) - 1)`,
so at s = 1: N=2 → 1.00, 4 → 1.41, 8 → 2.00, 10 → 2.24. At 2 players retail is unchanged. The square root reflects
that time-to-kill falls roughly as 1/N: a linear N/2 would keep fights from getting shorter at all. This is a design
choice; step 8 tunes it. Human-side health (`PlayerHealthMod`) stays a per-player choice.

Where it goes. Health: multiply team 1 pawns' `DefaultHealth` at spawn, as `GearMut_ExtraLocustHealth.ModifyPlayer`
does (`Health` may need it too [I]; open question 3). DG:104200-104217 and DG:11690-11694 are not enemy levers (they act
on damage dealt by or to humans). Accuracy: no N term; keep `AimErrorMultiplier` at 1.0.

### 4.3 Into the co-op hash

Today: `GroupHash(group, players)` is FNV-1a over N and every row (`src/gow2_script_patches.cpp:569-574`,
`:501-518`). `StackText` is `players=N` plus `name=state` per group (`src/gow2_script_patch_stack.cpp:16-22`),
`StackHash` is FNV-1a 64 over it (`:24-32`), and the value is published to `net_coop_hash` and gated by SDK
patch 0012 (`src/gow2_script_patch_hook.cpp:271-301`). A new group needs no hash code changes if:

1. The scale is a **row value** (so `GroupHash` covers it), not only a cvar.
2. The group is in `CoopGroupsByName()` (`src/gow2_script_patch_groups.h`), so it gets a `name=state` entry and `off` changes the hash.
3. It is fail-closed like the others, so `skipped` also changes the hash.

Test: peers with s = 1.0 and s = 0.5 must not see each other ("co-op hash ... differs"), as in Stage 4 test 3.
Open: the hash does not see each player's own tier; make the host's tier a `StackEntry`, or rely on DG:24059 [I].

## 5. Implementation plan

Sizes for one person: S < half a day, M 1-2 days, L several days. Tools: `scripts/coop-instances.ps1 -Coop -Players N -Count K`, `scripts/coop-stage2.ps1`.

| Step | Change | Size | Test on one PC | Pass when |
|---|---|---|---|---|
| 1 | Menu gets 8 and 10 (catalog `:878-882`); help text (section 1 row 12) | S | `start -Coop -Players 8 -Count 1`; log shows `coop: 6 rows applied for 8 players` | Log shows 8, no `skipped` |
| 2 | Offline check at N = 8 and 10 (`tools/script_patch*.py --check`); add N cases to the tests | S-M | `python -I tools/script_patch.py --check <folder>`; `python -m pytest tools/tests -q` | Every row decodes; N = 8 and 10 hashes recorded here |
| 3 | Join test at 8, then 9 instances (scaled Stage 1 steps 8-10) | M | `-Count 9 -Players 8 -Width 640 -Height 360 -Common "--resolution_scale=1 --audio_mute=true --gow2_unlock_fps=true --gow2_fps_cap=30"`; `stats` for RAM and VRAM | Eight join and spawn; the ninth is refused; `combined=` matches on all |
| 4 | Beacon discovery at 8 (row 6), no code change: one run with the default targets, one where joiners 5-8 drop `--net_broadcast_targets` and search in the same second | S (test) | `--system_link_trace=true`; `first datagram from` lines and each joiner's server list | All joiners see the host. Only if one misses it: make `kMaxReplyCopiesPerBroadcast` follow N in `sdk/patches/0011`, rebuild with `scripts/build-sdk.ps1`, repeat |
| 5 | Survival inventory at 8 and 10 (section 1.1): travel, respawn, leave, checkpoints, CR-3 | M (test) | `scripts/coop-stage2.ps1` with `-Players 8`, then 10; `--gow2_coop_census=true`; `COOPLEASH` lines | Every test in the 1.1 table passes; every extra spawns; no orphan bodies |
| 6 | Baseline at 8, before any change | S (run) | `--net_traffic_log_sec=10` on all; 5 minutes of scripted play; log `AdjustedNetSpeed` | Per-client peak measured against both models and the cap (section 3) |
| 6b | Cap probe: `ConfiguredInternetSpeed` and `AdjustedNetSpeed` at 12000, `MaxInternetClientRate` at 7000; 2 instances under load above 7 KB/s (e.g. a horde wave) | S-M | Peak stuck at 7000 means `MaxInternetClientRate` binds; above 7000 means it does not | Binding cap known (open question 1) |
| 7 | Group `coop-net`: IntConst at `InitGame+0x00C3`, `ConfiguredInternetSpeed`, `MaxInternetClientRate`, `MaxClientRate`, `MinDynamicBandwidth=7000` off LAN (bundle `src/gow2_coalesced.cpp`); gated on 3+ | M | Repeat step 6 | No window pins at the cap; host upload under the new ceiling |
| 8 | Group `coop-ai` with the section 4.2 curve, default off; check campaign spawns reach the Horde path (4.1) first | M | Fixed fight at N = 8 with s = 1, timed against N = 2 | Time-to-kill measured, then tuned |
| 9 | Hash check with mismatched s or a skipped group | S | As Stage 4 test 3 | Gate hides the mismatch both ways |
| 10 | Splits and vehicles at 8: `SP_Outpost_07a_S`, `SP_Intervention_10_S` (T6); a ride with 4 extras (CR-1/2); find the UI limits (row 9) | M | Watch parity, squads, vehicle exit | No game over from an extra; each extra with the right hero |
| 11 | Soak at 8 and 10: 10 minutes of combat plus one checkpoint reload | L | `stats` each minute; `[nettraffic]`; census | No crash, no desync, no rubber-banding |

Steps 1-5 need no bandwidth change and can go first. Step 7 is needed only if step 6 or 6b shows a binding cap.
Step 8 is independent and can run alongside 3-7.

**Testing 8 and 10 on one PC.** One RX 7800 XT ran 4 instances. For 8-10: `-Width 640 -Height 360` (`scripts/coop-instances.ps1:44-45`);
`--resolution_scale=1`; `--gow2_unlock_fps=true --gow2_fps_cap=30` (the cap only works with unlock on, `src/gow2_fps_cap.cpp:24-26`);
`--audio_mute=true`; `-StaggerSec 10`. Each instance gets its own 127.0.0.k address, user folder and log. Watch
`coop-instances.ps1 stats`. If one PC cannot hold 8, use two to four PCs with two to four instances each, on real LAN
addresses, with `net_broadcast_targets` listing every PC (`docs/COOP_N_PLAYERS.md` Stage 3 test).

## 6. Open questions

1. Which of `MaxClientRate` or `MaxInternetClientRate` clamps (step 6b).
2. The UI functions behind the 4-coordinate, 2-slot and 2-row limits (section 1 row 9).
3. Whether `GearMut_ExtraLocustHealth.ModifyPlayer` (DG:55003) runs on campaign locust spawns, and whether `Health` must also be set.
4. Whether DG:24059 copies the host's tier to every joiner (the hash depends on it).
5. Whether party `MaxPlayers` (DG:25262) affects campaign co-op (it appears set only in party setup).
6. Whether `UpdateNetSpeeds` runs in local tests: it skips only with a LAN `GameSettings` (DE:21293-21295).
