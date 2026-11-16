# Stage 2 group A: survival of players 3+

This document covers Stage 2 items 1-3 of [COOP_N_PLAYERS.md](COOP_N_PLAYERS.md):
chapter travel, game over, leaving. It also covers checkpoint saving while an
extra player is dead.

Status: rows written and checked offline against the retail packages
(`tools/script_patch_survival.py --check`) and on synthetic guest memory.
Run in game on 2026-10-16: see COOP_N_PLAYERS.md, Stage 2 results.

The group "survival" (rows S2A-1, S2A-2, S2A-3, S2A-5) applies right after the
Stage 1 "coop" group, only when that group applied. It is gated by
`gow2_coop_survival` (default true; false keeps Stage 1 only). Row S2A-4 is a
separate group "split-revive", off by default (`gow2_coop_split_revive`). Every
row is host-only logic: clients may run without it. With
the default gow2_coop_players=2 nothing in this group runs. Every player must
still run the same build: a v0.1.1 peer lacks the d074c0d codegen fix and is
expected to crash at the chapter 2 -> 3 map commit even in two-player co-op [I].

## Who is an extra player (the hero test)

A PC is a hero when `GearPC.DefaultPawnClass` is None, `GearPawn_COGMarcus` or
`GearPawn_COGDom`. The compares are exact class compares, `0x77` object `!=`.
Everyone else (Ben Carmine, RedShirt) is an extra. None counts as a hero because
a fresh PC has no class-default value and `ApplyCheckpointRecord+0x0953` can
leave it unset after a checkpoint load. Every other `ApplyCheckpointRecord`
path calls `RestartPlayer`, which rewrites it from `SuggestPawnClassFor`.
Failing toward "hero" means failing toward the retail rule (game over), never
toward a stranded extra. S2A-3 adds a second check on the pawn's own class
(`Gear.Class` is neither COGMarcus nor COGDom) before it destroys anything.
[V script, I run time]

## Evidence

Evidence marks follow [COOP_N_PLAYERS.md](COOP_N_PLAYERS.md): [V] verified,
[I] inferred, `DG:n` / `DE:n` decompiler lines, `Class.Function+0xNNNN` script
offsets, `sub_XXXXXXXX` guest functions.

### Chapter travel (item 1)

`UWorld::CommitMapChange` (`sub_824A1AC8`) walks the persistent level's actors
in the loop at 0x824A2758-0x824A27A0 (recomp.18.cpp ~7930-7960; the kill test is
at line 7942). For every actor with Actor+0xB0 & 0x40000000
(`bKillDuringLevelTransition`; `bPendingDelete` is mask 0x1000 of the same word;
offsets from `layout.py` with RenderCommandFence counted as 4 bytes) it calls
`DestroyActor(actor, bNetForce=0, bShouldModifyLevel=1)` (`sub_824C4790`). [V]
DestroyActor refuses an actor whose Role (+146) is not Authority unless
bNetForce or bNetTemporary (+0xAC mask 0x4000) is set, so a client never destroys
a replicated player pawn here and only the host matters. [V] No script reads or
writes the flag, and GearGame.upk does not import it. [V] The test at
recomp.18.cpp:6685 is a different flag (at +8 of a 12-byte world array entry).
[V] Ben Carmine and RedShirt inherit True from `Default__GearPawn`; Marcus and
Dom set False. The destroyed pawn goes Pawn.Destroyed -> DetachFromController ->
Controller.PawnDied -> state Dead -> a 5 s ProcessGameOver.
`PostCommitMapChange` (DG:46711) restarts only local PCs that are not Dead.
[V chain; outcome I] The only path that spawns a human pawn is
`GearGameSP_Base.SpawnDefaultPawnFor` (via RestartPlayer from PostLogin, a death
respawn, CheckpointLoadComplete, ApplyCheckpointRecord or PostCommitMapChange);
its +0x0000 branch sends AI controllers to Engine's version, and GearGameSP
(GearGame_P.upk) overrides no function. A read-only audit found no other Spawn
of a COG pawn class for a GearPC. [V]

### Every place that ends the game (item 2)

`GearPC.ProcessGameOver` is the single sink (PlayAnyaCoopDeathSound, then
`ClientGameOver` on every GearPC). It is armed or called at five sites
[V bytes]:

1. `GearPC.Dead.BeginState+0x0149` / `+0x015F`: SetTimer 2 s with a crate, else
   5 s, on the server in SP.
2. `GearPC.CheckSurvivalChance+0x0054` / `+0x009F`: no chance of survival ->
   DoFatalDeath(GDT_BledOut), then a 3 s timer if none is armed.
3. `GearPC.Reviving.BeginState+0x011E`: a 3 s timer while still DBNO
   (`bCanRecoverFromDBNO=false`).
4. `GearPawn.EndGameCondition+0x009F`: a 2 s timer on EVERY GearPC when an
   AI-controlled pawn whose class is exactly COGDom dies (any difficulty), or
   any AI squad pawn on the lowest difficulty with `bAllowAICOGDeathWhenDBNO`.
5. `GearPC.Dead.EndState+0x0072`: ProcessGameOver() directly when a remote PC
   leaves Dead with NextStateName 'None' and the timer armed.

Apart from `SeqAct_ForceGameOver` (Kismet, map-specific, unchanged) no other
script path ends an SP campaign on a death or leave. During co-op splits a human
never counts as a reviver (`TeamHasAnyChanceOfSurvival+0x0258`).

### Leaving (item 3)

`GearPC.Destroyed` (1080 bytes): +0x0063 sends a checkpoint dummy, or a co-op SP
PC whose pawn (or vehicle driver) is a GearPawn_COGGear, to the leave branch at
+0x00AD; a PC with no pawn takes the plain path at +0x00A4
(`Super(PlayerController).Destroyed()`, then return). The branch: +0x00AD with
special moves 42/43 runs Died(GDT_ScriptedGib) and returns without
Super.Destroyed; +0x0116 leaves turrets; +0x01BD UnPossess; +0x01C7
Spawn(GearAI_Dom); +0x0229 AI.Possess(OldPawn); +0x031A FindSeqObjectsByClass
(the search); +0x034C CheckActivate of each SeqEvent_CoopPlayerLeft (the
activation). Destroying a still-possessed pawn goes Pawn.Destroyed ->
DetachFromController -> PawnDied -> Dead -> game over (DE, [V]).
GearPawn.Destroyed has no death or game-over logic (DG:57898).

### Checkpoints while an extra player is dead

`GearPC.SaveCheckpoint+0x0082` refuses to save while any GearPC has the
ProcessGameOver timer armed or a pawn with Health <= 0 (DG:32667). After S2A-2
that timer is also an extra's respawn timer, so a Kismet checkpoint that fires
while player 3 or 4 is dead would be skipped silently.

## How the rows are built

The patcher has no name operands and the in-memory value of a name or object
operand is unknown offline, so no row edits name or object bytes. Each row
either keeps the whole script except its final EndOfScript (0x53), redirects one
literal jump WORD in the kept prefix into code appended after it and jumps back
("append and redirect"; S2A-1, S2A-3, S2A-5, S2A-4), or replaces the whole
function (keep 0; S2A-2, whose script starts with a name operand and has no
literal jump). Final calls (opcode 0x1C) stand in for virtual calls (0x1B) only
where the target has no override and no state version (checked): both opcodes
end in the same native CallFunction (`sub_82267BF8`; GNatives[0x1B] =
`sub_82254488`, GNatives[0x1C] = `sub_82254500`) with the same UFunction, so
`ClientGameOver` still reaches each client.
`Engine.Actor.bKillDuringLevelTransition` is resolved by name in GObjObjects at
run time (Engine is loaded at LoadAll); offline the tool adds a synthetic import
because GearGame.upk does not import it.

## Rows

Notation follows [COOP_N_PLAYERS.md](COOP_N_PLAYERS.md) section 4. `[X]` is a
4-byte object operand. Offsets count from the start of the function's script.

| Id | Target (ScriptSize -> grown) | Edits | Fail-closed checks | Needed |
|---|---|---|---|---|
| S2A-1 | `GearGame.GearGameSP_Base.SpawnDefaultPawnFor` (162 -> 183) | +0x008C 95 -> A1 (the empty `if !(ResultPawn == None) goto 0x95` at +0x008A now jumps to the tail); keep 0x00A1 | +0x0061 `0F 00`, +0x0067 `1C`, +0x008A `07 00`, +0x008D `72 00`, +0x0093 `2A 16 04 00`, +0x009B `04 3A`, +0x00A1 `53`; operands +0x0063/+0x008F/+0x0097 ResultPawn, +0x0068 Engine.Actor.Spawn, +0x009D ReturnValue | Yes: players 3+ survive chapter travel |
| S2A-2 | `GearGame.GearPC.ProcessGameOver` (64 -> 159) | whole function replaced (keep 0x0000) | +0x0000 `1B`, +0x0009 `16 2F 19 01`, +0x0011 `00 27 00 00 1C`, +0x001A `20`, +0x001F `00`, +0x0024 `16 00 3C 19 00`, +0x002D `00 0A 00 00 1B`, +0x003A `16 31 30 04 0B 53`; operands +0x000D Engine.Actor.WorldInfo, +0x0016 Engine.WorldInfo.AllControllers, +0x001B GearGame.GearPC, +0x0020/+0x0029 ProcessGameOver.PC | Yes: an extra's death no longer ends the game |
| S2A-3 | `GearGame.GearPC.Destroyed` (1080 -> 1275) | +0x0064 00 -> 04, +0x0065 AD -> 37 (the JumpIfNot at +0x0063 now jumps to the tail at 0x0437); keep 0x0437 | +0x0063 `07`, +0x0066 `82 81 2D 01`, +0x00A4 `1C`, +0x00A9 `16 06 04 35 07 01 16 84 19 00`, +0x01B2 `0F 00`, +0x01B8 `01`, +0x01BD `1B`, +0x01C6 `16 0F 00`, +0x01CD `1C`, +0x01D2 `20`, +0x0435 `04 0B 53`; operands +0x006A bCheckpointDummy, +0x00A5 Engine.PlayerController.Destroyed, +0x00B3 Destroyed.Gear, +0x00BC GearPawn.IsDoingSpecialMove, +0x01B4 Destroyed.OldPawn, +0x01B9 Engine.Controller.Pawn, +0x01C9 Destroyed.AI, +0x01CE Engine.Actor.Spawn, +0x01D3 GearGame.GearAI_Dom | Yes: an extra leaving takes his body with him |
| S2A-5 | `GearGame.GearPC.SaveCheckpoint` (392 -> 496) | +0x00D7..+0x00D9 `30 04 0B` -> `06 01 87` (the refusal jumps to the tail); keep 0x0187 | +0x0065 `2F 19 01`, +0x006C `00 6B 00 00 1C`, +0x0075 `20`, +0x007A `00`, +0x007F `16 00 DB 07 00 DA 84 19 00`, +0x00DA `31 30 0F 00`, +0x0185 `04 0B 53`; operands +0x0068 WorldInfo, +0x0071 AllControllers, +0x0076 GearGame.GearPC, +0x007B/+0x0088 SaveCheckpoint.PC, +0x0091 Engine.Actor.IsTimerActive | Should: checkpoints are not skipped while an extra waits to respawn |
| S2A-4 | `GearGame.GearGame.TeamHasAnyChanceOfSurvival` (664 -> 854) | +0x025A 88 -> 97; keep 0x0297 | +0x0248 `0F 00`, +0x024E `2E`, +0x0253 `00`, +0x0258 `07 02`, +0x025B `82 82 77 00`, +0x0263 `2A 16 18 00 09 81 2D 01`, +0x026F `16 16 18 00 09 81 2D 01`, +0x027B `16 16 14 2D 00`, +0x0284 `27 06 02 89 31 30 04 2D 00`, +0x0291 `04 3A`, +0x0297 `53`; operands +0x024A/+0x025F PC, +0x024F GearGame.GearPC, +0x0254 C, +0x026B bInCoopSplit, +0x0277 bInSoloSplit, +0x0280/+0x028D bFoundReviver, +0x0293 ReturnValue | Optional (off): needs group B's squad handling in splits |

Fail closed: any check, operand, Num or Max mismatch skips the whole group
with `SCRIPTPATCH survival: skipped (<row>: <reason>); Stage 1 stays applied`;
the coop group stays applied.

S2A-3 checks only the opcode byte at +0x0063. The design's two-byte check
`07 00` also covered +0x0064, the byte the row edits, so a second apply would
have skipped the whole group (`+0x0064 expected 00, found 04`). The synthetic
tests found it; `test_checks_do_not_overlap_edits_or_operands` now guards every
row against a check or operand that covers an edited byte.

## Tails

`[X]` is the 4-byte address of X in memory, a package reference on disk. Each
statement has a `;` comment.

### S2A-1 tail at +0x00A1

```
+0x00A1 14 19 00 [SpawnDefaultPawnFor.ResultPawn] 00 06 00 04 2D 01 [Engine.Actor.bKillDuringLevelTransition] 28   ; ResultPawn.bKillDuringLevelTransition = false
+0x00B3 06 00 95   ; goto 0x0095 (return ResultPawn)
+0x00B6 53
```

- Every spawned human pawn gets `bKillDuringLevelTransition = false`.
- Control returns to the retail return path at 0x0095.

### S2A-2 (whole function)

DPC = `GearGame.GearPC.DefaultPawnClass`.

```
+0x0000 07 00 67 82 82 77 01 [DPC] 2A 16 18 00 0D 77 01 [DPC] 20 [GearPawn_COGMarcus] 16 16 18 00 0D 77 01 [DPC] 20 [GearPawn_COGDom] 16 16   ; if !(DPC != None && DPC != Marcus && DPC != Dom) goto 0x67
+0x002D 07 00 65 82 81 2D 01 [Engine.Actor.bPendingDelete] 16 18 00 09 72 01 [Engine.Controller.Pawn] 2A 16 16   ; if !(!bPendingDelete && Pawn == None) goto 0x65
+0x0045 19 2E [GearGameSP_Base] 19 01 [Engine.Actor.WorldInfo] 00 05 00 04 01 [Engine.WorldInfo.Game] 00 07 00 00 1C [GearGameSP_Base.RestartPlayer] 17 16   ; GearGameSP_Base(WorldInfo.Game).RestartPlayer(self)
+0x0065 04 0B   ; return
+0x0067 1C [GearPC.PlayAnyaCoopDeathSound] 16   ; the retail body from here on, with final calls
+0x006D 2F 19 01 [WorldInfo] 00 23 00 00 1C [Engine.WorldInfo.AllControllers] 20 [GearGame.GearPC] 00 [ProcessGameOver.PC] 16 00 9B   ; foreach WorldInfo.AllControllers(class'GearPC', PC)
+0x008A 19 00 [ProcessGameOver.PC] 00 06 00 00 1C [GearPC.ClientGameOver] 16   ; PC.ClientGameOver()
+0x009A 31   ; iteratornext
+0x009B 30   ; iteratorpop
+0x009C 04 0B   ; return
+0x009E 53
```

- A hero gets the retail body. An extra with no pawn, not being destroyed,
  respawns through GameInfo's own RestartPlayer path (MatchInProgress
  FindPlayerStart: BFS within 1024 uu of a teammate, then a PlayerStart; the new
  pawn goes through S2A-1).
- The 5/2/3 s delay is the retail timer itself.
- An extra who still has a pawn (DBNO on the Reviving no-chance path, or alive
  when EndGameCondition armed every PC) does nothing and bleeds out later.
- An extra being destroyed does nothing.

The option `gow2_coop_extra_players_game_over` (PC Options > Multiplayer,
"Players 3+ dying ends the game") leaves S2A-2 out of the survival group, so the
retail game-over rule applies to every player: a dead player 3+ ends the game
for everyone, like Marcus and Dom. The hook logs `SCRIPTPATCH survival: S2A-2
off (gow2_coop_extra_players_game_over=true)` when it applies the group without
the row.

### S2A-3 tail at +0x0437

```
0437: if !((((((DefaultPawnClass != None) && (DefaultPawnClass != obj'GearPawn_COGMarcus')) && (DefaultPawnClass != obj'GearPawn_COGDom')) && (Gear.Class != obj'GearPawn_COGMarcus')) && (Gear.Class != obj'GearPawn_COGDom'))) goto 0xAD
049A: if !((GearPawn(Pawn) != None)) goto 0x4F7
04AA: if !((Gear.IsDoingSpecialMove(42) || Gear.IsDoingSpecialMove(43))) goto 0x4D9
04D6: goto 0xAD
04D9: OldPawn = Pawn
04E4: UnPossess()
04EA: OldPawn.Destroy()
04F7: goto 0xA4
04FA: <EndOfScript>
```

- Heroes and Dom dummies take the retail branch unchanged.
- An extra on foot is unpossessed and his pawn destroyed, then the plain leave
  runs. A vehicle or turret driver takes the plain leave directly
  (PlayerController.Destroyed kills the driver and S2A-2 swallows the resulting
  ProcessGameOver).
- Mid special move 42/43 keeps the retail gib.
- No AI Dom and no SeqEvent_CoopPlayerLeft for extras.

Checkpoint dummies take the extras' branch now: after a player 3+ leaves or
rejoins, the next checkpoint load restores his record as a dummy GearPC
(`GearPC.ApplyCheckpointRecord+0x00CC` sets DefaultPawnClass from the record,
`+0x026D` RestartPlayer spawns a GearPawn_COGGear body, `+0x0C3F` destroys the
dummy). With the old `!bCheckpointDummy` term that dummy took retail's branch
at 0x00AD, which spawns a GearAI_Dom on the body (+0x01C7) and fires every
SeqEvent_CoopPlayerLeft (+0x031A/+0x034C), whose handlers act on the real Dom
in 5 maps (SP_Intervention_10_S, SP_Nexis_7_SB, SP_Palace_BF, SP_Riftworm_4_S,
SP_Sinkhole_03_S). A Dom dummy still gets the retail AI takeover (its
DefaultPawnClass is Dom).

### S2A-5 tail at +0x0187

```
+0x0187 07 01 EC (condition: PC.DPC != None && != Marcus && != Dom && PC.Pawn == None)
+0x01E9 06 00 DA   ; ignore this PC: iteratornext
+0x01EC 30 04 0B   ; retail refusal: iteratorpop; return
+0x01EF 53
```

- A dead extra waiting to respawn (Pawn == None) no longer blocks the save.
- A dying pawn (Health <= 0), a DBNO extra with the timer and every hero keep
  the retail refusal.

### S2A-4 tail at +0x0297

```
+0x0297 07 02 88 (condition: PC != None && !bInSoloSplit && bInCoopSplit && PC is an extra && GearPRI(PC.PlayerReplicationInfo).SquadName == GearPRI(DBNOPawn.PlayerReplicationInfo).SquadName)
+0x034A 14 2D 00 [bFoundReviver] 27   ; bFoundReviver = true
+0x0352 06 02 89   ; goto iteratorpop
+0x0355 53
```

- A standing extra in the downed pawn's squad counts as a reviver during a
  co-op split (not a solo split).
- Hero-hero revives keep the retail rule.

## Settings

| Setting | Default | Effect |
|---|---|---|
| `gow2_coop_survival` | true | With `gow2_coop_players` 3+ applies the survival group after Stage 1; false logs `SCRIPTPATCH survival: off (gow2_coop_survival=false)` |
| `gow2_coop_extra_players_game_over` | false | Leaves S2A-2 out of the survival group, so a dead player 3+ ends the game for everyone |
| `gow2_coop_split_revive` | false | Debug, row S2A-4; applies the split-revive group only after the survival group applied |

Log lines: `SCRIPTPATCH survival S2A-1 ...` one per row, then
`SCRIPTPATCH survival: 4 rows applied for 4 players, set hash <16 hex>`.
`SCRIPTPATCH survival: off (the coop group was not applied)` when Stage 1 was
skipped.

## Tools and tests

`python -I tools/script_patch_survival.py --check <folder>` imports
`tools/script_patch.py` and reuses its planner, decoder and C++ renderer. It
plans both groups at 4 players. It checks every old byte and operand, decodes
each grown script to its end, checks that every jump lands on a statement start
and that append rows end in EndOfScript, and prints the C++ table.
`--cpp --write` prints `src/gow2_script_patch_survival.cpp`.

Tests: `python -m pytest tools/tests -q`
(`tools/tests/test_script_patch_survival.py`, synthetic bytecode; the expected
tails are typed from this document, not built from the rows) and the C++ target
`gow2_script_patch_survival_test`
(`tools/tests/script_patch_survival_test.cpp`, the rows applied to a synthetic
guest memory).

Offline results (2026-10-16): `--check` passes on the retail packages. The
grown scripts decode to their ends at 0xB7 (S2A-1), 0x9F (S2A-2), 0x508
(S2A-3), 0x1F0 (S2A-5) and 0x356 (S2A-4), every jump lands on a statement
start, and the four append rows end in EndOfScript.
`python -m pytest tools/tests -q`: 50 passed (25 Stage 1, 25 survival).
`gow2_script_patch_survival_test`, built with clang++ directly: 56 checks,
0 failures; the Stage 1 core test: 92 checks, 0 failures.
`src/gow2_script_patch_hook.cpp`, `src/gow2_script_patch_table.cpp` and
`src/gow2_script_patch_survival.cpp` pass `clang++ -fsyntax-only` with the
release build's flags. The game itself was not built or run for this group.

## In-game test plan

1. Solo `--gow2_coop_players=2`: no `SCRIPTPATCH survival` line, retail.
2. Solo `--gow2_coop_players=4`: the six coop lines unchanged, then four
   `SCRIPTPATCH survival S2A-...` lines and the rows-applied line; kill Marcus
   -> game over after 5 s.
3. `--gow2_coop_survival=false` -> the off line.
4. Four instances in chapter 2 (all four in the lobby before launch,
   `--gow2_coop_census=true` on the host):
   - Transition A (SP_HospitalIntro_P commits to sp_hospital_p after its intro).
     Transition B (SP_Hospital_P's end of chapter, SeqAct_ChapterComplete_5 ->
     SeqAct_CommitMapChange_1). Expect: census keeps
     GearPawn_COGBenjaminCarmine x2, no game over, players 3 and 4 still move.
   - S2A-2 (a) instance 3 bleeds out -> no game over, back on his feet next to
     a teammate with a Lancer within about 5 s. (b) a revive is retail. (c) kill
     the host -> retail game over and the checkpoint reload brings all four
     back. (d) kill Dom -> the same. (e) instance 3 dies and quits within 5 s ->
     no game over for the others.
   - S2A-3 (a) instance 4 quits from the menu: the Ben count drops by one, no
     new Dom pawn, player 2 keeps Dom, no game over. (b) rejoin: a fresh Ben and
     no orphan. (c) abrupt Stop-Process: the same after the host's connection
     timeout. (d) leave while driving/on a turret, if reachable. (e) control:
     Dom leaves -> retail AI takeover. (f) leave, then checkpoint reload: with 4
     players, SaveCheckpoint on instance 1, stopone instance 4, wait for
     XGISessionLeaveRemote on the host, kill the host, LOAD CHECKPOINT: no
     GearAI_Dom on a GearPawn_COGGear and no orphan extra body in the census;
     then rejoin.
   - S2A-5 best effort: instance 3 dies just before a Kismet checkpoint trigger,
     the host's HUD shows the saving icon, a forced wipe reloads there with all
     four.
5. A/B control: repeat step 4 transition A and S2A-2 (a) with
   `--gow2_coop_survival=false` on the host (the extras vanish at the commit /
   the game ends).

| Step | Result | Evidence |
|---|---|---|
| 1 | not run | - |
| 2 | not run | - |
| 3 | not run | - |
| 4 transition A | pass (same S2A-1 commit path as B; the 2 -> 3 commit and chapter travel passed) | Stage 2 results item 1 |
| 4 transition B | pass: chapter 2 -> 3 commit keeps all four pawns, nobody Dead, no game over; chapter travel to 13, 16, 27 gives every player a pawn | `out/stage2-test/1-transition` |
| 4 S2A-2 (a) | pass: extra respawned within 15 s next to a teammate (237-463 units), with Lancer, pistol and Gnasher, no game over | `out/stage2-test/2-deaths` |
| 4 S2A-2 (b) | not run | - |
| 4 S2A-2 (c) | pass: !kill P1 on foot is OBJECTIVE FAILED on all four, as in retail; LOAD CHECKPOINT restored all four | `out/stage2-test/2-deaths` |
| 4 S2A-2 (d) | not run | - |
| 4 S2A-2 (e) | not run | - |
| 4 S2A-3 (a) | pass via stopone (abrupt); menu quit not tried separately: no AI Dom, Dom untouched, no game over | `out/stage2-test/3-leave-rejoin` |
| 4 S2A-3 (b) | pass: P4 rejoined as GearPawn_COGGear with Dom's mesh and all three weapons, no RT needed | `out/stage2-test/3-leave-rejoin` |
| 4 S2A-3 (c) | pass: host logged XGISessionLeaveRemote 45 s after the kill; leaver's pawn left the census 10 s after the leave | `out/stage2-test/3-leave-rejoin` |
| 4 S2A-3 (d) | not run | - |
| 4 S2A-3 (e) | not run | - |
| 4 S2A-3 (f) | not run yet | - |
| 4 S2A-5 | pass: with P4 Dead (pawn None) SaveCheckpoint wrote the save 0.1 s later | Stage 2 results item 2 |
| 5 | not run | - |

## Open items

- (a) Where the engine ends the Dead state relative to `bPendingDelete` when a
  dead player leaves was not traced; S2A-2 and S2A-3 cover both orders, test
  S2A-2 (e) exercises it [I].
- (b) `GameInfo.RestartPlayer+0x00FB` (no start spot) or `+0x0136` (spawn
  failure -> GotoState('Dead') while already Dead, so no new timer) can leave an
  extra dead until the next checkpoint load; rare, since bNoCollisionFail is set
  and there is a PlayerStart fallback.
- (c) After a checkpoint load, the order in which native code matches saved
  records to players (COOP_N_PLAYERS item 7) could give Marcus to an extra; the
  rules follow the bodies, as Kismet does.
- (d) Players 3+ keep their pawn where it stood at a chapter commit; if Kismet
  only teleports the heroes they can be stranded until FellOutOfWorld, a
  checkpoint pull or group B's teleport rows.
- (e) Function ownership: group A grows `GearGameSP_Base.SpawnDefaultPawnFor`,
  `GearPC.ProcessGameOver`, `GearPC.Destroyed`, `GearPC.SaveCheckpoint` and
  `GearGame.TeamHasAnyChanceOfSurvival`; another group that grows one of them
  fails closed on the Script.Num check, so a body/mesh-at-spawn row (group C)
  must extend S2A-1's tail at merge instead of growing the function again.
- (f) PlayAnya's "Dom is down" cue no longer plays for extras.
- (g) SeqEvent_CoopPlayerLeft no longer fires for extras (its 14 handlers act on
  the real Dom).
