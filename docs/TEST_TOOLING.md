# Test tooling for co-op Stage 2

This tooling runs the Stage 2 scenarios of [COOP_N_PLAYERS.md](COOP_N_PLAYERS.md)
with four instances on one PC, without playing a whole chapter by hand. Each
instance polls a command file for console commands and host verbs, and scripts
drive the instances and check the logs. Everything here is dev only, off by
default, and needs a restart to take effect.

Status: built and checked offline (unit tests, -fsyntax-only with the release
flags, the offline package check of DEV-1, coop-instances actions run against a
fake game). Used in game on 2026-10-16: see COOP_N_PLAYERS.md, Stage 2 results.

No game data is in the repository: offsets and byte strings only. The evidence
files under out/ come from the player's own game and are not committed.

## Settings

| Setting | Type | Default | Effect |
|---|---|---|---|
| `gow2_dev_command_file` | string | empty = off | Path of the command file. Results go to `<file>.ack` and to DEVCMD log lines. |
| `gow2_dev_cheats` | bool | false | The DEV-1 patch, host only: the co-op host gets a cheat manager (see the end of "Console commands that work without cheats"). |

Both need a restart. With both at their defaults nothing in guest memory
changes; the engine tick hook only calls the original function.

## Command file

One command per line. Lines are LF-terminated: a last line without LF waits
until its newline arrives. Lines present when the game starts are skipped
(`skipped <n> earlier lines`). At most one line runs per frame. The file is
re-read every 100 ms. Empty lines and `#` lines are ignored and get no result.
Lines are limited to 400 characters, ASCII only. Truncating the file restarts
at line 1.

Three kinds of line:

- Console line. Runs as if typed in the console on the local player
  controller.

  ```
  suicide
  LoadCheckpoint
  servertravel geargame_p?loadcheckpoint=0?chapter=4
  ```

- `@target console line`. Runs the console line on server-side player
  controllers. On the host this is the remote route UNetConnection::Exec ->
  UPlayer::Exec. Targets: `local` or `host` (the local player controller),
  `all` (every player controller), `#N` (1..64, in WorldInfo.ControllerList
  order, newest first), or a player name (case-insensitive, e.g. `P3`). A
  target with no player controller yet waits up to 30 s (`local`, `all`,
  `#N`). A missing name fails at once.

  ```
  @all ToggleDropDetail
  @P3 LoadCheckpoint
  @#2 suicide
  ```

- `!verb ...` host verbs. Parsed in `src/gow2_dev_commands.cpp`, run in
  `src/gow2_dev_host_verbs.cpp` and `src/gow2_dev_map_change.cpp`.

| Verb | Arguments | What it does | Host only? | Example |
|---|---|---|---|---|
| `!list` | none | Logs one line per controller (format under "Results") and returns the counts. | no | `!list` |
| `!selftest` | none | Runs ToggleDropDetail twice on each player controller, local first, and checks the GearGRI bit flips and back. Returns `N/M routes ok`. | no | `!selftest` |
| `!god` | `<target> on\|off` | Sets or clears bGodMode on the targets. | yes (`only the host decides damage`) | `!god P1 on` |
| `!kill` | `<target>` | Runs Suicide on each target's pawn. Targets with no pawn are skipped. | yes (`only the host decides damage`) | `!kill P2` |
| `!call` | `<target> <Function>` | Runs a script function with zero parameters on each target controller. | remote targets only (`remote targets need the host`) | `!call P3 TeleportToPlayer` |
| `!callpawn` | `<target> <Function>` | Same, on each target's pawn. | remote targets only (`remote targets need the host`) | `!callpawn P2 Suicide` |
| `!mark` | `<text>` | Logs `DEVCMD mark <text>`. | no | `!mark chapter-2-start` |
| `!listmapchanges` | none | Logs one line per loaded SeqAct_PrepareMapChange and returns the prepare/commit counts. | no | `!listmapchanges` |
| `!mapchange` | `[Level]` | Activates the one matching retail PrepareMapChange native. With no Kismet action for a named level it sends ClientPrepareMapChange to all player controllers instead. | yes (`not the host`) | `!mapchange SP_AssaultIntro_P` |
| `!mapcommit` | `[force]` | Runs the retail commit native, which sends ClientCommitMapChange to all player controllers. Without `force` it refuses unless a change is being prepared. | yes (`not the host`) | `!mapcommit` |

`!call`, `!callpawn` and `!kill` fail with `<Class> has no function <F>` when
the object lacks the function, and with `no name <F>` when the name cannot be
made.

## Results

Each line that runs gets one ack line in `<file>.ack`:

```
<line number><TAB>ok|error<TAB><detail>
```

The same result goes to the log:

```
DEVCMD #<n> '<line>' -> ok: <detail>
DEVCMD #<n> '<line>' -> error: <detail>
```

A console line that runs reports `sent to <targets>`, plus `output '...'` when
the command returns text. A `!list` line looks like this:

```
DEVCMD   [<i>] <Obj> class=<Class> pc=<0|1> player='<name>' pawn=<Pawn>(<PawnClass>) health=<h> god=<0|1> state=<State>
```

Warning: console commands report no failure. The "Command not recognized" text
was compiled out of this build, so a typo and a real command ack the same.
Tests check the effect (!list output, census lines, screenshots), not the ack.

## Console commands that work without cheats

| Command | What it does | Who may run it |
|---|---|---|
| `suicide` | PlayerController.Suicide -> ServerSuicide. Refused within 10 s of spawning unless the game is standalone. | any player, on self |
| `SaveCheckpoint` | GearPC exec. Saves a checkpoint. | the server's own player only: send to instance 1 |
| `LoadCheckpoint` | GearPC exec. Reloads the last checkpoint: sets GearEngine.PendingCheckpointAction, or, when memory is short, travels to `geargame_p?loadcheckpoint=1?chapter=<CurrentChapter>`. | instance 1 |
| `RestartChapter` | GearPC exec. Travels to `geargame_p?loadcheckpoint=0?chapter=<CurrentChapter>` and restarts. | instance 1 |
| `Kill <ai\|bots\|all\|...>`, `KillPawns` | Kill pawns. | instance 1 (server-side effect) |
| `KillAll <Class>` | Kills every actor of the class. | instance 1 (server-side effect) |
| `Revive` | ServerReviveSelf. Revives the player. | any player, on self |
| `Stuck` | Unsticks the player. | any player, on self |
| `ToggleDropDetail` | Flips the GearGRI test bit. Used by `!selftest`. | any player |
| `CE <EventName>` / `CauseEvent` | Fires Kismet console events. | instance 1 |
| `servertravel <url>` | The server travels; every client follows. | instance 1 |
| `open <address>` | Connects the instance to a server. | any client |
| `disconnect` | Drops the instance from the game. | any player |

These need `gow2_dev_cheats` on the host: `God`, `Teleport`, `DBNOMe`,
`PrepMap`/`CommitMap`, `UnlimitedHealth`, `UnlockAllChapters`
(GearCheatManager; on a co-op host only with DEV-1).

## Chapter travel

Travel form and script wrapper (n = human chapter number, index = n - 1):

```
servertravel geargame_p?loadcheckpoint=0?chapter=<index>
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/coop-instances.ps1 chapter -Chapter <n> -OutDir out/coop-test/s2
```

`?chapter=` is read in `GearGameSP_Base.InitGame+0x0063`:
`GetIntOption(Options, "chapter", -1)`. Any value >= 0 sets the chapter and a
startup checkpoint save. [V script] The `chapter` action sends the travel to
instance 1.

| Index | Human chapter | Persistent map |
|---|---|---|
| 0 | 1 | sp_hospitalintro_p |
| 1 | 2 | sp_hospital_p |
| 2 | 3 | sp_assaultintro_p |
| 3 | 4 | sp_assault_p |
| 4-5 | 5-6 | sp_landown_p |
| 6-8 | 7-9 | sp_intervention_p |
| 9-10 | 10-11 | sp_rescue_p |
| 11 | 12 | sp_riftworm_p |
| 12-14 | 13-15 | sp_outpost_p |
| 15 | 16 | sp_mountkadar_p |
| 16-17 | 17-18 | sp_leviathan_p |
| 18-19 | 19-20 | sp_maria_p |
| 20-21 | 21-22 | sp_nexis_p |
| 22-23 | 23-24 | sp_palace_p |
| 24 | 25 | sp_escape_p |
| 25 | 26 | SP_JacintoIntro_P |
| 26-27 | 27-28 | sp_sinkhole_p |
| 28 | 29 | sp_closure_p |

(GearTypes.ChapterLevelNames, Coalesced_int_full.txt:5976-6004.) Human
chapter = index + 1.

## Running the instances

`scripts/coop-instances.ps1` runs N instances on one PC. `start` always wires
a per-instance command file (`<OutDir>/inst<i>/cmd.txt`, passed as
`gow2_dev_command_file`). One action per call:

```
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/coop-instances.ps1 start -Count 4 -OutDir out/coop-test/s2 -Coop -DevCheats
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/coop-instances.ps1 cmd -Instance 1 -Command "!list" -OutDir out/coop-test/s2
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/coop-instances.ps1 waitlog -Instance 1 -Pattern "DEVCMD on:" -OutDir out/coop-test/s2
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/coop-instances.ps1 stopone -Instance 4 -OutDir out/coop-test/s2
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/coop-instances.ps1 startone -Instance 4 -OutDir out/coop-test/s2
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/coop-instances.ps1 chapter -Chapter 5 -OutDir out/coop-test/s2
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/coop-instances.ps1 macro -Instance 1 -Steps "pad:A:200,wait:500" -OutDir out/coop-test/s2
```

`macro` steps, comma-separated: `pad:<buttons>:<ms>` (press, `+` joins
buttons), `hold:<buttons>`, `release`, `wait:<ms>`, `shot:<label>`,
`cmd:<command line>`, `log:<regex>:<sec>` (wait for a log match).

`cmd` exit codes: 0 ok, 1 error, 2 no result in time or no pump, 3 not sent
(the instance is not running, or the line is empty, a `#` comment or not
ASCII). The `cmd` action waits for `DEVCMD on:` in the log before it writes.
`chapter` sends the travel to instance 1 and takes 1..29. `waitlog`
`-SinceStart` reads from where the instance's current run began.

`-Coop` sets per instance: `--gow2_coop_players` (from `-Players`, default
4), `--net_link_active`, `--net_bind_ip=127.0.0.<i>`,
`--net_broadcast_targets`, `--user_name=P<i>`, `--system_link_trace`,
`--gow2_coop_census`, `--net_traffic_log_sec=10`. `-DevCheats` adds
`--gow2_dev_cheats=true` to instance 1 only.

`scripts/coop-stage2.ps1` drives one setup plus the transition, kill,
checkpoint, leave-rejoin and travel scenarios through
`scripts/coop-instances.ps1`, and checks the `!list` controller lines after
each step:

```
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/coop-stage2.ps1 -Scenario all -OutDir out/stage2/s1 -SeedUser out/seed/user -Players 4 -DevCheats
```

- `setup`: start with `-Coop` from the seeded save, check `DEVCMD on:` in
  each log, run the menu macros (press start, main menu, host lobby, browser,
  join, launch, spawn on RT), then check `!list` shows every player with a
  pawn.
- `transition`: `!mapchange <Level>` (default `SP_AssaultIntro_P`) then
  `!mapcommit`, with screenshots at 2 s, 10 s and 30 s. Check every player
  keeps a pawn and nobody is in state `Dead`.
- `kill-extra`: `suicide` on instance 3 after 12 s. Check P3 has a pawn
  again and P1 is not `Dead`.
- `kill-host`: `!kill P1`. Game over is expected; the `!list` lines go to
  the notes.
- `checkpoint`: `SaveCheckpoint` on instance 1, walk P3 away, `LoadCheckpoint`
  on instance 1. Check every player has a pawn.
- `leave-rejoin`: `stopone` the last instance, wait up to 120 s for the
  host to log XGISessionLeaveRemote, then check the player count dropped
  and that no GearAI_Dom drives an extra's body (GearPawn_COGBenjaminCarmine
  or GearPawn_COGGear). Then `startone`, rejoin through the menu macros,
  and check every player has a pawn and still no GearAI_Dom drives an
  extra's body.
- `travel`: `chapter -Chapter <n>` (default 5), then wait for `COOPCENSUS
  pawns` in each log. Check every player has a pawn.
- `all`: run every scenario in order (setup, transition, kill-extra,
  checkpoint, travel, leave-rejoin, kill-host), then stop.

Each scenario stores PASS/FAIL lines plus INFO notes in
`<OutDir>/<scenario>/result.txt`, with per-instance game logs and screenshots
beside it. Scenarios other than `setup` and `all` expect the instances to be
already running from an earlier setup; only `all` stops them at the end.
`-SeedUser` is required for `setup` and `all`. The script exits 1 when any
check failed, else 0. Every run is idle-gated (setup waits for an idle PC
before starting anything) and guarded (the coop-instances idle guard watches
the run unless `-NoGuard` is passed through). The menu macros were calibrated
from Stage 1's net-join.ps1; re-check them if the menus change.

## For other groups: per-frame code

Register per-frame code with `AddEngineTickListener` from
`src/gow2_engine_tick.h`. Never define `REX_HOOK_RAW(sub_824A2EA0)` again: it
is already defined in `src/gow2_engine_tick_hook.cpp`, and a second
definition is a duplicate symbol at link. Listener rules, from the header:

- Listeners run on the game thread, once per frame, before
  UGameEngine::Tick.
- Never modify ctx. Copy it into a local PPCContext for guest calls.
- Read settings once and return at once when off.
- Never block or loop for long: the game waits for every listener.

Guest calls (console commands, FName, FindFunction, ProcessEvent, allocator)
go through `src/gow2_dev_guest_calls.h`. Reads that may touch bad guest
memory go through its `Guarded` wrapper.

## In-game test plan

1. Four instances with `-Coop` at Act 1 chapter 2 (seeded save). Each log
   has `DEVCMD on`.
2. `!list` shows P1..P4 with Marcus, Dom, Ben, Ben.
3. `!selftest` returns 4/4 routes ok.
4. `!god P1 on`, then 30 s under fire. P1 lives.
5. `suicide` on instance 3 after 10 s. P3 dies.
6. `!kill P2`. P2 dies.
7. `!call P3 TeleportToPlayer`. P3 moves to a teammate.
8. `LoadCheckpoint` on instance 1. All four are restored.
9. `chapter -Chapter 5`. All four arrive in chapter 5.
10. `!mapchange SP_AssaultIntro_P` then `!mapcommit`. The map changes.
11. `-DevCheats`, then `God` on the host. Cheats work.
12. A control instance without the setting logs no DEVCMD line.
13. 200 x `@all ToggleDropDetail` with no memory growth in
    coop-instances `stats`.

Steps 1, 2 (with Dom's body: Marcus, Dom, GearPawn_COGGear x2), 3
(4/4 routes ok), 5, 6, 8, 9 (chapter 13, 16, 27) and 10 ran and
passed on 2026-10-16. [V] Step 4: with !god on, P1 lived in chapter 2,
but in a chapter 27 combat area P1 died with god=1 (cause not traced). [V] Step 7 ran: TeleportToPlayer teleports to
the newest other player. [V] Step 11 ran: CE and TeleportToLocation
work with -DevCheats. [V] Steps 12 and 13 not run.

## Evidence

The addresses and offsets the tooling relies on. [V] means read in code and
confirmed by a second pass. [I] means taken from `tools/ue3/layout.py`, to be
checked by the first `!list` / `!selftest` in game.

- UGameEngine::Tick `sub_824A2EA0`, called each frame from UGearEngine::Tick
  `sub_828A4180`. [V]
- GWorld `[0x8312380C]`. WorldInfo = `[[[GWorld]+80]+60][0]`. [V]
- GamePlayers data +812, Num +816. UPlayer.Actor +64.
  PlayerController.Player +836. [V]
- ConsoleCommand = actor vtable +272 (GearPC `sub_823BB720`). [V]
- FindFunction `sub_82287B10` (returns 0, no fatal error). [V]
- ProcessEvent = vtable +236. [V]
- FName::Init `sub_82281710`. [V]
- appFree `sub_82220050`. GMalloc `[0x8310B12C]`, Malloc = vtable +4. [V]
- SeqAct_PrepareMapChange activation `sub_8256C2A8`. MainLevelName +248,
  secondary levels +256 (Num +260), bIsHighPriority bit 31 of +268.
  ClientPrepareMapChange `sub_8256FE10`. [V] (`tools/ue3/layout.py` says
  232/240/252: it undercounts SequenceOp by 16 bytes.)
- SeqAct_CommitMapChange activation `sub_8256C870`. [V]
- GameEngine +1252 = pending map change count. [I]
- Controller Pawn +444. [V] PRI +448. NextController +456. bGodMode +460,
  mask 0x40000000. PRI.PlayerName +464. Pawn.Health +692. WorldInfo GRI +792.
  [I]
- WorldInfo NetMode +796. ControllerList +864. [V]
- GearGRI.bDropDetail_Test word +604, mask 0x00200000. [I]
- Engine.GameInfo.AllowCheats bytes (DEV-1). [V on the player's Engine.upk]

The [I] offsets for PRI.PlayerName (+464), Pawn.Health (+692) and
bGodMode (+460, mask 0x40000000) were confirmed by !list in game on
2026-10-16 (names, Health 1410/2100/900, god flag). [V]

## Faking a controller

`GOW2_PROMPTS_PAD_FILE=<file>` (environment, Windows) makes the live button prompts read "a controller is connected" from the first character of that file, `1` or `0`, every half second instead of asking XInput. Change the file while the game runs to watch the prompts switch between the keycaps and the pad buttons (a screen already open keeps its labels until it is opened again). `scripts/build-keycaps.ps1` and `tools/keycaps/README.md` describe the pack.
