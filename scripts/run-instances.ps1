# Start N gow2.exe instances side by side for a System Link test, then supervise them until stopped.
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run-instances.ps1 -OutDir out\pcpc-test\run1 [-Names 'host,client']
# With a phone on the LAN: -Names pc -Lan -Peers <phone IP> (one copy, no loopback binding; the
# query then also goes to the phone as a unicast, whatever adapter Windows picks for broadcasts).
# Every copy runs with System Link on and keeps its saves in <run>\<name>\saves.
param(
    [Parameter(Mandatory)] [string]$OutDir,
    [string]$Exe = 'out/build/release/gow2.exe',
    [string]$GameRoot = 'C:\personal\gow2-gamedata\retail',
    [string]$Names = 'host,client',
    [string]$IpPrefix = '127.0.0.',
    [int]$Width = 1152,
    [int]$Height = 648,
    [string]$ExtraArgs = '',
    [string]$HostArgs = '',
    [string]$ClientArgs = '',
    [int]$MaxSeconds = 600,
    [int]$StaggerSeconds = 15,
    [string]$SeedFrom = (Join-Path $env:LOCALAPPDATA 'GearsOfWar2PC'),
    # Real LAN (PC with a phone): no loopback binding, the copy uses its default-route address.
    [switch]$Lan,
    # net_peers for every copy: host[:port] list, for example the phone's address with -Lan.
    [string]$Peers = ''
)
$ErrorActionPreference = 'Stop'
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class GowRun {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
}
"@

function Split-ArgTokens([string]$text) {
    return @($text -split ' ' | Where-Object { $_ })
}

function Get-ElapsedSec([datetime]$start) {
    return [int]((Get-Date) - $start).TotalSeconds
}

function Format-Gamertag([string]$name) {
    if ([string]::IsNullOrEmpty($name)) { return $name }
    $tag = $name.Substring(0, 1).ToUpper() + $name.Substring(1)
    if ($tag.Length -gt 15) { $tag = $tag.Substring(0, 15) }
    return $tag
}

function Copy-ProfileSeed([string]$seedFrom, [string]$dest) {
    if (-not (Test-Path -LiteralPath $seedFrom)) { return }
    if (Test-Path -LiteralPath $dest) { return }
    Copy-Item -LiteralPath $seedFrom -Destination $dest -Recurse -Force
    Write-Output "seeded $dest"
}

function Get-LoopbackNetArgs([string]$ip, [string]$allIps) {
    return @("--net_bind_ip=$ip", "--net_broadcast_targets=$allIps")
}

function Get-PeerArgs([string]$peers) {
    if ([string]::IsNullOrWhiteSpace($peers)) { return @() }
    return @("--net_peers=$peers")
}

# A build without gow2_save_dir only warns about the unknown setting; one with it would otherwise
# write the save folder named in gow2.toml (the real, cloud-synced one) from every copy at once.
function Get-SaveDirArgs([string]$saveDir) {
    return @("--gow2_save_dir=`"$saveDir`"")
}

function Build-InstanceArgs([string]$gameRoot, [string]$log, [int]$width, [int]$height, [array]$net, [string]$xuid, [string]$tag, [array]$extra, [array]$role) {
    $list = @(
        "--game_data_root=`"$gameRoot`"",
        "--log_file=`"$log`"",
        '--gow2_launcher=false',
        '--fullscreen=false',
        "--window_width=$width",
        "--window_height=$height",
        "--user_xuid=$xuid",
        "--user_name=$tag",
        '--net_link_active=true',
        '--system_link_trace=true',
        '--mnk_mode=true'
    )
    $list += $net
    $list += $extra
    $list += $role
    return $list
}

function Start-GameInstance([string]$exePath, [string]$argLine, [string]$localAppData) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exePath
    $psi.WorkingDirectory = Split-Path $exePath
    $psi.UseShellExecute = $false
    $psi.Arguments = $argLine
    # psi.Environment exists only on PS7; EnvironmentVariables works on both.
    $psi.EnvironmentVariables['LOCALAPPDATA'] = $localAppData
    return [System.Diagnostics.Process]::Start($psi)
}

function Wait-InstanceWindow([System.Diagnostics.Process]$proc, [int]$timeoutSec) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    while ((Get-Date) -lt $deadline) {
        $proc.Refresh()
        if ($proc.HasExited) { return [IntPtr]::Zero }
        if ($proc.MainWindowHandle -ne [IntPtr]::Zero) { return $proc.MainWindowHandle }
        Start-Sleep -Milliseconds 500
    }
    $proc.Refresh()
    if ($proc.HasExited) { return [IntPtr]::Zero }
    return $proc.MainWindowHandle
}

function Move-InstanceWindow([IntPtr]$hwnd, [int]$x) {
    $flags = 0x0001 -bor 0x0004 -bor 0x0010
    [GowRun]::SetWindowPos($hwnd, [IntPtr]::Zero, $x, 0, 0, 0, $flags) | Out-Null
    $r = New-Object GowRun+RECT
    [GowRun]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    return $r
}

function Write-InstancesFile([string]$path, [array]$records) {
    $json = ConvertTo-Json -InputObject @($records) -Depth 4
    [System.IO.File]::WriteAllText($path, $json, [System.Text.Encoding]::UTF8)
}

$root = Split-Path -Parent $PSScriptRoot
if (-not [System.IO.Path]::IsPathRooted($OutDir)) { $OutDir = Join-Path $root $OutDir }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$OutDir = (Resolve-Path -LiteralPath $OutDir).Path
$exePath = $Exe
if (-not [System.IO.Path]::IsPathRooted($exePath)) { $exePath = Join-Path $root $exePath }
$exePath = (Resolve-Path -LiteralPath $exePath).Path
$instanceNames = @($Names -split ',' | Where-Object { $_ })
$ips = @()
for ($i = 0; $i -lt $instanceNames.Count; $i++) { $ips += "$IpPrefix$($i + 1)" }
$allIps = $ips -join ','
$extraTokens = Split-ArgTokens $ExtraArgs
$hostTokens = Split-ArgTokens $HostArgs
$clientTokens = Split-ArgTokens $ClientArgs
$instancesPath = Join-Path $OutDir 'instances.json'
$stopPath = Join-Path $OutDir 'stop'
$records = @()
$procs = @()
$start = Get-Date
try {
    $nextX = 0
    for ($i = 0; $i -lt $instanceNames.Count; $i++) {
        $n = $instanceNames[$i]
        $ip = $ips[$i]
        $folder = Join-Path $OutDir $n
        $local = Join-Path $folder 'localappdata'
        New-Item -ItemType Directory -Force -Path $folder, $local | Out-Null
        Copy-ProfileSeed $SeedFrom (Join-Path $local 'GearsOfWar2PC')
        $xuid = 'B13E0000000000' + ('{0:X2}' -f ($i + 1))
        $tag = Format-Gamertag $n
        $log = Join-Path $folder 'game.log'
        $roleTokens = $clientTokens
        if ($i -eq 0) { $roleTokens = $hostTokens }
        $netTokens = @()
        if ($Lan) { $ip = 'lan' } else { $netTokens = Get-LoopbackNetArgs $ip $allIps }
        $netTokens += Get-PeerArgs $Peers
        $netTokens += Get-SaveDirArgs (Join-Path $folder 'saves')
        $tokens = Build-InstanceArgs $GameRoot $log $Width $Height $netTokens $xuid $tag $extraTokens $roleTokens
        $argLine = $tokens -join ' '
        $proc = Start-GameInstance $exePath $argLine $local
        $sec = Get-ElapsedSec $start
        Write-Output "[+${sec}s] $n pid $($proc.Id) ip $ip xuid $xuid tag $tag"
        Write-Output $argLine
        $rec = [pscustomobject]@{
            name = $n; pid = $proc.Id; ip = $ip; xuid = $xuid; gamertag = $tag
            log = $log; localappdata = $local; hwnd = [Int64]0; x = $nextX
            started = (Get-Date -Format 'o')
        }
        $records += $rec
        $procs += $proc
        Write-InstancesFile $instancesPath $records
        $hwnd = Wait-InstanceWindow $proc 120
        # A copy that dies during start-up can leave no handle at all.
        if ($null -ne $hwnd -and $hwnd -ne [IntPtr]::Zero) {
            $r = Move-InstanceWindow $hwnd $nextX
            $rec.hwnd = $hwnd.ToInt64()
            $rec.x = $nextX
            $w = $r.R - $r.L
            $h = $r.B - $r.T
            $hex = '0x{0:X}' -f $hwnd.ToInt64()
            $sec = Get-ElapsedSec $start
            Write-Output "[+${sec}s] $n window $hex at ${nextX},0 size ${w}x${h}"
            $nextX = $r.R + 8
        } else {
            $sec = Get-ElapsedSec $start
            Write-Output "[+${sec}s] $n no window yet"
        }
        Write-InstancesFile $instancesPath $records
        if ($i -lt ($instanceNames.Count - 1)) { Start-Sleep -Seconds $StaggerSeconds }
    }
    $reported = @{}
    while ($true) {
        Start-Sleep -Seconds 1
        if (Test-Path -LiteralPath $stopPath) { Write-Output 'stop file seen'; break }
        $alive = 0
        for ($k = 0; $k -lt $procs.Count; $k++) {
            $p = $procs[$k]
            $p.Refresh()
            if ($p.HasExited) {
                if (-not $reported.ContainsKey($p.Id)) {
                    $reported[$p.Id] = $true
                    $sec = Get-ElapsedSec $start
                    Write-Output "[+${sec}s] $($records[$k].name) exited with code $($p.ExitCode)"
                }
            } else {
                $alive++
            }
        }
        if ($alive -eq 0) { break }
        if (((Get-Date) - $start).TotalSeconds -gt $MaxSeconds) { Write-Output 'MaxSeconds reached'; break }
    }
} finally {
    foreach ($p in $procs) {
        $p.Refresh()
        if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
    }
    if (Test-Path -LiteralPath $stopPath) { Remove-Item -LiteralPath $stopPath -Force }
    $sec = Get-ElapsedSec $start
    Write-Output "[+${sec}s] done; logs:"
    foreach ($rec in $records) { Write-Output $rec.log }
}
