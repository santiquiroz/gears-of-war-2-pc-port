# Opens the System Link ports on every UPnP router between this PC and the internet, with a lease,
# and closes them again. Usage: docs/ONLINE.md, "Host setup". Runs in Windows PowerShell 5.1 and pwsh.
#   powershell -ExecutionPolicy Bypass -File scripts\host-online.ps1 -Start [-LeaseHours 6] [-Hostname gow2.example.com] [-AllowPeers a,b] [-AllowPermanent] [-Players 4] [-Json]
#   powershell -ExecutionPolicy Bypass -File scripts\host-online.ps1 -Status [-Json]
#   powershell -ExecutionPolicy Bypass -File scripts\host-online.ps1 -Stop [-Json]
# With -Json, the last stdout line is "GOW2HOST <compact JSON>" with mode, ok, error, public_address,
# peer_name, ports, lease_hours, lease_end, stop_scheduled_at, routers, open_ports, warnings,
# error_code, error_router, partial, unmanaged_router and unmanaged_target.
# error_code is '' when ok: upnp_off, upnp_unresponsive, outer_no_upnp, cgnat, refused,
# mapping_failed, conflict, no_route or other. error_router names the router at fault or ''.
# The game's in-game "Host over the internet" menu runs the script hidden with -Json and reads that line.
param(
    [switch]$Start,
    [switch]$Stop,
    [switch]$Status,
    [ValidateRange(0.1, 24)][double]$LeaseHours = 6,
    [string]$Hostname = '',
    [string[]]$AllowPeers = @(),
    [ValidateRange(1024, 64512)][int]$PortOffset = 40000,
    [ValidateRange(2, 255)][int]$Players = 2,
    [switch]$AllowPermanent,
    [string]$LogFile = '',
    [switch]$Json
)

$ErrorActionPreference = 'Stop'

$MappingDescription = 'gow2 System Link'
$BeaconPort = 14001
$GamePortBase = 1000
$DefaultPortOffset = 40000
$SsdpGroup = '239.255.255.250'
$SsdpPort = 1900
$SsdpWaitMs = 2500
$TraceTarget = '1.1.1.1'
$PublicAddressService = 'https://api.ipify.org'
$MaxRouters = 4
$MaxGenericEntries = 256
$MaxResponseBytes = 256KB
$LeaseProbeDelayMs = 2500
$IgdSearchTargets = @(
    'urn:schemas-upnp-org:device:InternetGatewayDevice:2',
    'urn:schemas-upnp-org:device:InternetGatewayDevice:1'
)
$WanServiceTypes = @(
    'urn:schemas-upnp-org:service:WANIPConnection:2',
    'urn:schemas-upnp-org:service:WANIPConnection:1',
    'urn:schemas-upnp-org:service:WANPPPConnection:1'
)
$UpnpNotAuthorized = 606
$UpnpInvalidArrayIndex = 713
$UpnpNoSuchEntry = 714
$UpnpConflictInMapping = 718
$UpnpOnlyPermanentLeases = 725
$AndroidConfigDir = '/sdcard/Android/data/com.santiquiroz.gow2recomp/files/'
$DottedQuadPattern = '^((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)\.){3}(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)$'
$ScriptPath = $PSCommandPath
$StateDir = Join-Path $env:LOCALAPPDATA 'gow2-recomp'
$StatePath = Join-Path $StateDir 'host-online.json'
$StopLogPath = Join-Path $StateDir 'host-online-stop.log'
$StopTaskName = 'gow2 host-online stop'
$StopTaskDelayMinutes = 1
$StopTaskExpiryDays = 7
$PermanentRouters = @{}

. (Join-Path $PSScriptRoot 'host-online-upnp.ps1')
. (Join-Path $PSScriptRoot 'host-online-state.ps1')

function Write-Section([string]$Title) { Write-Host ''; Write-Host "== $Title" -ForegroundColor Cyan }
function Write-Note([string]$Text) { Write-Host "  $Text" }
$script:JsonWarnings = New-Object Collections.Generic.List[string]
function Write-Warn([string]$Text) {
    if ($null -eq $script:JsonWarnings) { $script:JsonWarnings = New-Object Collections.Generic.List[string] }
    $script:JsonWarnings.Add($Text)
    Write-Host "  WARNING: $Text" -ForegroundColor Yellow
}

function Select-Mode([bool]$IsStart, [bool]$IsStop, [bool]$IsStatus) {
    $chosen = @($IsStart, $IsStop, $IsStatus | Where-Object { $_ }).Count
    if ($chosen -gt 1) { throw 'Choose one of -Start, -Stop or -Status.' }
    if ($IsStart) { return 'Start' }
    if ($IsStop) { return 'Stop' }
    'Status'
}

function Get-SystemLinkPorts([int]$Offset) { @($BeaconPort, ($GamePortBase + $Offset)) }

function Get-RouterExternal($Igd, [bool]$Tolerant) {
    if (-not $Tolerant) { return Get-ExternalAddress $Igd }
    try { Get-ExternalAddress $Igd }
    catch { Write-Warn "$($_.Exception.Message) Going on with its WAN address unknown."; '' }
}

function Get-NextHopMismatch([string]$PreviousExternal, [string]$HopAddress) {
    $block = Get-PrivateBlock $PreviousExternal
    if (-not $block) { return "$PreviousExternal is not a private address" }
    if ($PreviousExternal -eq $HopAddress) { return "$PreviousExternal is the next hop's own address" }
    if ((Test-DottedQuad $HopAddress) -and (Get-PrivateBlock $HopAddress) -eq $block) { return $null }
    "the next hop $HopAddress is outside $block"
}

function Get-NextClient([string]$Client, [string]$HopAddress, [int]$Hop, [bool]$Tolerant) {
    if ($Hop -eq 1 -or -not $Client) { return $Client }
    $mismatch = Get-NextHopMismatch $Client $HopAddress
    if (-not $mismatch) { return $Client }
    $message = "Router $($Hop - 1) reports the WAN address $Client, but $mismatch, so its mappings on hop $Hop could not point at it."
    if (-not $Tolerant) { throw "$message Refusing to map." }
    Write-Warn $message
    ''
}

function New-Router([int]$Hop, $Igd, [string]$InternalClient, [string]$ExternalAddress) {
    [pscustomobject]@{ Hop = $Hop; Address = $Igd.Address; Igd = $Igd; InternalClient = $InternalClient; ExternalAddress = $ExternalAddress }
}

function New-RouterPath($Routers, [string]$End, [string]$EndAddress, [int]$EndHop) {
    # PowerShell 5.1 throws 'argument types do not match' on @() of an empty generic List here
    [pscustomobject]@{ Routers = $Routers.ToArray(); End = $End; EndAddress = $EndAddress; EndHop = $EndHop }
}

function Get-PathEnd([string]$ExternalAddress) {
    if (-not $ExternalAddress) { return $null }
    if (Test-SharedIPv4 $ExternalAddress) { return 'shared' }
    if (Test-PrivateIPv4 $ExternalAddress) { return $null }
    'public'
}

function Get-RouterPath([string]$LocalAddress, [bool]$Tolerant) {
    $routers = New-Object Collections.Generic.List[object]
    if (Test-PublicIPv4 $LocalAddress) { return New-RouterPath $routers 'direct' $LocalAddress 0 }
    $lanReplies = @(Search-Igds $LocalAddress $SsdpGroup)
    $client = $LocalAddress
    for ($hop = 1; $hop -le $MaxRouters; $hop++) {
        $address = Get-HopAddress $hop
        if (-not $address) { return New-RouterPath $routers 'no-hop' $null $hop }
        $client = Get-NextClient $client $address $hop $Tolerant
        $igd = Find-RouterIgd $address $LocalAddress $lanReplies
        if (-not $igd) { return New-RouterPath $routers 'no-igd' $address $hop }
        $external = Get-RouterExternal $igd $Tolerant
        $routers.Add((New-Router $hop $igd $client $external))
        $end = Get-PathEnd $external
        if ($end) { return New-RouterPath $routers $end $external $hop }
        $client = $external
    }
    New-RouterPath $routers 'too-deep' $null $MaxRouters
}

function Test-IncompletePath($Path) { @('no-hop', 'no-igd', 'too-deep') -contains $Path.End }

function Get-ServiceName([string]$ServiceType) { ($ServiceType -split ':')[-2..-1] -join ':' }

function Format-Known([string]$Value) { if ($Value) { return $Value } 'unknown' }

function Show-Router($Router) {
    $igd = $Router.Igd
    Write-Note ("Router {0}: {1} ({2}; {3}), WAN address {4}" -f $Router.Hop, $Router.Address, $igd.Server, (Get-ServiceName $igd.ServiceType), (Format-Known $Router.ExternalAddress))
    Write-Note ("  its mappings point to {0}; control URL {1}" -f (Format-Known $Router.InternalClient), $igd.ControlUrl)
}

function Test-SharedOuterHop($Path) { $Path.EndHop -ge 2 -and (Test-SharedIPv4 $Path.EndAddress) }

function Test-UnresponsiveEnd($Path) { $Path.End -eq 'no-igd' -and $script:UnresponsiveIgds.ContainsKey([string]$Path.EndAddress) }

function Get-ForwardTarget($Path) {
    if ($Path.EndHop -eq 1) { return Get-LocalIPv4 }
    Format-Known $Path.Routers[-1].ExternalAddress
}

function Get-UnresponsiveMessage($Path) {
    $target = Get-ForwardTarget $Path
    $reason = $script:UnresponsiveIgds[$Path.EndAddress]
    "The router at $($Path.EndAddress) (hop $($Path.EndHop)) answered the UPnP search but its UPnP service did not respond ($reason). Restart that router, or forward UDP $($script:Ports -join ' and ') to $target by hand, or point its DMZ at $target (docs/ONLINE.md)."
}

function Get-NoIgdMessage($Path) {
    $address = $Path.EndAddress
    if (Test-SharedOuterHop $Path) { return Get-CgnatMessage "the next hop $address is in 100.64.0.0/10" }
    if (Test-UnresponsiveEnd $Path) { return Get-UnresponsiveMessage $Path }
    if ($Path.EndHop -eq 1) {
        return "Your router $address answers no UPnP search: UPnP (IGD) is off or blocked. Turn it on in the router's settings, or forward UDP $($script:Ports -join ' and ') by hand (docs/ONLINE.md)."
    }
    $inner = $Path.Routers[-1]
    "The router at $address (hop $($Path.EndHop)) answers no UPnP search. The router before it has the private WAN address $(Format-Known $inner.ExternalAddress), so $address is either a second router of yours with UPnP off (turn UPnP on, or forward UDP $($script:Ports -join ' and ') to $(Format-Known $inner.ExternalAddress) by hand) or the ISP's carrier-grade NAT (then no forward can work)."
}

function Get-CgnatMessage([string]$Evidence) {
    "Carrier-grade NAT detected ($Evidence): the ISP shares one public address between customers, so no port mapping can reach this PC. Ask the ISP for a public IPv4 address, or use Tailscale or ZeroTier (docs/ONLINE.md)."
}

function Get-PathEndMessage($Path) {
    switch ($Path.End) {
        'public' { return $null }
        'direct' { return $null }
        'shared' { return Get-CgnatMessage "the outermost router's WAN address $($Path.EndAddress) is in 100.64.0.0/10" }
        'no-igd' { return Get-NoIgdMessage $Path }
        'no-hop' { return "Hop $($Path.EndHop) toward the internet did not answer a traceroute probe; the router path could not be followed." }
        default { return "More than $MaxRouters routers in a row have private WAN addresses; this script stops there." }
    }
}

function Show-Path($Path) {
    Write-Section 'Routers between this PC and the internet'
    if ($Path.End -eq 'direct') { Write-Note "This PC has the public address $($Path.EndAddress): no router to map." }
    foreach ($router in $Path.Routers) { Show-Router $router }
    $message = Get-PathEndMessage $Path
    if ($message) { Write-Warn $message }
}

function Get-InternetIPv4 {
    try { $address = ([string](Invoke-RestMethod -Uri $PublicAddressService -TimeoutSec 10 -MaximumRedirection 0 -UseBasicParsing)).Trim() }
    catch { return $null }
    if (Test-DottedQuad $address) { return $address }
    $null
}

function Get-OutermostAddress($Path) {
    if ($Path.End -eq 'direct') { return $Path.EndAddress }
    $Path.Routers[-1].ExternalAddress
}

function Get-PathEndErrorCode($Path) {
    if ($Path.End -eq 'shared') { return 'cgnat' }
    if (@('no-hop', 'too-deep') -contains $Path.End) { return 'no_route' }
    if ($Path.End -ne 'no-igd') { return 'other' }
    if (Test-SharedOuterHop $Path) { return 'cgnat' }
    if (Test-UnresponsiveEnd $Path) { return 'upnp_unresponsive' }
    if ($Path.EndHop -eq 1) { return 'upnp_off' }
    'outer_no_upnp'
}

function Assert-PathComplete($Path) {
    $message = Get-PathEndMessage $Path
    if (-not $message) { return }
    $code = Get-PathEndErrorCode $Path
    $router = if (@('upnp_off', 'upnp_unresponsive', 'outer_no_upnp') -contains $code) { $Path.EndAddress } else { '' }
    throw (Set-HostError $code $router $message)
}

function Test-UnmanagedOuterRouter($Path) {
    $Path.End -eq 'no-igd' -and $Path.EndHop -ge 2 -and @($Path.Routers).Count -gt 0 -and -not (Test-SharedOuterHop $Path)
}

function Use-UnmanagedOuterRouter($Path) {
    $internetView = Get-InternetIPv4
    if (-not $internetView) { Assert-PathComplete $Path }
    Write-Section 'Public address'
    Write-Note "$PublicAddressService sees this PC as: $internetView"
    $target = [string]$Path.Routers[-1].ExternalAddress
    Write-Warn "The router at $($Path.EndAddress) (hop $($Path.EndHop)) was not changed: friends reach this PC only if it already sends UDP $($script:Ports -join ' and ') to $(Format-Known $target) (a DMZ or a port forward), and only if it is your own router, not the ISP's carrier-grade NAT."
    $script:JsonResult['partial'] = $true
    $script:JsonResult['unmanaged_router'] = $Path.EndAddress
    $script:JsonResult['unmanaged_target'] = $target
    $internetView
}

function Assert-ReachableFromInternet($Path) {
    if (Test-UnmanagedOuterRouter $Path) { return Use-UnmanagedOuterRouter $Path }
    Assert-PathComplete $Path
    $routerView = Get-OutermostAddress $Path
    $internetView = Get-InternetIPv4
    Write-Section 'Public address'
    Write-Note "Outermost router's WAN address: $routerView"
    if (-not $internetView) { Write-Warn "$PublicAddressService did not answer; the address was not cross-checked."; return $routerView }
    Write-Note "$PublicAddressService sees this PC as: $internetView"
    if ($internetView -ne $routerView) {
        throw (Set-HostError 'cgnat' '' (Get-CgnatMessage "the outermost router reports $routerView but the internet sees $internetView; a VPN or proxy on this PC gives the same symptom"))
    }
    $routerView
}

function Get-MappingVerdict($Router, $Mapping, [int]$Port) {
    if (-not $Mapping) { return 'missing' }
    $isOurDescription = $Mapping.Description -eq $MappingDescription
    if (-not $Router.InternalClient) { if ($isOurDescription) { return 'ours' } else { return 'conflict' } }
    $pointsHere = ($Mapping.InternalClient -eq $Router.InternalClient) -and ($Mapping.InternalPort -eq $Port)
    if ($pointsHere -and $isOurDescription) { return 'ours' }
    if ($pointsHere) { return 'foreign' }
    if ($isOurDescription) { return 'ours-elsewhere' }
    'conflict'
}

function Format-Lease([int]$Seconds) {
    if ($Seconds -le 0) { return 'no lease (stays until deleted or the router restarts)' }
    $span = [TimeSpan]::FromSeconds($Seconds)
    '{0}h{1:00}m left' -f [int][Math]::Floor($span.TotalHours), $span.Minutes
}

function Format-Mapping($Router, $Mapping) {
    "{0} {1} on {2} -> {3}:{4} '{5}', enabled {6}, {7}" -f $Mapping.Protocol, $Mapping.Port, $Router.Address, $Mapping.InternalClient, $Mapping.InternalPort, $Mapping.Description, $Mapping.Enabled, (Format-Lease $Mapping.LeaseSeconds)
}

function Get-MappingPlan($Path, $Ports) {
    foreach ($router in $Path.Routers) {
        foreach ($port in $Ports) {
            $mapping = Get-PortMapping $router $port
            [pscustomobject]@{ Router = $router; Port = $port; Mapping = $mapping; Verdict = (Get-MappingVerdict $router $mapping $port) }
        }
    }
}

function Assert-NoConflicts($Plan) {
    $taken = @($Plan | Where-Object { $_.Verdict -eq 'conflict' -or $_.Verdict -eq 'ours-elsewhere' })
    if (-not $taken) { return }
    Write-Section 'Ports already taken'
    foreach ($item in $taken) { Write-Warn ("taken " + (Format-Mapping $item.Router $item.Mapping)) }
    $conflicts = @($taken | Where-Object { $_.Verdict -eq 'conflict' })
    if ($conflicts) {
        throw (Set-HostError 'conflict' $conflicts[0].Router.Address "Refusing to map: the ports above already go to another device or port. Free them on the router (or stop the program that made them), then run -Start again.")
    }
    throw (Set-HostError 'conflict' $taken[0].Router.Address "Refusing to map: the ports above carry this script's description but point elsewhere: an earlier -Start from another LAN address, or another PC that hosts. Run -Stop on that PC, or -Stop here (it deletes every mapping with this description), then -Start again.")
}

function Request-Mapping($Router, [int]$Port, [int]$LeaseSeconds) {
    $arguments = [ordered]@{
        NewRemoteHost = ''; NewExternalPort = $Port; NewProtocol = 'UDP'; NewInternalPort = $Port
        NewInternalClient = $Router.InternalClient; NewEnabled = 1; NewPortMappingDescription = $MappingDescription
        NewLeaseDuration = $LeaseSeconds
    }
    $result = Invoke-UpnpAction $Router.Igd 'AddPortMapping' $arguments
    if ($result.Code -ne $UpnpOnlyPermanentLeases) { return $result }
    if (-not $AllowPermanent) {
        throw "$($Router.Address) accepts only permanent mappings (UPnP error 725): UDP $Port would stay open until -Stop or a router restart. Run -Start -AllowPermanent to accept that; the automatic -Stop at the end of the lease then closes it."
    }
    $PermanentRouters[$Router.Address] = $true
    Write-Warn "$($Router.Address) accepts only permanent mappings: UDP $Port stays open until -Stop (-AllowPermanent)."
    $arguments['NewLeaseDuration'] = 0
    Invoke-UpnpAction $Router.Igd 'AddPortMapping' $arguments
}

function Add-MappingWithLease($Router, [int]$Port, [int]$LeaseSeconds, [bool]$IsOurs) {
    $result = Request-Mapping $Router $Port $LeaseSeconds
    if ($result.Code -eq $UpnpConflictInMapping -and $IsOurs) {
        # MiniUPnPd 1.x answers 718 even when the same client adds its own mapping again
        Remove-PortMappingEntry $Router $Port 'UDP' ''
        $result = Request-Mapping $Router $Port $LeaseSeconds
    }
    Assert-MappingSuccess $Router $Port $result
}

function Assert-MappingSuccess($Router, [int]$Port, $Result) {
    if ($Result.Code -eq 0) { return }
    if ($Result.Code -eq $UpnpNotAuthorized) { Assert-UpnpSuccess $Result "Mapping UDP $Port on $($Router.Address) to $($Router.InternalClient)" $Router.Address }
    $message = "Router $($Router.Address) refused the mapping (UPnP $($Result.Code) $($Result.Description)): another rule may already use the port; forward UDP $($script:Ports -join ' and ') to $(Format-Known $Router.InternalClient) by hand."
    throw (Set-HostError 'mapping_failed' $Router.Address $message)
}

function Confirm-Mapping($Router, [int]$Port, [string]$Label) {
    $mapping = Get-PortMapping $Router $Port
    if ((Get-MappingVerdict $Router $mapping $Port) -ne 'ours') {
        throw "$($Router.Address) accepted UDP $Port but does not list it for $($Router.InternalClient) afterwards."
    }
    Write-Note ($Label.PadRight(9) + (Format-Mapping $Router $mapping))
}

function ConvertTo-RecordedRouter($Entry) {
    if (-not (Test-UrlOnHost $Entry.ControlUrl $Entry.Router)) { throw "The state file names the control URL $($Entry.ControlUrl), which is not plain http on $($Entry.Router)." }
    $igd = [pscustomobject]@{ Address = $Entry.Router; Server = ''; Location = ''; ServiceType = $Entry.ServiceType; ControlUrl = $Entry.ControlUrl }
    New-Router 0 $igd $Entry.InternalClient ''
}

function Undo-Mapping($Router, [int]$Port) {
    try {
        $mapping = Get-PortMapping $Router $Port
        $verdict = Get-MappingVerdict $Router $mapping $Port
        if ($verdict -eq 'ours') { Remove-PortMapping $Router $mapping }
        elseif ($verdict -eq 'missing') { Write-Note "none     UDP $Port on $($Router.Address)" }
        else { Write-Note ("left     " + (Format-Mapping $Router $mapping) + " (not this run's)") }
        Remove-StateEntry (New-StateEntry $Router $Port)
    } catch { Write-Warn "Could not undo UDP $Port on $($Router.Address): $($_.Exception.Message) It stays in the state file for -Stop." }
}

function Undo-StartRun($Created) {
    Write-Warn '-Start did not finish: removing the mappings this run created.'
    foreach ($item in $Created) { Undo-Mapping $item.Router $item.Port }
    if (@(Read-HostState).Count) { Write-Warn "Mappings may still be active (state file $StatePath): run -Stop."; return }
    Unregister-StopTask | Out-Null
}

function Add-PlannedMappings($Plan, [int]$LeaseSeconds, $Created) {
    foreach ($item in $Plan) {
        if ($item.Verdict -eq 'foreign') { Write-Note ("kept     " + (Format-Mapping $item.Router $item.Mapping) + " (made by another program; -Stop leaves it)"); continue }
        $isOurs = $item.Verdict -eq 'ours'
        Add-StateEntry (New-StateEntry $item.Router $item.Port)  # before the request: a lost reply must not hide a mapping
        if (-not $isOurs) { $Created.Add($item) }
        Add-MappingWithLease $item.Router $item.Port $LeaseSeconds $isOurs
        Confirm-Mapping $item.Router $item.Port $(if ($isOurs) { 'renewed' } else { 'mapped' })
    }
}

function Test-LeaseCountsDown($Item, [int]$LeaseSeconds) {
    $mapping = Get-PortMapping $Item.Router $Item.Port
    (-not $mapping) -or (($mapping.LeaseSeconds -gt 0) -and ($mapping.LeaseSeconds -lt $LeaseSeconds))
}

function Find-StaticLeaseRouters($Plan, [int]$LeaseSeconds) {
    $probes = @($Plan | Where-Object { $_.Verdict -ne 'foreign' } | Group-Object { $_.Router.Address } | ForEach-Object { $_.Group[0] })
    if (-not $probes) { return }
    Start-Sleep -Milliseconds $LeaseProbeDelayMs  # a lease that counts down has dropped by then
    foreach ($item in $probes) {
        try { if (Test-LeaseCountsDown $item $LeaseSeconds) { continue } }
        catch { Write-Warn "Could not re-read UDP $($item.Port) on $($item.Router.Address) to check its lease: $($_.Exception.Message)"; continue }
        Write-Warn "$($item.Router.Address) does not count its lease down (still $LeaseSeconds s after $($LeaseProbeDelayMs / 1000) s). Routers like that can keep the mappings after the lease ends: the automatic -Stop below closes them, or run -Stop yourself."
        $item.Router.Address
    }
}

function Resolve-PeerName([string]$Name) {
    try { @([Net.Dns]::GetHostAddresses($Name) | Where-Object { $_.AddressFamily -eq 'InterNetwork' } | ForEach-Object { $_.ToString() }) }
    catch { @() }
}

function Select-PeerName([string]$Name, [string]$PublicAddress) {
    Write-Section 'Host name'
    if (-not $Name) { Write-Note "No -Hostname: friends use the address $PublicAddress (or a DNS name you point at it)."; return $PublicAddress }
    $resolved = @(Resolve-PeerName $Name)
    if ($resolved -contains $PublicAddress) { Write-Note "$Name resolves to this public address."; return $Name }
    Write-Warn "$Name does not resolve to this PC's public address, so friends would not reach this PC through it. Point its A record here; the settings below use the address."
    $PublicAddress
}

function Get-AllowPeersText([string[]]$Peers) {
    $list = @($Peers | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
    if ($list) { return $list -join ',' }
    '<friend-address-or-name>,<another>'
}

function Get-CoopPlayersArgument {
    if ($Players -lt 3) { return '' }
    " --gow2_coop_players=$Players"
}

function Get-OffsetArgument {
    if ($PortOffset -eq $DefaultPortOffset) { return '' }
    " --net_port_offset=$PortOffset"
}

function Show-GameSettings([string]$PeerName) {
    $offset = Get-OffsetArgument
    $coop = Get-CoopPlayersArgument
    Write-Section 'Game settings'
    Write-Note 'Host (this PC); net_allow_peers lists your friends'' public addresses or DNS names:'
    Write-Note "  gow2.exe --net_link_active=true --net_public_address=$PeerName --net_allow_peers=$(Get-AllowPeersText $AllowPeers) --user_name=<name>$offset$coop"
    Write-Note 'Friends on PC:'
    Write-Note "  gow2.exe --net_link_active=true --net_peers=$PeerName --user_name=<name>$offset$coop"
    Write-Note "Friends on Android, in ${AndroidConfigDir}gow2.toml:"
    Write-Note '  net_link_active = true'
    Write-Note "  net_peers = `"$PeerName`""
    Write-Note '  user_name = "<name>"'
    if ($coop) { Write-Note "  gow2_coop_players = $Players" }
    if ($offset) { Write-Note "  net_port_offset = $PortOffset" }
    Write-Note "Windows Firewall must allow the host's gow2.exe in on UDP $($script:Ports -join ' and ') (docs/ONLINE.md)."
}

function Get-RouterClosing($Router, [datetime]$LeaseEnd, $StaticRouters) {
    if ($PermanentRouters.ContainsKey($Router.Address)) { return 'no lease: open until -Stop or a router restart' }
    if (@($StaticRouters) -contains $Router.Address) { return ('lease not counted down: may stay open after {0:HH:mm} until -Stop or a router restart' -f $LeaseEnd) }
    'the lease ends at {0:yyyy-MM-dd HH:mm}' -f $LeaseEnd
}

function Show-Closing($Path, [datetime]$LeaseEnd, $StaticRouters, [bool]$IsScheduled, [datetime]$StopAt) {
    Write-Section 'Closing'
    foreach ($router in $Path.Routers) { Write-Note ("{0}: {1}" -f $router.Address, (Get-RouterClosing $router $LeaseEnd $StaticRouters)) }
    if ($IsScheduled) { Write-Note ("Automatic -Stop at {0:yyyy-MM-dd HH:mm} (scheduled task '{1}', log {2})." -f $StopAt, $StopTaskName, $StopLogPath) }
    Write-Note 'Run -Stop when the session ends.'
}

function Get-DiscoveredPath([bool]$Tolerant) {
    $local = Get-LocalIPv4
    Write-Section 'This PC'
    Write-Note "LAN address $local (default route), System Link ports UDP $($script:Ports -join ' and ')"
    $path = Get-RouterPath $local $Tolerant
    Show-Path $path
    $path
}

function Invoke-StartMappings($Path, [string]$Public, $Created) {
    $leaseSeconds = [int][Math]::Round($LeaseHours * 3600)
    $plan = @(Get-MappingPlan $Path $script:Ports)
    Assert-NoConflicts $plan
    Write-Section "Mappings (lease $LeaseHours h)"
    Add-PlannedMappings $plan $leaseSeconds $Created
    $leaseEnd = (Get-Date).AddSeconds($leaseSeconds)
    $script:JsonResult['lease_end'] = $leaseEnd.ToString('yyyy-MM-ddTHH:mm:ss')
    $static = @(Find-StaticLeaseRouters $plan $leaseSeconds)
    $stopAt = $leaseEnd.AddMinutes($StopTaskDelayMinutes)
    $isScheduled = Register-StopTaskOrWarn $stopAt
    if ($isScheduled) { $script:JsonResult['stop_scheduled_at'] = $stopAt.ToString('yyyy-MM-ddTHH:mm:ss') }
    $script:JsonResult['routers'] = @($Path.Routers | ForEach-Object { [pscustomobject]@{ address = $_.Address; closing = (Get-RouterClosing $_ $leaseEnd $static) } })
    $peerName = Select-PeerName $Hostname $Public
    $script:JsonResult['peer_name'] = $peerName
    Show-GameSettings $peerName
    Show-Closing $Path $leaseEnd $static $isScheduled $stopAt
}

function Invoke-Start {
    $path = Get-DiscoveredPath $false
    $script:JsonResult['routers'] = @($path.Routers | ForEach-Object { [pscustomobject]@{ address = $_.Address; closing = '' } })
    $public = Assert-ReachableFromInternet $path
    $script:JsonResult['public_address'] = $public
    $script:JsonResult['lease_hours'] = $LeaseHours
    $created = New-Object Collections.Generic.List[object]
    $completed = $false
    try {
        Invoke-StartMappings $path $public $created
        $completed = $true
    } finally {
        if (-not $completed) { Undo-StartRun $created }  # also runs on Ctrl+C, which skips catch blocks
    }
}

function Resolve-RecordedMapping($Entry) {
    $router = ConvertTo-RecordedRouter $Entry
    $mapping = Get-PortMapping $router ([int]$Entry.Port)
    if (-not $mapping) { Write-Note "none     UDP $($Entry.Port) on $($Entry.Router) (already gone)"; return }
    if ($mapping.Description -ne $MappingDescription) { Write-Note ("left     " + (Format-Mapping $router $mapping) + " (made by another program)"); return }
    Remove-PortMapping $router $mapping
}

function Remove-RecordedMappings($Failures) {
    Write-Section "Mappings recorded by -Start ($StatePath)"
    $entries = @(Read-HostState)
    if (-not $entries) { Write-Note 'none recorded'; return $false }
    foreach ($entry in $entries) {
        try { Resolve-RecordedMapping $entry; Remove-StateEntry $entry }
        catch { $Failures.Add("UDP $($entry.Port) on $($entry.Router): $($_.Exception.Message)") }
    }
    $true
}

function Get-PathOrFailure([bool]$Tolerant, $Failures) {
    try { Get-DiscoveredPath $Tolerant }
    catch { $Failures.Add("finding the routers: $($_.Exception.Message)"); $null }
}

function Remove-DescribedMappings($Router, $Failures) {
    try { $mappings = @(Get-DescribedMappings $Router) }
    catch { $Failures.Add("listing the mappings on $($Router.Address): $($_.Exception.Message)"); return }
    if (-not $mappings) { Write-Note "none     on $($Router.Address)"; return }
    foreach ($mapping in $mappings) {
        try { Remove-PortMapping $Router $mapping }
        catch { $Failures.Add("$($mapping.Protocol) $($mapping.Port) on $($Router.Address): $($_.Exception.Message)") }
    }
}

function Assert-StopComplete($Failures) {
    if (-not $Failures.Count) { return }
    Write-Section 'Not closed'
    foreach ($failure in $Failures) { Write-Warn $failure }
    throw "-Stop could not confirm that every mapping of this script is gone (above). Run -Stop again on this PC once the routers answer."
}

function Invoke-Stop {
    $failures = New-Object Collections.Generic.List[string]
    $hadState = Remove-RecordedMappings $failures
    $path = Get-PathOrFailure $true $failures
    if ($path) {
        $script:JsonResult['routers'] = @($path.Routers | ForEach-Object { [pscustomobject]@{ address = $_.Address; closing = '' } })
        Write-Section "Mappings described '$MappingDescription' on the routers found now"
        foreach ($router in $path.Routers) { Remove-DescribedMappings $router $failures }
        if ((Test-IncompletePath $path) -and -not $hadState) { $failures.Add("routers from hop $($path.EndHop) on were not checked ($($path.End)), and no state file recorded where -Start mapped") }
    }
    Assert-StopComplete $failures
    Write-Section 'Scheduled -Stop'
    if (-not (Unregister-StopTask)) { Write-Note 'none scheduled' }
    Write-Section 'Done'
    Write-Note 'No mapping made by this script is left on the routers it reached.'
}

function Show-RecordedMappings {
    Write-Section "Recorded by -Start ($StatePath)"
    $entries = @(Read-HostState)
    if (-not $entries) { Write-Note 'none'; return }
    foreach ($entry in $entries) { Write-Note ("UDP {0} on {1} -> {2} (control URL {3})" -f $entry.Port, $entry.Router, (Format-Known $entry.InternalClient), $entry.ControlUrl) }
}

function Format-Verdict([string]$Verdict) {
    switch ($Verdict) {
        'ours' { return '' }
        'ours-elsewhere' { return ' (this script''s description, another target: -Stop deletes it)' }
        'foreign' { return ' (same target, made by another program: -Stop leaves it)' }
        default { return ' (another device or program: -Stop leaves it)' }
    }
}

function Show-PortStatus($Router, [int]$Port) {
    $mapping = Get-PortMapping $Router $Port
    $verdict = Get-MappingVerdict $Router $mapping $Port
    if ($verdict -eq 'missing') { Write-Note "none     UDP $Port on $($Router.Address)"; return }
    Write-Note ("present  " + (Format-Mapping $Router $mapping) + (Format-Verdict $verdict))
}

function Show-RouterStatus($Router) {
    try {
        foreach ($port in $script:Ports) { Show-PortStatus $Router $port }
        foreach ($mapping in @(Get-DescribedMappings $Router | Where-Object { $script:Ports -notcontains $_.Port })) { Write-Note ("present  " + (Format-Mapping $Router $mapping) + ' (another -PortOffset)') }
    } catch { Write-Warn "Could not read the mappings on $($Router.Address): $($_.Exception.Message)" }
}

function Invoke-Status {
    Show-RecordedMappings
    $path = Get-DiscoveredPath $true
    $script:JsonResult['routers'] = @($path.Routers | ForEach-Object { [pscustomobject]@{ address = $_.Address; closing = '' } })
    Write-Section 'System Link mappings'
    foreach ($router in $path.Routers) { Show-RouterStatus $router }
    $openCount = 0
    foreach ($router in $path.Routers) {
        foreach ($port in $script:Ports) {
            try {
                $statusMapping = Get-PortMapping $router $port
                if ((Get-MappingVerdict $router $statusMapping $port) -eq 'ours') { $openCount++ }
            } catch { }
        }
    }
    $script:JsonResult['open_ports'] = $openCount
    Write-Section 'Scheduled -Stop'
    $at = Get-StopTaskTime
    if ($at) { Write-Note "task '$StopTaskName' at $at" } else { Write-Note 'none' }
    if ($at) {
        try { $script:JsonResult['stop_scheduled_at'] = ([datetime]$at).ToString('yyyy-MM-ddTHH:mm:ss') }
        catch { $script:JsonResult['stop_scheduled_at'] = $at }
    }
}

function Start-LogFile {
    if (-not $LogFile) { return }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $LogFile) | Out-Null
    Start-Transcript -LiteralPath $LogFile -Append | Out-Null
}

function Stop-LogFile { if ($LogFile) { Stop-Transcript | Out-Null } }

$script:JsonResult = [ordered]@{
    mode = 'Status'
    ok = $true
    error = ''
    error_code = ''
    error_router = ''
    public_address = ''
    peer_name = ''
    ports = @()
    lease_hours = $null
    lease_end = ''
    stop_scheduled_at = ''
    routers = @()
    open_ports = 0
    partial = $false
    unmanaged_router = ''
    unmanaged_target = ''
    warnings = @()
}
$exitCode = 0
try {
    Start-LogFile
    $mode = Select-Mode $Start.IsPresent $Stop.IsPresent $Status.IsPresent
    $script:JsonResult['mode'] = $mode
    $script:Ports = Get-SystemLinkPorts $PortOffset
    $script:JsonResult['ports'] = @($script:Ports)
    $script:Http = New-LanHttpClient
    Write-Host ("gow2 host-online -{0}, {1:yyyy-MM-dd HH:mm:ss}" -f $mode, (Get-Date))
    switch ($mode) {
        'Start' { Invoke-Start }
        'Stop' { Invoke-Stop }
        default { Invoke-Status }
    }
} catch {
    Write-Host ''
    Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
    if ($_.FullyQualifiedErrorId -ne $_.Exception.Message) { Write-Host $_.ScriptStackTrace }  # not one of this script's own messages
    $script:JsonResult['error'] = $_.Exception.Message
    $hostError = Get-HostError $_.Exception.Message
    $script:JsonResult['error_code'] = $hostError.Code
    $script:JsonResult['error_router'] = [string]$hostError.Router
    $exitCode = 1
} finally {
    try { Stop-LogFile } finally {
        if ($Json) {
            $script:JsonResult['ok'] = ($exitCode -eq 0)
            if ($null -ne $script:JsonWarnings) { $script:JsonResult['warnings'] = [object[]]$script:JsonWarnings.ToArray() } else { $script:JsonResult['warnings'] = [object[]]@() }
            $script:JsonResult['ports'] = [object[]]@($script:JsonResult['ports'])
            $script:JsonResult['routers'] = [object[]]@($script:JsonResult['routers'])
            $script:JsonResult['warnings'] = [object[]]@($script:JsonResult['warnings'])
            $jsonText = ConvertTo-Json -InputObject $script:JsonResult -Compress -Depth 4
            Write-Output ("GOW2HOST " + $jsonText)
        }
    }
}
exit $exitCode
