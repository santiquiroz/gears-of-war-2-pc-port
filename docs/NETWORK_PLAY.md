# Network play: System Link on a LAN and over the internet

This guide covers System Link co-op between a PC and a phone, on one home
network or over the internet.

## What you need

- Every player runs the same release, v0.1.2. It contains the Stage 2 co-op work and the codegen fix d074c0d. `gow2_coop_players` and the co-op settings do not need to match: the host's settings apply to everyone.
- With `gow2_coop_players` 3 or more, v0.1.2 adds a beacon gate (`net_beacon_gate`, default true): every peer announces its co-op hash with each System Link search and answer, and campaigns whose hash differs stay hidden (the friend sees NO CAMPAIGNS FOUND). Since v0.4.8 every peer announces the hash whatever `gow2_coop_players` says, two-player games included. Before v0.4.8, with 2 players there was no check: a v0.1.1 peer finds and joins a v0.1.2 host without any warning, but it lacks d074c0d and is expected to crash about 0.5 s after the chapter 2 -> 3 map commit ("[FATAL] Call to invalid or unregistered function at guest address 0x82ADBEF0"), even in two-player co-op [I].
- Every player turns System Link on with `net_link_active = true`. It is off
  by default (patch 0010): a solo game shows an unplugged cable and answers
  nobody on the network. `gow2_coop_players` 3 or more turns it on by
  itself; two-player co-op needs the setting.
- For three or four players, the host sets `gow2_coop_players`, for
  example 4 (the defaults for the rest are fine); friends follow the host's settings.
  A v0.1.1 friend cannot see a 4-player host, and neither can a friend whose
  patched packages fail the coop-client rows.
  Without the gate (older builds) a friend left at 2 sees players 3-4 with no body [I].
- Every player uses the same `net_port_offset`. Leave the default.

## Four players

The host sets `gow2_coop_players=4` (the defaults for the rest are fine). On PC pass `--gow2_coop_players=4`;
on Android put `gow2_coop_players = 4` in `gow2.toml`.

With 3 or more players the beacon gate applies: every peer announces its co-op
hash with each System Link search and answer, and campaigns whose hash differs
stay hidden. The friend sees NO CAMPAIGNS FOUND; the host log says
`[netlink] beacon from <address> dropped: no co-op hello (ours <hash>)` or
`... dropped: co-op hash <theirs> differs from ours <ours>`, and the friend log
may say `[netlink] co-op hash from <address> is <theirs>, ours is <ours>:
different build or co-op settings; its campaigns stay hidden`. A host at 2 has
no gate.
The gate holds over the internet, where the host answers only the peers whose
search it accepted. On one LAN the host also answers with a broadcast, so a copy
without a co-op hash (v0.1.1, or `gow2_coop_players=2`) can still list a 4-player
host while another player's search is running [I]. Check the hashes anyway.

### Pre-session check

Every player's log (the newest file in the logs folder next to gow2.exe, or
the --log_file path) shows these lines with the same hashes and no `skipped`:

```
SCRIPTPATCH coop: 6 rows applied for 4 players, set hash 937c35f80cb274b7
SCRIPTPATCH survival: 4 rows applied for 4 players, set hash 321b36117af31c35
SCRIPTPATCH coop-body: 5 rows applied for 4 players, set hash b62ed993f8c3fe5e
SCRIPTPATCH coop-ride: 3 rows applied for 4 players, set hash 9381496b16a09620
SCRIPTPATCH coop-scenes: 6 rows applied for 4 players, set hash cddd0eadc988a154
SCRIPTPATCH stack: players=4 coop=937c35f80cb274b7 survival=321b36117af31c35 split-revive=off coop-body=b62ed993f8c3fe5e coop-ride=9381496b16a09620 coop-scenes=cddd0eadc988a154 combined=e52feaafc39b3f26
```

The four players compare the one `combined=` value. The hashes depend on the
player count and the co-op settings; these are for 4 players with the default
settings.

### Session notes [V unless marked]

- The campaign lobby lists only two of the four names (expected; players 3-4
  are in the session).
- Join outside cinematics: a remote join more than 15 s into a level cancels
  the running cutscene for everyone.
- After a crash wait about one minute before searching again: the host frees
  the slot only after its 45 s connection timeout, and until then a full
  session shows NO CAMPAIGNS FOUND.
- After chapter travel the human Dom waits at PRESS RT TO BEGIN and presses
  Right Trigger.
- After a checkpoint load the human Dom also presses Right Trigger at once. In
  one chapter 16 reload in the tests Marcus fell at the map origin for about
  three minutes, while Dom waited, until the chapter-start cinematic ran.
- A chapter loaded by travel (the dev `chapter` command, servertravel) can give
  the Dom role to another friend; the player who was Dom then plays as a player
  3+ until the next travel. Chapter changes by playing through were not checked
  for this.
- Players 3+ who fall far behind are moved next to Marcus or Dom by the host
  (`gow2_coop_leash`, on by default on Windows, off by default on Android
  because the fail-closed fault handling of these patches exists only on
  Windows).
- While Marcus or Dom rides a vehicle, players 3+ on foot cannot be hurt.

### Not tried yet

A launch with all four players already in the lobby on v0.1.2 (the Stage 2
runs put host and Dom in the lobby and had players 3-4 join mid-game; that
order is the tested one), co-op splits, getting out of a vehicle, the
Ride-Reaver section, a join during a cinematic, long combat, four PCs over a
LAN or the internet, Android with 3+ players. Stage 2 ran four copies on one
PC over loopback only.

### Bandwidth [V, loopback, scripted play]

Per-client peak 5.3 KB/s (76 % of the game's 7000 B/s per-client cap,
MaxDynamicBandwidth), host upload peak 16 KB/s with 3 clients. The host needs
at most about 3 x 7000 B/s, about 170 kbit/s, of upload. Run the host with
`--net_traffic_log_sec=10 --system_link_trace=true` for the session:
`[nettraffic]` windows pinned near 7000 B/s mean the cap binds (a Stage 3
change).

See [COOP_N_PLAYERS.md](COOP_N_PLAYERS.md) for the co-op patch groups and the
Stage 2 results.

## Settings

On PC, `user_name`, `net_link_active`, `gow2_coop_players`, `net_peers`, `net_public_address` and `net_allow_peers` can be set in game under PC Options > Multiplayer (restart to apply; [PC_OPTIONS.md](PC_OPTIONS.md)). They can also be passed on the command line as `--name=value`, from a shortcut or
a small launch script. `gow2.toml` next to `gow2.exe` also works, but every
build from source copies `config/gow2.toml` over that file (a CMake
post-build step, `copy_if_different`), so settings edited there vanish at
the next build without a warning; the release zip is not affected. The command line also outranks
`gow2.toml`.

On Android, put a `gow2.toml` in the app's files folder,
`/sdcard/Android/data/com.santiquiroz.gow2recomp/files/` (the runtime reads
its config there; reach it with `adb push` or a USB file transfer from
Windows Explorer). For System Link it needs one line:

```
net_link_active = true
```

The app detects the Wi-Fi address itself.

Every setting below is read once when the game starts; change it, then
restart.

| Setting | Default | Effect |
|---|---|---|
| `net_link_active` | false | true turns System Link on (the game sees an active Ethernet link). |
| `net_port_offset` | 40000 | Guest ports below 1024 move up by this offset on the host, with System Link on or off (a solo game binds 41000 too). The game's UDP 1000 becomes 41000. The beacon port 14001 stays as it is. |
| `net_local_ip` | empty | This machine's LAN address, as a.b.c.d or a.b.c.d/prefix (prefix 24 by default). Empty means the app's detected address, then `net_bind_ip`, then the default-route address, then 127.0.0.1. |
| `net_peers` | empty | Comma-separated host[:port] list (IPv4 or DNS names). Every broadcast the game sends also goes to each peer as a unicast copy. |
| `net_public_address` | empty | host[:port] (IPv4 or DNS name) advertised to other players. Set `net_allow_peers` with it. |
| `net_allow_peers` | empty | IPv4 addresses, a.b.c.d/n ranges or DNS names that may send to the game; loopback and the local network always may. Empty: with `net_peers` or `net_public_address` set, only the `net_peers` and the local network; with neither, everyone. |
| `net_beacon_gate` | true | Only peers on the same release see each other's campaigns, whatever `gow2_coop_players` says (the hash covers only the coop-client rows every peer runs). False turns the hash check off, for diagnosis only. |
| `system_link_trace` | false | Set to true to log network activity under `[netlink]`. |

## PC and phone on the same Wi-Fi

The PC sits on Ethernet at 192.168.10.10/24. The phone (Galaxy S25 Ultra)
sits on Wi-Fi at 192.168.10.114/24. Both are on the same subnet.

Let the phone host:

1. On the phone, put the `gow2.toml` with `net_link_active = true` in place
   (see Settings), start the game, and host a co-op campaign.
2. Start the PC copy with `--net_link_active=true
   --net_peers=192.168.10.114`, search for System Link games and join.

`net_peers` sends the PC's search to the phone as a unicast as well as a
broadcast. The phone's answer then comes back on the same address and port
pair, which Windows Firewall lets in without any inbound rule. Without
`net_peers` the search relies on broadcasts. The PC sends each one to both
192.168.10.255 and 255.255.255.255, because Windows sends 255.255.255.255
out of one adapter only, and on this PC that is a VirtualBox or Hyper-V
adapter, not Ethernet (`Find-NetRoute -RemoteIPAddress 255.255.255.255`).
The phone's answer to a broadcast search is a broadcast too, and Windows
Firewall on a Public network drops it; the unicast copy the phone sends
back to the searching address is what gets through.

To run the PC side from a script:

```
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run-instances.ps1 -OutDir out\lan-test\run1 -Names pc -Lan -Peers 192.168.10.114 -ExtraArgs "--audio_mute=true"
```

It starts one windowed copy with System Link and `system_link_trace` on,
its own log and its own save folder, and without the loopback settings of
"Testing on one PC" below. `scripts\instance-input.ps1` sends it keys. The
menu path is the client's path from that section.

### Tested on 2026-10-15

The PC (Ethernet, 192.168.10.10) and a Galaxy S25 Ultra (Wi-Fi,
192.168.10.114) on one home network, both built from the same commit:

- Phone hosts, PC joins with `--net_peers=192.168.10.114`: the PC's
  System Link Browser lists the phone's campaign, both lobbies list both
  players, and both load Act 1 chapter 1. The PC log shows the phone's
  beacon answer arriving (`first datagram from 192.168.10.114:14001 on
  local port 14001 (190 bytes)`), so Windows Firewall lets the reply in.
- PC hosts, phone joins with no `net_peers`: the phone's broadcast search
  reaches the PC (`first datagram from 192.168.10.114:14001`), the phone
  lists the PC's campaign and joins, and both load chapter 2. This needs
  the inbound rule for that `gow2.exe` path (see Windows Firewall below).
- In game, each player sees the other move when the other moves.
- Repeated with the merged build (master, saves, coop-stage1 and crossplay),
  PC hosting from the path with the firewall rule: the phone's browser
  listed `Pc`, both lobbies listed Pc and User, both loaded chapter 2, and
  the phone spawned as Dom on RT. Both logs show
  `bind port 1000 -> 41000 (net_port_offset)` and
  `XGISessionJoinRemote ... members=2`; the PC sent the phone about 2.8 KB/s
  and received about 1 KB/s (`[nettraffic]`).
- Evening check: the Android performance build (`d65fe7d`) joined the
  v0.1.1 PC host with no `net_peers`. Both lobbies listed Pc and User,
  both loaded Act 1 chapter 2, and the phone spawned as Dom on RT. Each
  player saw the other in game. The phone received the PC's 188-byte
  beacon answer on port 14001. The phone logged
  `XGISessionJoinRemote ... first_xuid=B13E000000000001 members=2`;
  the PC logged
  `XGISessionJoinRemote ... first_xuid=B13E50A1BB0DC9A9 members=2`.
  Both sides used port 41000 for game traffic.

Both logs show `XGISessionJoinRemote ... members=2` with the other side's XUID, and game traffic on
port 41000 in both directions. As the client in chapter 2, the phone ran at about 21-24 fps with
hitches of about 0.3 s for the first 20 s after the level appeared (streaming, before RT), then at
30 fps (one 25.4 fps window) at 443-660 MHz, then 27-30 fps at 342-389 MHz (docs/ANDROID.md). Either
side can host; a PC copy hosts only from a path with the inbound firewall rule (below). In chapter 1
the second player's sticks did nothing until they held LB for the objectives prompt.

A script can drive the phone without touching the screen: put
`gow2_test_input = "/storage/emulated/0/Android/data/com.santiquiroz.gow2recomp/files/pad.txt"`
in the phone's `gow2.toml`, then write pad lines to that file with
`adb shell "echo 'A' > /sdcard/Android/data/com.santiquiroz.gow2recomp/files/pad.txt"`
(format in `src/gow2_test_input.h`; an empty line releases everything).
Hold a menu direction only briefly, or the menu repeats it.
Time the hold on the phone in one adb call, with `<files>` replaced by
the app's files folder:

```
adb shell "echo 'DOWN' > <files>/pad.txt; sleep 0.12; echo '' > <files>/pad.txt"
```

A 0.12 s hold for a menu direction and 0.2-0.3 s for A/START/X worked.
The file is read every 15 ms.

### Windows Firewall

The phone hosts because the PC's network profile is Public. Inbound rules
exist only for the main tree's `gow2.exe` path. A `gow2.exe` at any other
path (for example a worktree build) gets no unsolicited inbound traffic.
That copy can join a phone's game, but cannot host until a rule exists for
its path. Joining with `net_peers` set needs no inbound rule.

The first time a copy binds 0.0.0.0 (any run without the loopback
settings), Windows may ask whether to allow it. A user without admin
rights who presses Cancel gets inbound Block rules for that path, and a
Block rule beats every Allow rule. That has already happened to
`C:\personal\gow2-recomp-coop\out\build\release\gow2.exe` (two rules named
`gow2`, Inbound, Block, Public).

To let a PC host, run these in an elevated PowerShell prompt (replace the
path with the full path to that copy). First remove any Block rules for
the path, then add the Allow rule:

```
Get-NetFirewallApplicationFilter -Program '<full path to gow2.exe>' | Get-NetFirewallRule | Where-Object Action -eq 'Block' | Remove-NetFirewallRule
netsh advfirewall firewall add rule name="gow2 System Link" dir=in action=allow program="<full path to gow2.exe>" protocol=UDP localport=14001,41000 profile=public,private
```

To check the existing rules, run:

```
netsh advfirewall firewall show rule name=gow2 verbose
```

The existing rule for
`C:\personal\gow2-recomp\out\build\release\gow2.exe` already lets that
copy host.

A phone acting as a Wi-Fi hotspot also works. The app picks the hotspot
address (swlan0/ap0) itself.

## Over the internet (host on a PC)

[ONLINE.md](ONLINE.md) works these steps through in detail (one or two routers in a row, a DNS
name, the firewall rule, a test from mobile data) and covers Tailscale and ZeroTier.

1. On the router, forward UDP 14001 (System Link beacon) and UDP 41000
   (game traffic, guest port 1000 + 40000) to the hosting PC's LAN
   address. `scripts/host-online.ps1 -Start` does this through UPnP on
   every router in the path, with a lease, and `-Stop` undoes it.
   TCP is not needed for campaign co-op (TCP 8675 is the versus party
   beacon).
2. DNS: create an A record such as `gow2.example.com` that points at the
   home's public IP. If the ISP changes the address, use dynamic DNS.
   Check for CGNAT first: if the router's WAN address differs from the
   public address that a "what is my IP" page shows, port forwarding
   cannot work. Then use a virtual LAN such as Tailscale or ZeroTier and
   put the other players' virtual addresses in `net_peers`, because
   broadcasts do not cross those either.
3. The host sets `net_link_active=true`,
   `net_public_address=gow2.example.com` and `net_allow_peers` with every
   friend's public address or dynamic DNS name, for example
   `net_allow_peers=friend1.example.com,203.0.113.7`. Without
   `net_allow_peers` the host lets in only its own network and its
   `net_peers`, and logs a warning that names `net_allow_peers`.
4. Each friend sets `net_link_active=true` and
   `net_peers=gow2.example.com`, and searches for System Link games as
   usual.
5. The host's Windows Firewall needs the inbound rule for its `gow2.exe`
   path (see Windows Firewall above).
6. Phones on mobile data sit behind carrier NAT in most cases and cannot
   host. They can join: on 2026-10-15 a Galaxy S25 Ultra on mobile data
   joined a PC host behind two routers in a row and played Act 1 chapter 2
   as Dom (ONLINE.md, "Tested over the internet").

## Security note

The game's System Link traffic is not encrypted or authenticated, and the
2008 game code that parses it was written for a LAN. On a host with
forwarded ports, `net_allow_peers` decides who reaches that code:

- With `net_public_address` or `net_peers` set and `net_allow_peers` empty,
  only loopback, the local network and the `net_peers` get through.
- A DNS name in `net_allow_peers` or `net_peers` lets nothing through until
  it first resolves. When a later lookup fails, the last good addresses
  stay in use and the log says so.
- Datagrams are checked before any byte reaches the game. One that is
  larger than the game's buffer is checked too, then truncated, as on the
  console.
- The unicast copies of a beacon broadcast go only to senders heard in the
  last second on the beacon socket: 4 per broadcast at most, and at most 2
  per second to any one address. So a spoofed query cannot turn the host
  into a traffic amplifier.

## Testing on one PC

Two copies of `gow2.exe` can play System Link on one PC over loopback
[V]: the host binds 127.0.0.1, the client 127.0.0.2. Verified on
2026-10-14 with the crossplay build (patch 0011, before the rebase onto the
opt-in 0010): the client found the host's campaign in the System Link
Browser, joined the lobby, loaded Act 1 chapter 2 with the host and played
as Dom. A second run repeated it with the first-datagram trace (see
Troubleshooting). Evidence: `out\pcpc-test\run2` and `out\pcpc-test\run3`
(logs, screenshots, `netlink-evidence.txt`). `out\` is not in git.

`scripts\run-instances.ps1` starts the copies and supervises them. Each
copy gets its own `LOCALAPPDATA` under the run folder (the game keeps
profile, saves and caches in `%LOCALAPPDATA%\GearsOfWar2PC`, so the
script seeds each copy with a copy of the real one), its own log, XUID
(`B13E0000000000` + 2 hex digits) and gamertag (the instance name),
`--net_link_active=true`, `--net_bind_ip`, `--net_broadcast_targets`
listing every copy, `--system_link_trace=true`, and a windowed 1152x648
window placed side by side. Each copy also gets
`--gow2_save_dir=<run>\<name>\saves`: the build has the `gow2_save_dir`
setting ([SAVES.md](SAVES.md)), and a `gow2_save_dir` in `gow2.toml` (for
example a cloud folder) would otherwise make every copy write the same real
save at once. The command line wins over `gow2.toml`, and on its first
start each copy copies the save seeded into its own `LOCALAPPDATA` into
that folder. A build without the setting starts normally and ignores the
option (out\pcpc-test\run4, 2026-10-15).
The script stops every
copy it started when the file `stop` appears in the run folder, when all
copies exit, or after `-MaxSeconds` (600 by default). It writes
`instances.json`.

Example:

```
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run-instances.ps1 -OutDir out\pcpc-test\run1 -ExtraArgs "--audio_mute=true --resolution_scale=1"
```

`scripts\instance-input.ps1` brings one copy to the front and sends it
keys, clicks and screenshots, for example:

```
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\instance-input.ps1 -OutDir out\pcpc-test\run1 -Instance client -Steps "focus,key:S:200,wait:800,Space,wait:3000,shot:after-join"
```

Steps: a key name (`System.Windows.Forms.Keys`) or
`key:<name>:<holdMs>`, `wait:<ms>`, `shot:<label>` (PNG in
`<run>\shots`), `click` (left button at the window centre), `focus`. The
game only reads input from the focused window.

Keys in the menus with `--mnk_mode=true`: W/S move the selection (left
stick; the arrow keys are the right stick and do not move menus), Space
= A (select), F = B (back), E = X, Q = Y, Return = Start, left mouse
button = right trigger.

Menu path that worked:

1. Host: PRESS START (Return) -> main menu, S to CO-OP CAMPAIGN, Space
   -> CAMPAIGN MENU, HOST CO-OP CAMPAIGN (Space) -> SELECT SAVE SLOT
   (Space) -> CAMPAIGN LOBBY. CO-OP MODE already reads "System Link".
   The session is created when the lobby opens: the host log shows
   `[netlink] XGISessionCreate ... flags=0x00000021 ... host=true ...
   host_ip=127.0.0.1`. LAUNCH CAMPAIGN (Space) once the client is in the
   lobby.
2. Client: PRESS START -> CO-OP CAMPAIGN -> JOIN CO-OP CAMPAIGN (S,
   Space) opens the SYSTEM LINK BROWSER directly. E refreshes; the
   host's campaign appears under its gamertag; Space joins and both
   lobbies list both players. After the host launches, the client sees
   "PRESS RT TO BEGIN!"; the right trigger (left mouse button) spawns
   the client as Dom.

What the logs show:

```
client: [netlink] broadcasts to port 14001 go out to [127.0.0.1:14001]
client: [netlink] XGISessionCreate ... flags=0x00000020 ... host=false known=true xnkid=00F4837B3BB2B61B host_ip=127.0.0.1
client: [netlink] XNetXnAddrToInAddr caller=1 ina=127.0.0.1 inaOnline=0.0.0.0 -> 127.0.0.1 ret=0x0
client: [netlink] sendto port 1000 -> 41000 (net_port_offset)
host:   [netlink] bind port 1000 -> 41000 (net_port_offset)
host:   [netlink] broadcasts to port 14001 go out to [127.0.0.2:14001]
client: [netlink] first datagram to 127.0.0.1:14001 from local port 14001 (16 bytes)
host:   [netlink] first datagram from 127.0.0.2:14001 on local port 14001 (16 bytes)
host:   [netlink] first datagram to 127.0.0.2:14001 from local port 14001 (190 bytes)
client: [netlink] first datagram from 127.0.0.1:14001 on local port 14001 (190 bytes)
client: [netlink] first datagram to 127.0.0.1:41000 from local port 54430 (55 bytes)
host:   [netlink] first datagram from 127.0.0.2:54430 on local port 41000 (55 bytes)
```

The 16-byte datagram is the client's System Link query and the 190-byte
one the host's beacon answer; game traffic then goes to the host's
remapped port 41000.

The client's XNKID equals the one the host created, which is how the
beacon answer reached it.

Loopback traffic does not cross Windows Firewall, so this test says
nothing about the firewall; the PC-phone test does.

## Troubleshooting

Turn on `system_link_trace` and look for these log lines:

- `[netlink] link reported down (net_link_active=false): no System Link`:
  set `net_link_active=true` on that copy.
- `[netlink] link active: local 192.168.10.10/24 (default-route adapter),
  bind 0.0.0.0, 0 broadcast target(s), LAN broadcast 192.168.10.255,
  255.255.255.255, port offset 40000`: the address this copy advertises,
  where it came from (`net_local_ip`, `REX_DETECTED_LOCAL_IP`,
  `net_bind_ip`, `default-route adapter` or `no network adapter found`)
  and where its broadcasts go.
- `[netlink] net_peers '...', net_public_address '...', net_allow_peers
  '...'` and the warning about an empty `net_allow_peers` on a public
  host.
- `[netlink] resolved <name> -> <addresses>`, `[netlink] cannot resolve
  <name>; keeping <addresses> until it resolves again` and `[netlink]
  cannot resolve <name>`.
- `[netlink] broadcasts to port 14001 go out to [...]`
- `[netlink] dropped ... (net_allow_peers)`
- `[netlink] XNetXnAddrToInAddr ... -> ...`
- `[netlink] first datagram from <ip>:<port> on local port <port> (<n>
  bytes)` and `[netlink] first datagram to <ip>:<port> from local port
  <port> (<n> bytes)`: logged once per socket handle and peer in each
  direction, even after the trace's rate limit (first 50 calls, then
  every 500th) is spent. Windows reuses socket handles, so a reconnect
  to the same peer on a new socket may not log again.

On a PC that joins a phone's game, "first datagram to
192.168.10.114:14001" means the search reached the phone's address, and
"first datagram from 192.168.10.114:14001 on local port 14001" means the
phone's beacon answer got back. If the first line is there and the second
is missing while the phone's log shows the answer going out, check that
the PC copy runs with `net_peers` set to the phone's address; with it set,
look at the firewall rules for that `gow2.exe` path.

The successful and the failed (would-block) recvfrom/sendto calls have
separate trace budgets, so the polling of an idle socket does not hide
real datagrams.

If the PC advertises 127.0.0.1, set `net_local_ip` to its LAN address.
