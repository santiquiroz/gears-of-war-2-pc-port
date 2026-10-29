# Runs several gow2.exe instances on one PC for co-op tests: each with its own user data folder,
# log file and window slot. One action per call, so a test can look at a screenshot before it
# decides the next key. State lives in <OutDir>\instances.json.
#
#   coop-instances.ps1 start -Count 2 -OutDir out\coop-test\s1 -Common "--net_bind_ip=127.0.0.{i}"
#   coop-instances.ps1 shot  -Instance 2 -Label menu -OutDir out\coop-test\s1
#   coop-instances.ps1 key   -Instance 2 -Key Space -OutDir out\coop-test\s1
#   coop-instances.ps1 pad   -Instance 2 -Pad "A" -Hold 200 -OutDir out\coop-test\s1
#   coop-instances.ps1 hold  -Instance 2 -Pad "LY=1" -OutDir out\coop-test\s1   (release clears it)
#   coop-instances.ps1 stats -Seconds 10 -OutDir out\coop-test\s1
#   coop-instances.ps1 stop  -OutDir out\coop-test\s1
#   coop-instances.ps1 cmd -Instance 1 -Command "help" -OutDir out\coop-test\s1
#   coop-instances.ps1 waitlog -Instance 1 -Pattern "DEVCMD on:" -OutDir out\coop-test\s1
#   coop-instances.ps1 stopone -Instance 2 -OutDir out\coop-test\s1
#   coop-instances.ps1 startone -Instance 2 -OutDir out\coop-test\s1
#   coop-instances.ps1 chapter -Chapter 5 -OutDir out\coop-test\s1
#   coop-instances.ps1 macro -Instance 1 -Steps "pad:A:200,wait:500" -OutDir out\coop-test\s1
#
# key/down/up send real keystrokes to the focused window, so they need the focus. pad/hold/release
# write the instance's gow2_test_input file instead: the game reads it as guest user 0's pad even
# without focus, which is what lets several instances, or one behind a system dialog, be driven.
# -PerInstance lists each instance's own flags separated by '|' (first part for instance 1).
# {i} in -Common or -PerInstance becomes the instance number (1-based). -SeedUser copies a user
# data folder (saves and profile) into each new instance folder and -SeedCache a cache folder
# (shaders, edited Coalesced bundles) into its user\cache; the sources are only read. start also
# runs the idle guard (-Guard, the main checkout's out\idle-gate\guard-user.ps1) unless -NoGuard,
# and stop ends it and exits 3 when the guard saw the user come back.
param(
    [Parameter(Mandatory, Position = 0)]
    [ValidateSet('start', 'shot', 'shots', 'key', 'down', 'up', 'pad', 'hold', 'release', 'stats', 'status', 'stop', 'cmd', 'waitlog', 'stopone', 'startone', 'chapter', 'macro')]
    [string]$Action,
    [Parameter(Mandatory)] [string]$OutDir,
    [int]$Count = 1,
    [int]$Instance = 1,
    [string]$Key = '',
    [string]$Pad = '',
    [int]$Hold = 120,
    [string]$Label = 'shot',
    [string]$Common = '',
    [string]$PerInstance = '',
    [string]$SeedUser = '',
    [string]$SeedCache = '',
    [int]$Seconds = 10,
    [int]$Width = 800,
    [int]$Height = 450,
    [int]$StaggerSec = 10,
    [switch]$NoGuard,
    [string]$GameRoot = 'C:\personal\gow2-gamedata\retail',
    [string]$Exe = '',
    [string]$Guard = 'C:\personal\gow2-recomp\out\idle-gate\guard-user.ps1',
    [string]$Command = '',
    [string]$Pattern = '',
    [int]$TimeoutSec = 30,
    [switch]$FromStart,
    [switch]$SinceStart,
    [switch]$Coop,
    [int]$Players = 4,
    [switch]$DevCheats,
    [int]$Chapter = 0,
    [string]$Steps = ''
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $Exe) { $Exe = Join-Path $root 'out\build\release\gow2.exe' }
if (-not [IO.Path]::IsPathRooted($OutDir)) { $OutDir = Join-Path $root $OutDir }
$stateFile = Join-Path $OutDir 'instances.json'
$guardFlag = Join-Path (Split-Path $Guard) 'user-returned.txt'

Add-Type -AssemblyName System.Drawing, System.Windows.Forms
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class CoopWin {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr h, int attr, out RECT r, int size);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, IntPtr pid);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool attach);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint mapType);
}
'@

function Read-State {
    if (-not (Test-Path $stateFile)) { return @() }
    # Windows PowerShell 5.1 emits a JSON array as one object; enumerate it as pwsh 7 does.
    return @(Get-Content $stateFile -Raw | ConvertFrom-Json | ForEach-Object { $_ })
}

function Write-State($items) { ConvertTo-Json -InputObject @($items) -Depth 4 | Set-Content $stateFile }

function Expand-InstanceArgs([string]$text, [int]$i) { ($text -replace '\{i\}', "$i") -split ' ' | Where-Object { $_ } }

function New-InstanceFolder([int]$i) {
    $dir = Join-Path $OutDir "inst$i"
    $user = Join-Path $dir 'user'
    if ($SeedUser -and -not (Test-Path $user)) {
        Copy-Item -Recurse -Path $SeedUser -Destination $user
        # 0010's per-install System Link XUID: a copy would give every instance the same XUID.
        Remove-Item -LiteralPath (Join-Path $user 'system_link_xuid.txt') -Force -ErrorAction SilentlyContinue
    }
    New-Item -ItemType Directory -Force -Path $user | Out-Null
    Set-Content -Path (Join-Path $dir 'pad.txt') -Value ''
    $path = Join-Path $dir 'cmd.txt'
    [IO.File]::WriteAllText($path, '')
    Remove-Item -LiteralPath (Join-Path $dir 'cmd.txt.ack') -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $dir 'log-offset.txt') -Force -ErrorAction SilentlyContinue
    $cache = Join-Path $user 'cache'
    if ($SeedCache -and -not (Test-Path $cache)) { Copy-Item -Recurse -Path $SeedCache -Destination $cache }
    return $dir
}

function Get-CoopArgs([int]$i) {
    $list = @()
    for ($k = 1; $k -le $Count; $k++) {
        $list += "127.0.0.$k"
    }
    $targets = $list -join ','
    return @("--gow2_coop_players=$Players", '--net_link_active=true', "--net_bind_ip=127.0.0.$i", "--net_broadcast_targets=$targets", "--user_name=P$i", '--system_link_trace=true', '--gow2_coop_census=true', '--net_traffic_log_sec=10')
}

function Get-InstanceArgs([int]$i, [string]$dir) {
    $own = @("--game_data_root=`"$GameRoot`"", "--log_file=`"$(Join-Path $dir 'game.log')`"",
        "--gow2_test_input=`"$(Join-Path $dir 'pad.txt')`"",
        "--user_data_root=`"$(Join-Path $dir 'user')`"", '--gow2_launcher=false', '--fullscreen=false',
        "--window_width=$Width", "--window_height=$Height", '--resolution_scale=1')
    $own += "--gow2_dev_command_file=`"$(Join-Path $dir 'cmd.txt')`""
    if ($Coop) { $own += Get-CoopArgs $i }
    if ($DevCheats -and $i -eq 1) { $own += '--gow2_dev_cheats=true' }
    $extra = @(Expand-InstanceArgs $Common $i)
    $ownExtra = @($PerInstance -split '\|')
    if ($ownExtra.Count -ge $i) { $extra += Expand-InstanceArgs $ownExtra[$i - 1] $i }
    return $own + $extra
}

function Wait-Window($proc, [int]$timeoutSec = 60) {
    $clock = [Diagnostics.Stopwatch]::StartNew()
    while ($clock.Elapsed.TotalSeconds -lt $timeoutSec -and -not $proc.HasExited) {
        $proc.Refresh()
        if ($proc.MainWindowHandle -ne [IntPtr]::Zero) { return $proc.MainWindowHandle }
        Start-Sleep -Milliseconds 250
    }
    return [IntPtr]::Zero
}

# Slot i of a grid as wide as the main screen allows, from its top left, so no window covers another.
function Move-ToSlot([IntPtr]$h, [int]$i) {
    $r = New-Object CoopWin+RECT
    [CoopWin]::GetWindowRect($h, [ref]$r) | Out-Null
    $w = $r.R - $r.L; $hh = $r.B - $r.T
    $columns = [Math]::Max(1, [Math]::Floor([System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea.Width / $w))
    $x = (($i - 1) % $columns) * $w; $y = [Math]::Floor(($i - 1) / $columns) * $hh
    # SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
    [CoopWin]::SetWindowPos($h, [IntPtr]::Zero, $x, $y, 0, 0, 0x15) | Out-Null
}

function Start-Guard {
    if ($NoGuard) { return $null }
    $g = Start-Process powershell -WindowStyle Hidden -PassThru -ArgumentList '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $Guard
    Start-Sleep -Seconds 2
    return $g
}

function Get-StartTicks($proc) { if ($proc) { $proc.StartTime.Ticks } else { 0 } }

# A process id can be reused once its process is gone, so only the process that started then dies.
function Stop-SameProcess([int]$id, [long]$startTicks) {
    $p = Get-Process -Id $id -ErrorAction SilentlyContinue
    if ($p -and $p.StartTime.Ticks -eq $startTicks) { Stop-Process -Id $id -Force -ErrorAction SilentlyContinue }
}

function Start-Instances {
    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
    $items = @()
    $guard = Start-Guard
    for ($i = 1; $i -le $Count; $i++) {
        $dir = New-InstanceFolder $i
        $logPath = Join-Path $dir 'game.log'
        $logStart = 0
        if (Test-Path $logPath) { $logStart = (Get-Item $logPath).Length }
        $finalArgs = Get-InstanceArgs $i $dir
        $proc = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -PassThru -ArgumentList $finalArgs
        $h = Wait-Window $proc
        if ($h -ne [IntPtr]::Zero) { Move-ToSlot $h $i }
        $items += [pscustomobject]@{ Index = $i; Pid = $proc.Id; Ticks = (Get-StartTicks $proc); Dir = $dir
            Guard = $(if ($guard) { $guard.Id } else { 0 }); GuardTicks = (Get-StartTicks $guard); Args = $finalArgs; LogStart = $logStart }
        Write-State $items
        Write-Output "instance $i pid $($proc.Id) log $(Join-Path $dir 'game.log')"
        if ($i -lt $Count) { Start-Sleep -Seconds $StaggerSec }
    }
}

function Get-InstanceProcess([int]$i) {
    $item = Read-State | Where-Object { $_.Index -eq $i }
    if (-not $item) { throw "no instance $i in $stateFile" }
    $proc = Get-Process -Id $item.Pid -ErrorAction SilentlyContinue
    if (-not $proc -or $proc.ProcessName -ne 'gow2') { throw "instance $i (pid $($item.Pid)) is not running" }
    return $proc
}

function Request-Foreground([IntPtr]$h) {
    $fg = [CoopWin]::GetForegroundWindow()
    $me = [CoopWin]::GetCurrentThreadId()
    $other = [CoopWin]::GetWindowThreadProcessId($fg, [IntPtr]::Zero)
    $attached = ($other -ne 0) -and [CoopWin]::AttachThreadInput($me, $other, $true)
    [CoopWin]::BringWindowToTop($h) | Out-Null
    [CoopWin]::SetForegroundWindow($h) | Out-Null
    if ($attached) { [CoopWin]::AttachThreadInput($me, $other, $false) | Out-Null }
    Start-Sleep -Milliseconds 150
    return ([CoopWin]::GetForegroundWindow() -eq $h)
}

function Invoke-UnboundKeyThenRequest([IntPtr]$h) {
    Send-KeyEvent 'F24' 0; Send-KeyEvent 'F24' 2
    return (Request-Foreground $h)
}

# Windows lets the process that sent the last input take the foreground; an unbound key (F24)
# gives it that right when there is no foreground thread to attach to. Nothing gets past a system
# dialog such as the Windows Firewall prompt (it dims the screen and keeps the foreground): use the
# pad actions then.
function Set-Focus([IntPtr]$h) {
    if ([CoopWin]::GetForegroundWindow() -eq $h) { return $true }
    if (Request-Foreground $h) { return $true }
    return (Invoke-UnboundKeyThenRequest $h)
}

function Get-ForegroundOwner {
    $fg = [CoopWin]::GetForegroundWindow()
    if ($fg -eq [IntPtr]::Zero) { return 'no window' }
    $p = Get-Process | Where-Object { $_.MainWindowHandle -eq $fg } | Select-Object -First 1
    if ($p) { return "$($p.ProcessName) $($p.Id)" }
    return "window $fg"
}

function Get-Vk([string]$name) { [byte][System.Windows.Forms.Keys]::$name }
function Get-Scan([string]$name) { [byte][CoopWin]::MapVirtualKey([uint32](Get-Vk $name), 0) }
# KEYEVENTF_EXTENDEDKEY for the navigation keys, as a physical keyboard sends them.
function Get-KeyFlags([string]$name) { if ($name -in 'Left', 'Right', 'Up', 'Down', 'Home', 'End', 'Insert', 'Delete', 'PageUp', 'PageDown') { 1 } else { 0 } }

function Send-KeyEvent([string]$name, [uint32]$up) {
    [CoopWin]::keybd_event((Get-Vk $name), (Get-Scan $name), ((Get-KeyFlags $name) -bor $up), [UIntPtr]::Zero)
}

# Keys go to the foreground window, so nothing is sent unless the instance really has the focus.
function Send-InstanceKey([string]$mode) {
    $proc = Get-InstanceProcess $Instance
    if (-not (Set-Focus $proc.MainWindowHandle)) { throw "instance $Instance did not get the focus ($(Get-ForegroundOwner) has it); key not sent" }
    if ($mode -ne 'up') { Send-KeyEvent $Key 0 }
    if ($mode -eq 'key') { Start-Sleep -Milliseconds $Hold }
    if ($mode -ne 'down') { Send-KeyEvent $Key 2 }
    Write-Output "$(Get-Date -Format 'HH:mm:ss.fff') instance $Instance $mode $Key"
}

function Write-PadFile([int]$i, [string]$line) {
    $path = Join-Path (Join-Path $OutDir "inst$i") 'pad.txt'
    for ($try = 0; $try -lt 20; $try++) {
        try {
            $fs = [IO.File]::Open($path, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::ReadWrite)
            $bytes = [Text.Encoding]::ASCII.GetBytes($line + "`n")
            $fs.Write($bytes, 0, $bytes.Length); $fs.Dispose()
            return
        } catch [IO.IOException] { Start-Sleep -Milliseconds 5 }
    }
    throw "could not write $path"
}

function Send-InstancePad([string]$mode) {
    Get-InstanceProcess $Instance | Out-Null
    if ($mode -eq 'release') { Write-PadFile $Instance ''; Write-Output "$(Get-Date -Format 'HH:mm:ss.fff') instance $Instance pad released"; return }
    Write-PadFile $Instance $Pad
    if ($mode -eq 'pad') { Start-Sleep -Milliseconds $Hold; Write-PadFile $Instance '' }
    Write-Output "$(Get-Date -Format 'HH:mm:ss.fff') instance $Instance $mode '$Pad'"
}

# No focus change: the windows sit in separate slots, so the screen shows each one.
function Save-InstanceShot([int]$i, [string]$shotLabel) {
    $proc = Get-InstanceProcess $i
    $h = $proc.MainWindowHandle
    $r = New-Object CoopWin+RECT
    # The visible frame: GetWindowRect also covers the invisible resize border, which captures
    # a strip of whatever is behind the window. DWMWA_EXTENDED_FRAME_BOUNDS = 9.
    if ([CoopWin]::DwmGetWindowAttribute($h, 9, [ref]$r, 16) -ne 0) { [CoopWin]::GetWindowRect($h, [ref]$r) | Out-Null }
    $bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
    $path = Join-Path $OutDir ("{0}-i{1}-{2}.png" -f (Get-Date -Format 'HHmmss'), $i, $shotLabel)
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Output "shot $path"
}

function Get-GpuMemoryMB([int]$id) {
    $c = Get-Counter "\GPU Process Memory(pid_$($id)_*)\Dedicated Usage" -ErrorAction SilentlyContinue
    if (-not $c) { return 0 }
    return [Math]::Round(($c.CounterSamples | Measure-Object CookedValue -Sum).Sum / 1MB)
}

function Get-Gpu3dPercent([int]$id) {
    $c = Get-Counter "\GPU Engine(pid_$($id)_*engtype_3D)\Utilization Percentage" -ErrorAction SilentlyContinue
    if (-not $c) { return 0 }
    return [Math]::Round(($c.CounterSamples | Measure-Object CookedValue -Sum).Sum, 1)
}

function Write-Stats {
    $live = Read-State | ForEach-Object { Get-Process -Id $_.Pid -ErrorAction SilentlyContinue } | Where-Object { $_ }
    $before = @{}; foreach ($p in $live) { $before[$p.Id] = $p.TotalProcessorTime.TotalSeconds }
    Start-Sleep -Seconds $Seconds
    $cores = [Environment]::ProcessorCount
    foreach ($p in $live) {
        $p.Refresh()
        $cpu = ($p.TotalProcessorTime.TotalSeconds - $before[$p.Id]) / $Seconds / $cores * 100
        $ram = [Math]::Round($p.WorkingSet64 / 1MB)
        $vram = Get-GpuMemoryMB $p.Id
        $gpu = Get-Gpu3dPercent $p.Id
        $fmt = 'pid {0}: cpu {1:N1}% of {2} threads, ram {3} MB, vram {4} MB, gpu3d {5}%'
        Write-Output ($fmt -f $p.Id, $cpu, $cores, $ram, $vram, $gpu)
    }
}

function Write-Status {
    foreach ($item in Read-State) {
        $alive = [bool](Get-Process -Id $item.Pid -ErrorAction SilentlyContinue)
        Write-Output "instance $($item.Index) pid $($item.Pid) running=$alive"
    }
    if (Test-Path $guardFlag) { Write-Output "USER RETURNED: $(Get-Content $guardFlag)" }
}

function Stop-Instances {
    $items = Read-State
    foreach ($item in $items) { Stop-SameProcess $item.Pid $item.Ticks }
    $pids = @($items | ForEach-Object { $_.Pid })
    if ($pids.Count) { Wait-Process -Id $pids -Timeout 15 -ErrorAction SilentlyContinue }
    $first = $items | Select-Object -First 1
    if ($first -and $first.Guard) { Stop-SameProcess $first.Guard $first.GuardTicks }
    Write-Output "stopped $(@($items).Count) instance(s)"
    if (Test-Path $guardFlag) { Write-Output "USER RETURNED: $(Get-Content $guardFlag)"; exit 3 }
}

function Get-InstanceDir([int]$i) {
    return Join-Path $OutDir "inst$i"
}

function Get-CommandFile([int]$i) {
    return Join-Path (Get-InstanceDir $i) 'cmd.txt'
}

function Get-AckFile([int]$i) {
    return Join-Path (Get-InstanceDir $i) 'cmd.txt.ack'
}

function Get-LogFile([int]$i) {
    return Join-Path (Get-InstanceDir $i) 'game.log'
}

function Get-LineCount([string]$path) {
    if (-not (Test-Path $path)) { return 0 }
    $fs = [IO.File]::Open($path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        $count = 0
        $buf = New-Object byte[] 4096
        while ($true) {
            $n = $fs.Read($buf, 0, $buf.Length)
            if ($n -le 0) { break }
            for ($k = 0; $k -lt $n; $k++) {
                if ($buf[$k] -eq 10) { $count++ }
            }
        }
        return $count
    } finally {
        $fs.Dispose()
    }
}

function Add-CommandLine([string]$path, [string]$line) {
    if (-not $line) { throw 'command line is empty' }
    if ($line.StartsWith('#')) { throw "command line must not start with '#'" }
    foreach ($ch in $line.ToCharArray()) {
        $code = [int]$ch
        if ($code -eq 9) { continue }
        if ($code -lt 32 -or $code -gt 126) { throw ("command line has invalid char 0x{0:X2}" -f $code) }
    }
    $bytes = [Text.Encoding]::ASCII.GetBytes($line + "`n")
    for ($try = 0; $try -lt 20; $try++) {
        try {
            $fs = [IO.File]::Open($path, [IO.FileMode]::Append, [IO.FileAccess]::Write, [IO.FileShare]::ReadWrite)
            try {
                $fs.Write($bytes, 0, $bytes.Length)
            } finally {
                $fs.Dispose()
            }
            return
        } catch [IO.IOException] {
            Start-Sleep -Milliseconds 5
        }
    }
    throw "could not write $path"
}

function Read-SharedText([string]$path, [long]$start) {
    if (-not (Test-Path $path)) { return '' }
    $share = [IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete
    $fs = [IO.File]::Open($path, [IO.FileMode]::Open, [IO.FileAccess]::Read, $share)
    try {
        if ($fs.Length -le $start) { return '' }
        $fs.Seek($start, [IO.SeekOrigin]::Begin) | Out-Null
        $len = [int]($fs.Length - $start)
        $buf = New-Object byte[] $len
        $read = 0
        while ($read -lt $len) {
            $n = $fs.Read($buf, $read, $len - $read)
            if ($n -le 0) { break }
            $read += $n
        }
        return [Text.Encoding]::ASCII.GetString($buf, 0, $read)
    } finally {
        $fs.Dispose()
    }
}

function Save-LogOffset([int]$i) {
    $log = Get-LogFile $i
    $len = 0
    if (Test-Path $log) { $len = (Get-Item $log).Length }
    $offPath = Join-Path (Get-InstanceDir $i) 'log-offset.txt'
    Set-Content -Path $offPath -Value "$len"
}

function Find-AckLine([string]$ackPath, [int]$n) {
    $text = Read-SharedText $ackPath 0
    if (-not $text) { return $null }
    $prefix = "$n`t"
    foreach ($line in ($text -split "`n")) {
        $clean = $line.TrimEnd("`r")
        if ($clean.StartsWith($prefix)) { return $clean }
    }
    return $null
}

function Wait-Ack([string]$ackPath, [int]$n, [int]$timeoutSec) {
    $clock = [Diagnostics.Stopwatch]::StartNew()
    while ($true) {
        $line = Find-AckLine $ackPath $n
        if ($null -ne $line) { return $line }
        if ($clock.Elapsed.TotalSeconds -ge $timeoutSec) { return $null }
        Start-Sleep -Milliseconds 100
    }
}

function Get-RunLogStart([int]$i) {
    $found = Read-State | Where-Object { $_.Index -eq $i } | Select-Object -First 1
    if ($found -and ($found.PSObject.Properties.Name -contains 'LogStart')) { return [long]$found.LogStart }
    return 0
}

function Wait-PumpReady([int]$i, [int]$timeoutSec) {
    $start = Get-RunLogStart $i
    $log = Get-LogFile $i
    $clock = [Diagnostics.Stopwatch]::StartNew()
    while ($true) {
        $text = Read-SharedText $log $start
        if ($text -and $text.Contains('DEVCMD on:')) { return $true }
        if ($clock.Elapsed.TotalSeconds -ge $timeoutSec) { return $false }
        Start-Sleep -Milliseconds 250
    }
}

function Invoke-InstanceCommand([int]$i, [string]$text, [int]$timeoutSec) {
    Get-InstanceProcess $i | Out-Null
    if (-not (Wait-PumpReady $i $timeoutSec)) {
        return @{ Code = 2; Text = "instance $i has no command pump (no 'DEVCMD on' in its log)" }
    }
    $cmdPath = Get-CommandFile $i
    $n = (Get-LineCount $cmdPath) + 1
    Save-LogOffset $i
    Add-CommandLine $cmdPath $text
    $ackPath = Get-AckFile $i
    $line = Wait-Ack $ackPath $n $timeoutSec
    if ($null -eq $line) {
        return @{ Code = 2; Text = "instance $i #$n no result after $timeoutSec s" }
    }
    $parts = $line -split "`t", 3
    $status = ''
    $detail = ''
    if ($parts.Count -ge 2) { $status = $parts[1] }
    if ($parts.Count -ge 3) { $detail = $parts[2] }
    $code = 1
    if ($status -eq 'ok') { $code = 0 }
    return @{ Code = $code; Text = "instance $i #$n $status $detail" }
}

function Get-LogWaitStart([int]$i) {
    if ($FromStart) { return 0 }
    if ($SinceStart) { return (Get-RunLogStart $i) }
    $offPath = Join-Path (Get-InstanceDir $i) 'log-offset.txt'
    if (Test-Path $offPath) {
        $txt = (Get-Content $offPath -Raw).Trim()
        $val = [long]0
        if ([long]::TryParse($txt, [ref]$val)) { return $val }
    }
    $log = Get-LogFile $i
    if (Test-Path $log) { return (Get-Item $log).Length }
    return 0
}

function Find-LogMatch([int]$i, [string]$pattern, [int]$timeoutSec) {
    $pos = Get-LogWaitStart $i
    $log = Get-LogFile $i
    $pending = ''
    $clock = [Diagnostics.Stopwatch]::StartNew()
    while ($true) {
        $chunk = Read-SharedText $log $pos
        if ($chunk.Length -gt 0) {
            $pos += $chunk.Length
            $text = $pending + $chunk
            $lines = $text -split "`n"
            $pending = $lines[$lines.Count - 1]
            for ($k = 0; $k -lt ($lines.Count - 1); $k++) {
                $line = $lines[$k].TrimEnd("`r")
                if ($line -match $pattern) { return $line }
            }
        }
        if ($clock.Elapsed.TotalSeconds -ge $timeoutSec) { return $null }
        Start-Sleep -Milliseconds 250
    }
}

function Test-LogPattern([int]$i, [string]$pattern, [int]$timeoutSec) {
    $m = Find-LogMatch $i $pattern $timeoutSec
    return ($null -ne $m)
}

function Wait-LogPattern([int]$i, [string]$pattern, [int]$timeoutSec) {
    $m = Find-LogMatch $i $pattern $timeoutSec
    if ($null -ne $m) {
        Write-Output "instance $i matched: $m"
        exit 0
    }
    Write-Output "instance $i no line matching '$pattern' after $timeoutSec s"
    exit 2
}

function Stop-OneInstance([int]$i) {
    $items = Read-State
    $item = $items | Where-Object { $_.Index -eq $i } | Select-Object -First 1
    if (-not $item) { throw "no instance $i in $stateFile" }
    Stop-SameProcess $item.Pid $item.Ticks
    $item.Pid = 0
    $item.Ticks = 0
    Write-State $items
    Write-Output "instance $i stopped"
}

function Start-OneInstance([int]$i) {
    $items = Read-State
    $item = $items | Where-Object { $_.Index -eq $i } | Select-Object -First 1
    if (-not $item) { throw "no instance $i in $stateFile" }
    $old = $null
    if ($item.Pid) { $old = Get-Process -Id $item.Pid -ErrorAction SilentlyContinue }
    if ($old -and $old.ProcessName -eq 'gow2') { throw "instance $i is already running (pid $($item.Pid))" }
    if (-not ($item.PSObject.Properties.Name -contains 'Args')) { throw "instance $i has no stored Args; start it again with 'start'" }
    $dir = $item.Dir
    if (-not $dir) { $dir = Get-InstanceDir $i }
    $logPath = Join-Path $dir 'game.log'
    $logStart = 0
    if (Test-Path $logPath) { $logStart = (Get-Item $logPath).Length }
    $stored = @($item.Args)
    $proc = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -PassThru -ArgumentList $stored
    $h = Wait-Window $proc
    if ($h -ne [IntPtr]::Zero) { Move-ToSlot $h $i }
    $item.Pid = $proc.Id
    $item.Ticks = Get-StartTicks $proc
    $item.LogStart = $logStart
    Write-State $items
    Write-Output "instance $i pid $($proc.Id) log $logPath"
}

function Invoke-PadMacroStep([int]$i, [string]$buttons, [string]$msText) {
    if (-not $buttons) { return 'bad pad step' }
    $ms = 0
    if (-not ([int]::TryParse($msText, [ref]$ms))) { return "bad ms '$msText'" }
    try {
        $padLine = $buttons -replace '\+', ' '
        Write-PadFile $i $padLine
        Start-Sleep -Milliseconds $ms
        Write-PadFile $i ''
        return $null
    } catch {
        return $_.Exception.Message
    }
}

function Invoke-HoldMacroStep([int]$i, [string]$buttons) {
    if (-not $buttons) { return 'bad hold step' }
    try {
        $padLine = $buttons -replace '\+', ' '
        Write-PadFile $i $padLine
        return $null
    } catch {
        return $_.Exception.Message
    }
}

function Invoke-WaitMacroStep([string]$msText) {
    $ms = 0
    if (-not ([int]::TryParse($msText, [ref]$ms))) { return "bad ms '$msText'" }
    Start-Sleep -Milliseconds $ms
    return $null
}

function Invoke-ShotMacroStep([int]$i, [string]$shotLabel) {
    if (-not $shotLabel) { return 'bad shot step' }
    try {
        # Print the path here: anything a step function outputs counts as its failure reason.
        Write-Host (Save-InstanceShot $i $shotLabel)
        return $null
    } catch {
        return $_.Exception.Message
    }
}

function Invoke-CmdMacroStep([int]$i, [string]$cmdText, [int]$timeoutSec) {
    if (-not $cmdText) { return 'bad cmd step' }
    try {
        $r = Invoke-InstanceCommand $i $cmdText $timeoutSec
    } catch {
        return $_.Exception.Message
    }
    if ($r.Code -ne 0) { return $r.Text }
    return $null
}

function Invoke-LogMacroStep([int]$i, [string]$regex, [string]$secText) {
    if (-not $regex) { return 'bad log step' }
    $sec = 0
    if (-not ([int]::TryParse($secText, [ref]$sec))) { return "bad sec '$secText'" }
    try {
        $ok = Test-LogPattern $i $regex $sec
    } catch {
        return $_.Exception.Message
    }
    if (-not $ok) { return "no line matching '$regex' after $sec s" }
    return $null
}

function Invoke-MacroStep([int]$i, [string]$step, [int]$timeoutSec) {
    if ($step -eq 'release') {
        try {
            Write-PadFile $i ''
            return $null
        } catch {
            return $_.Exception.Message
        }
    }
    if ($step.StartsWith('pad:')) {
        $rest = $step.Substring(4)
        $parts = $rest -split ':', 2
        if ($parts.Count -ne 2) { return 'bad pad step' }
        return Invoke-PadMacroStep $i $parts[0] $parts[1]
    }
    if ($step.StartsWith('hold:')) {
        $buttons = $step.Substring(5)
        return Invoke-HoldMacroStep $i $buttons
    }
    if ($step.StartsWith('wait:')) {
        $msText = $step.Substring(5)
        return Invoke-WaitMacroStep $msText
    }
    if ($step.StartsWith('shot:')) {
        $shotLabel = $step.Substring(5)
        return Invoke-ShotMacroStep $i $shotLabel
    }
    if ($step.StartsWith('cmd:')) {
        $cmdText = $step.Substring(4)
        return Invoke-CmdMacroStep $i $cmdText $timeoutSec
    }
    if ($step.StartsWith('log:')) {
        $rest = $step.Substring(4)
        $idx = $rest.LastIndexOf(':')
        if ($idx -lt 0) { return 'bad log step' }
        $regex = $rest.Substring(0, $idx)
        $secText = $rest.Substring($idx + 1)
        return Invoke-LogMacroStep $i $regex $secText
    }
    return 'unknown step'
}

function Invoke-Macro([int]$i, [string]$stepsText, [int]$timeoutSec) {
    $list = $stepsText -split ','
    $k = 0
    foreach ($raw in $list) {
        $k++
        $text = $raw.Trim()
        Write-Output "$(Get-Date -Format 'HH:mm:ss.fff') instance $i step $text"
        $why = Invoke-MacroStep $i $text $timeoutSec
        if ($why) {
            Write-Output "macro failed at step $k '$text': $why"
            exit 1
        }
    }
    exit 0
}

switch ($Action) {
    'start' { Start-Instances }
    'shot' { Save-InstanceShot $Instance $Label }
    'shots' { foreach ($item in Read-State) { Save-InstanceShot $item.Index $Label } }
    'key' { Send-InstanceKey 'key' }
    'down' { Send-InstanceKey 'down' }
    'up' { Send-InstanceKey 'up' }
    'pad' { Send-InstancePad 'pad' }
    'hold' { Send-InstancePad 'hold' }
    'release' { Send-InstancePad 'release' }
    'stats' { Write-Stats }
    'status' { Write-Status }
    'stop' { Stop-Instances }
    'cmd' {
        try {
            $cmdResult = Invoke-InstanceCommand $Instance $Command $TimeoutSec
        } catch {
            Write-Output $_.Exception.Message
            exit 3
        }
        Write-Output $cmdResult.Text
        exit $cmdResult.Code
    }
    'waitlog' { Wait-LogPattern $Instance $Pattern $TimeoutSec }
    'stopone' { Stop-OneInstance $Instance }
    'startone' { Start-OneInstance $Instance }
    'chapter' {
        if ($Chapter -lt 1 -or $Chapter -gt 29) { throw 'chapter must be 1..29' }
        $travel = "servertravel geargame_p?loadcheckpoint=0?chapter=$($Chapter - 1)"
        try {
            $chapterResult = Invoke-InstanceCommand 1 $travel $TimeoutSec
        } catch {
            Write-Output $_.Exception.Message
            exit 3
        }
        Write-Output $chapterResult.Text
        exit $chapterResult.Code
    }
    'macro' { Invoke-Macro $Instance $Steps $TimeoutSec }
}
