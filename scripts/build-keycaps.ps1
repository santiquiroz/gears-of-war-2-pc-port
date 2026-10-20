<#
.SYNOPSIS
  Builds the keycap texture pack (the keyboard prompts drawn as keys) from your own game.

.DESCRIPTION
  The game draws its button prompts with one font; two 256x256 pages hold the Xbox buttons and
  the spinner. tools\keycaps\pack.py draws a keycap for every key a keybind can name into the free
  space of those pages and keeps the glyphs the game still needs. The pack holds game art, so build
  it yourself and do not share it. Steps:

    1. installs the Python packages the tool needs (pillow numpy resvg-py fonttools, for your user),
    2. when your texture dump lacks the two font pages, starts gow2.exe with texture_dump on. Play
       to the difficulty screen (Solo Campaign), then quit the game,
    3. writes textures\4D53082D\replace\*.dds and keycaps.id next to gow2.exe.

  After that gow2.exe shows keycaps (gow2_keycaps = true, the default) and switches to the pad's
  buttons while a controller is connected. The pack must match the exe's keycap table: run this
  again after a build that changes tools\keycaps or src\gow2_keycap_layout.inc.

.PARAMETER Exe
  The game exe (default out\build\release\gow2.exe).

.PARAMETER GameFolder
  Your game folder, passed to the game when it has to be started to dump the pages.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\build-keycaps.ps1 -GameFolder C:\Games\GoW2
#>
param(
    [string]$Exe = '',
    [string]$GameFolder = '',
    [string]$TextureRoot = ''   # texture pack root, when it is not the textures folder next to the exe
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
if (-not $Exe) { $Exe = Join-Path $root 'out\build\release\gow2.exe' }
function Fail($msg) { Write-Host "ERROR: $msg" -ForegroundColor Red; exit 1 }
function Step($msg) { Write-Host "`n== $msg" -ForegroundColor Cyan }

if (-not (Test-Path $Exe)) { Fail "$Exe not found: run scripts\build.ps1 first." }
if (-not (Get-Command py -ErrorAction SilentlyContinue)) { Fail 'Python 3 (the py launcher) was not found.' }
$exeDir = Split-Path -Parent (Resolve-Path $Exe)
$packRoot = if ($TextureRoot) { $TextureRoot } else { Join-Path $exeDir 'textures' }
$pack = Join-Path $packRoot '4D53082D'
$dump = Join-Path $pack 'dump'
$replace = Join-Path $pack 'replace'

Step 'Python packages'
py -m pip install --user --quiet pillow numpy resvg-py fonttools
if ($LASTEXITCODE -ne 0) { Fail 'pip could not install the Python packages.' }

$info = Get-Content (Join-Path $root 'tools\keycaps\font_info.json') -Raw | ConvertFrom-Json
$atlas = Get-Content (Join-Path $root 'tools\keycaps\atlas_info.json') -Raw | ConvertFrom-Json
function Missing-Pages {
    $info.page_hashes | Where-Object { -not (Get-ChildItem $dump -Filter "$_`_*.dds" -ErrorAction SilentlyContinue) }
}

if ((Missing-Pages) -or -not (Get-ChildItem $dump -Filter "$($atlas.hash)_*.dds" -ErrorAction SilentlyContinue)) {
    Step 'Dumping the font pages and the tutorial pictograms'
    Write-Host 'gow2.exe starts now and plays itself for about two minutes (title screen, Solo Campaign,'
    Write-Host 'the difficulty screen, then the first level). Do not press keys in it; it closes by itself.'
    $work = Join-Path $env:TEMP 'gow2-keycaps'
    New-Item -ItemType Directory -Force $work | Out-Null
    $cmd = Join-Path $work 'cmd.txt'; [IO.File]::WriteAllText($cmd, '')
    $pad = Join-Path $work 'pad.txt'; [IO.File]::WriteAllText($pad, '')
    $arguments = @('--texture_dump=true', '--gow2_keycaps=false', '--gow2_prompts=controller',
        "--gow2_dev_command_file=$cmd", "--gow2_test_input=$pad")
    if ($TextureRoot) { $arguments += "--texture_pack_dir=$TextureRoot" }
    if ($GameFolder) { $arguments += "--game_data_root=$GameFolder" }
    $game = Start-Process -FilePath $Exe -ArgumentList $arguments -WorkingDirectory $exeDir -PassThru
    function Press($buttons, $holdMs = 300) {
        Set-Content $pad $buttons -Encoding ascii; Start-Sleep -Milliseconds $holdMs
        Set-Content $pad '' -Encoding ascii
    }
    try {
        Start-Sleep -Seconds 55
        Press 'START'; Start-Sleep -Seconds 6          # title screen -> main menu
        Press 'A'; Start-Sleep -Seconds 6              # Solo Campaign -> save slots
        Press 'A'; Start-Sleep -Seconds 8              # slot -> Select Difficulty (its spinner is the second font page)
        [IO.File]::AppendAllText($cmd, "servertravel geargame_p?loadcheckpoint=0?chapter=1`n")  # first level: the HUD textures
        Start-Sleep -Seconds 90
    } finally {
        if (-not $game.HasExited) { Stop-Process -Id $game.Id -Force }
    }
    $missing = @(Missing-Pages)
    if ($missing.Count) { Fail "The dump still lacks font page(s) $($missing -join ', '). Run this again." }
    if (-not (Get-ChildItem $dump -Filter "$($atlas.hash)_*.dds" -ErrorAction SilentlyContinue)) {
        Write-Host 'Warning: the tutorial pictograms were not dumped; the keyboard variant of them is skipped.' -ForegroundColor Yellow
    }
}

Step 'Building the pack'
py (Join-Path $root 'tools\keycaps\pack.py') --dump $dump --out $replace
if ($LASTEXITCODE -ne 0) { Fail 'pack.py failed.' }
Write-Host "`nDone: $replace" -ForegroundColor Green
Write-Host 'Start gow2.exe; the prompts follow your keybinds and the connected controller.'
