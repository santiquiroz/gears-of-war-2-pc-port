# Boot gow2.exe, take screenshots of its window at the given seconds, then stop it.
# Usage: pwsh -File scripts/run-and-capture.ps1 [-Seconds "10,25,45"] [-Keys "75:Return,90:W:3000"] [-ExtraArgs "--mnk_mode=true"]
param(
    [string]$Seconds = '10,25,45',
    [string]$Keys = '',
    [string]$ExtraArgs = '',
    # "sec:duration" - record frame times with PresentMon (tools path below) starting at that second.
    [string]$PresentMonAt = '',
    [string]$PresentMon = 'C:\personal\tools\presentmon\PresentMon-2.6.0-x64.exe',
    [string]$OutDir = 'out/shots',
    [string]$GameRoot = 'C:\personal\gow2-gamedata\retail',
    [string]$Exe = 'out/build/release/gow2.exe'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class Win {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint mapType);
  [DllImport("user32.dll")] public static extern void mouse_event(uint flags, int dx, int dy, uint data, UIntPtr extra);
}
"@

function Save-WindowShot([System.Diagnostics.Process]$proc, [string]$path) {
    $proc.Refresh()
    $h = $proc.MainWindowHandle
    if ($h -eq [IntPtr]::Zero) { Write-Output "no window yet for $path"; return }
    [Win]::SetForegroundWindow($h) | Out-Null
    $r = New-Object Win+RECT
    [Win]::GetWindowRect($h, [ref]$r) | Out-Null
    $w = $r.R - $r.L; $hgt = $r.B - $r.T
    if ($w -le 0 -or $hgt -le 0) { Write-Output "empty window rect for $path"; return }
    $bmp = New-Object System.Drawing.Bitmap $w, $hgt
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Output "saved $path ($w x $hgt)"
}

function Send-Key([System.Diagnostics.Process]$proc, [string]$name, [int]$holdMs = 120) {
    $proc.Refresh()
    if ($proc.MainWindowHandle -ne [IntPtr]::Zero) { [Win]::SetForegroundWindow($proc.MainWindowHandle) | Out-Null }
    $vk = [byte][System.Windows.Forms.Keys]::$name
    # SDL reads scan codes: without the real one (extended for arrows) arrow keys never reach the game.
    $scan = [byte][Win]::MapVirtualKey($vk, 0)
    $ext = if ($name -in 'Left', 'Right', 'Up', 'Down', 'Home', 'End', 'Insert', 'Delete', 'PageUp', 'PageDown') { 1 } else { 0 }
    [Win]::keybd_event($vk, $scan, $ext, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds $holdMs
    [Win]::keybd_event($vk, $scan, $ext -bor 2, [UIntPtr]::Zero)
    Write-Output "sent key $name (${holdMs}ms)"
}

New-Item -ItemType Directory -Force -Path $OutDir, 'out/runs' | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$log = Join-Path $root "out/runs/capture-$stamp.log"
$exe = if ([System.IO.Path]::IsPathRooted($Exe)) { $Exe } else { Join-Path $root $Exe }
$proc = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru `
    -ArgumentList (@("--game_data_root=`"$GameRoot`"", "--log_file=`"$log`"", '--gow2_launcher=false') + ($ExtraArgs -split ' ' | Where-Object { $_ }))
$start = Get-Date
# pwsh -File passes "8,20,40" as one string, so split it here.
$events = @()
$events += $Seconds -split ',' | Where-Object { $_ } | ForEach-Object { [pscustomobject]@{ At = [int]$_; Key = '' } }
function Send-MouseMove([System.Diagnostics.Process]$proc, [int]$dx, [int]$dy) {
    $proc.Refresh()
    if ($proc.MainWindowHandle -ne [IntPtr]::Zero) { [Win]::SetForegroundWindow($proc.MainWindowHandle) | Out-Null }
    # Relative moves in small steps, like a real mouse (MOUSEEVENTF_MOVE = 1).
    $steps = 20
    for ($i = 0; $i -lt $steps; $i++) {
        [Win]::mouse_event(1, [int]($dx / $steps), [int]($dy / $steps), 0, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 15
    }
    Write-Output "moved mouse $dx,$dy"
}

function Start-FrameCapture([string]$csv, [int]$seconds) {
    if (-not (Test-Path $PresentMon)) { Write-Output "PresentMon not found: $PresentMon"; return }
    Start-Process -FilePath $PresentMon -WindowStyle Hidden -ArgumentList @(
        '--process_name', 'gow2.exe', '--output_file', "`"$csv`"", '--timed', "$seconds",
        '--terminate_after_timed', '--no_console_stats', '--v2_metrics') | Out-Null
    Write-Output "frame capture -> $csv (${seconds}s)"
}

# Keys: "sec:Key", "sec:Key:holdMs" or "sec:Mouse:dx:dy".
$events += $Keys -split ',' | Where-Object { $_ } | ForEach-Object {
    $p = $_ -split ':'
    if ($p[1] -eq 'Mouse') { [pscustomobject]@{ At = [int]$p[0]; Key = 'Mouse'; Dx = [int]$p[2]; Dy = [int]$p[3] } }
    else { [pscustomobject]@{ At = [int]$p[0]; Key = $p[1]; Hold = $(if ($p.Count -gt 2) { [int]$p[2] } else { 120 }) } }
}
if ($PresentMonAt) {
    $p = $PresentMonAt -split ':'
    $events += [pscustomobject]@{ At = [int]$p[0]; Key = 'PresentMon'; Duration = [int]$p[1] }
}
foreach ($e in ($events | Sort-Object At)) {
    while (((Get-Date) - $start).TotalSeconds -lt $e.At) {
        if ($proc.HasExited) { break }
        Start-Sleep -Milliseconds 250
    }
    if ($proc.HasExited) { Write-Output "process exited with code $($proc.ExitCode) before $($e.At)s"; break }
    if ($e.Key -eq 'PresentMon') { Start-FrameCapture (Join-Path $root "out/perf/frames-$stamp.csv") $e.Duration }
    elseif ($e.Key -eq 'Mouse') { Send-MouseMove $proc $e.Dx $e.Dy } elseif ($e.Key) { Send-Key $proc $e.Key $e.Hold } else { Save-WindowShot $proc (Join-Path $OutDir "shot-$stamp-$($e.At)s.png") }
}
if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
Write-Output "log: $log"
