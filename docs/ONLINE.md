# Playing over the internet

System Link was made for a LAN. Patch 0011 (`sdk/patches/0011-network-crossplay.patch`) lets one
player host a campaign that friends on other networks join: the host opens two UDP ports, and
every friend names the host with `net_peers`. On a PC, `scripts/host-online.ps1` opens those ports
through UPnP for a few hours and closes them again. This guide covers that set-up, forwarding by
hand for routers without UPnP, what it exposes, and two VPN alternatives that open no port at all.

Addresses below are examples from the documentation ranges: 203.0.113.10 is the host's public
address, 198.51.100.20 and 198.51.100.30 are friends, `gow2.example.com` is a DNS
name the host controls, and 192.168.1.10 is the host PC on its home network.

Status: on 2026-10-15 a phone on mobile data joined a campaign hosted by a PC at home over the
internet, through ports that `host-online.ps1` opened on two routers in a row (see "Tested over
the internet"). PC and phone also joined each other's campaign over one home LAN
(NETWORK_PLAY.md), and the merged build repeated it: two copies on loopback, and a phone that
joined a PC-hosted campaign on the LAN. A PC friend outside the network has not been tried; see
"Not verified yet".

## How the connection works
Only the host needs open ports. In the loopback runs (2026-10-14 and 2026-10-15), every datagram
the host sent went to an address and port it had first heard from: the client's ports 14001 and
the client's game socket. Only the client called XNetXnAddrToInAddr. [V on loopback, I over the
internet]

1. A friend's search goes as a unicast copy to each `net_peers` entry on UDP 14001. [V code]
2. The host answers with a broadcast and a unicast copy to the address the search came from;
   that unicast crosses the internet. [V code]
3. The beacon answer carries the host's XNADDR: its LAN address and, with `net_public_address`,
   its online address. [V code]
4. A peer outside the host's local network connects to the online address; a peer on the same
   LAN keeps using the LAN address (PeerInAddr in patch 0011). [V code]
5. Game traffic then goes to the host's UDP 41000: the game's UDP port 1000 plus
   `net_port_offset` (40000). The beacon port 14001 is above 1024 and keeps its number.
   [V code, unit tests]

A friend whose home network uses the same private range as the host (say 192.168.1.0/24) still
connects to the online address: a peer's LAN address counts as local only when the peer advertises
no online address or this side heard from that LAN address in the last 10 minutes. A phone on the
host's own Wi-Fi keeps using the LAN address. [V code]

## Host setup (PC)
Campaign co-op takes two players. For three or four players every player, host and each
friend adds `--gow2_coop_players=4` (Android: `gow2_coop_players = 4`), all on v0.1.2. A friend left at 2 or on an older build cannot see the host (beacon gate). `scripts\host-online.ps1 -Start -Players 4` prints the command lines with it; see [NETWORK_PLAY.md](NETWORK_PLAY.md) "Four players". Three or more players have not been tried over the
internet.

### Open the ports with host-online.ps1 (UPnP)
Most home routers let programs on the LAN add port mappings through UPnP (IGD).
`scripts/host-online.ps1` asks every router between the PC and the internet for UDP 14001 and UDP
41000, for a limited time, and removes them again. It comes with the repository, not with the
release zip, and needs `host-online-upnp.ps1` and `host-online-state.ps1` next to it. It needs no
administrator and runs in Windows PowerShell 5.1 and PowerShell 7:
```
powershell -ExecutionPolicy Bypass -File scripts\host-online.ps1 -Start [-Hostname gow2.example.com]
powershell -ExecutionPolicy Bypass -File scripts\host-online.ps1 -Status
powershell -ExecutionPolicy Bypass -File scripts\host-online.ps1 -Stop
```
`-Start` does this, in order:
1. Finds the routers. It sends an SSDP search for InternetGatewayDevice:1 and :2 on the LAN, then
   probes the route toward 1.1.1.1 one hop at a time (as traceroute does) and sends each hop a
   unicast UPnP search. A router whose WAN address (GetExternalIPAddress) is private has another
   router in front of it, so the script goes on to the next hop; it stops at the router whose WAN
   address is public. It speaks UPnP to each hop only at that hop's own address: an SSDP answer,
   a base URL or a control URL that points at another host, or at anything but plain http, is
   ignored. Before the next router maps to a router's WAN address, that address must be a private
   a.b.c.d address in the next hop's private range. Router answers are capped at 256 KB, and XML
   with a DTD is refused.
2. Checks that the outermost WAN address is what https://api.ipify.org sees.
3. Reads UDP 14001 and 41000 on every router (GetSpecificPortMappingEntry) and refuses to go on
   if either port already goes to another device or port, or carries the script's description
   but points elsewhere (an earlier `-Start` from another LAN address, or another PC that hosts).
4. Maps both ports on every router, with the description `gow2 System Link` and a lease
   (`-LeaseHours`, default 6, at most 24), each to the next device inward: on the PC's own router
   to the PC's LAN address, and on the router in front of it to the WAN address of the router
   behind, which is the address that router sees the request come from. Before each request it
   records the mapping in a state file, `%LOCALAPPDATA%\gow2-recomp\host-online.json`, so a reply
   lost on the way back cannot hide a mapping. It reads every mapping back. If a step fails, or
   the run is interrupted (Ctrl+C included), it removes the mappings that run added, each only
   when it still carries the script's description and points where the script pointed it, and
   says so when some may still be active.
5. Stops and undoes the run when a router accepts only permanent mappings (UPnP error 725),
   unless `-AllowPermanent` is given; that router's mappings then have no lease.
6. Schedules the automatic `-Stop`: a one-time Task Scheduler task for the current user,
   `gow2 host-online stop`, one minute after the lease ends (no administrator needed). It runs
   `-Stop` without a window (`conhost.exe --headless`) and logs to
   `%LOCALAPPDATA%\gow2-recomp\host-online-stop.log`. If the PC is off or asleep at that time, it
   runs as soon as it can.
7. Prints the settings for the host and for friends on PC and Android, and when each router's
   mappings close. `-Hostname` (no default) is the host's DNS name: the script checks that it
   resolves to the public address, and prints the address instead when it does not.
   `-AllowPeers 198.51.100.20,friend2.example.net` fills in the host's `net_allow_peers`.

`-Start` again renews the leases and moves the automatic `-Stop`. `-Status` lists what the state
file records, both ports on every router, any other mapping with the script's description, and
the scheduled `-Stop`. `-PortOffset` follows a different `net_port_offset`.

Add `-Json` to any mode for a machine-readable result: the last stdout line is `GOW2HOST <compact JSON>` with the fields `mode`, `ok`, `error`, `public_address`, `peer_name`, `ports`, `lease_hours`, `lease_end`, `stop_scheduled_at`, `routers`, `open_ports`, `warnings`, `error_code`, `error_router`, `partial`, `unmanaged_router` and `unmanaged_target`. The game's in-game "Host over the internet" menu runs the script hidden with `-Json` and reads that line; every existing message stays as it is.

`-Stop` must run on the PC and under the Windows account that ran `-Start`, because it reads that
account's state file; the PC's LAN address may have changed since. It:
1. deletes every mapping the state file records, at the router and control URL recorded, if it
   still carries the script's description;
2. finds the routers again and deletes every mapping described `gow2 System Link` on each,
   whatever its port or target. A router whose WAN address cannot be read, for example while the
   internet link is down, is still searched, and so are the hops after it;
3. deletes the scheduled task.

It leaves every other mapping alone and goes on after a failed delete. It exits with code 1 when
it cannot confirm that everything is gone: a recorded router did not answer, a delete failed, or
the path could not be followed and no state file said where `-Start` had mapped. Run it again
once the routers answer; the scheduled task stays until a `-Stop` succeeds.

Run `-Stop` after every session anyway: not every router honours the lease. In a test with a
6-minute lease (`-LeaseHours 0.1`) the OpenWRT router dropped both mappings when the lease ended.
The ISP modem reported the full lease without counting it down, and `-Start` warned about it. In
the first such test the modem still listed both mappings, with lease 0, ten minutes after the
lease ended. In the second it still listed UDP 14001 when the automatic `-Stop` ran, one minute
after the lease ended, and `-Stop` deleted it. With two routers in a row the path closes once the
inner router drops its mappings, but a modem like that keeps its own until `-Stop`. [V on that
network]

It stops with a message, and maps nothing, when the first router does not answer UPnP (turn UPnP
on there, or forward by hand as below), when a router refuses changes (UPnP error 606), and when
it detects carrier-grade NAT: the outermost WAN address is in 100.64.0.0/10, or differs from the
address the internet sees. If a router's WAN address is private and the next hop does not answer
UPnP, that hop is either a second router of yours with UPnP off or the ISP's carrier-grade NAT;
the message names both. A router that refuses an AddPortMapping with any other UPnP error (501
ActionFailed, 718) stops the run too: another rule may already use the port, so forward UDP 14001
and 41000 to the address the message names by hand. [V code]

A router can answer the UPnP search (SSDP) and then not serve its UPnP description or control
URL, for example an ISP modem whose HTTP server accepts the connection and never replies. The
message then says that the router answered the search but its UPnP service did not respond, with
the reason (`timeout`), and suggests restarting that router, forwarding UDP 14001 and 41000 by
hand, or pointing its DMZ at the device just inside it: this PC for the first router, otherwise
the WAN address of the router before it. [V code]

Partial mode, `-Start` only: when a router after the first one cannot be managed (no answer to
the search, or a UPnP service that does not respond) and its address is not in 100.64.0.0/10, the
script still maps both ports on the routers inside it and succeeds with a warning. The public
address is then the one https://api.ipify.org reports (the run stops as before when it does not
answer), with no comparison against a router. The warning says that the outer router was not
changed: friends reach this PC only if it already sends UDP 14001 and 41000 to the WAN address of
the router inside it (a DMZ or a port forward), and only if it is your own router, not the ISP's
carrier-grade NAT. This fits an ISP modem whose DMZ points at your own router. Leases, the state
file, the conflict checks, the undo on failure and the scheduled `-Stop` work as in a full run;
`-Stop` and `-Status` do not change. [V code]

With `-Json` the result also carries:
- `error_code`, empty when `ok` is true: `upnp_off` (the first router answers no search),
  `upnp_unresponsive` (a router answered the search but its UPnP service did not work),
  `outer_no_upnp` (a later router answers no search and partial mode does not apply), `cgnat`,
  `refused` (UPnP error 606), `mapping_failed` (any other AddPortMapping error), `conflict` (a
  port already goes to another device or port), `no_route` (a traceroute hop did not answer, or
  too many routers) or `other`;
- `error_router`, the router the error is about, empty when it is not about one router;
- `partial`, true after a partial-mode `-Start`, with `unmanaged_router` (the router left
  unchanged) and `unmanaged_target` (where it must send the ports); both are empty otherwise.

`error` keeps the full English message. The in-game page shows a short English or Spanish text
for each code and `Error: ` with that message for `other`. [V code, unit tests]

On 2026-10-15, on a home network with two routers in a row (an OpenWRT router behind the ISP's
cable modem), it ran in both PowerShell versions: `-Start` with a 6-minute lease, whose automatic
`-Stop` fired on its own without a window and cleaned up; `-Start`, a second `-Start` (renewal),
`-Status`, and `-Stop` with the state file moved away (by description only), then `-Stop` again.
An earlier version also refused a test mapping that sent UDP 14001 to another port. Each router's
own list of mappings (GetGenericPortMappingEntry, read separately with curl) showed the four
mappings while hosting and none of them afterwards. Failures that this network cannot produce (a
lost reply, Ctrl+C, a router that does not answer, a changed LAN address or port offset, error
725, a hostile description) were run against a model of two routers. [V on that network, and in
the model]

The script opens router ports only: the Windows Firewall rule below is still needed, and it needs
an administrator.

### Is the connection reachable at all?
`host-online.ps1 -Start` makes this check itself. By hand: compare the WAN address on the
router's status page with the public address that https://api.ipify.org shows. If they differ and
the router's WAN address is private (10.x, 172.16-31.x, 192.168.x) or in 100.64.0.0/10, either
there is a second router in front of it (double NAT, below) or the ISP shares one public address
between customers (carrier-grade NAT).
Behind carrier-grade NAT no port forward can work: ask the ISP for a public address or use one of
the VPN options at the end. The game speaks IPv4 only (an XNADDR holds an IPv4 address), so IPv6
does not help.

### Ports to forward by hand
Forward exactly UDP 14001 (System Link beacon) and UDP 41000 (game traffic) to the host PC. With
another `net_port_offset` the second port is 1000 plus that offset, and every player must use the
same offset; leave the default. No TCP port is needed for campaign co-op. TCP 8675 (versus party
beacon) must stay closed because stream sockets are not filtered by `net_allow_peers`. [V code]

The game opens no port by itself: the runtime's UPnP exports are stubs
(`REX_EXPORT_STUB(__imp__NetDll_Upnp...)` in the SDK's `src/kernel/xam/xam_net.cpp`). [V code]
`host-online.ps1` does it from outside the game; forward by hand when a router has no UPnP or it
must stay off.

With one router:

| Device | Rule |
|---|---|
| Router (LAN 192.168.1.1) | UDP 14001 -> 192.168.1.10:14001, UDP 41000 -> 192.168.1.10:41000 |

With two routers in a row (an ISP modem-router, then your own router), every forward is needed
on both, each pointing at the next device inward:

| Device | Rule |
|---|---|
| Router 1, the ISP modem (LAN 192.168.0.1) | UDP 14001 and 41000 -> router 2's WAN address, for example 192.168.0.2 |
| Router 2 (WAN 192.168.0.2, LAN 192.168.1.1) | UDP 14001 and 41000 -> 192.168.1.10 |

Keep the targets fixed: reserve router 2's WAN address on router 1 and the PC's address on router
2, or the forwards break after a DHCP lease change. Alternatives to the forward on router 1 [I]:
set its DMZ host to router 2's WAN address (all unsolicited inbound traffic then reaches router 2,
whose own firewall still applies), or ask the ISP to put the modem in bridge mode so router 2 gets
the public address. Some ISPs lock parts of the modem's admin page; if the forward page is
missing, ask the ISP.

### DNS name (optional)
A literal public address works (`--net_public_address=203.0.113.10`), but home addresses are
often dynamic. A DNS name lets friends keep the same settings when it changes. In your
domain's DNS editor, add one of these records:
```
gow2   A       203.0.113.10        (TTL 300)
gow2   CNAME   <name>.duckdns.org.
```
A free dynamic DNS name such as DuckDNS follows the address when a scheduled task updates it every
few minutes (without an `ip=` parameter the service records the caller's address):
```
Invoke-RestMethod "https://www.duckdns.org/update?domains=<name>&token=<token>"
```
Check from any machine:
```
Resolve-DnsName gow2.example.com
```
The result must end in the host's current public address (https://api.ipify.org).
`host-online.ps1 -Start -Hostname gow2.example.com` checks this each time it runs.

### Game settings
On PC, pass settings on the command line. Every build copies `config/gow2.toml` over
`out\build\release\gow2.toml`, so edits to the latter are lost, and the command line outranks
`gow2.toml`. See [NETWORK_PLAY.md](NETWORK_PLAY.md), "Settings". The same settings can be set in game under PC Options > Multiplayer (restart to apply), and on Windows the in-game "Host over the internet" page runs `host-online.ps1` for you; the command line and the script still work ([PC_OPTIONS.md](PC_OPTIONS.md)). `host-online.ps1 -Start` prints
this line with the name it checked:
```
gow2.exe --net_link_active=true --net_public_address=gow2.example.com --net_allow_peers=198.51.100.20,friend2.example.net --system_link_trace=true
gow2.exe --net_link_active=true --net_public_address=gow2.example.com --net_allow_peers=198.51.100.20,friend2.example.net --system_link_trace=true --gow2_coop_players=4
```
The second line is for three or four players: every player adds `--gow2_coop_players=4` with the same value. `net_link_active` turns System Link on; it is off by default. `net_public_address` is advertised
as the online address (XNADDR inaOnline). `net_allow_peers` lists IPv4 addresses, a.b.c.d/n
ranges or DNS names that may send to the game; loopback and the local network always may. With
`net_public_address` set and `net_allow_peers` empty, only the local network and `net_peers` get
in: friends are dropped and the log warns
`[netlink] net_public_address is set but net_allow_peers is empty: ...` (fail closed).
`system_link_trace` logs network activity. Leave `net_port_offset` at 40000 on every player.
[V code]

Every player also sets a gamertag with `--user_name=<name>` (up to 15 characters; the default is
`User`). With `net_link_active` on, each install makes a random XUID once and keeps it in
`system_link_xuid.txt` in its default user data folder. Do not copy that file between installs:
two players with the same XUID would look like one player to the game. [V code for the file, I
for the effect] A `gow2_save_dir` that several PCs sync ([SAVES.md](SAVES.md)) does not carry it:
the file stays on each PC and the game passes it as `user_xuid`. [V code]

Friends find their public IPv4 at https://api.ipify.org. If a friend's address changes, use their
dynamic DNS name. An ISP range a.b.c.d/n also works, but lets in many more strangers.

Names resolve at start and every five minutes. A name lets nothing in until its first lookup
succeeds; when a lookup fails, the last good addresses stay in use. The online address enters the
XNADDR only after the host's own name resolves. Open the lobby only after the host log shows:
```
[netlink] resolved gow2.example.com -> 203.0.113.10
```
On the host, look for these lines:
```
[netlink] link active: local 192.168.1.10/24 (default-route adapter), bind 0.0.0.0, ...
[netlink] net_peers '', net_public_address 'gow2.example.com', net_allow_peers '...'
[netlink] first datagram from <friend-ip>:<port> on local port 14001 (<n> bytes)
[netlink] first datagram from <friend-ip>:<port> on local port 41000 (<n> bytes)
[netlink] dropped <n> bytes from <ip>:<port> (net_allow_peers)
```
The last line means a sender is not on the allow list. The first-datagram and dropped lines
appear only with `system_link_trace=true`. [V code]

### Bandwidth [V, loopback, scripted play]

Per-client peak 5.3 KB/s (76 % of the game's 7000 B/s per-client cap,
MaxDynamicBandwidth), host upload peak 16 KB/s with 3 clients. The host needs
at most about 3 x 7000 B/s, about 170 kbit/s, of upload. Run the host with
`--net_traffic_log_sec=10 --system_link_trace=true` for the session:
`[nettraffic]` windows pinned near 7000 B/s mean the cap binds (a Stage 3
change).

### Windows Firewall (needs an administrator)
The first time a copy of `gow2.exe` listens, Windows asks whether to allow it. Allowing needs an
administrator; pressing Cancel as a standard user creates a Block rule for that exact path, and a
Block rule beats every Allow rule. Each build folder is a different program to the firewall.

Run these commands in an elevated PowerShell prompt. Replace `<exe>` with the full path to the
copy that hosts:
```
Get-NetFirewallApplicationFilter -Program '<exe>' | Get-NetFirewallRule | Where-Object Action -eq 'Block' | Remove-NetFirewallRule
netsh advfirewall firewall add rule name="gow2 System Link" dir=in action=allow program="<exe>" protocol=UDP localport=14001,41000 profile=public,private
```
A narrower option is to append `remoteip=LocalSubnet,198.51.100.20,198.51.100.30` so Windows also
drops everyone else. Disable the rule after the session and enable it again next time:
```
netsh advfirewall firewall set rule name="gow2 System Link" new enable=no
netsh advfirewall firewall set rule name="gow2 System Link" new enable=yes
```
A rule that allows every port for the executable (what clicking Allow creates) leaves the router
forwards as the only boundary: forward nothing beyond the two UDP ports.

## Friends' setup
Every friend needs the host's build and their own copy of the game's files: this project ships no
game data. Friends name the host the way its `host-online.ps1 -Start` printed it; the lines
below use `gow2.example.com`.

### PC
Use the same release as the host, v0.1.2. It contains the Stage 2 co-op work and the codegen fix
d074c0d. With 3 or more players the beacon gate hides campaigns whose co-op hash differs; with 2 players there is still no check, and a v0.1.1 peer is expected to crash about 0.5 s after the chapter 2 -> 3 map commit even in two-player co-op [I]. For three or four players add `--gow2_coop_players=4` (every player sets the same value).
```
gow2.exe --net_link_active=true --net_peers=gow2.example.com --user_name=<name>
gow2.exe --net_link_active=true --net_peers=gow2.example.com --user_name=<name> --gow2_coop_players=4
```
Then choose CO-OP CAMPAIGN -> JOIN CO-OP CAMPAIGN to open the System Link browser; X (E on the
keyboard with `--mnk_mode=true`) refreshes. See [NETWORK_PLAY.md](NETWORK_PLAY.md) for the menu
path. No port forward or firewall rule is needed on a friend's side: answers come back on the
flows the friend opened. [I] With `net_peers` set, the friend's game accepts only its own local
network and the resolved host address. [V code]

### Android
Use the same APK build, v0.1.2. Put a `gow2.toml` in
`/sdcard/Android/data/com.santiquiroz.gow2recomp/files/` with these lines:
```
net_link_active = true
net_peers = "gow2.example.com"
user_name = "<name>"
gow2_coop_players = 4   # only for three or four players; every player sets the same value
```
Copy it over USB, or push it from Git Bash (without `MSYS_NO_PATHCONV=1`, Git Bash rewrites the
`/sdcard/...` path):
```
MSYS_NO_PATHCONV=1 adb push gow2.toml /sdcard/Android/data/com.santiquiroz.gow2recomp/files/gow2.toml
```
With patch 0011 the phone binds the game's port 1000 as 41000, as the PC does
([android/README.md](../android/README.md)). [V code]

Wi-Fi or mobile data can join (mobile data verified on 2026-10-15, see "Tested over the
internet"). A phone on mobile data cannot host: carriers put phones behind
carrier-grade NAT. A phone on the host's home Wi-Fi needs no `net_peers` to join the PC over the
LAN; see [NETWORK_PLAY.md](NETWORK_PLAY.md).

Hosting on a phone at home is possible in principle [I]: point the forwards at the phone (with a
reserved address) instead of the PC, and put `net_public_address` and `net_allow_peers` in the
phone's `gow2.toml`. Android shows no inbound firewall prompt. Untested.

## Test before inviting friends
1. Run `scripts\host-online.ps1 -Start` and put the firewall rule in place. When you forward by
   hand instead, also confirm that `Resolve-DnsName gow2.example.com` returns the public address.
2. On a phone, turn Wi-Fi off and mobile data on. Open https://api.ipify.org and note the phone's
   address.
3. Start the PC host with the command from "Game settings" and
   `--net_allow_peers=<phone's mobile address>`.
4. Put the Android `gow2.toml` from the Android subsection in place, then join the host.
5. Check the host log for `first datagram from <phone's mobile address>:<port> on local port
   14001`, then the same on local port 41000.
6. Close the game and run `scripts\host-online.ps1 -Stop`.

For three or four players, before the session every player starts the game once and checks the
log for `SCRIPTPATCH stack: ... combined=e52feaafc39b3f26` (four players, default co-op settings)
and no `skipped` line; the four `combined=` values must match. The full list is in
[NETWORK_PLAY.md](NETWORK_PLAY.md), "Pre-session check".

Do not test the internet path from a device on the home Wi-Fi: from inside, the public name leads
back through the router (hairpin NAT), which proves nothing about access from outside. Carriers
can change a phone's public address between connections. If the log shows
`dropped ... (net_allow_peers)` from a new address, update the allow list.

## Tested over the internet (2026-10-15)
A phone on mobile data joined a campaign hosted by a PC at home, following the steps above.
[V on that network]

- Host: the PC on Ethernet behind two routers in a row (an OpenWRT router behind the ISP's cable
  modem), a build of master with patch 0011, from the path that has the Windows Firewall allow
  rule (the network profile is Public). `host-online.ps1 -Start -Hostname <host name> -AllowPeers
  <a.b.c>.0/24` mapped UDP 14001 and 41000 on both routers, where `<a.b.c>.0/24` is the /24 around
  the phone's public address. The game ran with `--net_link_active=true
  --net_public_address=<host name> --net_allow_peers=<a.b.c>.0/24 --system_link_trace=true`.
- Friend: a Galaxy S25 Ultra with Wi-Fi off. On mobile data the carrier gave it a private 10.x
  address (carrier-grade NAT) and one public IPv4 address, the same for HTTPS and for the game's
  UDP. Its `gow2.toml` had `net_link_active = true` and `net_peers = "<host name>"`.
- The phone's System Link browser listed the PC's campaign (`Pc`), both lobbies listed both
  players, both loaded Act 1 chapter 2, the phone spawned as Dom, and each screen showed the other
  player.
- Host log: `first datagram from <phone's public address>:<port> on local port 14001 (16 bytes)`
  (the phone's query), the beacon answer sent back to that address and port, then `first datagram
  from <phone's public address>:<port> on local port 41000 (55 bytes)` and
  `XGISessionJoinRemote ... members=2`.
- Phone log: `first datagram from <host's public address>:14001 on local port 14001 (188 bytes)`
  (the beacon answer), `XNetXnAddrToInAddr ... ina=<host's LAN address> inaOnline=<host's public
  address> -> <host's public address>`, then game traffic to and from `<host's public
  address>:41000`. The host's first answer on port 41000 arrived about 160 ms after the phone's
  first datagram.
- The carrier's NAT gave the phone's sockets other port numbers than their own (local 14001
  became a five-digit port), and the host's answers to those ports came through. So a phone on
  mobile data can join; it still cannot host, because nothing can reach it first.
- Afterwards `host-online.ps1 -Stop` deleted the four mappings and the scheduled task, and each
  router's own list of mappings was back to what it held before.

## Security trade-offs
- [I] System Link traffic is not encrypted or authenticated. The code that parses it was written
  in 2008 for a LAN, recompiled as is, and never reviewed for hostile input. A crafted datagram
  could crash the game or worse.
- [V code] `net_allow_peers` is the gate: a datagram from an unlisted address is dropped before
  any byte reaches the game. Keep the list to friends' addresses; avoid wide ranges.
- UDP source addresses can be forged. The allow list narrows who can reach the parser; it is not
  authentication.
- [V code] Reflection is capped: unicast copies of a beacon broadcast go only to senders heard in
  the last second on the beacon socket, at most 4 per broadcast and 2 per second per address. The
  host cannot be used as a traffic amplifier.
- TCP is not filtered. Forward no TCP port.
- Host only while playing: run `host-online.ps1 -Start` (or enable the two forwards, or the DMZ)
  and the firewall rule for the session; after it, close `gow2.exe`, run `-Stop` (or disable the
  forwards) and disable the rule. Do not rely on the lease alone (see the script's section). Nothing
  else needs to be exposed for the game.
- UPnP lets any program on the LAN open router ports, which is why some people turn it off. The
  script maps only the two UDP ports, with a lease and an automatic `-Stop`, and `-Status` shows
  whether anything else holds them. `-Stop` deletes every mapping described `gow2 System Link`
  on the routers it reaches, including one that another PC in the house made with this script.
- Everyone runs the same build. An older or different build can misread the traffic.

## Safer alternatives with no open ports
Use one of these when `host-online.ps1` reports carrier-grade NAT, when a router in the path has
no UPnP and you cannot forward on it, or when you would rather expose nothing.

### Tailscale
Tailscale is a WireGuard mesh with a free personal plan and apps for Windows and Android.
Installing on Windows needs an administrator once. Everyone joins the host's tailnet by invitation
or by sharing the host machine. Peers authenticate through Tailscale; nothing is exposed to the
internet.

Tailscale carries unicast only, not broadcasts, so `net_peers` is required. The host must
advertise its Tailscale address because friends cannot reach its LAN address:
```
host:    gow2.exe --net_link_active=true --net_public_address=<host-100.x.y.z> --net_allow_peers=100.64.0.0/10
friends: gow2.exe --net_link_active=true --net_peers=<host-100.x.y.z>
```
100.64.0.0/10 covers every Tailscale address; Tailscale already decides who is in the tailnet. A
MagicDNS name works in place of the address. On Android, install the Tailscale app, then use this
in `gow2.toml`:
```
net_link_active = true
net_peers = "<host-100.x.y.z>"
```
The app keeps advertising the phone's Wi-Fi address: it ranks wlan0 first and never picks a VPN
interface. For a client, that is harmless. [I] The host's inbound firewall rule ("Windows
Firewall" above) is still needed. When no direct path exists, Tailscale relays traffic with more
latency. [I]

### ZeroTier
ZeroTier is a virtual Ethernet network with a free plan. It passes broadcasts (enabled per network
by default), so System Link discovery can work as on a LAN.

On Windows, the game broadcasts to its local subnet and to 255.255.255.255. Windows sends
255.255.255.255 out of one adapter only (see [NETWORK_PLAY.md](NETWORK_PLAY.md)). Set each PC's
ZeroTier address as its local address:
```
host:    gow2.exe --net_link_active=true --net_local_ip=<host-zerotier-ip>/<prefix>
friends: gow2.exe --net_link_active=true --net_local_ip=<own-zerotier-ip>/<prefix> --net_peers=<host-zerotier-ip>
```
The advertised address, the local network that the allow list always admits, and the directed
broadcast then all use the ZeroTier network, so `net_allow_peers` is not needed. `net_peers` on PC
clients is optional but costs nothing. On Android, install the ZeroTier One app and use this
`gow2.toml`:
```
net_link_active = true
net_local_ip = "<phone-zerotier-ip>/<prefix>"
net_peers = "<host-zerotier-ip>"
```
Broadcasts through the Android VPN app were not tested, hence `net_peers`. [I] Everyone in the
session, including a phone on the host's Wi-Fi, must be on the ZeroTier network. A host
advertising its ZeroTier address is unreachable for devices outside it.

### Comparison
| Option | Open ports | Admin on host | Broadcasts | Friends need |
|---|---|---|---|---|
| UPnP (`host-online.ps1`) | UDP 14001 + 41000 on every router in the path, until `-Stop` (automatic at the lease end) | Firewall rule | n/a | `net_peers=<host name>` |
| Port forwarding by hand | UDP 14001 + 41000 on every router in the path | Router pages + firewall rule | n/a | `net_peers=<host name>` |
| Tailscale | None | Installer + firewall rule | No | Tailscale + `net_peers` |
| ZeroTier | None | Installer + firewall rule | Yes | ZeroTier + `net_local_ip`, `net_peers` |

## Not verified yet
- One internet session so far: a phone on mobile data that joined a PC host through the UPnP
  mappings ("Tested over the internet"). A PC friend outside the network, forwards made by hand,
  the DMZ alternative, a host on a single router, and three or more players over the internet
  have not been tried.
- PC and phone on one LAN joined each other's campaign on 2026-10-15 with the crossplay build
  ([NETWORK_PLAY.md](NETWORK_PLAY.md)); the merged build repeated the PC-hosted case (the phone
  joined) and two copies on one PC over loopback.
- Tailscale and ZeroTier have not been tried.
- A remote NAT that maps a friend's port to an external port below 1024 would be remapped wrongly.
  This is rare.
- There is no version gate for two players; with 3+ players the beacon gate hides mismatched campaigns.
- Stage 2 ran four players only on one PC over loopback; four PCs over the internet, Android with 3+ players and a launch with four in the lobby on v0.1.2 are not tried.
