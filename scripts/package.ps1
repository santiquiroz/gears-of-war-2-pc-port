<#
.SYNOPSIS
  Packages an already-built gow2-recomp tree into a Windows release zip (no game files).

.DESCRIPTION
  Copies the built binaries, the Visual C++ runtime DLLs, repo docs, the
  online-host scripts under scripts\ and a generated QUICKSTART.txt into a
  staging folder, and compresses it to
  <OutDir>\gow2-recomp-<Version>-win64.zip, plus a .sha256 sidecar file.

  It also downloads texconv and Real-ESRGAN-ncnn-vulkan for PC Options' Build texture pack
  (cached under out\texture-pack-tools, every file checked against its pinned SHA-256; a
  mismatch stops packaging) and copies only the pinned files into tools\texconv and
  tools\realesrgan. Their upstream LICENSE files go into tools\licenses, and packaging stops
  when THIRD_PARTY_NOTICES.md does not quote one of them word for word.

  The zip never contains game data. Packaging is refused (exit 1) if a file
  named default.xex or any *.xxx / *.tfc / *.bik file would end up in staging.

  The zip never contains NVIDIA DLSS either: packaging is refused (exit 1) if a
  staged .dll or .exe holds the name nvsdk_ngx / NVSDK_NGX (the NGX library) or
  nvngx (the DLSS runtime) in any case. Build the release DLLs with
  scripts\build-sdk.ps1 -NoDlss.

.PARAMETER Version
  Release version string, e.g. "0.1.1". Used in the zip and top folder name.

.PARAMETER BuildDir
  Folder with the already-built binaries. Default: out\build\release.

.PARAMETER SdkDir
  Optional folder with source-built rexruntime.dll / rexgpu-xenos.dll and AMD
  FidelityFX DLLs. When it exists, files found there replace the BuildDir ones
  and every amd_fidelityfx_*.dll found there is added. Default: out\sdk-nodlss
  (scripts\build-sdk.ps1 -NoDlss). out\sdk, the everyday build, links DLSS and
  is refused.

.PARAMETER OutDir
  Output folder for the zip and staging area. Default: out\package.

.PARAMETER Config
  Settings file shipped as gow2.toml. Default: config\gow2.toml.

.EXAMPLE
  .\scripts\build-sdk.ps1 -NoDlss
  .\scripts\package.ps1 -Version "0.1.1"
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [string]$BuildDir = 'out\build\release',

    [string]$SdkDir = 'out\sdk-nodlss',

    [string]$OutDir = 'out\package',

    [string]$Config = 'config\gow2.toml'
)

$ErrorActionPreference = 'Stop'

# Find-NgxBinaries: NVIDIA NGX names left in a built binary.
. (Join-Path $PSScriptRoot 'ngx-scan.ps1')

function Fail([string]$Message) {
    Write-Host "ERROR: $Message" -ForegroundColor Red
    exit 1
}

function Step([string]$Message) {
    Write-Host "`n== $Message" -ForegroundColor Cyan
}

function Get-RepoRoot {
    return (Split-Path -Parent $PSScriptRoot)
}

function Resolve-RootedPath([string]$Root, [string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) { return $Path }
    return (Join-Path $Root $Path)
}

function New-CleanDir([string]$Path) {
    if (Test-Path $Path) { Remove-Item -Recurse -Force $Path }
    New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

function Copy-BuildBinaries([string]$FromDir, [string]$StageDir) {
    $required = @('gow2.exe', 'rexruntime.dll', 'rexgpu-xenos.dll', 'gamecontrollerdb.txt')
    foreach ($name in $required) {
        $src = Join-Path $FromDir $name
        if (-not (Test-Path $src)) {
            Fail "Required file missing in BuildDir: $src"
        }
        Copy-Item -Force $src (Join-Path $StageDir $name)
    }
}

function Copy-SdkOverlay([string]$Dir, [string]$StageDir) {
    if (-not (Test-Path $Dir)) { return }
    foreach ($name in @('rexruntime.dll', 'rexgpu-xenos.dll')) {
        $src = Join-Path $Dir $name
        if (Test-Path $src) {
            Copy-Item -Force $src (Join-Path $StageDir $name)
            Write-Host "SdkDir override: $name"
        }
    }
    # nvngx_dlss.dll stays out: NVIDIA's DLSS SDK license does not fit an open-source release
    # (THIRD_PARTY_NOTICES.md, "NVIDIA DLSS").
    $ffx = Get-ChildItem -Path $Dir -Recurse -File -Filter 'amd_fidelityfx_*.dll' -ErrorAction SilentlyContinue
    foreach ($f in $ffx) {
        Copy-Item -Force $f.FullName (Join-Path $StageDir $f.Name)
        Write-Host "SdkDir FidelityFX: $($f.Name)"
    }
}

function Find-VcRuntimeDir {
    $vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        Fail "vswhere.exe not found at $vswhere. Install Visual Studio or the Build Tools."
    }
    $needed = @('msvcp140.dll', 'msvcp140_atomic_wait.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')
    $installs = & $vswhere -all -products * -property installationPath
    if (-not $installs) {
        Fail 'No Visual Studio / Build Tools installation found via vswhere.exe.'
    }
    $candidates = @()
    foreach ($install in $installs) {
        $msvcBase = Join-Path $install 'VC\Redist\MSVC'
        if (-not (Test-Path $msvcBase)) { continue }
        $verDirs = Get-ChildItem -Path $msvcBase -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending
        foreach ($ver in $verDirs) {
            $crtDirs = Get-ChildItem -Path (Join-Path $ver.FullName 'x64') -Directory -Filter 'Microsoft.VC14*.CRT' -ErrorAction SilentlyContinue |
                Sort-Object Name -Descending
            foreach ($crt in $crtDirs) { $candidates += $crt.FullName }
        }
    }
    $sorted = $candidates | Sort-Object -Descending
    foreach ($dir in $sorted) {
        $ok = $true
        foreach ($dll in $needed) {
            if (-not (Test-Path (Join-Path $dir $dll))) { $ok = $false; break }
        }
        if ($ok) { return $dir }
    }
    Fail 'VC++ runtime DLLs (msvcp140.dll, msvcp140_atomic_wait.dll, vcruntime140.dll, vcruntime140_1.dll) not found under any VC\Redist\MSVC\<ver>\x64\Microsoft.VC14*.CRT\ folder.'
    return $null
}

function Copy-VcRuntime([string]$StageDir) {
    $crtDir = Find-VcRuntimeDir
    Write-Host "VC++ redist: $crtDir"
    foreach ($dll in @('msvcp140.dll', 'msvcp140_atomic_wait.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')) {
        Copy-Item -Force (Join-Path $crtDir $dll) (Join-Path $StageDir $dll)
    }
}

function Copy-RepoDocs([string]$Root, [string]$StageDir, [string]$ConfigPath) {
    foreach ($name in @('LICENSE', 'THIRD_PARTY_NOTICES.md', 'README.md')) {
        $src = Join-Path $Root $name
        if (-not (Test-Path $src)) {
            Fail "Required repo file missing: $src"
        }
        Copy-Item -Force $src (Join-Path $StageDir $name)
    }
    if (-not (Test-Path $ConfigPath)) { Fail "Settings file not found: $ConfigPath" }
    Copy-Item -Force $ConfigPath (Join-Path $StageDir 'gow2.toml')
    # Texture pack helper (Python 3 + Pillow), see README "Texture packs".
    $tools = Join-Path $StageDir 'tools'
    New-Item -ItemType Directory -Force -Path $tools | Out-Null
    Copy-Item -Force (Join-Path (Join-Path $Root 'tools') 'texpack.py') (Join-Path $tools 'texpack.py')
    # Online-host helpers: the in-game "Host over the internet" option runs
    # scripts\host-online.ps1 from the folder next to gow2.exe. Copy-Item keeps
    # the files byte-identical (CRLF or LF as in the repository).
    $onlineScripts = Join-Path $StageDir 'scripts'
    New-Item -ItemType Directory -Force -Path $onlineScripts | Out-Null
    foreach ($scriptName in @('host-online.ps1', 'host-online-upnp.ps1', 'host-online-state.ps1')) {
        Copy-Item -Force (Join-Path (Join-Path $Root 'scripts') $scriptName) (Join-Path $onlineScripts $scriptName)
    }
}

# Keycap prompts without Python (issue #10): build-keycaps.ps1 next to gow2.exe runs the frozen
# keycaps\keycaps-pack.exe on the player's own texture dump; no game art is in the zip.
function Copy-KeycapTool([string]$Root, [string]$StageDir) {
    & (Join-Path $PSScriptRoot 'build-keycaps-tool.ps1')
    if ($LASTEXITCODE -ne 0) { Fail 'scripts\build-keycaps-tool.ps1 failed.' }
    $dist = Join-Path $Root 'out\keycaps-tool\dist'
    $tool = Join-Path $StageDir 'keycaps'
    New-Item -ItemType Directory -Force -Path $tool | Out-Null
    Copy-Item -Force (Join-Path $dist 'keycaps-pack.exe') (Join-Path $tool 'keycaps-pack.exe')
    Copy-Item -Recurse -Force (Join-Path $dist 'licenses') (Join-Path $tool 'licenses')
    foreach ($name in 'font_info.json', 'atlas_info.json') {
        Copy-Item -Force (Join-Path $Root "tools\keycaps\$name") (Join-Path $tool $name)
    }
    Copy-Item -Force (Join-Path $Root 'scripts\build-keycaps.ps1') (Join-Path $StageDir 'build-keycaps.ps1')
    $cmd = @('@echo off', 'powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-keycaps.ps1" %*', 'pause')
    Set-Content -Path (Join-Path $StageDir 'Build keycap pack.cmd') -Value $cmd -Encoding ASCII
}

# Build texture pack (PC Options) runs these on the player's own dump. Pinned by SHA-256; cached under out\.
$TexconvUrl = 'https://github.com/microsoft/DirectXTex/releases/download/may2026/texconv.exe'
$TexconvSha = 'dcfdec10244e02cf5037fba089c55fb7e1326b1c8181742d77d15fa5cb5eef06'
$EsrganUrl = 'https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/realesrgan-ncnn-vulkan-20220424-windows.zip'
$EsrganZipSha = 'abc02804e17982a3be33675e4d471e91ea374e65b70167abc09e31acb412802d'
$EsrganFiles = [ordered]@{
    'realesrgan-ncnn-vulkan.exe'       = '07e49f7cbb4ede01ae4dd4c399d3a7e5846e3d2085c3128eff881e55cb7b1a0c'
    'vcomp140.dll'                     = '8f72ef2e483465444b2059fc6744d6cb22cd8d8a27f6fa56befd2a42dcd0f78b'
    'models\realesr-animevideov3-x4.bin'   = '548a36f9c3f4ab8da56cd3b13badf23968bee207b396dad14d04b830e5f2ab2d'
    'models\realesr-animevideov3-x4.param' = '850a248e7c14c27e5bd8cf7265113a9441036a7db63963bb8aa5169d788a435e'
}

function Test-Sha256([string]$Path, [string]$Expected) {
    if (-not (Test-Path $Path)) { return $false }
    return (Get-FileHash -Path $Path -Algorithm SHA256).Hash -ieq $Expected
}

function Assert-Sha256([string]$Path, [string]$Expected) {
    if (Test-Sha256 $Path $Expected) { return }
    $actual = if (Test-Path $Path) { (Get-FileHash -Path $Path -Algorithm SHA256).Hash } else { 'missing' }
    Fail "SHA-256 mismatch for ${Path}: expected $Expected, got $actual"
}

function Get-PinnedDownload([string]$Url, [string]$Path, [string]$Sha) {
    if (Test-Sha256 $Path $Sha) { return }
    Write-Host "Downloading $Url"
    Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $Path
    Assert-Sha256 $Path $Sha
}

# The zip's own folder layout is not assumed: each pinned file is found by its path suffix.
function Find-ZipFile([string]$Dir, [string]$Relative) {
    $suffix = '\' + $Relative
    $found = Get-ChildItem -Path $Dir -Recurse -File | Where-Object { $_.FullName.EndsWith($suffix, 'OrdinalIgnoreCase') } |
        Select-Object -First 1
    if (-not $found) { Fail "$Relative not found in the Real-ESRGAN zip" }
    return $found.FullName
}

function Copy-TexturePackTools([string]$Root, [string]$StageDir) {
    $cache = Join-Path $Root 'out\texture-pack-tools'
    New-Item -ItemType Directory -Force -Path $cache | Out-Null
    $texconv = Join-Path $cache 'texconv.exe'
    Get-PinnedDownload $TexconvUrl $texconv $TexconvSha
    $zip = Join-Path $cache 'realesrgan-ncnn-vulkan-20220424-windows.zip'
    Get-PinnedDownload $EsrganUrl $zip $EsrganZipSha
    $extract = Join-Path $cache 'realesrgan'
    New-CleanDir $extract
    Expand-Archive -Path $zip -DestinationPath $extract -Force

    $texconvDir = Join-Path $StageDir 'tools\texconv'
    New-Item -ItemType Directory -Force -Path $texconvDir | Out-Null
    Copy-Item -Force $texconv (Join-Path $texconvDir 'texconv.exe')
    Assert-Sha256 (Join-Path $texconvDir 'texconv.exe') $TexconvSha
    $esrganDir = Join-Path $StageDir 'tools\realesrgan'
    New-Item -ItemType Directory -Force -Path (Join-Path $esrganDir 'models') | Out-Null
    foreach ($relative in $EsrganFiles.Keys) {
        $source = Find-ZipFile $extract $relative
        Assert-Sha256 $source $EsrganFiles[$relative]
        $target = Join-Path $esrganDir $relative
        Copy-Item -Force $source $target
        Assert-Sha256 $target $EsrganFiles[$relative]
        Write-Host "Texture pack tool: tools\realesrgan\$relative"
    }
    Copy-TexturePackLicenses $Root $cache (Join-Path $StageDir 'tools\licenses')
}

$TexturePackLicenseRepos = [ordered]@{
    'DirectXTex'              = 'microsoft/DirectXTex'
    'Real-ESRGAN-ncnn-vulkan' = 'xinntao/Real-ESRGAN-ncnn-vulkan'
    'Real-ESRGAN'             = 'xinntao/Real-ESRGAN'
    'ncnn'                    = 'Tencent/ncnn'
}

function Get-CollapsedText([string]$Text) {
    return ($Text -replace '\s+', ' ').Trim()
}

# The upstream LICENSE files ship verbatim, and THIRD_PARTY_NOTICES.md must quote each one word for word.
function Copy-TexturePackLicenses([string]$Root, [string]$Cache, [string]$LicenseDir) {
    New-Item -ItemType Directory -Force -Path $LicenseDir | Out-Null
    $notices = Get-CollapsedText (Get-Content -Raw -Path (Join-Path $Root 'THIRD_PARTY_NOTICES.md'))
    foreach ($name in $TexturePackLicenseRepos.Keys) {
        $repo = $TexturePackLicenseRepos[$name]
        $path = Join-Path $Cache "$name-LICENSE.txt"
        Write-Host "Downloading the LICENSE of $repo"
        Invoke-WebRequest -UseBasicParsing -Uri "https://api.github.com/repos/$repo/license" `
            -Headers @{ Accept = 'application/vnd.github.raw' } -OutFile $path
        if (-not $notices.Contains((Get-CollapsedText (Get-Content -Raw -Path $path)))) {
            Fail "THIRD_PARTY_NOTICES.md does not quote the LICENSE of $repo word for word; paste $path into it"
        }
        Copy-Item -Force $path (Join-Path $LicenseDir "$name.txt")
    }
}

function Write-QuickstartFile([string]$StageDir) {
    $lines = @(
        'Gears of War 2 - unofficial PC port (gow2-recomp) - QUICKSTART',
        '',
        'WHAT THIS IS',
        'Unofficial native PC port of Gears of War 2 (static recompilation).',
        'No game data is included: you must bring your own retail copy (see below).',
        '',
        'YOU NEED YOUR OWN RETAIL COPY',
        'Dump your own retail Xbox 360 disc of Gears of War 2 (title 4D53082D) to an .iso,',
        'or to a folder containing default.xex and the GearGame folder.',
        '',
        'RUNNING',
        'Run gow2.exe. The first launch asks for your .iso (or default.xex in your game folder)',
        'and remembers it. An .iso is extracted once into a "game" folder next to gow2.exe',
        '(about 8 GB free needed there); you can delete the .iso afterwards.',
        'Alternatives: put the exe folder inside/next to the game folder, or pass',
        '  --game_data_root="C:\path\to\game"',
        '',
        'DEFAULT KEYBOARD CONTROLS (a controller works too)',
        'W/A/S/D move, mouse (or arrow keys) look, left mouse fire, right mouse aim,',
        'Space cover/roadie run, F or V melee (hold: chainsaw), E use/revive,',
        'R reload, Q point of interest, G tactical display, Z or middle mouse zoom,',
        '1-4 weapons, Enter or Esc pause, Tab back. All keys are in gow2.toml.',
        '',
        'KEYCAP PROMPTS (optional)',
        'Double-click "Build keycap pack.cmd" to draw the menu and hint prompts as your keys.',
        'It starts the game for about two minutes to read the font from your own copy',
        '(do not press keys while it runs) and writes the pack into textures\. No Python needed.',
        '',
        'SETTINGS AND LOGS',
        'F10 (or Back+Start) opens PC Options; its Quit game button closes the game.',
        'Language: PC Options > System > Subtitles and text, and Voices (English, Latin American Spanish or French); restart to apply.',
        'Settings live in gow2.toml next to the exe.',
        'Logs and crash reports are written next to the exe (gow2_crash.log).',
        '',
        'Not affiliated with Microsoft, Epic Games or The Coalition.',
        '',
        '------------------------------------------------------------',
        '',
        'Gears of War 2 - port no oficial para PC (gow2-recomp) - INICIO RAPIDO',
        '',
        'QUE ES ESTO',
        'Port no oficial y nativo de Gears of War 2 (recompilacion estatica).',
        'No incluye datos del juego: necesitas tu propia copia original (ver abajo).',
        '',
        'NECESITAS TU PROPIO DISCO ORIGINAL',
        'Copia tu propio disco original de Xbox 360 de Gears of War 2 (titulo 4D53082D) a un .iso,',
        'o a una carpeta que contenga default.xex y la carpeta GearGame.',
        '',
        'COMO EJECUTARLO',
        'Ejecuta gow2.exe. El primer arranque te pide tu .iso (o default.xex en la carpeta del juego)',
        'y lo recuerda. Un .iso se extrae una sola vez a una carpeta "game" junto a gow2.exe',
        '(necesita unos 8 GB libres ahi); despues puedes borrar el .iso.',
        'Alternativas: coloca la carpeta del exe dentro o junto a la del juego, o pasa',
        '  --game_data_root="C:\path\to\game"',
        '',
        'CONTROLES DE TECLADO (tambien funciona un mando)',
        'W/A/S/D moverse, raton (o flechas) mirar, clic izquierdo disparar, clic derecho apuntar,',
        'Espacio cobertura/carrera, F o V cuerpo a cuerpo (mantener: motosierra), E usar/revivir,',
        'R recargar, Q punto de interes, G pantalla tactica, Z o clic central zoom,',
        '1-4 armas, Enter o Esc pausa, Tab atras. Todas las teclas estan en gow2.toml.',
        '',
        'TECLAS DIBUJADAS EN LOS AVISOS (opcional)',
        'Haz doble clic en "Build keycap pack.cmd" para que los avisos muestren tus teclas.',
        'Arranca el juego unos dos minutos para leer la fuente de tu propia copia',
        '(no pulses teclas mientras corre) y guarda el paquete en textures\. No necesita Python.',
        '',
        'AJUSTES Y REGISTROS',
        'F10 (o Back+Start) abre Opciones de PC; su boton Salir del juego cierra el juego.',
        'Idioma: Opciones de PC > Sistema > Subtitulos y textos, y Voces (ingles, espanol latino o frances); se aplica al reiniciar.',
        'Los ajustes estan en gow2.toml junto al exe.',
        'Los registros e informes de fallo se escriben junto al exe (gow2_crash.log).',
        '',
        'Sin afiliacion con Microsoft, Epic Games o The Coalition.',
        ''
    )
    $out = Join-Path $StageDir 'QUICKSTART.txt'
    Set-Content -Path $out -Value $lines -Encoding ASCII
}

function Assert-NoGameFiles([string]$StageDir) {
    $bad = Get-ChildItem -Path $StageDir -Recurse -File -ErrorAction SilentlyContinue | Where-Object {
        ($_.Name -ieq 'default.xex') -or
        ($_.Extension -ieq '.xxx') -or
        ($_.Extension -ieq '.tfc') -or
        ($_.Extension -ieq '.bik')
    }
    if ($bad) {
        foreach ($b in $bad) { Write-Host "Forbidden game file in staging: $($b.FullName)" -ForegroundColor Red }
        Fail 'Refusing to package: staging folder would contain game data files (default.xex / *.xxx / *.tfc / *.bik).'
    }
}

function Assert-NoNgx([string]$StageDir) {
    $withNgx = @(Find-NgxBinaries $StageDir)
    if (-not $withNgx.Count) {
        Write-Host 'No NVIDIA NGX names in the staged .dll and .exe files.'
        return
    }
    foreach ($b in $withNgx) { Write-Host "NVIDIA NGX in staging: $($b.Path) ($($b.Markers))" -ForegroundColor Red }
    Fail 'Refusing to package: NVIDIA DLSS (NGX) must not ship in the release. Build the DLLs with scripts\build-sdk.ps1 -NoDlss and package with -SdkDir out\sdk-nodlss (the default).'
}

function New-ReleaseZip([string]$StageDir, [string]$ZipPath) {
    if (Test-Path $ZipPath) { Remove-Item -Force $ZipPath }
    Compress-Archive -Path $StageDir -DestinationPath $ZipPath -Force
}

function Write-HashFile([string]$ZipPath, [string]$HashPath) {
    $hash = Get-FileHash -Path $ZipPath -Algorithm SHA256
    $line = '{0}  {1}' -f $hash.Hash, (Split-Path -Leaf $ZipPath)
    Set-Content -Path $HashPath -Value $line -Encoding ASCII
    return $hash.Hash
}

# ---- main ---------------------------------------------------------------------------
$root = Get-RepoRoot
$BuildDir = Resolve-RootedPath $root $BuildDir
$SdkDir = Resolve-RootedPath $root $SdkDir
$OutDir = Resolve-RootedPath $root $OutDir
$Config = Resolve-RootedPath $root $Config

$topName = 'gow2-recomp-{0}-win64' -f $Version
$stageRoot = Join-Path $OutDir 'staging'
$stageDir = Join-Path $stageRoot $topName
$zipPath = Join-Path $OutDir ('{0}.zip' -f $topName)
$hashPath = '{0}.sha256' -f $zipPath

if (-not (Test-Path $BuildDir)) {
    Fail "BuildDir not found: $BuildDir. Build the game first or pass -BuildDir <folder>."
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

Step 'Staging binaries'
New-CleanDir $stageDir
Copy-BuildBinaries $BuildDir $stageDir
Copy-SdkOverlay $SdkDir $stageDir

Step 'VC++ runtime DLLs'
Copy-VcRuntime $stageDir

Step 'Docs and QUICKSTART.txt'
Copy-RepoDocs $root $stageDir $Config
Write-QuickstartFile $stageDir

Step 'Keycap pack tool'
Copy-KeycapTool $root $stageDir

Step 'Texture pack builder tools'
Copy-TexturePackTools $root $stageDir

Step 'Game-data safety check'
Assert-NoGameFiles $stageDir

Step 'NVIDIA DLSS check'
Assert-NoNgx $stageDir

Step 'Compressing release zip'
New-ReleaseZip $stageDir $zipPath

Step 'Result'
Get-ChildItem -Path $stageDir -File | Sort-Object Name | Format-Table Name, Length -AutoSize
$zipItem = Get-Item $zipPath
$mb = [math]::Round($zipItem.Length / 1MB, 2)
Write-Host ("Zip: {0} ({1} bytes, {2} MB)" -f $zipPath, $zipItem.Length, $mb)
$sha = Write-HashFile $zipPath $hashPath
Write-Host "SHA-256: $sha"
Write-Host "Hash file: $hashPath"
