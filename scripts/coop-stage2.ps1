# Stage 2 co-op scenario runner: drives one setup plus the transition, kill, checkpoint,
# leave-rejoin and travel scenarios through scripts\coop-instances.ps1, checks the '!list'
# controller lines after each step, and stores PASS/FAIL results plus per-instance game logs
# and screenshots under <OutDir>\<scenario>. Every run is idle-gated (setup waits for an
# idle PC before starting anything) and guarded (the coop-instances idle guard watches the
# run unless -NoGuard is passed through). The menu macros below were calibrated from
# Stage 1's net-join.ps1; re-check them if the menus change.
#
#   coop-stage2.ps1 -Scenario setup -OutDir out\stage2\s1 -SeedUser out\seed\user
#   coop-stage2.ps1 -Scenario transition -OutDir out\stage2\s1 -Level SP_AssaultIntro_P
#   coop-stage2.ps1 -Scenario kill-extra -OutDir out\stage2\s1
#   coop-stage2.ps1 -Scenario kill-host -OutDir out\stage2\s1
#   coop-stage2.ps1 -Scenario checkpoint -OutDir out\stage2\s1
#   coop-stage2.ps1 -Scenario leave-rejoin -OutDir out\stage2\s1
#   coop-stage2.ps1 -Scenario travel -OutDir out\stage2\s1 -TravelChapter 5
#   coop-stage2.ps1 -Scenario all -OutDir out\stage2\s1 -SeedUser out\seed\user -Players 4 -DevCheats
#
# Scenarios other than setup and all expect the instances to be already running from an
# earlier setup; only all stops them at the end.
param(
    [Parameter(Mandatory)]
    [ValidateSet('setup', 'transition', 'kill-extra', 'kill-host', 'checkpoint', 'leave-rejoin', 'travel', 'all')]
    [string]$Scenario,
    [Parameter(Mandatory)]
    [string]$OutDir,
    [string]$SeedUser = '',
    [string]$SeedCache = '',
    [int]$Players = 4,
    [switch]$DevCheats,
    [string]$Level = 'SP_AssaultIntro_P',
    [int]$TravelChapter = 5,
    [string]$Exe = '',
    [switch]$NoGuard,
    [switch]$NoIdleGate,
    [int]$TitleWaitSec = 0,
    [int]$LoadSec = 0,
    [double]$WaitScale = 1.0,
    [string]$Common = '',
    [string]$PerInstance = ''
)
$ErrorActionPreference = 'Stop'

# Presses last 150 ms: with eight copies loading at once, 80 ms presses were sometimes missed.
# Calibrated menu macros from Stage 1's net-join.ps1; re-check them if the menus change.
# Each macro value is a coop-instances 'macro' step string.
if ($TitleWaitSec -le 0) { $TitleWaitSec = 50 }
if ($LoadSec -le 0) { $LoadSec = 30 }
$PressStartMacro = 'pad:START:150,wait:1500'
$MainMenuMacro = 'pad:DOWN:150,wait:300'
$HostLobbyMacro = 'pad:A:150,wait:3000,pad:A:150,wait:3000,pad:A:150,wait:6000'
$OpenBrowserMacro = 'pad:A:150,wait:3000,pad:DOWN:150,wait:1000,pad:A:150,wait:6000'
$JoinMacro = 'pad:A:150,wait:15000'
$LaunchMacro = 'pad:A:150'
$JoinListedMacro = 'pad:A:150'
$SpawnMacro = 'pad:RT:300,wait:5000'

$script:CheckCount = 0
$script:FailCount = 0

$ScriptDir = $PSScriptRoot
$HelperPath = Join-Path $ScriptDir 'coop-instances.ps1'
if (-not (Test-Path $HelperPath)) { throw "helper not found: $HelperPath" }
$OutRoot = $OutDir
if (-not [IO.Path]::IsPathRooted($OutRoot)) { $OutRoot = Join-Path (Split-Path -Parent $ScriptDir) $OutRoot }

function Invoke-Instances {
    $helper = $HelperPath
    $callArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $helper) + $args
    $callArgs += @('-OutDir', $OutDir)
    if ($Exe) { $callArgs += @('-Exe', $Exe) }
    if ($NoGuard) { $callArgs += '-NoGuard' }
    $prevErr = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $raw = @(powershell @callArgs 2>&1)
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prevErr
    }
    $lines = @()
    foreach ($item in $raw) { $lines += "$item" }
    foreach ($line in $lines) { Write-Host "$(Get-Date -Format 'HH:mm:ss.fff') $line" }
    return @{ Code = $code; Lines = $lines }
}

function Wait-IdleGate {
    if ($NoIdleGate) { return }
    $gate = 'C:\personal\gow2-recomp\out\idle-gate\wait-idle.ps1'
    while ($true) {
        powershell -NoProfile -ExecutionPolicy Bypass -File $gate -Seconds 180 -MaxWaitSec 540
        $code = $LASTEXITCODE
        if ($code -eq 0) { return }
        if ($code -eq 75) {
            Write-Output 'waiting for an idle PC'
            continue
        }
        throw "idle gate failed with exit $code"
    }
}

function Send-Command {
    param([int]$Index, [string]$Text)
    return Invoke-Instances 'cmd' '-Instance' "$Index" '-Command' $Text '-TimeoutSec' '60'
}

function Read-SharedText {
    param([string]$Path, [long]$Start)
    if (-not (Test-Path $Path)) { return '' }
    $fs = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        if ($fs.Length -le $Start) { return '' }
        $fs.Seek($Start, [IO.SeekOrigin]::Begin) | Out-Null
        $len = [int]($fs.Length - $Start)
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

function Get-ListLines {
    Send-Command 1 '!list' | Out-Null
    $dir = Join-Path $OutRoot 'inst1'
    $offPath = Join-Path $dir 'log-offset.txt'
    $start = [long]0
    if (Test-Path $offPath) {
        $txt = (Get-Content $offPath -Raw).Trim()
        [long]::TryParse($txt, [ref]$start) | Out-Null
    }
    $text = Read-SharedText (Join-Path $dir 'game.log') $start
    $rx = "DEVCMD   \[(\d+)\] (\S+) class=(\S+) pc=([01]) player='([^']*)' pawn=(\S+)\((\S+)\) health=(\S+) god=([01]) state=(\S+)"
    $found = @()
    foreach ($m in [regex]::Matches($text, $rx)) {
        $g = $m.Groups
        $found += [pscustomobject]@{
            Index = [int]$g[1].Value
            Name = $g[2].Value
            Class = $g[3].Value
            IsPc = ($g[4].Value -eq '1')
            Player = $g[5].Value
            Pawn = $g[6].Value
            PawnClass = $g[7].Value
            Health = $g[8].Value
            God = ($g[9].Value -eq '1')
            State = $g[10].Value
        }
    }
    return $found
}

function Add-Result {
    param([string]$Scenario, [bool]$Ok, [string]$Check, [string]$Evidence)
    $script:CheckCount++
    $dir = Join-Path $OutRoot $Scenario
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $word = 'PASS'
    if (-not $Ok) {
        $word = 'FAIL'
        $script:FailCount++
    }
    Add-Content -Path (Join-Path $dir 'result.txt') -Value "$word ${Check}: $Evidence"
}

function Add-Note {
    param([string]$Scenario, [string]$Text)
    $dir = Join-Path $OutRoot $Scenario
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    Add-Content -Path (Join-Path $dir 'result.txt') -Value "INFO $Text"
}

function Copy-SharedFile {
    param([string]$Source, [string]$Dest)
    $fs = [IO.File]::Open($Source, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        $out = [IO.File]::Open($Dest, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::Read)
        try {
            $fs.CopyTo($out)
        } finally {
            $out.Dispose()
        }
    } finally {
        $fs.Dispose()
    }
}

function Move-ScenarioShots {
    param([string]$Scenario, [string]$Label)
    $dir = Join-Path $OutRoot $Scenario
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    foreach ($shot in @(Get-ChildItem -Path $OutRoot -File -Filter "*-$Label.png")) {
        Move-Item -Force -Path $shot.FullName -Destination (Join-Path $dir $shot.Name)
    }
}

function Save-Evidence {
    param([string]$Scenario)
    $dir = Join-Path $OutRoot $Scenario
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    for ($i = 1; $i -le $Players; $i++) {
        $log = Join-Path (Join-Path $OutRoot "inst$i") 'game.log'
        if (Test-Path $log) {
            Copy-SharedFile $log (Join-Path $dir "game-i$i.log")
        }
    }
    Invoke-Instances 'shots' '-Label' $Scenario | Out-Null
    Move-ScenarioShots $Scenario $Scenario
}

function Wait-Scaled {
    param([double]$Seconds)
    $ms = [int]($Seconds * 1000.0 * $WaitScale)
    if ($ms -gt 0) { Start-Sleep -Milliseconds $ms }
}

function Get-PlayerSummary {
    param([array]$Pawns)
    $bits = @()
    foreach ($pc in $Pawns) {
        $bits += "$($pc.Player) pawn=$($pc.Pawn) state=$($pc.State)"
    }
    if ($bits.Count -eq 0) { return 'none' }
    return ($bits -join ', ')
}

function Add-PlayerChecks {
    param([string]$Scenario, [array]$Pcs)
    Add-Result $Scenario ($Pcs.Count -eq $Players) "$Players player controllers" "found $($Pcs.Count)"
    $nopawn = @($Pcs | Where-Object { $_.Pawn -eq 'None' })
    # No player lines at all is a failure, not a vacuous pass.
    $ok = ($Pcs.Count -gt 0) -and ($nopawn.Count -eq 0)
    Add-Result $Scenario $ok 'every player has a pawn' (Get-PlayerSummary $Pcs)
}

function Invoke-CheckedMacro {
    param([string]$Scenario, [int]$Index, [string]$Steps, [string]$What)
    $r = Invoke-Instances 'macro' '-Instance' "$Index" '-Steps' $Steps
    if ($r.Code -ne 0) {
        Add-Result $Scenario $false "macro $What i$Index" ($r.Lines -join ' | ')
        return $false
    }
    return $true
}

function Invoke-SetupScenario {
    Write-Output '=== setup: start, join lobby, launch, spawn ==='
    Wait-IdleGate
    $startArgs = @('start', '-Count', "$Players", '-Coop', '-Players', "$Players")
    if ($DevCheats) { $startArgs += '-DevCheats' }
    $startArgs += @('-SeedUser', $SeedUser)
    if ($SeedCache) { $startArgs += @('-SeedCache', $SeedCache) }
    if ($Common) { $startArgs += @('-Common', $Common) }
    if ($PerInstance) { $startArgs += @('-PerInstance', $PerInstance) }
    Invoke-Instances @startArgs | Out-Null
    for ($i = 1; $i -le $Players; $i++) {
        $r = Invoke-Instances 'waitlog' '-Instance' "$i" '-Pattern' 'DEVCMD on:' '-FromStart' '-TimeoutSec' '120'
        Add-Result 'setup' ($r.Code -eq 0) "pump on i$i" ($r.Lines -join ' | ')
    }
    Wait-Scaled $TitleWaitSec
    for ($i = 1; $i -le $Players; $i++) {
        if (-not (Invoke-CheckedMacro 'setup' $i $PressStartMacro 'press start')) { Save-Evidence 'setup'; return }
    }
    Wait-Scaled 9
    for ($i = 1; $i -le $Players; $i++) {
        if (-not (Invoke-CheckedMacro 'setup' $i $MainMenuMacro 'main menu')) { Save-Evidence 'setup'; return }
    }
    Wait-Scaled 1
    if (-not (Invoke-CheckedMacro 'setup' 1 $HostLobbyMacro 'host lobby')) { Save-Evidence 'setup'; return }
    $r = Invoke-Instances 'waitlog' '-Instance' '1' '-Pattern' 'XGISessionCreate .*host=true' '-TimeoutSec' '90' '-SinceStart'
    Add-Result 'setup' ($r.Code -eq 0) 'host session created' ($r.Lines -join ' | ')
    if (-not (Invoke-CheckedMacro 'setup' 2 $OpenBrowserMacro 'open browser')) { Save-Evidence 'setup'; return }
    if (-not (Invoke-CheckedMacro 'setup' 2 $JoinMacro 'join')) { Save-Evidence 'setup'; return }
    $r = Invoke-Instances 'waitlog' '-Instance' '2' '-Pattern' 'XGISessionJoinRemote .*members=' '-TimeoutSec' '90' '-SinceStart'
    Add-Result 'setup' ($r.Code -eq 0) 'client 2 joined' ($r.Lines -join ' | ')
    if (-not (Invoke-CheckedMacro 'setup' 1 $LaunchMacro 'launch')) { Save-Evidence 'setup'; return }
    Wait-Scaled $LoadSec
    if (-not (Invoke-CheckedMacro 'setup' 2 $SpawnMacro 'spawn')) { Save-Evidence 'setup'; return }
    for ($i = 3; $i -le $Players; $i++) {
        if (-not (Invoke-CheckedMacro 'setup' $i $OpenBrowserMacro 'open browser')) { Save-Evidence 'setup'; return }
        if (-not (Invoke-CheckedMacro 'setup' $i $JoinListedMacro 'join listed')) { Save-Evidence 'setup'; return }
        $r = Invoke-Instances 'waitlog' '-Instance' "$i" '-Pattern' 'XGISessionJoinRemote .*members=' '-TimeoutSec' '90' '-SinceStart'
        Add-Result 'setup' ($r.Code -eq 0) "client $i joined" ($r.Lines -join ' | ')
        Wait-Scaled $LoadSec
        if (-not (Invoke-CheckedMacro 'setup' $i $SpawnMacro 'spawn')) { Save-Evidence 'setup'; return }
    }
    $list = @(Get-ListLines)
    $pcs = @($list | Where-Object { $_.IsPc })
    Add-PlayerChecks 'setup' $pcs
    foreach ($pc in $pcs) {
        Add-Note 'setup' "$($pc.Player) pawn=$($pc.PawnClass) state=$($pc.State)"
    }
    Save-Evidence 'setup'
}

function Invoke-TransitionScenario {
    Write-Output '=== transition: mapchange and commit ==='
    $r = Send-Command 1 '!listmapchanges'
    Add-Note 'transition' ($r.Lines -join ' | ')
    $r = Send-Command 1 "!mapchange $Level"
    Add-Result 'transition' ($r.Code -eq 0) "mapchange $Level" ($r.Lines -join ' | ')
    Wait-Scaled 25
    $r = Send-Command 1 '!mapcommit'
    Add-Result 'transition' ($r.Code -eq 0) 'mapcommit' ($r.Lines -join ' | ')
    Wait-Scaled 2
    Invoke-Instances 'shots' '-Label' 'transition-2s' | Out-Null
    Move-ScenarioShots 'transition' 'transition-2s'
    Wait-Scaled 8
    Invoke-Instances 'shots' '-Label' 'transition-10s' | Out-Null
    Move-ScenarioShots 'transition' 'transition-10s'
    Wait-Scaled 20
    Invoke-Instances 'shots' '-Label' 'transition-30s' | Out-Null
    Move-ScenarioShots 'transition' 'transition-30s'
    $list = @(Get-ListLines)
    $pcs = @($list | Where-Object { $_.IsPc })
    Add-PlayerChecks 'transition' $pcs
    $dead = @($pcs | Where-Object { $_.State -eq 'Dead' })
    Add-Result 'transition' ($dead.Count -eq 0) 'nobody in state Dead' (Get-PlayerSummary $pcs)
    Save-Evidence 'transition'
}

function Invoke-KillExtraScenario {
    Write-Output '=== kill-extra: P3 suicide, expect respawn ==='
    Wait-Scaled 12
    $r = Send-Command 3 'suicide'
    Add-Note 'kill-extra' ($r.Lines -join ' | ')
    Wait-Scaled 15
    $list = @(Get-ListLines)
    $pcs = @($list | Where-Object { $_.IsPc })
    $p3 = @($pcs | Where-Object { $_.Player -eq 'P3' })
    $p3ev = 'P3 missing'
    if ($p3.Count -gt 0) { $p3ev = Get-PlayerSummary $p3 }
    Add-Result 'kill-extra' ($p3.Count -eq 1 -and $p3[0].Pawn -ne 'None') 'P3 has a pawn again' $p3ev
    $p1 = @($pcs | Where-Object { $_.Player -eq 'P1' })
    $p1ev = 'P1 missing'
    if ($p1.Count -gt 0) { $p1ev = Get-PlayerSummary $p1 }
    Add-Result 'kill-extra' ($p1.Count -eq 1 -and $p1[0].State -ne 'Dead') 'P1 not Dead' $p1ev
    foreach ($c in $list) {
        Add-Note 'kill-extra' "$($c.Name) pc=$($c.IsPc) player=$($c.Player) pawn=$($c.PawnClass) state=$($c.State)"
    }
    Save-Evidence 'kill-extra'
}

function Invoke-KillHostScenario {
    Write-Output '=== kill-host: kill P1, game over is expected ==='
    $r = Send-Command 1 '!kill P1'
    Add-Result 'kill-host' ($r.Code -eq 0) 'kill P1' ($r.Lines -join ' | ')
    Wait-Scaled 8
    Invoke-Instances 'shots' '-Label' 'kill-host-8s' | Out-Null
    Move-ScenarioShots 'kill-host' 'kill-host-8s'
    $list = @(Get-ListLines)
    foreach ($c in $list) {
        Add-Note 'kill-host' "$($c.Name) pc=$($c.IsPc) player=$($c.Player) pawn=$($c.PawnClass) state=$($c.State)"
    }
    Save-Evidence 'kill-host'
}

function Invoke-CheckpointScenario {
    Write-Output '=== checkpoint: save, move P3, load ==='
    $r = Send-Command 1 'SaveCheckpoint'
    Add-Result 'checkpoint' ($r.Code -eq 0) 'SaveCheckpoint' ($r.Lines -join ' | ')
    if (-not (Invoke-CheckedMacro 'checkpoint' 3 'pad:LY=1:3000' 'walk away')) { Save-Evidence 'checkpoint'; return }
    $r = Send-Command 1 'LoadCheckpoint'
    Add-Result 'checkpoint' ($r.Code -eq 0) 'LoadCheckpoint' ($r.Lines -join ' | ')
    Wait-Scaled 30
    $list = @(Get-ListLines)
    $pcs = @($list | Where-Object { $_.IsPc })
    Add-PlayerChecks 'checkpoint' $pcs
    Save-Evidence 'checkpoint'
}

function Get-OrphanExtras {
    param([array]$List)
    return @($List | Where-Object { (-not $_.IsPc) -and ($_.Class -eq 'GearAI_Dom') -and (($_.PawnClass -eq 'GearPawn_COGBenjaminCarmine') -or ($_.PawnClass -eq 'GearPawn_COGGear')) })
}

function Add-OrphanCheck {
    param([string]$Scenario, [array]$List, [string]$Check)
    $orphans = @(Get-OrphanExtras $List)
    $names = 'none'
    if ($orphans.Count -gt 0) { $names = (($orphans | ForEach-Object { $_.Name }) -join ', ') }
    Add-Result $Scenario ($orphans.Count -eq 0) $Check $names
}

function Invoke-LeaveRejoinScenario {
    Write-Output '=== leave-rejoin: stop last instance, rejoin ==='
    Send-Command 1 "!mark leave-$Players" | Out-Null
    Invoke-Instances 'stopone' '-Instance' "$Players" | Out-Null
    $r = Invoke-Instances 'waitlog' '-Instance' '1' '-Pattern' 'XGISessionLeaveRemote' '-TimeoutSec' '120'
    Add-Result 'leave-rejoin' ($r.Code -eq 0) 'host saw the leave' ($r.Lines -join ' | ')
    Wait-Scaled 3
    $list = @(Get-ListLines)
    $pcs = @($list | Where-Object { $_.IsPc })
    Add-Note 'leave-rejoin' (Get-PlayerSummary $pcs)
    $want = $Players - 1
    Add-Result 'leave-rejoin' ($pcs.Count -eq $want) 'player count dropped' "found $($pcs.Count), want $want"
    Add-OrphanCheck 'leave-rejoin' $list 'no AI-driven extra after the leave'
    Invoke-Instances 'startone' '-Instance' "$Players" | Out-Null
    $r = Invoke-Instances 'waitlog' '-Instance' "$Players" '-Pattern' 'DEVCMD on:' '-TimeoutSec' '120'
    Add-Note 'leave-rejoin' ($r.Lines -join ' | ')
    Wait-Scaled $TitleWaitSec
    if (-not (Invoke-CheckedMacro 'leave-rejoin' $Players $PressStartMacro 'press start')) { Save-Evidence 'leave-rejoin'; return }
    Wait-Scaled 9
    if (-not (Invoke-CheckedMacro 'leave-rejoin' $Players $MainMenuMacro 'main menu')) { Save-Evidence 'leave-rejoin'; return }
    Wait-Scaled 1
    if (-not (Invoke-CheckedMacro 'leave-rejoin' $Players $OpenBrowserMacro 'open browser')) { Save-Evidence 'leave-rejoin'; return }
    if (-not (Invoke-CheckedMacro 'leave-rejoin' $Players $JoinListedMacro 'join listed')) { Save-Evidence 'leave-rejoin'; return }
    Wait-Scaled $LoadSec
    if (-not (Invoke-CheckedMacro 'leave-rejoin' $Players $SpawnMacro 'spawn')) { Save-Evidence 'leave-rejoin'; return }
    $list = @(Get-ListLines)
    $pcs = @($list | Where-Object { $_.IsPc })
    Add-PlayerChecks 'leave-rejoin' $pcs
    Add-OrphanCheck 'leave-rejoin' $list 'no AI-driven extra after the rejoin'
    Save-Evidence 'leave-rejoin'
}

function Invoke-TravelScenario {
    Write-Output '=== travel: chapter lobby and census ==='
    $r = Invoke-Instances 'chapter' '-Chapter' "$TravelChapter"
    Add-Result 'travel' ($r.Code -eq 0) "chapter $TravelChapter" ($r.Lines -join ' | ')
    for ($i = 1; $i -le $Players; $i++) {
        $r = Invoke-Instances 'waitlog' '-Instance' "$i" '-Pattern' 'COOPCENSUS pawns' '-TimeoutSec' '180'
        Add-Result 'travel' ($r.Code -eq 0) "census i$i" ($r.Lines -join ' | ')
    }
    $list = @(Get-ListLines)
    $pcs = @($list | Where-Object { $_.IsPc })
    Add-PlayerChecks 'travel' $pcs
    Save-Evidence 'travel'
}

if (($Scenario -eq 'setup' -or $Scenario -eq 'all') -and (-not $SeedUser)) {
    throw '-SeedUser is required for the setup and all scenarios'
}
switch ($Scenario) {
    'setup' { Invoke-SetupScenario }
    'transition' { Invoke-TransitionScenario }
    'kill-extra' { Invoke-KillExtraScenario }
    'kill-host' { Invoke-KillHostScenario }
    'checkpoint' { Invoke-CheckpointScenario }
    'leave-rejoin' { Invoke-LeaveRejoinScenario }
    'travel' { Invoke-TravelScenario }
    'all' {
        Invoke-SetupScenario
        Invoke-TransitionScenario
        Invoke-KillExtraScenario
        Invoke-CheckpointScenario
        Invoke-TravelScenario
        Invoke-LeaveRejoinScenario
        Invoke-KillHostScenario
        Invoke-Instances 'stop' | Out-Null
    }
}
Write-Output "Stage 2 scenarios: $script:CheckCount checks, $script:FailCount failed; results in $OutDir"
if ($script:FailCount -gt 0) { exit 1 }
exit 0
