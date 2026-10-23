# Drives one gow2.exe instance through chapters From..To: travels to each chapter, plays it with
# scripted pad input, takes a screenshot and classifies the new log lines. Writes <OutDir>\sweep.md
# after every chapter and prints hints/runtime.toml lines for the unregistered guest addresses found.
#
#   chapter-sweep.ps1 -OutDir out\chapter-sweep\s1
#   chapter-sweep.ps1 -OutDir out\chapter-sweep\s1 -From 5 -To 8 -LoadSec 60 -PlaySec 30 -NoIdleGate
param(
    [Parameter(Mandatory)] [string]$OutDir,
    [ValidateRange(1, 29)] [int]$From = 1,
    [ValidateRange(1, 29)] [int]$To = 29,
    [int]$LoadSec = 90,
    [int]$PlaySec = 60,
    [string]$Common = '',
    [int]$Width = 1600,
    [int]$Height = 900,
    [switch]$NoIdleGate
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not [IO.Path]::IsPathRooted($OutDir)) { $OutDir = Join-Path $root $OutDir }
if ($From -gt $To) { throw "-From $From is after -To $To" }

$helper = Join-Path $PSScriptRoot 'coop-instances.ps1'
$gate = Join-Path $root 'out\idle-gate\wait-idle.ps1'
$userFlag = Join-Path $root 'out\idle-gate\user-returned.txt'
$logPath = Join-Path $OutDir 'inst1\game.log'
$sweepPath = Join-Path $OutDir 'sweep.md'
$sticks = @('LY=1', 'RX=1', 'LX=-1', 'LY=-1', 'RX=-1', 'LX=1')
$hints = [ordered]@{}
$script:gameStart = Get-Date

function Invoke-Helper {
    $callArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $helper) + $args + @('-OutDir', $OutDir)
    $prevErr = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $raw = @(powershell @callArgs 2>&1)
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prevErr
    }
    $lines = @($raw | ForEach-Object { "$_" })
    foreach ($line in $lines) { Write-Host $line }
    return @{ Code = $code; Lines = $lines }
}

function Wait-IdleGate {
    if ($NoIdleGate) { return }
    while ($true) {
        powershell -NoProfile -ExecutionPolicy Bypass -File $gate -Seconds 180 -MaxWaitSec 540
        $code = $LASTEXITCODE
        if ($code -eq 0) { return }
        if ($code -ne 75) { throw "idle gate failed with exit $code" }
        Write-Output 'waiting for an idle PC'
    }
}

function Wait-Ready {
    $r = Invoke-Helper 'waitlog' '-Instance' '1' '-Pattern' 'DEVCMD on:' '-SinceStart' '-TimeoutSec' '180'
    if ($r.Code -ne 0) { throw "no 'DEVCMD on:' in the instance log: $($r.Lines -join ' | ')" }
}

function Start-Game {
    Wait-IdleGate
    $common = "--gow2_frame_log=true --vsync=false $Common".Trim()
    $script:gameStart = Get-Date
    $r = Invoke-Helper 'start' '-Count' '1' '-Common' $common '-Width' "$Width" '-Height' "$Height"
    if ($r.Code -ne 0) { throw "start failed: $($r.Lines -join ' | ')" }
    Wait-Ready
}

function Restart-Game {
    Invoke-Helper 'stopone' '-Instance' '1' | Out-Null
    Wait-IdleGate
    $script:gameStart = Get-Date
    $r = Invoke-Helper 'startone' '-Instance' '1'
    if ($r.Code -ne 0) { throw "startone failed: $($r.Lines -join ' | ')" }
    Wait-Ready
}

function Test-Running {
    $r = Invoke-Helper 'status'
    return (($r.Lines -join ' ') -match 'running=True')
}

function Get-LogLength {
    if (Test-Path $logPath) { return (Get-Item $logPath).Length }
    return 0
}

function Read-LogFrom([long]$start) {
    if (-not (Test-Path $logPath)) { return '' }
    $share = [IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete
    $fs = [IO.File]::Open($logPath, [IO.FileMode]::Open, [IO.FileAccess]::Read, $share)
    try {
        if ($fs.Length -le $start) { return '' }
        $fs.Seek($start, [IO.SeekOrigin]::Begin) | Out-Null
        $ms = New-Object IO.MemoryStream
        $fs.CopyTo($ms)
        return [Text.Encoding]::ASCII.GetString($ms.ToArray())
    } finally {
        $fs.Dispose()
    }
}

function Copy-Log([string]$dest) {
    if (-not (Test-Path $logPath)) { return }
    $share = [IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete
    $src = [IO.File]::Open($logPath, [IO.FileMode]::Open, [IO.FileAccess]::Read, $share)
    try {
        $dst = [IO.File]::Create($dest)
        try { $src.CopyTo($dst) } finally { $dst.Dispose() }
    } finally {
        $src.Dispose()
    }
}

function Read-Findings([long]$start) {
    $fps = @(); $errors = @(); $addrs = @()
    foreach ($line in ((Read-LogFrom $start) -split "`n")) {
        $clean = $line.TrimEnd("`r")
        if ($clean -cmatch 'FRAMESTATS.*fps=([0-9.]+)') { $fps += [double]$Matches[1] }
        if ($clean -cmatch 'unregistered function at guest address (0x[0-9A-F]{8})') { $addrs += $Matches[1] }
        if ($clean -cmatch 'unregistered function at guest address 0x[0-9A-F]{8}|\[critical\]|FATAL') { $errors += $clean }
    }
    return @{ Fps = $fps; Errors = $errors; Addrs = $addrs }
}

function Hold-Pad([string]$pad, [int]$sec) {
    if ((Invoke-Helper 'hold' '-Instance' '1' '-Pad' $pad).Code -ne 0) { return $false }
    Start-Sleep -Seconds $sec
    Invoke-Helper 'release' '-Instance' '1' | Out-Null
    return $true
}

function Invoke-Play {
    $clock = [Diagnostics.Stopwatch]::StartNew()
    while ($clock.Elapsed.TotalSeconds -lt $PlaySec) {
        foreach ($stick in $sticks) {
            if ($clock.Elapsed.TotalSeconds -ge $PlaySec) { return }
            if (-not (Hold-Pad $stick 3)) { return }
        }
        if (-not (Hold-Pad 'LT RT' 2)) { return }
        if ((Invoke-Helper 'pad' '-Instance' '1' '-Pad' 'A' '-Hold' '150').Code -ne 0) { return }
    }
}

function Take-Shot([string]$label) {
    $r = Invoke-Helper 'shot' '-Instance' '1' '-Label' $label
    foreach ($line in $r.Lines) {
        if ($line -match '^shot (.+\.png)\s*$') { return $Matches[1] }
    }
    return ''
}

function Test-UserBack([datetime]$since) {
    return (Test-Path $userFlag) -and ((Get-Item $userFlag).LastWriteTime -ge $since)
}

function Format-Row($row) {
    return '| {0} | {1} | {2} | {3} | {4} |' -f $row.Chapter, $row.Status, $row.Fps, $row.Shot, $row.Error
}

function Write-Sweep($list) {
    $md = @('# Chapter sweep', '', '| chapter | status | fps mean | screenshot | first error |', '|---|---|---|---|---|')
    foreach ($row in $list) { $md += Format-Row $row }
    Set-Content -Path $sweepPath -Value $md -Encoding UTF8
}

function Invoke-Chapter([int]$n) {
    $label = 'ch{0:D2}' -f $n
    $logStart = Get-LogLength
    $since = Get-Date
    Invoke-Helper 'chapter' '-Chapter' "$n" '-TimeoutSec' '60' | Out-Null
    Start-Sleep -Seconds $LoadSec
    Invoke-Play
    $shotPath = Take-Shot $label
    $running = Test-Running
    $findings = Read-Findings $logStart

    $status = 'ok'
    if ($findings.Errors.Count) { $status = 'fatal' }
    # The idle guard kills the game when the user comes back.
    elseif (-not $running -and (Test-UserBack $script:gameStart)) { $status = 'interrupted' }
    elseif (-not $running) { $status = 'crash' }
    elseif (-not $findings.Fps.Count) { $status = 'no-load' }

    $fpsCell = '-'
    if ($findings.Fps.Count) {
        $mean = ($findings.Fps | Measure-Object -Average).Average
        $fpsCell = $mean.ToString('F1', [Globalization.CultureInfo]::InvariantCulture)
    }
    $errCell = '-'
    if ($findings.Errors.Count) {
        $errCell = $findings.Errors[0]
        if ($errCell.Length -gt 200) { $errCell = $errCell.Substring(0, 200) }
        $errCell = $errCell.Replace('|', '\|')
    }
    foreach ($a in $findings.Addrs) {
        if (-not $hints.Contains($a)) { $hints[$a] = $n }
    }

    $shotCell = '-'
    if ($shotPath) { $shotCell = Split-Path -Leaf $shotPath }
    if (Test-UserBack $since) {
        if ($shotPath -and (Test-Path $shotPath)) { Remove-Item -LiteralPath $shotPath -Force }
        $shotCell = 'discarded (user)'
    }

    return [pscustomobject]@{ Chapter = $label; Status = $status; Fps = $fpsCell; Shot = $shotCell; Error = $errCell }
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$rows = @()
try {
    Start-Game
    $n = $From
    while ($n -le $To) {
        $row = Invoke-Chapter $n
        Write-Output (Format-Row $row)
        if ($row.Status -eq 'interrupted') {
            Restart-Game
            continue
        }
        $rows += $row
        Write-Sweep $rows
        if ($row.Status -in 'fatal', 'crash') {
            Copy-Log (Join-Path $OutDir ('ch{0:D2}-{1}.log' -f $n, $row.Status))
            Restart-Game
        }
        $n++
    }
} finally {
    $stop = Invoke-Helper 'stop'
    if ($stop.Code -eq 3) { Write-Output 'the user came back during the sweep (out\idle-gate\user-returned.txt); their chapter screenshots were deleted' }
}

if ($hints.Count) {
    Write-Output 'suggested hints/runtime.toml lines (not written):'
    foreach ($addr in $hints.Keys) { Write-Output ('{0} = {{}}  # chapter {1:D2} sweep' -f $addr, $hints[$addr]) }
}
