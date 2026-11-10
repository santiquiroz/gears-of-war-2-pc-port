# Campaign co-op beyond two players

Status: Stage 1 has run on Windows. On 2026-10-15 four `gow2.exe` instances on one PC
(127.0.0.1-127.0.0.4) played Act 1 chapter 2 together over System Link: the host as Marcus,
player 2 as Dom, players 3 and 4 as Ben Carmine (section 3, Stage 1 results). The run found three
bugs, now fixed: `getsockname` was a stub, so every join failed; the S1-2 tail returned None, so
players 3+ became Marcus clones; and `--user_data_root` never reached the runtime, so every
instance shared one save folder. Not done yet: ten minutes of real combat with four players, a
desync check, and co-op beyond two players on Android. The merged tree builds and links every
patch and `src/` file for Android (below), but nobody has run `gow2_coop_players` 3+, the census or
the traffic log on a phone. Stage 2 is implemented on branch `stage2` and was run with four
instances on one PC on 2026-10-16 (Stage 2 results below). Not covered: co-op splits, vehicle
exit (CR-2), the Ride-Reaver section, a join during a cinematic, a combat soak, 4 PCs over LAN
or the internet, Android with 3+ players. The F2 codegen fix (d074c0d) makes chapter commits
past Act 1 chapter 2 work. Stage 3 stays design only. Stage 4's networking (patch 0011) is
built and has joined two players, PC with PC and PC with phone.

Merged on the `integration` branch (2026-10-15): master, then `saves`, `coop-stage1` and
`crossplay`. The SDK series 0002-0011 applies in order on `923c1a5` (`git apply --check`, one patch
after the other). Master's 0007 shifts Android guest ports 1-1023 by 30000 and translates socket
options and FIONBIO/FIONREAD on Android. 0010 is this branch's latest (getsockname,
`net_traffic_log_sec`) rebased onto that 0007; it keeps the shift inside its own `Bind` rewrite.
0011-network-crossplay (Stage 4 networking) was re-created on that 0010: it removes 0007's Android
shift, option table and ioctl code from `src/system/xsocket.cpp`, so `net_port_offset` is the one
port mapping on Android, Windows and Linux (a phone that bound 31000 and a PC that sent to 41000
would never meet); its option table gained SO_KEEPALIVE and SO_OOBINLINE, the two 0007 options it
lacked; its `getsockname` replaces 0010's; and `net_traffic_log_sec` counts the datagrams that pass
its filters, by host port. The Android build applies every `sdk/patches/*.patch` and globs
`src/*.cpp`.

Checked on the merged tree (2026-10-15): `scripts/build-sdk.ps1` and `scripts/build.ps1`; the
SDK's System Link and session tests ("All tests passed (2151 assertions in 81 test cases)"); the
host tests `gow2_test_input_test`, `gow2_script_patch_test` and `gow2_save_dir_test`; two copies
on loopback (`scripts/run-instances.ps1` with `--net_traffic_log_sec=10`) that joined each other's
System Link campaign and loaded Act 1 chapter 2, the client as Dom, with `[nettraffic]` lines for
port 41000 in both logs (`out\pcpc-test\int1`); and `scripts/build-android.ps1`, whose library and
APK checks passed, plus the app's JVM unit tests. The APK was installed on a phone later that day (below).

Smoke tests after the review fixes (2026-10-15, `out\smoke` in the integration worktree):
- Solo with the defaults (own `--user_data_root`, windowed): the seeded Act 1 chapter 2 save loads
  and Marcus plays; `SCRIPTPATCH coop: off (gow2_coop_players=2)`, `[netlink] link reported down
  (net_link_active=false)`, `Signed-in player: XUID B13EBABEBABEBABE, gamertag 'User'`, no
  `XGISessionCreate`, and with `--system_link_trace=true` the one bind is `bind port 1000 -> 41000
  (net_port_offset)`. With `--gow2_temporal=true --gow2_upscaler=fsr4` the FSR providers read
  `'4.1.1'`, `'3.1.5'`, `'2.3.4'` as on master and `FSR: context created '4.1.1'`; with
  `--gow2_mouse_aim_log=true` mouse moves log `MOUSEAIM sent x=-400.0 y=0.0 frames=10
  handled=10/10 ...`.
- Two copies with `scripts/coop-instances.ps1` (pad files, separate `--user_data_root`, no
  `--user_xuid`): the client found `Host` in the System Link Browser, both lobbies listed Host and
  Client, the client spawned as Dom on RT; `XGISessionJoinRemote ... members=2`, game traffic to
  `127.0.0.1:41000`, and two different per-install XUIDs (B13E4E1A1211F81E, B13E7C3AE4648C7F).
- `--gow2_coop_players=4` logs `SCRIPTPATCH coop: 6 rows applied for 4 players` and turns the link
  on. Two "PCs" (own `LOCALAPPDATA` each) sharing one `--gow2_save_dir` kept their own XUIDs
  (`System Link XUID B13E01519A002D78 made and kept in ...\pcA\...\system_link_xuid.txt`, then
  `B13E453A46694FCA` for pcB, then `read from` pcA's file again), and the shared folder got the
  saves but no `system_link_xuid.txt`.
- The integration APK on the Galaxy S25 Ultra joined a campaign that the integration PC build hosted
  from the firewall-allowed path: browser entry `Pc`, both lobbies with Pc and User, chapter 2 on
  both, the phone spawned as Dom. The phone logged `SCRIPTPATCH coop: off (gow2_coop_players=2)`,
  `bind port 1000 -> 41000 (net_port_offset)`, `first datagram to 192.168.10.10:41000` and
  `XGISessionJoinRemote ... members=2`.

On the `crossplay` branch `scripts/build-android.ps1` built and linked 0002-0011 (with the
getsockname 0010, before the traffic log). Two copies of the crossplay build joined each other's
campaign over loopback on one PC, and on 2026-10-15 a PC and a Galaxy S25 Ultra joined each other's
campaign on one Wi-Fi network, with either one hosting. See [NETWORK_PLAY.md](NETWORK_PLAY.md).

This document condenses four read-only investigations from 2026-10-14 (join and spawn path,
campaign systems, patch mechanism, network). A second pass re-checked each one, and the corrections
from that pass are already applied here. The target is 4 players first, then 8-10, then PC-Android
cross-play over System Link.

## Evidence conventions

- `DG:n` / `DE:n`: line n of `out/re-mouse/decomp_GearGame.txt` / `out/re-mouse/decomp_Engine.txt`,
  the script decompiled with this repo's own UnrealScript tools (now in `tools/ue3/`: `ubc.py`,
  `upk.py`, `ue3script.py`, `ue3pkg.py`). A `~` marks an approximate line.
- `Class.Function+0xNNNN`: byte offset of a statement inside that function's bytecode.
- `upk 0x...`: file offset in the decompressed `GearGame.upk` (`out/re-mouse/pkg/`).
- `sub_XXXXXXXX`: a recompiled guest function. `recomp.N.cpp:L` is line L of
  `generated/default/gow2_recomp.N.cpp`.
- Detailed notes and scratch tools are in `out/re-coop/`: `join-spawn.md`, `campaign-systems.md`,
  `patch-mechanism.md`, `network.md`, `join-spawn/verify_patches.py`, `script_patch_poc.py`,
  `hero_links.txt`, `kismet/`.
- `out/` is gitignored. Its contents come from the player's own game files and are not in the
  repository. To reproduce them, decompress your own packages with the tools in `tools/ue3/` (see
  its README).
- **[V]** verified: read in code or bytes and confirmed by a second, independent pass.
  **[I]** inferred: follows from code structure or UE3 knowledge, but has not been confirmed in this
  binary or at run time.

## 1. What limits co-op to two today

Two independent layers stand in the way:

- The host's game script caps the session at two players and has no usable body for a third.
- Underneath, the port's System Link emulation cannot yet run a network session of any size.

### 1.1 The join cap

| Limit | Where | Effect |
|---|---|---|
| `AtCapacity` returns `NumPlayers == 2` | `GearGameSP_Base.AtCapacity+0x0000` (DG:46651), bytes `04 9A 01 FFFFF5F8 2C 02 16` at upk 0x8B112C | Three call sites invoke it as a virtual call: `AccessControl.PreLogin+0x003B` (DE:2486), `GameInfo.Login+0x014A` (DE:21033) and `GearGameSP_Base.PostLogin+0x0010` (DG:46890). `NumPlayers` goes up only later, in `GameInfo.PostLogin+0x004F` (and `HandleSeamlessTravelPlayer+0x0376`). Player 3 therefore arrives with `NumPlayers == 2` and PreLogin refuses him with `MaxedOutMessage`. `MaxPlayers` plays no part in this check. [V] |
| The LAN beacon goes silent | The host's LAN response builder `sub_828294A0` (recomp.75.cpp:~15608) | It answers a query only when `bIsLanMatch` (+156, mask 0x40000000) is set and `NumOpenPublicConnections` (+140) > 0. On every PostLogin of a LAN match, `GameInfo.UpdateGameSettingsCounts` (DE:22135-22142) sets that count to `NumPublicConnections - GetNumPlayers()`. With 2 public connections the host stops answering once two players are in, so player 3 never sees the game. [V] |
| `NumPublicConnections = 2` | `GearMenuGame.ChangeCoopInviteSetting+0x0047` (DG:25140) and the default object `Default__GearCoopGameSettings` (tagged IntProperty, upk 0x86E033) | This value feeds the beacon gate and the menu game's `MaxPlayers` (+0x0086). Settings objects are created at DG:25208, 25438, 89251 and 92950. The one at 25208 passes through `InitializeCoopSettingsFromProfile` and then `ChangeCoopInviteSetting` (+0x0206). Nobody traced whether the others always do, so both values get patched. The two passes disagreed on whether the default object matters. [V values, I paths] |
| `MaxPlayers = 2` | `GearGameSP_Base.InitGame+0x00B4` (DG:46877), `GearCampaignLobbyGame_Base.InitGame+0x009E` (DG:25642) | The SP game's `AtCapacity` ignores it. The campaign lobby, however, uses `GameInfo.AtCapacity`, which reads `MaxPlayers` (DE:20994-21002), so it caps joins made in the GearStart lobby. `Default__GearGameSP_Base.MaxPlayersAllowed = 2` only clamps the `?MaxPlayers=` URL option, and InitGame overwrites the result anyway. [V] |

Two things that look like caps are not:

- **The `PlayerWaiting` branch of `GearGameSP_Base.RestartPlayer` (0x06EE-0x077A).** It sits inside
  the block guarded at +0x0050, which requires `GetNumPlayerControllersActive() > 1 &&
  HumanPlayer != None && DefaultPawnClass == GearPawn_COGDom` (DG:46784). Every other class jumps to
  +0x077D `Super(GearGame).RestartPlayer`, which spawns a new pawn. The branch only handles player
  2 taking over the AI Dom. [V] Do not apply the proof-of-concept edit that turns it into a jump
  (`07 07 27` -> `06 07 19` at +0x06EE).
- **The native session layer.** In this SDK, `XGISessionCreate` and `XGISessionJoinRemote/Local`
  return success whatever the slot counts (`sdk/patches/0003-system-link-trace.patch`, context
  around lines 56-87). [V from the patch context]

### 1.2 The third player's body

- `GearGameSP_Base.SuggestPawnClassFor` (DG:46987) hands out classes in this order:
  `GearPawn_COGMarcus` if no player has it, then `GearPawn_COGDom`, then `GearPawn_COGGear` (+0x0084).
  [V]
- **`GearPawn_COGGear` would spawn with no body.** The class is not abstract (ClassFlags
  0x088002B6), so the spawn itself succeeds. But no template in its chain sets a `SkeletalMesh` or a
  `PhysicsAsset`:
  - `Default__GearPawn.GearPawnMesh` sets neither.
  - `GearPawn_Infantry` sets only AnimTree, AnimSets and Translation.
  - `GearPawn_COGGear` sets only LightEnvironment.

  By contrast, Marcus sets `COG_Marcus_Fenix_CamSkel`. [V data] A player spawned as `COGGear` has
  no body and no skeleton, and native animation, weapon-socket or gore code may misbehave or crash.
  [I] The roadmap line "SuggestPawnClassFor already falls back to GearPawn_COGGear" is therefore not
  enough.
- **Two more differences from Marcus and Dom.**
  - `COGGear` and the Carmine classes inherit `bKillDuringLevelTransition = True` from
    `Default__GearPawn`. Only Marcus and Dom set it to False, which matters for chapter travel in
    Stage 2.
  - Their default inventory is the Lancer alone. Marcus and Dom also get a pistol and a shotgun.

  [V]
- **Ben Carmine is the right look, but it is only loadable in some maps.**
  `GearPawn_COGBenjaminCarmine` ("B. Carmine") uses mesh `COG_Minh.Minh_Kim` and helmet
  `Item_Helmet_COGTwo`, and its super is `GearPawn_COGGear`.
  - There is no `GearGameContent` package on disk. Content classes exist only as forced exports
    inside map packages, so a lookup by path (`FindObject`, which S1-2 uses, or
    `DynamicLoadObject`) finds one only while a loaded map holds it.
  - The chapter numbers below come from `Coalesced_int_full.txt:5976-5986`. The class is in the
    persistent maps of chapter 2 (`SP_Hospital_P`), chapters 5-6 (`SP_Landown_P`), 7-9
    (`SP_Intervention_P`) and 10-11 (`SP_Rescue_P`).
  - For chapters 3-4 it is only in streaming sub-levels. Chapter 1 and Acts 3-5 do not have it.
  - Related classes: `GearPawn_COGRedShirt` (a generic COG soldier with the same mesh and a random
    helmet) is in 18 map packages. `GearPawn_COGCarmine` ("A. Carmine", the class the cheat at
    DG:24457 loads) exists only in `SP_Landown_5_S`.
  - The startup packages contain no `GearPawn_COG*` class.

  [V, header scan of all 455 SP, GearGame_P and GearStart packages:
  `out/re-coop/join-spawn/scan_cog_pawn_classes.txt`]

### 1.3 Extra players must not be Marcus or Dom

- Kismet's `SeqVar_Marcus` and `SeqVar_Dom` are native. Each walks `WorldInfo.PawnList` and returns
  the first pawn that `IsA` `GearPawn_COGMarcus` or `GearPawn_COGDom`, or that pawn's controller or
  vehicle (`sub_828F6040` / `sub_828F60C8` -> `sub_828F5FC8`). [V]
- `PawnList` is ordered newest first. `sub_825E2780` (recomp.214.cpp:10171-10240) unlinks a pawn if
  it is already in the list, sets its `NextPawn` (+460) to `WorldInfo.PawnList` (+868), and stores
  the pawn as the new head. [V code; that this is `AWorldInfo::AddPawn` and runs for every spawned
  pawn is I]
- So a Marcus clone spawned after the host would become "Marcus" for every Kismet teleport,
  cinematic toggle and inventory gift, and the host would be left out. [I] Extra players need a class
  of their own.
- Other hero lookups are safe for extra classes. `SeqVar_Player` already means all players:
  `bAllPlayers` is True by default and none of the 1092 instances in 450 scanned maps overrides it.
  `SeqCond_IsCoop` and `SeqEvt_CoopTouch` look only at the Marcus and Dom classes. [V]

### 1.4 System Link in the port blocks even two players

From `network.md`. Line numbers refer to the patched SDK tree in `out/sdk-build/rexglue-sdk` before
patch 0010, which addresses every item in this list (section 3, Stage 1, item 2). [V unless marked]

1. `XNetGetEthernetLinkStatus` returns 0 (`xam_net.cpp:600-606`). `HasLinkConnection()` is
   therefore false and `GearMenuGame.InitializeCoopSettingsFromProfile+0x0065` aborts. The link
   between the native `HasLinkConnection` and `sub_8282A5E0` is [I].
2. `XGISessionCreate` (msg 0x000B0010) writes neither `XSESSION_INFO` nor the nonce. The host's
   beacon response copies the host address, XNKID and XNKEY out of that buffer (`sub_828294A0`,
   from SessionInfo+20/+12/+56), so clients receive nothing usable.
3. `GetResolvedConnectString` (`sub_8282CC90`, OnlineSubsystemLive vtable at .rdata 0x82177814)
   always calls `XNetXnAddrToInAddr`, because init sets `GSocketSubsystem+108` to 1 (`sub_8283C760`
   at 0x8283C7B8). The stub returns 1 without writing anything, so the client never runs
   `open <ip>` (`OnJoinCoopGameComplete+0x003A`). `XNetRegisterKey` is not on the campaign path; it
   belongs to the party-beacon resolver that only versus parties use.
4. `XSocket::SendTo` stores host-order values in network-order fields (`xsocket.cpp:237-245`). Port
   1000 goes out as 59395 and beacon port 14001 as 45366, and unicast IPs are reversed. `RecvFrom`
   (`:202-210`) does not swap the port and writes `from` even when the call fails.
5. Every instance reports XUID `0xB13EBABEBABEBABE` and the name `User` (`user_profile.cpp:28-29`).
   `GearCampaignLobbyGame_Base.OnFindCoopMatch+0x00AA` drops results owned by the local XUID, so a
   peer's session looks like the player's own.
6. `XNetGetTitleXnAddr` returns 127.0.0.1 and MAC CC:CC:CC:CC:CC:CC on every instance
   (`xam_net.cpp:496-530`). `XNetRandom` fills its buffer with 0xBB (`:286-292`), so every query
   nonce is identical.
7. `sendto` never sets the guest last-error (`xam_net.cpp:1224-1249`), yet `LowLevelSend`
   (`sub_8283C020`) reads `WSAGetLastError` after a failed send. Once the link reports active, errors
   10051, 10053, 10054, 10060 and 10065 mark the connection as failed (0x8283C130).
8. Several instances on one PC all bind `0.0.0.0:1000` and `0.0.0.0:14001` with `SO_REUSEADDR`, and
   Windows delivers each unicast datagram to only one of them. [V binds; delivery semantics I]
9. `XSessionLeave` (0x000B0013) has no handler, every session shares one handle, and
   `XSessionGetDetails` leaves its buffer empty.

Not needed:

- The game imports none of `XNetCreateKey`, `XNetConnect`, `XNetGetConnectStatus` or
  `XNetInAddrToXnAddr`.
- It skips QoS for LAN sessions (`sub_8282B640`).
- VDP packets are at most 512 bytes and pass through unchanged.

## 2. Patch mechanism: script patches in guest memory

Decision: change UnrealScript bytecode and default objects in guest memory, from a host hook in
`src/` that runs right after the engine loads its native script packages. Do not ship a rewritten
`GearGame.xxx`.

### Why

- **The file route is expensive.** `GearGame.xxx` is one fully compressed stream of 506 independent
  128 KiB LZX blocks: 25,001,984 bytes on disk, 66,293,489 decompressed. [V] Its HashSec entry holds
  the SHA-1 of the decompressed package. [V for Core, Engine, GameFramework and GearGame] Changing
  the file needs one of two things:
  - an LZX encoder, which does not exist here (the SDK's `lzx.h` and libmspack only decode);
  - a 66 MB uncompressed copy plus a TOC change and a new hash, regenerated for every game version.
- **Memory has no such costs.** No hash, no TOC and no cached copy. The same `src/` code runs on
  Windows and Android (the Android build globs `src/*.cpp`).
- **Same-size edits are safe.** Bytecode has the same length in memory as on disk.
  - `ScriptSize` equals the on-disk length for all 10,331 GearGame functions and all 4,869 Engine
    functions.
  - Object references are 4 bytes: a package index on disk, a pointer in the 32-bit guest.
  - Names are 8 bytes (an FName).
  - Jump targets are absolute big-endian WORDs, so a same-size edit never moves an offset.

  [V]
- **References resolve at run time.** A patch can point at classes the package does not import:
  GearGame.upk has no Carmine import. [V]

### Design

- **Hook.** `REX_HOOK_RAW(sub_8283F780)`, on `FStartupPackages::LoadAll`, which has a single call
  at 0x8284075C in `sub_828406F0`. It calls `__imp__sub_8283F780`, then applies the patch table.
  - LoadAll loads Core, Engine, GameFramework, IpDrv, GearGame and OnlineSubsystemLive. No gameplay
    script has run yet, and every Stage 1 target lives in GearGame.
  - The generated `sub_` symbols are weak aliases (`gow2_pch.h`). A strong definition in `src/`
    replaces both the direct call and the function-table entry; the GTA IV ReXGlue port does this
    at scale.
  - Implemented in `src/gow2_script_patch_hook.cpp`. With clang and lld-link on Windows the
    override wins: in the link map `sub_8283F780` comes from `gow2_script_patch_hook.cpp.obj` and
    `__imp__sub_8283F780` from `gow2_recomp.172.cpp.obj`, and `llvm-objdump` shows the call that
    was `bl 0x8283f780` in `sub_828406F0` going to the hook. [V link; that it runs (the log line
    `SCRIPTPATCH startup script packages loaded`) and the Android link are not confirmed yet]
- **Lookup.** [V]
  - `GObjObjects` is the TArray at 0x83136860 and `GNames` the one at 0x83136834. An FNameEntry has
    its flags at +0 (bit 0 = UTF-16) and its text at +8.
  - UObject fields: Outer +40, Name +44 (index, number), Class +52.
  - A function is found by package, class, optional state and name. State functions sit one level
    deeper (Function -> State -> Class -> Package), so the walker accepts both depths and requires
    exactly one match.
- **Script array.** `UStruct::Script` is a TArray at +84. Data is at +84 [V, from `execJump`];
  Num at +88 and Max at +92 follow the standard TArray layout [I].
- **Edit kinds.**
  1. Same-size bytes.
  2. Object operand swap: write another object's address into a 4-byte operand.
  3. Default-object field write.
  4. Grown script. Allocate a new buffer with the game's own allocator (`GMalloc` at
     [0x8310B12C], vtable +4 = Malloc(size, align), called with align 8 as the package loader does at
     0x8226F610), fill it with the kept prefix and then the tail, and write Data, Num and Max. The
     row's own byte and operand edits go into the new prefix, so the old buffer is never written,
     and never freed. Allocating outside `GMalloc` risks a mismatched free when the array is
     destroyed. [I] The buffer is allocated while the group is still being checked, so a later
     failed check leaks that one block; nothing, the new buffer included, is written before the
     whole group has passed. An edit after the kept prefix would be lost, so it is a table error:
     the group is skipped with `+0x0007 is past the kept prefix +0x0006`, and
     `tools/script_patch.py` refuses to print such a row. Rows may also keep
     everything but the final EndOfScript and append code reached by redirecting a
     literal jump word in the prefix (Stage 2), or replace a whole function (keep 0);
     name and object operand bytes are never edited because their in-memory values are
     only known at run time.
- **Guest addresses.** Every read, write and copy goes through `src/gow2_guest.h`, which places a
  guest address where the runtime does. On Windows the heap at 0xE0000000 and above sits 0x1000
  higher in host memory (`PhysicalHostOffset` in the SDK's `xmemory.h`; a `static_assert` in the
  hook keeps the two equal), and copies go byte by byte, so a range may cross 0xE0000000. `GMalloc`'s
  pool pages come from `XPhysicalAlloc` and land at 0xA0000000 today, but `sub_82AAA130` clears the
  large-page flag when [0x830D9B2C] is set (`rlwinm r5,r5,0,3,1`), and a 4 KiB-page block would come
  from the 0xE0000000 heap. [V for the flag change, I for the heap] The core test puts a grown script
  at 0xE0000000 and checks that nothing lands 0x1000 below it.
- **Fail closed.** The patcher validates a group completely before it writes any byte. Each row must
  pass all of these:
  - the function is found exactly once;
  - `Script.Num == ScriptSize`, and before a grown write also `Max == Num`;
  - every expected byte matches;
  - every pointer operand equals the object resolved by name.

  Bytes that are already patched count as applied. Any failure skips the whole group, leaves the
  game retail and logs `SCRIPTPATCH <group>: skipped (<row>: <reason>); the game stays retail`.
  Success logs one line per row plus a hash of the applied set.
- **Lookup as built.** Names match without regard to case, as in UE3, and a name's text must occur
  once in `GNames`. An object matches when its own name and each outer spell the path exactly, the
  outermost outer is null, and the name of its class is the expected one (`Function`, `Class`,
  `IntProperty` and so on). The set hash is FNV-1a 64 over the rows and N, never over guest
  addresses, so it is the same on every run and platform. On Windows a fault while reading guest
  objects (a wrong layout) is caught and skips the group; Android has no such guard.
- **Setting.** `gow2_coop_players` (int). The default 2 means off. Values from 3 to 255 enable the
  Stage 1 group with N players; 255 is the ceiling because the cap is an `IntConstByte`. Two debug
  settings: `gow2_coop_marcus_clones` uses row D-1 in place of S1-2, and `gow2_coop_patch_fail_test`
  corrupts the first expected byte of the table (S1-1 +0x0000), which must skip the group.
- **Offline tool.** `tools/script_patch.py` grew out of `out/re-coop/script_patch_poc.py` and
  `join-spawn/verify_patches.py`. It owns the rows: `--cpp` prints
  `src/gow2_script_patch_table.cpp` (a test fails when the two differ), and `--check <folder>` runs
  on the player's own decompressed packages and, for each row of the group and for D-1:
  - checks the old bytes, the context bytes and every object operand (as a package reference);
  - applies the edit to a copy and re-decompiles it;
  - confirms the function still decodes to its end and, for the grown row, that no jump in the
    kept prefix lands in the replaced tail;
  - checks the default object's value, then prints the C++ table.

  Its tests (`tools/tests/test_script_patch.py`) and the C++ test target `gow2_script_patch_test`
  (`tools/tests/script_patch_core_test.cpp`, the patcher core on a synthetic guest memory) use
  synthetic bytecode only. The repo commits byte strings and offsets, never game data.
- **Build wiring.** `src/gow2_script_patches.cpp` (the portable core), `gow2_script_patch_table.cpp`
  and `gow2_script_patch_hook.cpp` (hook, settings, `GMalloc`, logging; the only part that needs
  the runtime) are in `GOW2_SOURCES` in `CMakeLists.txt`, which is an explicit list. Android picks
  them up by glob.
- **Peers.** In-memory patches do not change package GUIDs, so the engine will not notice a host and
  a client running different patch sets. Stage 1 logs the patch-set hash; Stage 4 puts it in the
  session data.

### Fallback B: only for edits that need new names or imports

Serve the decompressed, edited package uncompressed through `GOW2_VFS_OVERRIDES`, together with an
edited `Xbox360TOC.txt`:

- Keep the GearGame line and its sector, but set column 2 (uncompressed size) to 0. The 360 loader
  decides compression only from that column (`sub_827F5B58` returns -1 for 0), and `FileSize` comes
  from the TOC.
- Write the new file's SHA-1 into the `geargame.xxx` HashSec entry.

[V loader paths; never run] The file route touches LZX blocks 63, 64, 67 and 69 and costs a 66 MB
cached copy per game version.

### Related finding (not solved by Stage 1)

The game reads and hashes each edited Coalesced bundle at its TOC size. `sub_82225018` creates the
file reader, takes its `TotalSize()`, allocates and serializes that many bytes and wraps them in
FBufferReaderWithSHA (vtable 0x820954C0); `sub_827F5508` sets the reader's size (+148) from the TOC
`FileSize` while the flag at 0x830C9734 is set. [V] The served bundles carried 4 KiB of zero
padding, so HashSec received the SHA-1 of a file about 2.1 KB longer than the bytes the game
hashes. That most likely raised the DISCERR that every run with Coalesced overrides logged and that
`DiscErrorNoop` parks on a worker thread. [I, from code plus logs] `src/gow2_coalesced.cpp` now
pads each edited bundle to exactly its `FileSize` in `GearGame/Xbox360TOC.txt` (the original file
size when the TOC has no entry; every retail Coalesced TOC size equals its file size) and does not
serve a bundle whose edit is longer. The comment in `src/gow2_xam_input_guard.cpp` that blamed a
missing disc drive is corrected.

The Stage 1 runs show the padding is not the whole story. Every run with the Coalesced overrides
still logs `DISCERR the game's 'Disc Read Error' box was suppressed` once, about a second after
boot. That holds with co-op on or off, and with the bundles freshly rebuilt at their TOC size
(`COALESCED: ... 0 reused from cache`; `out/coop-test/run3-coop2`, `run4-coop4`, `net8`). A run
with the overrides off logs no DISCERR (`--mnk_mouse=false --gow2_mouse_aim=false
--gow2_prompts=controller`, `out/coop-test/discerr-no-coalesced`). [V] So the Coalesced layer still
raises it, and the cause is open. The game plays on either way: `DiscErrorNoop` parks only the
thread that asked for the box.

## 3. Staged plan

### Stage 1: four players in Act 1 on one PC over loopback

Goal: a host plus three clients, as four `gow2.exe` instances on 127.0.0.1-127.0.0.4, all spawned
and playing in Act 1 chapter 2. Players 3 and 4 spawn as Ben Carmine, or as RedShirt if Ben is not
loaded.

Why chapter 2: its persistent map `SP_Hospital_P` holds `GearPawn_COGBenjaminCarmine`, so the class
stays loaded on the host and on every client for the whole chapter. Chapter 1 lacks it.

Work:

1. **Script patches.** The group in section 4, behind `gow2_coop_players`. Done; section 2 names the
   files. For diagnosis, `--gow2_coop_census=true` logs, whenever they change, the loaded
   `GearPawn_COG*` classes with their path, the pawns per class and every player controller's
   `DefaultPawnClass` (`src/gow2_coop_census.cpp`; read-only, from a host thread).
2. **SDK patch `0010-system-link-sessions` (M2a), applied after 0002-0007:**
   - SendTo/RecvFrom: fill `sockaddr_in` in network order, swap the received port, and write
     `from` only when the call succeeded.
   - `XNetGetEthernetLinkStatus`: return 0x0B (active, 100 Mbps, full duplex) when System Link is
     turned on, and 0 (cable unplugged) otherwise, as before.
   - `XGISessionCreate`:
     - When flags bit 0 (HOST) is set, write into `XSESSION_INFO` a random XNKID with the
       system-link marker, the host XNADDR and a random XNKEY, and write a random nonce. [V for the
       bit: the join path `sub_8282CA88` clears it]
     - When bit 0 is clear, leave the buffer untouched, because the joiner passes the search result
       in it.
     - Give each session its own handle and slot count.
     - Return success for 0x000B0013 (leave), and fill `XSessionGetDetails`.
   - `XNetXnAddrToInAddr`: copy `xnaddr->ina` (raw, in network order) to `*in_addr` and return 0.
     Making `XNetRegisterKey` succeed is harmless but optional.
   - Last-error: `sendto`, `setsockopt` and `bind` set WSA codes. On Windows, turn off
     `SIO_UDP_CONNRESET` on UDP sockets; otherwise an ICMP port-unreachable becomes error 10054 and
     drops the connection once the link is active.
   - `XNetRandom`: use a real RNG, with a deterministic setting for tests.
   - Identity settings:
     - `user_xuid` (persisted, random per install, bits 54-55 clear) and `user_name`;
     - `net_local_ip`, which feeds `XNetGetTitleXnAddr.ina`;
     - a per-instance locally administered MAC.
   - One-PC settings:
     - `net_bind_ip` rewrites INADDR_ANY binds to the instance's 127.0.0.x address.
     - `net_broadcast_targets` sends each broadcast as one unicast copy per listed peer, skipping
       self.

     A socket bound to a specific address never receives broadcasts, so this mode requires the
     fan-out. [I] Both settings stay off on a real LAN.

   Implemented in `sdk/patches/0010-system-link-sessions.patch`. Settings:

   | Setting | Default | Effect |
   |---|---|---|
   | `net_link_active` | false | true: `XNetGetEthernetLinkStatus` returns 0x0B and System Link works. false: it returns 0 (unplugged cable), as before 0010. `gow2_coop_players` 3 or more turns it on unless `gow2.toml` or the command line set it |
   | `user_xuid` | empty | XUID the game sees. Empty with the link on: the one in `system_link_xuid.txt` in the user data folder, created on the first such run as `0xB13E` plus 48 random bits. When `gow2_save_dir` moves the user data folder, the game reads or makes that file in the PC's default user folder instead and passes it as `user_xuid`, so PCs that sync one save folder keep different XUIDs. Empty with the link off: the fixed offline XUID 0xB13EBABEBABEBABE, and no file is created |
   | `user_name` | `User` | Gamertag, up to 15 characters |
   | `net_local_ip` | empty | `XNADDR.ina`. Empty: `net_bind_ip` when set, else the address of the adapter that holds the default route, else 127.0.0.1 (with a warning that other PCs cannot join). The log line `[netlink] link active: local <ip> (<where from>)` names it; set it by hand when that is not the LAN address |
   | `net_bind_ip` | empty | Rewrites `0.0.0.0` binds and logs `[netlink] bind 0.0.0.0:1000 -> 127.0.0.2:1000 (net_bind_ip)` |
   | `net_broadcast_targets` | empty | Unicast copies of every 255.255.255.255 send, skipping this instance (`net_bind_ip`, else `net_local_ip`); the list is logged once |
   | `net_random_seed` | 0 | Non-zero: `XNetRandom`, XNKIDs, XNKEYs and nonces come from a SplitMix64 stream with this seed |
   | `net_traffic_log_sec` | 0 | Non-zero: every that many seconds, log the UDP bytes and packets sent to and received from each peer: `[nettraffic] last 10.0 s: sent to 127.0.0.2:59687 31322 B in 300 packets (3132 B/s)` |

   Every one of these settings needs a restart. The network ones are read once, when the kernel
   starts, and the game threads only read that copy: the settings panel cannot move the advertised
   address under a session that already handed it out, and no thread reads a string the panel is
   rewriting.

   Decisions taken while implementing:

   - System Link is opt-in. With the defaults a solo game behaves as before 0010: the link reports
     down, so the game creates no LAN session; it sees the offline XUID, and nothing is written to
     the user data folder. Its one game socket binds UDP 0.0.0.0:1000 with 0010 alone (Stage 1 test
     run 3) and 0.0.0.0:41000 with 0011 on top, because 0011's port remap does not depend on the
     link (Android cannot bind port 1000 even in a solo game). What still differs without the link
     are fixes that do not depend on it: socket addresses in network order, a random `XNetRandom`,
     a per-instance MAC in `XNetGetTitleXnAddr` instead of CC:CC:CC:CC:CC:CC, and 0011's port
     remap and POSIX socket fixes.
     `gow2_coop_players` 3 or more turns the link on in `OnPreSetup`, before the kernel reads it, and
     logs `SCRIPTPATCH coop: gow2_coop_players=4 turns System Link on (net_link_active=true)`; an
     explicit `net_link_active` in `gow2.toml` or on the command line wins. Two-player System Link
     needs `--net_link_active=true`.

   - Saves and profile files stay keyed by the old fixed identity. `UserProfile::xuid()`
     (0xB13EBABEBABEBABE) still names the content folder and `name()` ("User") the profile-settings
     folder. The game sees `network_xuid()` and `gamertag()` instead, through `XamUserGetXUID`,
     `XamUserGetSigninInfo`, `XamUserGetName`, `XamUserGetGamerTag`, profile-setting reads and
     `XamContentGetCreator`. Changing `user_xuid` or `user_name` never moves a save. Whether the game
     accepts an existing save under a new XUID is a test item; `user_xuid=B13EBABEBABEBABE` restores the
     old value. With the link off the game keeps seeing 0xB13EBABEBABEBABE.
   - The MAC is `02` plus five bytes of SplitMix64 over the XUID and the IP, so instances on one PC
     differ even when they share a XUID.
   - Session handles are 0xCAFE0004, 0xCAFE0008 and so on. Each session object is a zeroed 16-byte
     system-heap block, and `ObDereferenceObject` skips these blocks: the title dereferences the object
     after every XSession call (for example `sub_829A5810` and `sub_829A3C70`). Unknown handles still
     get the old 0xDEADF00D.
   - Joins and leaves keep a member list per session; slot limits are not enforced. Leave handles
     both forms, but the title only uses the remote one (`sub_829A3C70` passes a XUID array).
     `XSessionGetDetails` fills the 128-byte `XSESSION_LOCAL_DETAILS` and as many 16-byte members as the
     caller's size allows, right after it. The only caller, `sub_828333C8`, passes 640 bytes.
   - `XNetRegisterKey`, `XNetUnregisterKey` and `XNetUnregisterInAddr` return 0.
   - `getsockname` returns the host's address and port for the socket. It was a stub that failed
     with WSAEOPNOTSUPP. The engine binds its game socket to port 0 and reads the port back, so
     every join ended in `Error initializing network layer. LiveSock: binding to port 0 failed
     (10045)` (Stage 1 test, step 6).
   - On Android the patch has only passed an NDK arm64 `-fsyntax-only` check; it has not been built
     or linked. errno maps to WSA codes for `sendto`, `setsockopt` and `bind` only, and
     `XSocket::SetOption` and `socket_ioctl` still hand the 360's SOL_SOCKET (0xFFFF) and FIONBIO
     (0x8004667E) to the POSIX calls unchanged. Android System Link needs the Stage 4 mappings.

   With the link on, a solo campaign also creates its LAN session and answers System Link queries
   on UDP 14001, as a 360 with a cable would, so anyone on the LAN can find it. [I] That is why the
   link is off unless co-op asks for it.
3. **Tools.** Done: the package and script tools are in `tools/ue3/` (one line each in its
   README), next to `tools/script_patch.py`. `scripts/coop-instances.ps1` runs several instances on
   one PC, and `--gow2_test_input=<file>` drives one without window focus (section 5, Running four
   instances on one PC).

Out of scope for Stage 1 (known to break; see Stage 2): chapter transitions, the death of any
player, a player leaving, cinematics with extra players present, vehicles and co-op splits.

#### Stage 1 test

Offline:

1. Run `python -I tools/script_patch.py --check <folder>` on your own decompressed Core, Engine,
   GameFramework and GearGame packages. Every old byte must match, and every patched function must
   decode to its end: AtCapacity 0x11, ChangeCoopInviteSetting 0xB2, lobby InitGame 0xA9, SP
   InitGame 0xCA, and SuggestPawnClassFor 0x13E with the new tail. The default object must hold 2.
   Passed on 2026-10-14 against the retail packages, D-1 included, and again on 2026-10-15 with the
   318-byte tail; the jumps in the kept prefix of SuggestPawnClassFor go to 0x48, 0x67 and 0x84.
2. Unit tests on synthetic bytecode: `python -m pytest tools/tests/test_script_patch.py` and the
   `gow2_script_patch_test` target. A wrong old byte, a wrong `Num`, a wrong pointer operand, or a
   function found twice must each skip the whole group, and so must an edit past the kept prefix
   of a grown row. A grown row's own edits must land in the new buffer, and a grown script placed
   at 0xE0000000 must be written where the game reads it. Both pass (25 tests, 92
   checks).

One instance, no network:

3. Run with `--gow2_coop_players=2` (the default). The log shows `SCRIPTPATCH startup script
   packages loaded` and `SCRIPTPATCH coop: off (gow2_coop_players=2)`, and play is unchanged.
4. Run with `--gow2_coop_players=4`. The log shows one `SCRIPTPATCH coop S1-n <function>: ...`
   line per row (S1-2 `script grown to 318 bytes`) and `SCRIPTPATCH coop: 6 rows applied for 4
   players, set hash <16 hex digits>`. Play the start of chapter 1 solo: nothing changes, because
   the patches only act on joins.
5. Add `--gow2_coop_patch_fail_test=true`. The log shows `SCRIPTPATCH coop: skipped (S1-1: +0x0000
   expected FB, found 04); the game stays retail`, and the game stays retail.

With SDK patch 0010, the single-instance runs also check its defaults. Run 3 (no co-op, no network
settings) must look like a run before 0010: the log shows `[netlink] link reported down
(net_link_active=false): no System Link` and `Signed-in player: XUID B13EBABEBABEBABE, gamertag
'User'`, an existing save loads, no `system_link_xuid.txt` appears in the user data folder, no
Windows Firewall prompt appears beyond any the port already raised for UDP 1000, and with
`--system_link_trace=true` the trace shows `XNetGetEthernetLinkStatus ... ret=0x0`, a single bind
(`0.0.0.0:1000`; with patch 0011, `bind port 1000 -> 41000 (net_port_offset)`) and no
`XGISessionCreate`.

Run 4 (`--gow2_coop_players=4`) also logs `SCRIPTPATCH coop: gow2_coop_players=4 turns System Link
on (net_link_active=true)`, `[netlink] link active: local <ip> (<where from>)` and a `Signed-in
player` line with the per-install XUID (`B13E` plus 12 hex digits), creates `system_link_xuid.txt`,
and with `--system_link_trace=true` the solo campaign logs `XGISessionCreate ... flags=0x00000021 ...
host=true known=true ... host_ip=<that ip>` and play is unchanged. Load an existing save in this run
too: it is the first run under a new XUID.

Network (needs SDK patch 0010):

6. Two instances: host on 127.0.0.1, client on 127.0.0.2, unique XUIDs, fan-out targets set. The
   client finds the session (the beacon names 127.0.0.1), runs `open 127.0.0.1` and takes over the
   AI Dom. This is the two-player baseline; the script patches play no part yet. Capture with
   `system_link_trace`. Settings: host `--net_bind_ip=127.0.0.1`, client `--net_bind_ip=127.0.0.2`,
   both `--net_link_active=true --net_broadcast_targets=127.0.0.1,127.0.0.2,127.0.0.3,127.0.0.4
   --system_link_trace=true`,
   and a different XUID per instance (`--user_xuid=...`, or a separate `--user_data_root`, which also
   separates the saves; `scripts/coop-instances.ps1 -SeedUser` drops the seed's
   `system_link_xuid.txt` from each copy, so every instance makes its own XUID).

   Done 2026-10-14 [V] with the crossplay build (patches 0010 and 0011): the client found the host in
   the System Link Browser, joined the lobby, loaded chapter 2 with the host and spawned as Dom on
   Right Trigger. Harness: scripts/run-instances.ps1 and scripts/instance-input.ps1; steps and log
   lines in docs/NETWORK_PLAY.md, "Testing on one PC". Separate user data came from a per-copy
   LOCALAPPDATA, because that build set user_data_root from %LOCALAPPDATA% and ignored
   --user_data_root; since fdbfc4e an explicit --user_data_root works as well
   (scripts/coop-instances.ps1), and on the command line it outranks a `gow2_save_dir` in
   `gow2.toml` (docs/SAVES.md).
7. Host and player 2 play into Act 1 chapter 2. One way to get there is to play chapter 1 first; a
   profile with chapter 2 unlocked also works. `GearGameSP_Base.InitGame+0x0063` reads `?chapter=`,
   but nobody has tested passing it.
8. Instances 3 and 4 (127.0.0.3 and .4) search and join. Expect each of these:
   - The host keeps answering the beacon until four players are in.
   - PreLogin and Login accept both joiners.
   - Each joiner views the AI Ben (`PostLogin` finds a pawn of his class), spawns on Right Trigger
     as Ben Carmine within 1024 uu of a teammate, and joins squad Alpha. The host stays leader.
9. Play 10 minutes of combat with four players. Everyone sees the others move, shoot and take
   damage, with no crash and no desync.
10. A fifth instance must not see the game. If forced to connect, it must be refused with
    `MaxedOutMessage`.
11. Control run: repeat step 8 with `gow2_coop_players=2` on the host. Instance 3 must be refused.
12. Record per-client net stats as the bandwidth baseline (7000 bytes/s cap, see Stage 3).

Join when no cinematic is playing: a remote join more than 15 s into a level cancels the running
cutscene for everyone (Stage 2, item 5).

#### Stage 1 results (2026-10-15)

Run in the `coop-stage1` worktree build, Windows, RX 7800 XT, every instance windowed at
`resolution_scale=1`. Each instance got its own `--user_data_root` holding a copy of a player's save
at Act 1 chapter 2, *Desperation*. Input went through `--gow2_test_input` pad files, and
screenshots were taken of each instance's window. The evidence is under `out/coop-test/` in the
worktree. That folder is not in the repository: its saves and screenshots come from the player's
own game.

| Step | Result | Evidence |
|---|---|---|
| 1 | Passes with the new tail: every row validates, and S1-2 decodes to its end at 0x13E. | `step1-check-final.txt` |
| 2 | 25 Python tests pass, and 92 core checks with 0 failures. | `step2-pytest-final.txt`, `step2-core-test-final.txt` |
| 3 | `SCRIPTPATCH coop: off (gow2_coop_players=2)`, `[netlink] link reported down (net_link_active=false)`, `Signed-in player: XUID B13EBABEBABEBABE, gamertag 'User'`; the trace has `XNetGetEthernetLinkStatus ... ret=0x0`, one bind (`0.0.0.0:1000`) and no `XGISessionCreate`; the save loads and Marcus plays; no `system_link_xuid.txt`. That first bind of a new exe raised the Windows Firewall prompt, the one the port raises for UDP 1000 (below). | `run3-coop2/` |
| 4 | Every row logs (`S1-2 ...: script grown to 318 bytes` after the fix), `6 rows applied for 4 players`, `turns System Link on`, `link active: local 192.168.10.10 (default-route adapter)` and a per-install XUID (`B13E79F4E114F9B3`). `system_link_xuid.txt` is created, and the save loads under the new XUID. Solo play is unchanged. But about 40 s of solo campaign made no `XGISessionCreate`, so the claim that a solo campaign hosts a LAN session ([I] in Stage 1 item 2) was not seen. | `run4-coop4/` |
| 5 | `SCRIPTPATCH coop: skipped (S1-1: +0x0000 expected FB, found 04); the game stays retail` | `run5-failtest/` |
| 6 | The client found the host (`SYSTEM LINK BROWSER: Player1, Act 1`), then failed with `LiveSock: binding to port 0 failed (10045)`; fixed by `getsockname` in patch 0010. After the fix: `XGISessionCreate ... flags=0x00000021 ... host=true` on the host, `flags=0x00000020 ... host=false` and `XNetXnAddrToInAddr ... ina=127.0.0.1 ret=0x0` on the client, `getsockname ... addr=127.0.0.2:64817`, `XGISessionJoinRemote ... members=2` on both. Player 2 shows in the lobby, presses RT after the launch and takes over Dom. Both move independently. | `net6-try1/`, `net6/` |
| 7 | Reached through the save: its slot is Act 1, chapter 2 *Desperation*, so HOST CO-OP CAMPAIGN, then the slot, then LAUNCH starts chapter 2 directly. `?chapter=` and chapter select were not needed. | `net6/003128-i1-lobby-after-2.png` |
| 8 | Players 3 and 4 find the session while 2 are in game and join (`XGISessionJoinRemote ... members=3`, then `members=4`; `slots_pub=4`). First run: both spawned as Marcus clones, with `DefaultPawnClass=None` in the census although `GearGameContent.GearPawn_COGBenjaminCarmine` was loaded. Cause and fix in section 4 (S1-2). After the fix: `GearPC_SP_2 DefaultPawnClass=GearPawn_COGBenjaminCarmine`, the same for `SP_3`, and `GearPawn_COGBenjaminCarmine x2` pawns; both carry a Lancer and spawn next to the squad. The same classes come out when all four are in the lobby before the launch (Marcus, Dom, Ben, Ben). There the instance that arrives second takes Dom, and in that run it was instance 3. [I] | `net8b/`, `net8c/` (before), `net8e/`, `net8-lobby4/` (after) |
| 9 | Partial. Four players moved, turned and fired in turn for 5.5 minutes, about 7 minutes with all four connected, with no crash. All four processes were alive at every minute's check, the classes stayed Marcus, Dom, Ben and Ben, and there are bullet holes in the walls; one Ben player emptied his Lancer (`net9-soak`). In `net9-five` the host's ammo went 413 -> 393, and other squad members show in each view. Before the fix, four connected instances stayed up for about 15 minutes without a crash, while Locust drones spawned and died in the census (`net8d`). Not done: ten minutes of real combat, and a desync check beyond screenshots. | `net9-soak/`, `net9-five/`, `net8d/` |
| 10 | With four players in, a fifth instance sees `NO CAMPAIGNS FOUND`, also after a refresh. Forcing it to connect has no menu path and was not tried. | `net9-five/014040-i5-browser-refreshed.png` |
| 11 | With the host at `gow2_coop_players=2` (retail caps, `slots_pub=2`), instance 3 sees `NO CAMPAIGNS FOUND` once player 2 is in. | `net11-control/014535-i3-browser.png` |
| 12 | `--net_traffic_log_sec=10` on all four. Host to each client: 3.0-3.5 KB/s on average, peak 5.3 KB/s, about 29 packets/s. Each client to the host: about 0.7 KB/s, peak 1.6 KB/s. That is under the 7000 B/s cap in this section of chapter 2. | `net9-host-traffic-summary.txt` |

The Windows Firewall prompt: the first time a new `gow2.exe` path binds UDP 1000, Windows asks
whether public and private networks may reach it. The dialog dims the whole screen
(`Shell_SystemDim`), and until someone answers it no other window can take the foreground or be
clicked. Loopback traffic does not need the rule, so the tests above ran with the prompt still open,
driven through the pad files. Answering it is the player's decision; the test left it open.

### Stage 2: campaign systems

Each item gives the evidence and then a candidate fix. Script changes are sized with
`tools/script_patch.py`; most of them grow their function and need edit kind 4.

1. **Chapter travel kills extra players and ends the game.**
   - `UWorld::CommitMapChange` (`sub_824A1AC8`; recomp.18.cpp:6466, 7942, 8565) destroys every
     persistent-level actor with `bKillDuringLevelTransition` set (Actor+0xB0, mask 0x40000000).
   - The extra's pawn dies. `PawnDied` sends his PC to state `Dead`, which arms `ProcessGameOver`
     for 5 s.
   - `PostCommitMapChange` (DG:46711) restarts only local PCs that are not in `Dead`.
   - `SaveCheckpoint+0x0082` refuses to save while that timer runs. The reload therefore lands
     before the transition and can loop.
   - 24 SP maps contain `SeqAct_CommitMapChange`.

   [V chain; outcome I]

   Fix: clear the flag on player pawns at spawn (in `SpawnDefaultPawnFor`, through the empty `if` at
   +0x008A). Do not clear it on the `COGGear` default object: AI squadmate classes loaded later
   from map packages would inherit the change and survive transitions they are meant to die in.
   [I] Clients never destroy a replicated pawn here: `DestroyActor` refuses non-authority actors
   without `bNetForce`. [V]

   Group A: rows S2A-1 and S2A-5 (checkpoints while an extra is dead), written and
   checked offline; see [COOP_STAGE2_SURVIVAL.md](COOP_STAGE2_SURVIVAL.md).
   In-game results: Stage 2 results below.
2. **Game-over rules.**
   - In SP, any human PC that enters `Dead` arms `ProcessGameOver` (5 s, or 2 s in a crate section;
     `GearPC.Dead.BeginState`, DG:35009), which sends `ClientGameOver` to everyone.
   - `Dead.EndState` (DG:35040) calls `ProcessGameOver` at once when a remote PC is destroyed while
     the timer is armed.
   - `EndGameCondition` (DG:64261) also ends the game when the AI-controlled Dom dies (any
     difficulty) or, with `bAllowAICOGDeathWhenDBNO`, any AI pawn of the player squad; it arms a 2 s
     `ProcessGameOver` on every GearPC.
   - `CheckSurvivalChance` (+0x009F) and `Reviving.BeginState` (+0x011E) arm a 3 s
     `ProcessGameOver` when a downed player can no longer be saved.
   - During co-op splits a human never counts as a reviver (`TeamHasAnyChanceOfSurvival+0x0258`).

   [V]

   Fix: extra players respawn near a teammate with a timed `RestartPlayer` instead of ending the
   game, while the heroes keep the original rule. During splits, allow a human of the same squad to
   revive.

   Group A: row S2A-2 (and S2A-4 for split revives, off by default), written and checked
   offline; see [COOP_STAGE2_SURVIVAL.md](COOP_STAGE2_SURVIVAL.md). In-game results: Stage 2
   results below.
3. **Leaving.**
   - `GearPC.Destroyed` (DG:31254) hands the pawn of any leaving `COGGear`-derived player to a new
     `GearAI_Dom` (+0x01C7) and fires every `SeqEvent_CoopPlayerLeft` (searched at +0x031A,
     activated at +0x034C).
   - The 14 handlers, in 5 maps, all act on the real Dom: teleport, god mode, map state.
   - A rejoining extra player spawns a fresh pawn, and the old AI-driven one stays in the world.

   [V]

   Fix: keep the AI takeover and the event for Dom only. For an extra player, `UnPossess` first and
   then `Destroy` the pawn. A gib, or a `Destroy` while still possessed, goes through `PawnDied` and
   ends the game.

   Group A: row S2A-3, written and checked offline; see
   [COOP_STAGE2_SURVIVAL.md](COOP_STAGE2_SURVIVAL.md). In-game results: Stage 2 results below.
4. **Teleports.** 371 `SeqAct_Teleport` target links move only Marcus and/or Dom (213 + 157 + 1);
   48 target Player. [V counts]

   Fix: after Marcus's pawn is teleported, place every extra player who is not in a vehicle near the
   destination with `FindAvailableTeleportSpot`. Skip moves shorter than about 1500 uu, so local
   placements do not drag extras along. Answered: `Pawn.OnTeleport` (DE:31182) calls
   `SetLocation` and silently does nothing when it fails.

   Status (2026-10-15): implemented by coop-scenes row SC-4. It passes the offline check
   against the retail packages; in-game results: Stage 2 results below. Details:
   [COOP_STAGE2_SCENES.md](COOP_STAGE2_SCENES.md).
5. **Cinematics.**
   - 151 of 254 `SeqAct_ToggleCinematicMode` targets are hero-only.
   - `GearPC.SetCinematicMode` (DG:34716) hides the pawn, freezes it and sets `bInvisible`, which
     makes AI ignore it. During those scenes the extra players are the only targets AI can see.
   - Joining: a remote join more than 15 s into a level runs `CANCELCINE` for everyone
     (`PostLogin+0x040D`).
   - The joiner's cinematic flags are synced before his pawn exists (+0x0395 runs before
     +0x03CB), so the new pawn is not hidden.

   [V]

   Fix: mirror Marcus's cinematic mode to every extra PC (never mirror Dom's), including right
   after an extra spawns.

   Status (2026-10-15): implemented by coop-scenes rows SC-1, SC-2 and SC-6. They pass the
   offline check against the retail packages; in-game results: Stage 2 results below. Details:
   [COOP_STAGE2_SCENES.md](COOP_STAGE2_SCENES.md).
6. **Co-op splits.** `SeqAct_NotifyCoopSplit`, 19 objects in 12 packages, teleports Marcus and
   Dom onto separate paths and strands the extra players. [V]

   Fix: send the extras alternately with each hero, and copy the hero's `SquadName` when Kismet runs
   `ChangeSquad` on him.

   Status (2026-10-15): implemented by coop-scenes rows SC-4 and SC-5. They pass the
   offline check against the retail packages; the split was not reached in game, see Stage 2
   results below. Details: [COOP_STAGE2_SCENES.md](COOP_STAGE2_SCENES.md).
7. **Checkpoints.**
   - `GearPC.OnCheckpoint` (DG:32633) pulls far players to the host, except at 29 checkpoints whose
     `TeleportClasses` lists only Marcus and Dom (plus `Vehicle_Jack`).
   - `ApplyCheckpointRecord+0x0577` gives any crate carrier who is not Marcus the Dom slot.
   - `CheckpointLoadComplete` (DG:47164) restarts every PC without a pawn.

   [V script] Not traced: how native code matches records to PCs (by `MyGuid` at GearPC+0xC4C, by
   order, or by class) and who sets `bCheckpointDummy`.

   Status (2026-10-15): implemented by coop-scenes rows SC-3 and SC-2. They pass the
   offline check against the retail packages; in-game results: Stage 2 results below. Details:
   [COOP_STAGE2_SCENES.md](COOP_STAGE2_SCENES.md).

   Fix: treat the extra classes as matching `TeleportClasses`.
8. **Vehicles.** [V]
   - `GearEnterVehicle` links target only the heroes, never Player: 6 Marcus, 6 Dom, 1 both.
   - The Centaur has no free seat. Marcus drives and Dom rides in seat 1. Script checks
     `Centaur.Seats.Length == 2` (DG:108043).
   - Each Ride-Reaver has seat 1 free. The Crane has one seat. The Brumak has one `HumanGunner`.
   - `SeqAct_GearExitVehicle` has no script. A hero exit ejects only the main driver.
   - `PreCommitMapChange` makes every PC leave its vehicle.

   [V, `out/c-body-re/vehicles-kismet.md`, `vehicles-script.md` in the worktree, not in the repo]

   Fix: put extras into free seats via `TryToDrive`; when none is free, use a hidden rider mode
   bound to the vehicle.

   Status (2026-10-16, `stage2-c-body`, review fix 09fd5d7): implemented as rows CR-1 to CR-3 of group `coop-ride`;
   run in game on 2026-10-16, see Stage 2 results (CR-1 seen, CR-2 not reached). When a hero
   rides, each extra on foot gets god mode. When the hero gets out, each extra loses it and
   appears next to him.

   - The groups, their logs and the offline check are described under item 9.
     `gow2_coop_vehicle_ride=false` turns `coop-ride` off. Success logs
     `SCRIPTPATCH coop-ride: 3 rows applied for 4 players, set hash ...`; a failed check logs
     `SCRIPTPATCH coop-ride: skipped (<row>: <reason>); vehicles stay retail`. All three patched
     functions decode to their end (`OnGearEnterVehicle` 0x171, `OnGearExitVehicle` 0x1C3,
     `SyncKismetSetProperties` 0x214).
     [V offline]
   - CR-1 grows `GearPawn.OnGearEnterVehicle` (169 -> 369). When Marcus or Dom enters a
     `GearVehicle`, every `GearPC` whose `DefaultPawnClass` is neither hero and whose pawn is a
     `GearPawn` (an extra on foot) gets `bGodMode = true`. `GameInfo.ReduceDamage` zeroes damage
     under god mode (DE:21463). `FellOutOfWorld` and `CurrentLevelUnloaded` pull such pawns back
     without killing them. The host leash (item 11) now keeps this god mode: every second it is
     granted, or CR-1's flag adopted, while a hero drives a `GearVehicle` or sits in a
     `GearWeaponPawn`; revoked only on the PCs it granted or adopted once no hero rides. Kismet
     alone misses cases: `CheckpointLoadComplete+0x0155` resets `bGodMode` on every PC,
     `ApplyCheckpointRecord` puts the hero back with `TryToDrive` (no Kismet, so no CR-1), an extra
     who respawns mid-ride never got it, and `PreCommitMapChange+0x005C` ejects with `DriverLeave`
     (no CR-2). [V]
   - CR-2 grows `GearPawn.OnGearExitVehicle` (216 -> 451). When a hero gets out, every extra in
     god mode loses it. Each is placed next to that hero with
     `GearPC(Controller).FindAvailableTeleportSpot`.
   - CR-3 grows `GearGameSP_Base.SyncKismetSetProperties` (460 -> 532). +0x001D becomes
     `goto 0x1CB`; the tail tests `(OtherPC != PC) && (OtherPC.DefaultPawnClass == COGMarcus || ==
     COGDom)` and jumps back to 0x002C or 0x01C7. A joiner copies `bCinematicMode`, `bInvisible`,
     the `bCinemaDisableInput*` flags and `bGodMode` from a hero, not from the newest extra
     (retail's source with two players). Evidence: `SyncKismetSetProperties` +0x0000/+0x001D/+0x00D1,
     called from `PostLogin+0x0395` and `RestartPlayer+0x0088`. [V]
   - No seating. No driver-seat theft, no `KickOutBot`, no camera risk. All calls are final.
   - Limits: the extras miss the ride (no seat, no Reaver gun) but cannot die. An extra who joins
     mid-ride gets it from the leash within a second. A god mode left on when a map change ejects
     the heroes without a Kismet exit is revoked by the leash within a second (the leash is on by
     default on Windows only). The Brumak (chapter 29) and the Act 1 Derrick
     convoy are not covered. The Derrick has no seats and needs item 4's teleport mirroring.
     Dormant before chapter 16 (Centaur). Ride-Reavers chapter 25. Crane chapters 27-28.
   - Next (P2, design only): seat extras in a free Reaver seat 1 with `TryToDrive` when
     `Driver != None` and `AnySeatAvailable()`. Eject them with `DriverLeave(true)` on the hero
     exit.
9. **Loadout and look.** [V]
   - `SeqEvent_ChapterPoint` gives `MarcusInventory` only to Marcus pawns.
   - 25 `GiveInventory` links target heroes only.
   - Extra players start with a Lancer.

   Chapters without Carmine or RedShirt in the persistent map (1, 3-4 and Acts 3-5) need a class
   that is always loaded and is not Marcus or Dom. A body that exists in every chapter must be
   Marcus's or Dom's. The only skeletal meshes in the always-loaded `GearGame.upk` are
   `COG_MarcusFenix.COG_Marcus_Fenix_CamSkel`, `COG_Dom.Game.COG_Dom_Game_CamSkel` (plus their gore
   meshes) and `Locust_Grunt_CamSkel`. [V]

   Status (2026-10-16, `stage2-c-body`): implemented as rows CB-1 to CB-4 of group `coop-body`;
   run in game on 2026-10-16, see Stage 2 results. Players 3+ are `GearPawn_COGGear` with Dom's
   body in every chapter. They also get the pistol and the Gnasher at spawn.

   - Two follow-up groups, `coop-body` (CB-1..CB-5) and `coop-ride` (CR-1..CR-3), live in
     `src/gow2_script_patch_body_table.cpp`. `tools/script_patch_body.py` prints them (`--cpp`)
     and checks them (`--check <folder>`). The check plans the Stage 1 rows first and checks
     these rows on top of them.
   - The hook applies each group fail-closed on its own, after `coop` and `survival` and before
     `coop-scenes`, and only when `coop` applied (see Stage 2 integration below).
     `gow2_coop_players=2` runs and logs nothing new.
     `gow2_coop_body=false` turns `coop-body` off (players 3+ keep the Stage 1 behaviour).
   - Success logs `SCRIPTPATCH coop-body: 5 rows applied for 4 players, set hash ...`. A failed
     check logs `SCRIPTPATCH coop-body: skipped (<row>: <reason>); players 3+ keep the Stage 1
     behaviour`. For `coop-ride` the tail is `...; vehicles stay retail`.
   - New edit kind: the object field (`ObjectFieldRow`). A 4-byte object pointer in a default
     subobject template goes from None to an object's address. Anchor fields on other templates
     must already hold known objects. That proves the native offset on the running build.
   - `--check` on the retail packages validates every check and operand. Each patched function
     decodes to its end (`PostLogin` 0x69A, `SuggestPawnClassFor` 0x13E, `GearGame.RestartPlayer`
     0x292, `OnGearEnterVehicle` 0x171, `OnGearExitVehicle` 0x1C3, `SyncKismetSetProperties`
     0x214). [V offline]
   - `Default__GearPawn_COGGear.GearPawnMesh` sets neither `SkeletalMesh` nor `PhysicsAsset`. Its
     archetypes (Infantry, GearPawn) set neither either. The Dom and Marcus templates differ
     from it only in those two fields. Their archetype is the COGGear template. [V]
   - CB-1 and CB-2 write Dom's mesh (+480, 0x1E0) and physics asset (+508, 0x1FC) into the COGGear
     mesh template at startup. They anchor on the Dom and Marcus templates (measured in game on
     2026-10-16: `layout.py` says 468 and 496, 12 bytes short). Every peer instances new COGGear
     pawns from that template.
     Content subclasses that set their own mesh are unaffected (Ben, RedShirt, Tai and A. Carmine
     checked). Baird, Gus and Hoffman were not checked. [I]
   - CB-3 makes players 3+ always `GearPawn_COGGear`. It reuses the 318-byte script S1-2 grows
     with same-size edits: `goto 0x0100`, then `return class'GearPawn_COGGear'`. The class is
     always loaded, never garbage-collected and resolvable on every peer. There are no Marcus
     clones in chapter 1, chapters 3-4 or Acts 3-5. `gow2_coop_extra_carmine=true` uses CB-3c
     instead: Ben Carmine where `FindObject` finds him, `GearPawn_COGGear` elsewhere. RedShirt is
     dropped: it lives only in sub-levels. `gow2_coop_marcus_clones` drops CB-3 and CB-3c (D-1
     leaves the script at 145 bytes).
   - section 1.2's crash worry does not apply. `GearPawn_COGGear` inherits `SoundGroup`
     GSG_Empty (not None) and has no gore mesh, the same as Ben in retail. The extras look
     like Dom, have no voice and an empty `CharacterName`. [V data; cosmetic]
   - Every peer must run the same `gow2_coop_players` (for example 4). CB-1 and CB-2 must also run
     on the clients. A client left at 2 sees players 3 and 4 invisible. Gameplay is not affected.
     [I]
   - Loadout: `GearPawn_COGGear.DefaultInventory` is only the Lancer. Marcus and Dom also carry
     `GearWeap_COGPistol` and `GearWeap_Shotgun`. Changing the COGGear default would arm every AI
     COG content class. CB-4 instead grows `GearGame.GearGame.RestartPlayer` (387 -> 658 bytes).
     The tail starts with `GearPC(NewPlayer) != None && DefaultPawnClass != None && != COGMarcus && != COGDom && NewPlayer.Pawn != None`,
     so heroes, AI and versus respawns are untouched: `GearGameAnnex_Base.MatchInProgress.RestartPlayer`
     and `GearGameCTM_Base.MatchInProgress.RestartPlayer` call `Super(GearGame).RestartPlayer`;
     `DefaultPawnClass` stays None in multiplayer. In SP, `GearGameSP_Base.RestartPlayer+0x077D` calls
     it for every non-Dom player and the Dom checkpoint path. After the spawn, `FindInventoryType` and
     `CreateInventory` add the pistol and the Gnasher when missing. Checkpoint loads still restore
     the saved loadout (`ApplyInventoryRecords` discards the inventory first). [V]
   - Open: `SeqEvent_ChapterPoint` still gives `MarcusInventory` (frag grenades, chapter loadouts)
     only to Marcus. A map with `WorldInfo.bNoDefaultInventoryForPlayer` no longer hands heroes
     the pair: the CB-4 tail skips them (no map scanned yet).
10. **Join wait.** A remote joiner whose class already has a pawn in the world (here the AI Ben)
    waits in `PlayerWaiting` until he presses Right Trigger (`PostLogin+0x03A4`,
    `GearPlayerInput.HandleButtonInput_RightTrigger+0x0060`). Optional fix: spawn non-Dom joiners
    immediately. `PostLogin+0x01D2` also sets `bFoundPawn` when a vehicle's driver has the game's
    `DefaultPawnClass` (Marcus), because the script compares the game's class, not the joiner's.
    The Right Trigger wait also happens while Marcus drives. With `GearPawn_COGGear` as the class,
    the first extra spawns at once and the second waits, because he finds the first extra's pawn.
    [V]

    Status (2026-10-16, `stage2-c-body`): implemented as row CB-5 of group `coop-body` (see item
    9 for the group, its logs and the offline check); run in game on 2026-10-16, see Stage 2
    results. Every joiner who is not Dom spawns at once. A remote Dom keeps the retail wait.

    - CB-5 is same size: 2 byte and 2 operand edits at +0x03B3..+0x03C7. The condition
      `LocalPlayer(PC.Player) != None` becomes
      `class<GearPawn_COGDom>(PC.DefaultPawnClass) == None`. Every joiner who is not Dom is
      restarted at once. A remote Dom keeps retail's Right Trigger takeover. `PostLogin` still
      decodes to its end (0x69A). [V offline]
    - With 3+ players a local split-screen player 2 (Dom) now also waits for Right Trigger
      (network co-op has no split-screen). `CANCELCINE` on remote joins after 15 s is unchanged
      (item 5).
11. **Left-behind extras (leash).**
    - At the start of the Derrick convoy (`sp_assault_p`) Kismet teleports every player to
      `PlayerStart_1` on the road, then teleports and attaches only Marcus and Dom onto the
      trucks. 60 s later the extras were 37000 units behind. [V]
    - After chapter travel to 16 one extra stood on a roof 17517 units from the Centaur. [V]
    - A Kismet teleport of Marcus into a level that is not loaded yet also leaves extras
      behind: SC-4's `FindAvailableTeleportSpot` finds no navigation point there. The same
      teleport a second time, with the level loaded, brought both extras to 213-220 units
      from Marcus. [V]

    Fix: `gow2_coop_leash` (64b1c49, `src/gow2_coop_leash.cpp`), host only, only with
    `gow2_coop_players` 3+. Settings: `gow2_coop_leash` (true on Windows, false elsewhere),
    `gow2_coop_leash_distance` (4000), `gow2_coop_leash_seconds` (8). An extra who stays more
    than the distance from the nearest human Marcus or Dom for the seconds is moved next to him.
    Log lines: `COOPLEASH on: ...`, `COOPLEASH GearPC_SP_3 moved next to GearPC_SP_1 (was
    5731 units away)`, `COOPLEASH GearPC_SP_6 moved next to GearPC_SP_7 near GearPC_SP_5 (was
    17517 units away)`. [V] In s3 both extras moved onto Dom's Derrick 8 s after arriving and
    rode the convoy. In s4 the stranded extra moved next to the extra standing beside the tank. [V]
    v1 failed in chapter 16 (`no free spot`: the tank's collision covered all 8 spots 120
    units around it); v2 anchors on a player on foot next to the hero and tries 24 spots at
    120/360/600 units. [V] Limits: it never moves a player in cinematic mode, dead, or in a
    vehicle or turret; a downed extra is revived by `FindAvailableTeleportSpot`; extras
    inside a vehicle section only keep up in hops every 8 s.

    Review fixes (6ad7234): the leash runs only when the coop group applied (else it logs
    `COOPLEASH off (the coop group was not applied)`) and only for `GearPC_SP` controllers, so a
    failed check or a versus/Horde game never sees it. It pulls at once, with no 8 s wait (cooldown
    `gow2_coop_leash_seconds`), an extra within 60 units of another player's pawn or more than 2000
    units below every hero: after chapter travel an extra spawned inside Marcus (dMarcus=0 for 26 s),
    because `SpawnDefaultPawnFor+0x0061` spawns with bNoCollisionFail at one PlayerStart, and an
    extra fell from (0,0,-1). Log lines: `... (was N units away; overlapped GearPC_SP_5)` and
    `... fell N units below every hero)`. [V]
    The leash also keeps CR-1's god mode (item 8): every second it grants it to, or adopts it on,
    extras on foot while a hero drives a `GearVehicle` or sits in a `GearWeaponPawn`, and revokes it
    on those PCs once no hero rides. Log lines: `COOPLEASH GearPC_SP_6: god mode on (GearPC_SP_5 rides
    Vehicle_Centaur)` / `... god mode off (no hero rides)`. [V]
    `gow2_coop_leash` defaults to true only on Windows; it is off by default on Android and Linux,
    because the fail-closed fault handling (`__try`/`__except` in `GuardedRun`, `GuardedApply` and
    `GuardedProbeSweep`) exists only on Windows: elsewhere a stale guest pointer is a SIGSEGV. A real
    POSIX guard is still open: the SDK's own SIGSEGV/SIGBUS handler from patch 0007 chains unclaimed
    faults, so a scoped sigsetjmp guard has to cooperate with it. [I]

    0.3.16 (26e0b4e), from the logs of a three-player online session on 0.3.14 (2026-11-08):
    - **Overlap during a cinematic.** At two chapter starts an extra spawned exactly inside the
      human Dom (0 units), about 1.5 minutes after each act achievement. The leash never moved it,
      because players in cinematic mode or hidden were excluded, and when the cinematic ended Dom
      fell out of the level and kept falling. [V] An extra in cinematic mode or hidden that is
      within 60 units of another pawn is now pulled next to that pawn at once (`PullOrder.cinematic`,
      log `...; during a cinematic)`). [I]
    - **Fallen heroes.** The leash only moved extras, so the falling Dom was never rescued, and it
      even moved the extra next to him. [V] Now a Marcus or Dom more than `kFellBelowHeroes` (2000)
      below every other standing player who also drops more than 300 units a second for 3 readings
      is moved next to the nearest standing player (`PullReason::kHeroFell`, log `COOPLEASH ...
      rescued next to ... (fell N units below every other player)`). A hero on a lower floor does
      not drop, so he stays. A fallen player is never a target or an anchor. [I]
    - **Left behind fast.** A third player started the convoy outside the trucks. [V] When an
      extra's distance to the nearest hero grows more than 300 units in a second, each far second
      counts 4, so the pull comes after about 2 s instead of 8. [I]
    Host tests cover each rule (`tools/tests/coop_leash_test.cpp`); none of it has been played
    online yet.

#### Stage 2 integration (branch `stage2`, 2026-10-16)

The four group branches (a-survival, b-scenes, c-body, d-tools) are merged into `stage2`.
The hook (`src/gow2_script_patch_hook.cpp`) applies, in this order: `coop` (Stage 1),
`survival`, `split-revive` (only with `gow2_coop_split_revive=true` and when `survival`
applied), `coop-body`, `coop-ride`, then the self-registered groups by name (today only
`coop-scenes`). One function, `ApplyAndLogGroup`, applies and logs every group. [V]

- Every group is fail-closed on its own: a failed check logs
  `SCRIPTPATCH <group>: skipped (<row>: <reason>); <what stays>` and the others still apply.
- Every Stage 2 group runs only when `coop` applied; otherwise each logs
  `SCRIPTPATCH <group>: off (the coop group was not applied)`.
- `gow2_coop_leash` (`src/gow2_coop_leash.cpp`) is not a patch group: an engine tick listener
  on the host, see the settings table.

With `gow2_coop_players=2` (the default) the hook still returns before Stage 1, so the game stays
retail. But every player must run the same build: a v0.1.1 peer lacks d074c0d and is expected to
crash about 0.5 s after the chapter 2 -> 3 map commit even in two-player co-op. [I] The devcheats
group (dev only) is
separate: it applies once on the engine tick when `gow2_dev_cheats=true`.

| Setting | Default | Effect |
|---|---|---|
| gow2_coop_survival | true | false: Stage 1 only (no survival rows) |
| gow2_coop_split_revive | false | true: row S2A-4, a standing player 3+ of the downed player's squad counts as a reviver in co-op splits |
| gow2_coop_body | true | false: players 3+ keep the Stage 1 class and loadout (no CB rows) |
| gow2_coop_extra_carmine | false | true: players 3+ are Ben Carmine where his class is loaded (CB-3c instead of CB-3) |
| gow2_coop_vehicle_ride | true | false: no CR rows; vehicles stay retail |
| gow2_coop_scenes | true | false: no SC rows (cinematics, teleports, checkpoints, splits stay as in Stage 1) |
| gow2_coop_scene_probe | false | debug: logs each player controller's cinematic state every 2 s |
| gow2_coop_leash | true on Windows, false elsewhere | false: no leash. true: on the host, a player 3+ who stays more than gow2_coop_leash_distance from the nearest human Marcus or Dom for gow2_coop_leash_seconds is moved next to him (FindAvailableTeleportSpot near him, or near a player on foot next to him when he is in a vehicle; else one of 24 spots 120, 360 or 600 units around that player); an extra within 60 units of another pawn is pulled at once, also during a cinematic; a Marcus or Dom 2000 units below every other standing player and still dropping for 3 s is moved next to the nearest one; the leash also keeps god mode for extras on foot while a hero rides; log lines COOPLEASH |
| gow2_coop_leash_distance | 4000 | game units |
| gow2_coop_leash_seconds | 8 | seconds |
| net_beacon_gate | true | with gow2_coop_players 3+, System Link lists and answers only peers whose co-op hash matches (SDK patch 0012); false turns the check off |
| gow2_dev_command_file / gow2_dev_cheats | empty / false | dev only, see TEST_TOOLING.md |

Every peer must use the same gow2_coop_players and the same group settings (coop-body's
CB-1/CB-2 must also run on the clients).

Expected log at 4 players with the default settings:

```
SCRIPTPATCH coop S1-1 GearGame.GearGameSP_Base.AtCapacity: 2 write(s)
SCRIPTPATCH coop S1-2 GearGame.GearGameSP_Base.SuggestPawnClassFor: script grown to 318 bytes
SCRIPTPATCH coop S1-3 GearGame.GearMenuGame.ChangeCoopInviteSetting: 1 write(s)
SCRIPTPATCH coop S1-5 GearGame.GearCampaignLobbyGame_Base.InitGame: 1 write(s)
SCRIPTPATCH coop S1-6 GearGame.GearGameSP_Base.InitGame: 1 write(s)
SCRIPTPATCH coop S1-4 GearGame.Default__GearCoopGameSettings: 1 write(s)
SCRIPTPATCH coop: 6 rows applied for 4 players
SCRIPTPATCH survival S2A-1 GearGame.GearGameSP_Base.SpawnDefaultPawnFor: script grown to 183 bytes, with 1 write(s)
SCRIPTPATCH survival S2A-2 GearGame.GearPC.ProcessGameOver: script grown to 159 bytes
SCRIPTPATCH survival S2A-3 GearGame.GearPC.Destroyed: script grown to 1275 bytes, with 2 write(s)
SCRIPTPATCH survival S2A-5 GearGame.GearPC.SaveCheckpoint: script grown to 496 bytes, with 3 write(s)
SCRIPTPATCH survival: 4 rows applied for 4 players
SCRIPTPATCH coop-body CB-3 GearGame.GearGameSP_Base.SuggestPawnClassFor: 3 write(s)
SCRIPTPATCH coop-body CB-4 GearGame.GearGame.RestartPlayer: script grown to 658 bytes
SCRIPTPATCH coop-body CB-5 GearGame.GearGameSP_Base.PostLogin: 4 write(s)
SCRIPTPATCH coop-body CB-1 GearGame.Default__GearPawn_COGGear.GearPawnMesh: 1 write(s)
SCRIPTPATCH coop-body CB-2 GearGame.Default__GearPawn_COGGear.GearPawnMesh: 1 write(s)
SCRIPTPATCH coop-body: 5 rows applied for 4 players
SCRIPTPATCH coop-ride CR-1 GearGame.GearPawn.OnGearEnterVehicle: script grown to 369 bytes
SCRIPTPATCH coop-ride CR-2 GearGame.GearPawn.OnGearExitVehicle: script grown to 451 bytes
SCRIPTPATCH coop-ride CR-3 GearGame.GearGameSP_Base.SyncKismetSetProperties: script grown to 532 bytes, with 2 write(s)
SCRIPTPATCH coop-ride: 3 rows applied for 4 players
SCRIPTPATCH coop-scenes SC-1 GearGame.GearPC.SetCinematicMode: script grown to 679 bytes
SCRIPTPATCH coop-scenes SC-2 GearGame.GearGameSP_Base.RestartPlayer: script grown to 2403 bytes
SCRIPTPATCH coop-scenes SC-3 GearGame.GearPC.OnCheckpoint: script grown to 1029 bytes, with 1 write(s)
SCRIPTPATCH coop-scenes SC-4 GearGame.GearPawn.OnTeleport: script grown to 607 bytes
SCRIPTPATCH coop-scenes SC-5 GearGame.GearPC.SetSquadName: script grown to 371 bytes
SCRIPTPATCH coop-scenes SC-6 GearGame.GearGameSP_Base.PostLogin: 1 write(s)
SCRIPTPATCH coop-scenes: 6 rows applied for 4 players
SCRIPTPATCH stack: players=4 coop=937c35f80cb274b7 survival=321b36117af31c35 split-revive=off coop-body=b62ed993f8c3fe5e coop-ride=9381496b16a09620 coop-scenes=cddd0eadc988a154 combined=e52feaafc39b3f26
[netlink] beacon gate on: co-op hash e52feaafc39b3f26
```

The game appends ", set hash <16 hex>" to each "rows applied" line. For 4 players with the
default settings the five groups' set hashes are: coop 937c35f80cb274b7 (Stage 1 results),
survival 321b36117af31c35, coop-body b62ed993f8c3fe5e, coop-ride 9381496b16a09620 and coop-scenes
cddd0eadc988a154 (COOP_STAGE2_SCENES.md), with combined=e52feaafc39b3f26. The stack line's
`combined=` is FNV-1a 64 over the text before it. Hashes depend on the player count and the
group settings. The game gives this value to System Link (`net_coop_hash`) for the beacon gate
(SDK patch 0012). The `[netlink] beacon gate on` line is logged at the first beacon datagram.

Functions two groups change:

- `GearGameSP_Base.SuggestPawnClassFor`: S1-2 grows it to 318 bytes and CB-3 makes same-size
  edits inside that grown tail, so coop-body must apply after coop.
- `GearGameSP_Base.PostLogin`: CB-5 edits +0x03B3..+0x03C7 and SC-6 swaps one operand at
  +0x0413; no group grows it.
- Function ownership for later rows (no other group may grow these): group A (survival and
  split-revive) owns SpawnDefaultPawnFor, ProcessGameOver, GearPC.Destroyed, SaveCheckpoint,
  TeamHasAnyChanceOfSurvival; coop-body owns GearGame.RestartPlayer, OnGearEnterVehicle,
  OnGearExitVehicle, SyncKismetSetProperties (coop-ride rows) and PostLogin; coop-scenes owns
  GearPC.SetCinematicMode, GearGameSP_Base.RestartPlayer, GearPC.OnCheckpoint, GearPawn.OnTeleport,
  GearPC.SetSquadName.

Offline check: `python -I tools/script_patch.py --check <folder with the decompressed
packages>` checks the Stage 1 rows and then `tools/script_patch_stage2.py`'s stack: every group
is planned on top of the bytes the earlier groups produce, in the hook's order, for the default
settings and the split-revive, extra-carmine and marcus-clones variants; every function two
groups change is decoded to its end with every jump on a statement start; and the devcheats row.
`--stage1-only` checks only Stage 1. On the player's retail packages it exits 0 for 4 players.
[V offline] Each group's own tool (`script_patch_survival.py`, `script_patch_scenes.py`,
`script_patch_body.py`, `dev_cheats_patch.py`) still prints its C++ table with `--cpp`.

Run in game on 2026-10-16, see Stage 2 results below.

Test plans are in [COOP_STAGE2_SURVIVAL.md](COOP_STAGE2_SURVIVAL.md),
[COOP_STAGE2_SCENES.md](COOP_STAGE2_SCENES.md), items 8-10 above and
[TEST_TOOLING.md](TEST_TOOLING.md). TEST_TOOLING.md "Evidence" lists the engine addresses the dev tooling relies
on and what the first `!selftest` must confirm.

Builds (2026-10-16, this branch): `scripts/build.ps1` built `gow2.exe`, `scripts/build-android.ps1`
built the APK (every new source compiles for arm64; not run on a phone) and
`scripts/test-host.ps1` passed all five steps. [V build]

#### Stage 2 test

Four players, one scenario per item:

1. Cross the chapter 2 -> 3 transition: nobody dies, no game over, all four keep their pawns.
2. Kill an extra player: he respawns and the game continues. Kill the host: game over, as in
   retail.
3. Have an extra player leave and rejoin: Dom is untouched, no `CoopPlayerLeft` behaviour fires, and
   no orphan pawn remains.
4. Load a checkpoint: all four are restored.
5. Play through a cinematic: the extras are hidden and reappear next to Marcus.
6. Play one co-op split from the list in `campaign-systems.md` section 9 (for example
   `SP_Outpost_07a_S`).
7. Play the Ride-Reaver section (`SP_Forest_S_01`) and the Centaur section
   (`SP_MountKadar_Vehicle`).

Each scenario also runs with `gow2_coop_players=2` to confirm retail behaviour is unchanged.

Tooling (group d, 2026-10-16): the scenarios can be driven with `scripts/coop-stage2.ps1`
(setup, transition, kill-extra, kill-host, checkpoint, leave-rejoin, travel) and the command file.
`!mapchange`/`!mapcommit` run the retail map change natives to reproduce item 1 without playing
to the chapter end. `!kill`, `suicide`, `LoadCheckpoint` and `chapter -Chapter <n>` cover items 2,
4 and chapter travel. Status: used for the Stage 2 results below. See
[TEST_TOOLING.md](TEST_TOOLING.md).

#### Stage 2 results (2026-10-16)

Four `gow2.exe` instances on 127.0.0.1-.4 with `gow2_coop_players=4`, windowed 800x450,
`resolution_scale=1`, each with its own `--user_data_root` seeded from a save at Act 1 chapter
2. [V] `scripts/coop-stage2.ps1` and `scripts/coop-instances.ps1` drove them through the dev
command file and pad files. [V] Checks read `!list`, the SCENEPROBE and COOPCENSUS log lines
and a read-only reader of the game's memory (pawn class, mesh, weapons, position). [V] Five
sessions s1..s5 plus a two-player control, all on 2026-10-16, Windows, RX 7800 XT. [V] Builds:
s1 is integration build 27b9560; s2 adds the coop-body offset fix and the sub_82ADBEF0 hint;
s3 adds leash v1; s4 and s5 add leash v2 (commits ab364f6, d074c0d, 64b1c49 on branch
`stage2`). [V]
The evidence folders below sit under `out/stage2-test/` (one per item, with the logs and
screenshots of all four instances; `README.txt` there lists the sessions). That folder is
gitignored: its files come from the player's own game and are not committed.

Problems found and fixed in this run (each now committed):
- F1. coop-body did not apply on any of the four: `SCRIPTPATCH coop-body: skipped (CB-1:
  anchor GearGame.Default__GearPawn_COGDom.GearPawnMesh (SkeletalMeshComponent) +0x01D4 points
  at 0x00000000, expected COG_Dom.Game.COG_Dom_Game_CamSkel (SkeletalMesh)); players 3+ keep
  the Stage 1 behaviour`. [V] The memory read puts SkeletalMesh at +0x1E0 and PhysicsAsset at
  +0x1FC: `tools/ue3/layout.py` is 12 bytes short. [V] Fixed in ab364f6 (480/508). After the
  fix every instance logs `SCRIPTPATCH coop-body: 5 rows applied for 4 players, set hash
  dadb8145442fe83f`. [V]
- F2. All four instances died about 0.5 s after the chapter 2 -> 3 map commit: `[FATAL] Call
  to invalid or unregistered function at guest address 0x82ADBEF0`. [V] That is a leaf right
  after the noreturn rethrow that ends sub_82ADBED8, reached by an indirect call during the
  commit. [I] It is not specific to co-op patches: it is a codegen gap. [I] Fixed in d074c0d
  (`hints/runtime.toml`). After the fix the same commit plays the Act 2 intro movie (about 4
  min) and the AssaultIntro Kismet commits on to `sp_assault_p`, with all four keeping their
  pawns; also with 2 players. [V]
- F3. Players 3-4 left behind at the start of the Derrick convoy, after chapter travel to
  16 and after a Kismet teleport into a level that is not loaded yet: new host leash
  `gow2_coop_leash` (64b1c49), item 11. With it both extras rode the convoy on Dom's Derrick
  (s3) and the extra stranded in chapter 16 joined the tank (s4). [V]
- F4. Runner fixes (6b3afbd). Session waits missed lines written during the menu macro (false
  FAILs `host session created`, `client 2 joined`). [V] The host notices a killed client after
  its connection timeout, 45 s, while leave-rejoin checked after 10 s. [V] Screenshots go into
  the scenario folder.

| Item | Result | Evidence |
|---|---|---|
| 1. Chapter 2 -> 3 transition | s1 crashed (F2). s2/s3 pass: nobody Dead, all four keep pawns, SCENEPROBE shows all four in cinematic mode during the movie (cine=1 hidden=1), then in sp_assault_p; no game over. [V] Chapter travel by servertravel (chapters 13, 16, 27) passed too: every player got a pawn with body and loadout; extras sometimes spawn at (0,0,0) and fall until the chapter-start Kismet teleport or the leash moves them. [V] | `1-transition` |
| 2. Deaths | Suicide on P3 (s1 with Ben, s2 with GearPawn_COGGear) and !kill P4 (s2): each extra respawned within 15 s next to a teammate (237-463 units away), with the Lancer, pistol and Gnasher, no game over; P1 not Dead. [V] !kill P1 on foot (s3): OBJECTIVE FAILED on all four, as in retail; LOAD CHECKPOINT restored all four. [V] Dom's death with four players (s5, suicide on instance 2): OBJECTIVE FAILED on all four, as in retail and in the two-player control. [V] !kill P1 while Marcus drives the Centaur does nothing (Suicide on a vehicle). [V] S2A-5: with P4 Dead (state Dead, pawn None) SaveCheckpoint wrote the save 0.1 s later (file time 02:09:49.97, size 97264 -> 117551). [V] | `2-deaths` |
| 3. Leave and rejoin | stopone of instance 4: the host logged XGISessionLeaveRemote 45 s after the kill (connection timeout); no GearAI_Dom drove an extra's body; the leaver's GearPawn_COGGear left the census 10 s after the leave (s2); Dom untouched (same GearPC_SP_1 and pawn). [V] Rejoin: P4 got a GearPawn_COGGear with Dom's mesh and all three weapons without pressing Right Trigger (CB-5). [V] With Ben (s1, coop-body broken) the rejoin needed RT. [V] CoopPlayerLeft handlers do not exist in chapter 2 maps, so that part was not observable. [V] | `3-leave-rejoin` |
| 4. Checkpoint | SaveCheckpoint, walk P3 away, LoadCheckpoint: all four restored with new pawns at the saved places, extras keep Dom's mesh and the three weapons, no orphan COGGear bodies. [V] Also after a game over (OBJECTIVE FAILED -> LOAD CHECKPOINT) in chapter 2 and in the convoy: all four restored; in the convoy the extras were restored on the truck and rode it. [V] | `4-checkpoint` |
| 5. Cinematics | Hero-only toggle (CE gogo in sp_outpost_p toggles SeqVar_Marcus+SeqVar_Dom only): both extras went cine=1 invis=1 hidden=1 within 1 s (SC-1 mirror). [V] Chapter 13 start (ChapterPoint toggles Marcus+Dom): extras hidden with the heroes and visible again after, 0 and 115 units from Marcus. [V] Act 2 intro movie and convoy in-engine cutscenes: all four hidden, back after. [V] Join during a cinematic (SC-2/SC-6) not checked. The gogo debug event left the screen black (its Kismet waits for a chapter load); not a co-op issue. [V] | `5-cinematic` |
| 6. Co-op split | Not reached. Splits start from touch volumes and a host path choice (SP_Sinkhole_02_S, SP_Hospital_02_S, SP_Outpost_07a_S). The Sinkhole dev teleport (CE Split) put Marcus in a combat area where he died even with !god. [V] SC-4 pairing and SC-5 not exercised. The leash still applies during a split (nearest human hero). | `6-split` |
| 7. Body and weapons | With coop-body fixed, players 3-4 are GearPawn_COGGear with mesh COG_Dom.Game.COG_Dom_Game_CamSkel and weapons GearWeap_AssaultRifle, GearWeap_COGPistol, GearWeap_Shotgun in chapter 2 (on the host and on the client's own copy) and in chapter 4 (sp_assault_p, after the transition), and the same plus GearWeap_FragGrenade after chapter travel to chapters 13 and 27 (host reads). [V] Baird and Gus keep their own meshes (COG_Baird.COG_Baird, COG_Gus.COG_Gus). [V] After travel to 27 the human Dom waited in PlayerWaiting with PRESS RT TO BEGIN until Right Trigger (retail rule for a remote Dom, kept by CB-5). [V] | `7-body-weapons` (`memory-reads.txt`) |
| 8. Vehicles | Chapter 16 (Centaur): Marcus drives (Vehicle_Centaur), Dom in the gun (GearWeaponPawn), both extras on foot with god=1 (CR-1). [V] Hero exit (CR-2) not reached. Ride-Reaver not reached. | `8-vehicle` |
| Soak | s5: the four players moved, turned and fired in turn for 5 minutes in chapter 2; all four processes alive at every minute, no death, no leash move, about 1.6 GB RAM and 0.8 GB VRAM per instance. Real combat was not played. [V] | `soak-5min` |
| Control (2 players) | gow2_coop_players=2, two instances: logs show only SCRIPTPATCH startup script packages loaded and SCRIPTPATCH coop: off (gow2_coop_players=2) (no survival, coop-body, coop-ride, coop-scenes or COOPLEASH line); slots_pub=2; Marcus and Dom spawn; the chapter 2 -> 3 commit passes with both pawns kept and no FATAL. [V] Two-player co-op against a v0.1.1 peer was not run (by construction nothing new runs at 2). | `control-coop2` |
| Bandwidth | Host `[nettraffic]` lines (`net_traffic_log_sec=10`, 1709 host-to-client 10 s windows on the game port): per-client peak 5327 B/s (76 % of the 7000 B/s cap, `MaxDynamicBandwidth` unchanged) at about 30 packets/s in the s1 transition; 15 windows above 5000 and none above 6500. Host upload peak 15.8 KB/s with 3 clients (s3 transition). Two-player control peak 5235 B/s. Scripted play only, on loopback. [V] | `1-transition`, `control-coop2` |

Not covered:
- Co-op split (SC-4 pairing, SC-5). Not reached.
- CR-2 hero exit from a vehicle. Not reached.
- Ride-Reaver section. Not reached.
- Join during a cinematic (SC-2/SC-6).
- CoopPlayerLeft maps (no handler in chapter 2 maps).
- A 10-minute soak with real combat (only the 5-minute scripted soak above ran).
- Android: the APK builds with every Stage 2 source, the leash included
  (`scripts/build-android.ps1`, 2026-10-16), but was not run on a phone.
- Two-player play against a v0.1.1 peer (expected to crash at the chapter 2 -> 3 commit, F2) [I].
- Internet play with 4 PCs.

#### Review fixes (2026-10-16)

Run on 2026-10-16 with the review fixes (commits 09fd5d7..9b8507e): `scripts/build-sdk.ps1`
(series 0002-0012), `scripts/build.ps1 -SkipCodegen`, `scripts/test-host.ps1 -LowPriority` (all
5 steps passed: leash 109 checks, SDK System Link and session tests "All tests passed (2177
assertions in 86 test cases)", 247 Python tests), and `python -I tools/script_patch.py --check`
on the player's packages (exit 0; CB-4 decodes to its end at 0x292, CR-3 at 0x214, S2A-3 at
0x4FB). `scripts/build-android.ps1` built the APK with the series 0002-0012 and every
Stage 2 source (library and APK checks passed; not run on a phone). [V] Three game sessions on one PC, four instances on 127.0.0.1-.4 unless noted, seeded
from a copy of an Act 1 chapter 2 save; the folders are `out/review-test/r1`, `r1c`, `r3-*`
(gitignored: logs and screenshots from the player's own game). [V]

| Check | Result |
|---|---|
| Stack line and gate | All four logs: the same `SCRIPTPATCH stack: players=4 coop=937c35f80cb274b7 survival=321b36117af31c35 split-revive=off coop-body=b62ed993f8c3fe5e coop-ride=9381496b16a09620 coop-scenes=cddd0eadc988a154 combined=e52feaafc39b3f26`, `[netlink] beacon gate on: co-op hash e52feaafc39b3f26`, no `skipped`. The three clients found the host through the gate and joined. [V] |
| CB-4 | P3 `suicide`: respawned with GearWeap_AssaultRifle, GearWeap_COGPistol, GearWeap_Shotgun (both extras). Marcus and Dom kept AssaultRifle, COGPistol, FragGrenade: no Gnasher added. [V] |
| S2A-3 dummy | SaveCheckpoint with four players (the save holds two GearPawn_COGGear records), P4 left (`XGISessionLeaveRemote` on the host), LoadCheckpoint: no GearAI on a GearPawn_COGGear, exactly one extra body (P3's), the four GearAI_Dom drive the RedShirts as before, no CoopPlayerLeft line. [V] P4's record had no player, so the loader made it a dummy [I]. |
| Beacon gate (Stage 4 test 3, loopback) | P4 restarted with `--gow2_coop_body=false` (combined 6751fc7ecf3d6eec): NO CAMPAIGNS FOUND; host `[netlink] beacon from 127.0.0.4:14001 dropped: co-op hash 6751fc7ecf3d6eec differs from ours e52feaafc39b3f26`; P4 `[netlink] co-op hash from 127.0.0.1:14001 is e52feaafc39b3f26, ours is 6751fc7ecf3d6eec: different build or co-op settings; its campaigns stay hidden`. P4 at `--gow2_coop_players=2` (no hash, as a v0.1.1 copy): NO CAMPAIGNS FOUND, host `... dropped: no co-op hello (ours e52feaafc39b3f26)`. P4 with the same settings again: listed, joined, spawned without Right Trigger, no AI-driven extra. [V] |
| God mode while riding | Chapter 16: Marcus in Vehicle_Centaur, the human Dom in GearWeaponPawn, both extras on foot god=1. SaveCheckpoint inside the ride, LoadCheckpoint: `COOPLEASH GearPC_SP_7: god mode on (GearPC_SP_5 rides GearWeaponPawn)` and the same for GearPC_SP_6; both extras god=1 again with the heroes back in their seats. Travel to chapter 13: both extras god=0. [V] |
| CR-3 | Chapter 16 with both extras at god=1: the human Dom (P4) left and rejoined: god=0 in PlayerWaiting and god=0 after Right Trigger put him back in the gun. A rejoining extra got god mode from the leash in the second it joined. [V] |
| Overlap | Travel to chapter 13: `COOPLEASH GearPC_SP_14 moved next to GearPC_SP_11 (was 0 units away; overlapped GearPC_SP_11)`; after the chapter cinematic the extras stood 240 and 115 units from Marcus. [V] |
| Coop not applied | One instance with `gow2_coop_players=4 --gow2_coop_patch_fail_test=true`: `SCRIPTPATCH coop: skipped (S1-1: +0x0000 expected FB, found 04); the game stays retail`, `COOPLEASH off (the coop group was not applied)`, no `COOPLEASH on`, `SCRIPTPATCH stack: players=4 coop=skipped survival=off split-revive=off coop-body=off coop-ride=off coop-scenes=off combined=28d1693acdf0ba83`. [V] |
| Control (2 players) | Two instances at the default `gow2_coop_players=2`: `SCRIPTPATCH coop: off (gow2_coop_players=2)`, no stack, gate or COOPLEASH line, `slots_pub=2`, the client joined with no gate, Marcus and Dom spawned. [V] |

New observations, not fixed:
- Roles move after servertravel. In both sessions P2 was Dom at launch, and after chapter travel
  by servertravel P4 was Dom and P2 an extra. [V] SuggestPawnClassFor gives Dom to the first
  non-Marcus player restarted after the travel. [I] Chapter changes by map commit were not
  checked for this.
- LoadCheckpoint of the chapter 16 start checkpoint (saved by the travel, before the ride) put
  Marcus at (0,0,0), falling, for 3 min 15 s while the remote Dom sat in PlayerWaiting; it ended
  when the chapter-start cinematic ran. Meanwhile the leash logged `no free spot next to
  GearPC_SP_6` for both extras every 8 s and did not move them. [V] Cause not traced.
- `COOPLEASH on` is logged on clients too; they never act (TakeSnapshot returns nothing on a
  client). [V code]

### Stage 3: 8-10 players and bandwidth

- **Caps.** The same Stage 1 rows with N = 8 or 10.
- **Bandwidth.** [V unless marked]
  - A LAN match skips `UpdateNetSpeeds` (DE:21289, +0x003E), so nothing is divided by the player
    count.
  - The host's `AdjustedNetSpeed` stays at `MaxDynamicBandwidth = 7000`
    (`GearGameSP_Base.InitGame+0x00BC`: `0F 01 [GameInfo.MaxDynamicBandwidth] 1D 00 00 1B 58`), and
    `ServerSetNetSpeed` clamps to it.
  - The client connects with `open a.b.c.d`, without `?LAN`, so it uses `ConfiguredInternetSpeed`
    (7000), not `ConfiguredLanSpeed` (`sub_827D4108`).
  - Raise `MaxDynamicBandwidth` (a same-size IntConst edit), plus `ConfiguredInternetSpeed` and the
    server rate caps `MaxClientRate` / `MaxInternetClientRate` in Coalesced. Nobody traced which of
    the two server caps clamps.
  - Use the Stage 1 measurements as the baseline before choosing values.
- **Tick rate.** Coalesced sets `LanServerMaxTickRate = 35`, `NetServerMaxTickRate = 45` and
  `bClampListenServerTickRate = TRUE`. [V values; semantics I]
- **Packet size** is no issue: at most 512 bytes per packet.
- **UI limits.** [V]
  - Downed-teammate icons have 4 coordinates per split (`AssessSquadCoords`).
  - The spectator battle cam has 2 paths, gated to ControllerId 0 and 1.
  - `VoiceTaccomIconPos` has 2 slots.
  - The campaign lobby shows 2 rows.
  - Assess mode itself iterates over all pawns.
- **Spawn spots.** The search is a BFS within 1024 uu of a teammate's anchor that skips cover
  markers and occupied spots, falls back to a PlayerStart, and spawns with `bNoCollisionFail`. [V
  code] With more players, nearby spots may run out. [I]
- **Vehicles and game-over rules** scale worse as N grows and need the Stage 2 fixes.

#### Stage 3 test

Use 2-4 PCs with at most two instances each, on M2b settings: real LAN addresses, subnet-directed
broadcast and `net_bind_ip` off.

1. Join 8, then 10 players.
2. A chapter with combat and one checkpoint reload.
3. Measure per-client bytes/s and the server tick before and after the bandwidth changes.

Pass when there is no rubber-banding at 8 players and nobody else is refused below N.

### Stage 4: PC-Android cross-play over System Link

Status (2026-10-15): SDK patch 0011-network-crossplay implements the Stage 4 networking.
It now sits on the opt-in 0010 (6a8e519): System Link needs `net_link_active=true` on every
copy, PC and phone (docs/NETWORK_PLAY.md). It builds for Windows and passes its unit tests
(70 System Link cases: "All tests passed (1439 assertions in 70 test cases)",
out/logs/unit-tests-4.log). The loopback run below used the 0011 before this rebase. The
Android app adds the INTERNET, ACCESS_NETWORK_STATE,
ACCESS_WIFI_STATE, CHANGE_WIFI_MULTICAST_STATE and WAKE_LOCK permissions, holds a Wi-Fi
MulticastLock and a low-latency WifiLock (WIFI_MODE_FULL_LOW_LATENCY) from onCreate to
onDestroy, and sets REX_DETECTED_LOCAL_IP to the Wi-Fi or hotspot address. For setup,
see docs/NETWORK_PLAY.md. On 2026-10-14 two PC copies on loopback played chapter 2
together with 0011 (docs/NETWORK_PLAY.md, "Testing on one PC"). On 2026-10-15 the PC
(192.168.10.10, Ethernet) and the phone (192.168.10.114, Wi-Fi) joined in both directions:
phone host with PC client in Act 1 chapter 1, PC host with phone client in chapter 2, each
player seeing the other move (docs/NETWORK_PLAY.md, "Tested on 2026-10-15"). The text below
the table is the original design; it is unchanged.

| Setting | Default | Effect |
|---|---|---|
| `net_port_offset` | 40000 | Guest ports below 1024 use port + offset on the host (the game's UDP 1000 becomes 41000; beacon port 14001 is unchanged). Every peer uses the same value. |
| `net_local_ip` | empty (new meaning) | a.b.c.d or a.b.c.d/prefix (prefix 24 by default; the prefix defines the local subnet). Empty means REX_DETECTED_LOCAL_IP, then `net_bind_ip`, then the default-route address, then 127.0.0.1. |
| `net_peers` | empty | Comma-separated host[:port] list (IPv4 or DNS names). Every broadcast the game sends also goes to each peer as a unicast copy. |
| `net_public_address` | empty | host[:port] (IPv4 or DNS name) advertised as XNADDR inaOnline / wPortOnline. Set `net_allow_peers` with it. |
| `net_allow_peers` | empty | IPv4 addresses, a.b.c.d/n ranges or DNS names. When set, datagrams from any other address are dropped before the game sees them; loopback and the local subnet always pass. When empty and `net_peers` or `net_public_address` is set, only the peers and the local subnet pass (fail closed, also while a name is still unresolved). When all three are empty, everyone is allowed, as on a 360 LAN. |

All five are read once at the first use after the kernel starts and need a restart, like the
0010 settings: the game threads read an immutable snapshot, never the cvar strings.

Decisions taken while implementing:

- The guest's beacon reply is a broadcast, not a reply to the sender [V]:
  sub_828294A0 (LAN response builder) sends with sub_827E3548
  (out/re-coop/sub_828294A0.txt line 98), and sub_827E3548 passes the beacon object
  itself as the destination address (generated/default/gow2_recomp.41.cpp:15762,
  `mr r7,r3`), which holds the 255.255.255.255:14001 address set at beacon init
  (sub_827E33C0).
- A broadcast from the beacon socket (guest port 14001, UE3's LanAnnouncePort) also
  goes, as a unicast copy, to the senders heard on that socket in the last second:
  at most 4 per broadcast (the player cap), newest first, and at most 2 copies per
  second to any one sender (a token bucket). That reaches a client behind NAT and a
  Windows PC on a Public network (Windows Firewall lets a unicast reply to its own
  broadcast through for about 3 seconds, but drops a broadcast reply). The caps keep
  an internet host from becoming a UDP reflector: one spoofed 16-byte query would
  otherwise send the 190-byte beacon to up to 32 addresses. Other sockets get no
  copies.
- A datagram identical to one from the same source in the last 250 ms is dropped, so
  a client that also hears the broadcast does not see the reply twice.
- On Android and Linux the runtime sends 255.255.255.255 as the subnet's directed
  broadcast, because Android routes the limited broadcast through the default
  network, which can be mobile data. Windows sends both the directed broadcast and
  255.255.255.255, because it sends the limited broadcast out of one adapter only, and
  on this PC that is not Ethernet: `Find-NetRoute -RemoteIPAddress 255.255.255.255`
  gave "Ethernet 3" (192.168.56.1, a VirtualBox host-only adapter, metric 25 like
  Ethernet) on 2026-10-15 and the Hyper-V Default Switch in an earlier check, while
  192.168.10.255 routes through Ethernet. A loopback or unknown subnet keeps
  255.255.255.255 alone.
- Datagram sockets receive into a 64 KiB host buffer. The allow list runs first, so a
  blocked sender's bytes never reach guest memory, and a datagram larger than the
  guest buffer cannot raise WSAEMSGSIZE before its source is checked; an allowed one
  is truncated and fails with WSAEMSGSIZE, as Winsock does. Stream sockets skip the
  allow list and the duplicate filter (TCP bytes are not datagrams).
- DNS: each name keeps its last good addresses when a lookup fails (logged as
  "cannot resolve <name>; keeping ..."); a name that never resolved contributes
  nothing, and the allow list stays closed rather than open. Lookups run on one
  background thread every 5 minutes, only while the link is on.
- The Android app uses REX_DETECTED_LOCAL_IP instead of REX_NET_LOCAL_IP, because
  REX_NET_LOCAL_IP would set the cvar itself and outrank the config file.
- `XNetXnAddrToInAddr` counts a peer as local when its ina is loopback, or when its
  ina is on our subnet and (it has no inaOnline, or a datagram arrived from that ina
  in the last 10 minutes). Local peers get ina, others get inaOnline (or ina when
  inaOnline is 0). This handles two homes that both use 192.168.1.x.
- An empty `net_local_ip` falls back to REX_DETECTED_LOCAL_IP, then `net_bind_ip`, then
  0010's routed address (UDP connect towards 192.0.2.1, which sends nothing), with a
  /24 prefix. The settings line names the source: "[netlink] link active: local
  192.168.10.10/24 (default-route adapter), ..., LAN broadcast 192.168.10.255,
  255.255.255.255, port offset 40000".

Patch 0012 adds a co-op hash gate on the beacon for 3+ players (see sdk/patches/README.md).
Still open: a build-id check for two-player co-op; the 30 minutes of Stage 4 test 1 and tests 2
to 4. Stage 4 test 3 is the mismatch join test (to be run).

**Wire compatibility.** Guest code builds every packet byte in big-endian guest memory, and the
runtime passes the bytes through raw, so both platforms put the same bytes on the wire. [I,
plausible] The values the runtime supplies (XNADDR, XNKID/XNKEY, XUID and `XNetRandom` output) must
be generated the same way on both. Floating-point differences between SSE and NEON only cause
server corrections, because UE3 is server-authoritative. [I]

**Android and POSIX work.** [V gaps in code unless marked]

- **Manifest and Wi-Fi.** The manifest declares only `VIBRATE` today.
  - Add INTERNET, ACCESS_NETWORK_STATE, ACCESS_WIFI_STATE and CHANGE_WIFI_MULTICAST_STATE.
  - Hold a `WifiManager.MulticastLock` and a low-latency WifiLock while network play is possible.
  - Discover the wlan0/ap0 address and pass it as `net_local_ip`.
- **`setsockopt` and `getsockopt`.** Map the Winsock levels and options:
  - SOL_SOCKET 0xFFFF -> 1
  - SO_REUSEADDR 4 -> 2
  - SO_BROADCAST 0x20 -> 6
  - SO_SNDBUF 0x1001 -> 7
  - SO_RCVBUF 0x1002 -> 8
  - SO_LINGER 0x80 -> 13

  Convert linger as two big-endian u16 into two ints. That also fixes the u32 swap on Windows.
- **`ioctlsocket`.** Map the 360's FIONBIO (0x8004667E) to `O_NONBLOCK` and handle FIONREAD
  (0x4004667F), both endian-correct.
- **`select`.** Compute nfds from the native descriptors; UE3 passes 0.
- **errno to WSA codes.**
  - EAGAIN -> 10035
  - EMSGSIZE -> 10040
  - ENETUNREACH -> 10051
  - ECONNRESET / ECONNREFUSED -> 10054
  - EHOSTUNREACH -> 10065
  - EADDRINUSE -> 10048
  - EACCES -> 10013
- **Ports.** Android apps cannot bind below 1024 [I, platform rule], and the game binds UDP 1000.
  Remap guest ports below 1024 to port + offset (for example 1000 -> 41000) on every platform,
  symmetrically in bind, sendto, recvfrom and getsockname. The remap must be on by default
  everywhere before cross-play. Also implement `getsockname` and `getpeername`, which are stubs
  today.
- **SDK tree.** The Android SDK tree must carry 0003 and 0010. Patches 0005 and 0006 are not there
  yet (see `docs/ANDROID.md`).
- **Version gate.** Patch 0012 adds a co-op hash gate on the beacon for 3+ players (see
  sdk/patches/README.md): System Link lists and answers only peers whose co-op hash matches. A
  build-id check for two-player co-op is still open.

#### Stage 4 test

1. Run a PC host with an Android client, then an Android host with a PC client, on one Wi-Fi
   network: discovery, join and 30 minutes of chapter 2.
2. Mix two PCs and two phones for four players.
3. Join with a patch-set mismatch: the join must be refused.
4. Repeat with the phone as a Wi-Fi hotspot (`ap0`).

## 4. Stage 1 patch list

Everything below is one group, enabled by `gow2_coop_players = N` (N >= 3; Stage 1 uses 4). The
patcher validates every row and then writes every row. A single failed check leaves the whole game
retail. The rows as built are `src/gow2_script_patch_table.cpp`, printed by `tools/script_patch.py`;
they also check the bytes between the edits (for example S1-1 +0x09 `16 04 3A`).

Notation:

- Bytes are as stored in the decompressed package, and offsets count from the start of the
  function's script.
- `[X]` is a 4-byte object operand: a package index on disk, an address in memory. It must equal
  the address of object X, resolved by name.
- Disk values are given in brackets so the offline tool can check them. "Context" bytes are checked
  but not changed.

| Id | Target (ScriptSize) | Offset | Old | New | Fail-closed check | Needed |
|---|---|---|---|---|---|---|
| S1-1 | `GearGame.GearGameSP_Base.AtCapacity` (17) | +0x0000; changes at +0x01 and +0x08 (upk 0x8B112D, 0x8B1134) | `04 9A 01 [Engine.GameInfo.NumPlayers] 2C 02 16` = `return NumPlayers == 2` (disk ref FFFFF5F8) | `04 99 01 [..] 2C NN 16` = `return NumPlayers >= N` (0x99 = native 153, `>=`) | Num == 17. Context +0x0A `04 3A [ReturnValue]`, +0x10 `53`. Operand +0x03 equals `Engine.GameInfo.NumPlayers`. | Yes: the cap at PreLogin, Login and PostLogin |
| S1-2 | `GearGame.GearGameSP_Base.SuggestPawnClassFor` (145 -> 318, grown) | Replace +0x0084..+0x0090; +0x0000..+0x0083 unchanged (upk 0x8B34B1) | `04 20 [GearGame.GearPawn_COGGear] 04 3A [ReturnValue] 53` = `return GearPawn_COGGear` (disk 6F95) | The 186-byte tail below: Ben Carmine, then RedShirt, by `FindObject` | Num == Max == 145. Context +0x0061 `04 20 [GearPawn_COGMarcus]`, +0x007E `04 20 [GearPawn_COGDom]`. Every resolved object has the expected class (Class, Function, property). | Yes: gives players 3+ a body that is not Marcus or Dom |
| S1-3 | `GearGame.GearMenuGame.ChangeCoopInviteSetting` (178) | +0x0047; change at +0x58 (upk 0x7FD7B9) | `0F 19 00 [Coop] 00 05 00 04 01 [Engine.OnlineGameSettings.NumPublicConnections] 2C 02` = `Coop.NumPublicConnections = 2` (disk refs 00002F73, FFFFF5E7) | `... 2C NN` | Num == 178. `[Coop]` is the parameter of this function. Context +0x0086 `0F 01 [GameInfo.MaxPlayers] 92` (`MaxPlayers = Public + Private`). | Yes: keeps the beacon answering until N players are in |
| S1-4 | Default object `GearGame.Default__GearCoopGameSettings` | Native +132 (`NumPublicConnections`) | int 2 (disk: tagged IntProperty at upk 0x86E033, `00 00 00 02`) | int N | Name and outer match, and Class is `GearGame.GearCoopGameSettings`. Value at +132 == 2. | Yes, as a guard: settings objects created at DG:25438, 89251 and 92950 copy this default [I which paths skip S1-3] |
| S1-5 | `GearGame.GearCampaignLobbyGame_Base.InitGame` (169) | +0x009E; change at +0xA5 (upk 0x80118B) | `0F 01 [Engine.GameInfo.MaxPlayers] 2C 02` = `MaxPlayers = 2` (disk FFFFF5FA) | `... 2C NN` | Num == 169. Context +0x00A6 `04 0B`, +0x00A8 `53`. | Yes for joins in the GearStart lobby (`GameInfo.AtCapacity` reads MaxPlayers) |
| S1-6 | `GearGame.GearGameSP_Base.InitGame` (202) | +0x00B4; change at +0xBB (upk 0x8B282B) | `0F 01 [Engine.GameInfo.MaxPlayers] 2C 02` = `MaxPlayers = 2` | `... 2C NN` | Num == 202. Context +0x00BC `0F 01 [GameInfo.MaxDynamicBandwidth] 1D 00 00 1B 58`, +0x00C9 `53`. | Consistency only: not on the join path |

The offset +132 for S1-4 comes from `layout.py` on Engine.upk (`OnlineGameSettings`: Public +132,
Private +136, OpenPublic +140, `bIsLanMatch` at +156 mask 0x40000000). It agrees with two
disassembly anchors: `sub_828294A0` reads +140 and +156, and `sub_8282B370` passes +132 as `maxPub`
to `XSessionCreate`. [V]

S1-2 tail. In memory, each `[..]` is the object's address; on disk it is the package reference in
parentheses. The jump target 0x0100 is absolute.

```
+0x0084  07 01 00 77 1C [Core.Object.FindObject (FFFFFAB3)]
         1F "GearGameContent.GearPawn_COGBenjaminCarmine" 00 20 [Core.Class (FFFFFE2C)] 16 2A 16
         ; if !(FindObject("...BenjaminCarmine", class'Class') != None) goto 0x0100
+0x00C2  04 13 [GearGame.GearPawn (00006E55)] 1C [Core.Object.FindObject]
         1F "GearGameContent.GearPawn_COGBenjaminCarmine" 00 20 [Core.Class] 16
         ; return class<GearPawn>(FindObject("...BenjaminCarmine", class'Class'))
+0x0100  04 13 [GearGame.GearPawn] 1C [Core.Object.FindObject]
         1F "GearGameContent.GearPawn_COGRedShirt" 00 20 [Core.Class] 16
         ; return class<GearPawn>(FindObject("...RedShirt", class'Class'))
+0x0137  04 3A [ReturnValue (00004DB7)] 53
         ; the compiler's closing 'return return', kept but never reached
```

- Each branch returns its own expression. The first version stored the class in `ReturnValue` and
  ended with `return return`. In this build, `EX_ReturnNothing` (`GNatives[0x3A]`, `sub_822536E8`)
  zeroes the result instead of reading the local, so that version always returned None. Players
  3+ then fell back to Marcus (Stage 1 results, step 8). [V in code and at run time]
- `FindObject` searches memory only. The game's own checkpoint code uses it to resolve pawn class
  paths (`GearAI.ApplyCheckpointRecord+0x006F`). `DynamicLoadObject` could go to disk for a
  `GearGameContent` package that does not exist when the class is missing.
- `0x77` is the native `!=` on objects, as in `GearGameSP_Base.GetDefaultPlayerClass+0x0010`
  (`07 00 40 77 ... 2A 16`).
- `[ReturnValue]` is copied from the original operand at +0x008C, after the patcher checks that it
  is a property named `ReturnValue` whose outer is this function.
- `tools/script_patch.py --check` decodes the 318-byte script to its end on the retail package.
  It ran in the game on 2026-10-15, and players 3 and 4 got `GearPawn_COGBenjaminCarmine`. [V]
- If both loads return None, `GetDefaultPlayerClass` (DG:47025) falls back to the game's
  `DefaultPawnClass`, which is Marcus. That reintroduces the problem from section 1.3, so Stage 1
  runs only where Ben is loaded.
- Any spawn failure is a full-squad game over (`GameInfo.RestartPlayer+0x0136` -> `Dead`), so watch
  for it in the log.

Diagnostic variant D-1, off by default and exclusive with S1-2: swap the operand at +0x0086 of
`SuggestPawnClassFor` from `[GearGame.GearPawn_COGGear]` to `[GearGame.GearPawn_COGMarcus]` (disk
`04 20 00006F95` -> `04 20 00006FA9`, same size). Players 3+ become Marcus clones, which are always
loaded, have a mesh and survive chapter commits. Use D-1 only to separate network bring-up from the
grown-script path; section 1.3 explains why it breaks Kismet. It is built in, behind
`gow2_coop_marcus_clones`.

Deliberately not in Stage 1:

- `Default__GearGameSP_Base.MaxPlayersAllowed` (no effect, see 1.1).
- The `RestartPlayer` jump at +0x06EE (wrong target, see 1.1).
- `MaxDynamicBandwidth` (Stage 3).
- Any change to `PostLogin`.

## 5. Open questions

Settled from code while writing patch 0010:

- **`XSessionCreate` flags.** The flag builder `sub_8282A760` returns 0x21 (HOST |
  USES_PEER_NETWORK, the XDK's `XSESSION_CREATE_SYSTEMLINK` plus HOST) as soon as `bIsLanMatch`
  (+156, mask 0x40000000) is set, before it looks at any other setting, so a LAN host passes exactly
  0x21. The join path `sub_8282CA88` calls the same builder and clears bit 0 (`rlwinm r24,r3,0,0,30`),
  so a joiner passes 0x20. [V] `system_link_trace` prints the flags of every create to confirm it at
  run time.
- **XNKID marker.** The title never builds or tests an XNKID: it copies SessionInfo+12 into the
  beacon response (`sub_828294A0`) and hands it back to XNet (`XNetRegisterKey`,
  `XNetXnAddrToInAddr`). Only XNet reads the marker. The XDK convention is that a system-link XNKID
  has the top bits of byte 0 clear (`XNET_XNKID_SYSTEM_LINK` = 0x00 under mask 0xE0/0xF0); patch 0010
  clears the top nibble of a random XNKID. [V title side, I XDK constants]
- **SessionInfo zero-fill.** Yes. The host path `sub_8282AA10` allocates the 72-byte SessionInfo
  (`li r3,72` / `bl 0x827df658`), clears it with `memset(p, 0, 72)` (`bl 0x82a93150`) and stores it
  at Session+8 before `sub_8282B370` calls `XSessionCreate`. The block is the nonce (+0), the
  handle (+8) and `XSESSION_INFO` (+12, 60 bytes). A joiner reuses the block that `sub_8282A9A0` fills
  from the search result. [V]
- **`XSessionGetDetails` size.** The title's wrapper `sub_829A5810` requires at least 128 bytes
  (`sizeof(XSESSION_LOCAL_DETAILS)`) and takes `pcbResultsBuffer` as a pointer; the XGI message
  carries that pointer at +4, not the size. [V]

Network and SDK:

- Does Windows deliver the game's 255.255.255.255:14001 broadcast to other local instances bound to
  0.0.0.0 with `SO_REUSEADDR`? If it does, fan-out is needed only in `net_bind_ip` mode.
- Which server cap clamps client rates, `MaxClientRate` or `MaxInternetClientRate`? Not answered;
  the measured rates (next item) stay below both.
- Does the client's System Link browser filter on `bAllowJoinInProgress` or
  `NumOpenPublicConnections` in native code? Partly answered: a full session is not listed. With
  four players in (cap 4), a fifth instance sees `NO CAMPAIGNS FOUND`. With the retail cap, a third
  instance sees the same once player 2 is in. A session in progress that still has room is listed
  and joinable. This matches the host side, where `sub_828294A0` stops answering once
  `NumOpenPublicConnections` is 0. Whether the client also filters was not separated.
- Is a 7000 bytes/s cap per client enough for 4 players? In the opening of chapter 2, yes. The
  host sent each client 3.0-3.5 KB/s on average, with a peak of 5.3 KB/s at about 29 packets/s,
  and received about 0.7 KB/s from each (Stage 1 results, step 12). In Stage 2 (1709 host-to-client
  10 s windows) the per-client peak was 5327 B/s at about 30 packets/s, 15 windows were above 5000
  and none above 6500, and host upload peaked at 15.8 KB/s with 3 clients. Heavy real combat was
  not measured; log traffic in the real session with `--net_traffic_log_sec=10 --system_link_trace=true`
  on the host: windows pinned near 7000 B/s mean the cap binds (Stage 3's MaxDynamicBandwidth edit).

Running four instances on one PC:

`scripts/coop-instances.ps1` (actions `start`, `shot`/`shots`, `key`/`down`/`up`,
`pad`/`hold`/`release`, `stats`, `status` and `stop`) starts N instances. Each gets its own
`--user_data_root`, which can be seeded with a copy of a save; its own log file and test pad file;
`{i}`-expanded flags (for example `--net_bind_ip=127.0.0.{i}`); and a slot in a non-overlapping
window grid. It also starts and stops the idle guard.

Stage 2 test tooling (dev only): a per-instance command file (`gow2_dev_command_file`) runs console
commands and host verbs on each instance, and `scripts/coop-instances.ps1` gained `cmd`, `waitlog`,
`stopone`, `startone`, `chapter` and `macro`. See [TEST_TOOLING.md](TEST_TOOLING.md).

- **Shared folders.** Yes, until this branch. `OnConfigurePaths` replaced the user data and cache
  paths with `%LOCALAPPDATA%\GearsOfWar2PC\{user,cache}` even when `--user_data_root` was given, so
  every instance used one save, one profile and one shader cache, and a test instance would have
  written into the player's own save folder. Now an explicit `--user_data_root` (the cache follows into its
  `cache` folder) or `--cache_root` wins. A separate user data root also gives each instance its
  own `system_link_xuid.txt`, and so its own XUID. `gow2.toml` beside the exe stays shared and is
  only read. Logs go to `<exe folder>\logs` unless `--log_file` is given. [V]
- **Single instance.** Nothing enforces one: five instances of one exe ran together. Two
  instances that share a cache folder do not both get persistent shader storage, because the
  pipeline storage file is opened with `_wfopen_s`, which does not share. [I from code; the tests
  used separate caches]
- **Input.** The runtime gates every input device on window focus. The input system's active
  callback returns false while the window lacks focus, and that covers keyboard, mouse and pads.
  Only the focused instance gets input, and one controller drives whichever window is in front.
  For tests, `--gow2_test_input=<file>` merges a pad state from a per-instance file into guest
  user 0 with no focus needed (`src/gow2_test_input.cpp`). It is how the four-instance runs were
  driven. [V]
- **CPU, GPU and VRAM.** In game, with `resolution_scale=1` and small windows, each instance used
  2-5 of the 24 logical CPUs (8-22 %, the host lowest), 1.35-1.53 GB of RAM, 650-830 MB of VRAM and
  10-40 % of the RX 7800 XT's 3D engine. Four in game: about 6 GB of RAM and 3 GB of VRAM in all.
  A fifth instance waiting in the menus added 1.16 GB and 443 MB (`stats-*.txt`). Every instance
  picked the RX 7800 XT
  (`gow2 d3d12_adapter -> 1 (AMD Radeon RX 7800 XT; first dedicated GPU in Windows' high-performance order)`).

Script and engine:

- `REX_HOOK_RAW(sub_8283F780)` runs on Windows: every run logs `SCRIPTPATCH startup script
  packages loaded`. Num and Max of `UStruct::Script` are at +88 and +92, since all six rows validate
  and the 318-byte script runs. [V] Whether it links the same way on Android is still open.
- How does the native checkpoint loader match records to PCs, and who sets `bCheckpointDummy`?
- How does `UWorld::CommitMapChange` treat remote players' pawns once
  `bKillDuringLevelTransition` is cleared?
- Is `WorldInfo.bNoDefaultInventoryForPlayer` set in the Act 1 maps? Not in chapter 2: players 3
  and 4 spawned with a Lancer (350 rounds in their HUD).
- When a streaming sub-level that exported Ben Carmine unloads (chapters 3-4), does a live pawn keep
  the class alive? Can a client that has not streamed that sub-level resolve the class?
- Does the Minh_Kim skeleton have the player camera bones (`b_MF_Camera`, `b_MF_Camera_Root`)?
  The Ben Carmine players get the normal over-the-shoulder camera (`net8e/013055-i3-joined.png`), so
  it works in practice; the skeleton itself was not parsed.
- S1-2 now uses `FindObject`, which never touches the disk, so a miss (chapter 1) returns None
  and the player becomes a Marcus clone. That chapter has not been run.
- With the link on, does a solo campaign host a LAN session, as Stage 1 item 2 infers? About 40 s
  of solo play made no `XGISessionCreate`. Only HOST CO-OP CAMPAIGN created one.
- The campaign lobby shows two player slots. Players 3 and 4 join and play, but each lobby shows
  only two of the four names.
- Is `sub_825E2780` the only way pawns enter `PawnList`?
- The Kismet of `SP_Hospital_03` and `SP_Sinkhole_W` was not scanned (LZX decompression failed at
  chunk 20). The first is a chapter 2 map.
- Does the vehicle camera work for a rider whose view target is a vehicle he is not seated in?
- Does `Pawn.OnTeleport` fail silently for the third and fourth pawn sent to one destination?
- If Title Update 6 replaces `GearGame.xxx`, the patch table needs one variant per version, keyed
  by ScriptSize and the context bytes.
