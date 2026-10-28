<#
.SYNOPSIS
  Builds the ReXGlue SDK runtime DLLs the game runs against.

.DESCRIPTION
  1. Clones the ReXGlue SDK at the pinned commit this project is built against,
  2. applies sdk\patches\*.patch on top of it,
  3. fetches the public NVIDIA DLSS SDK and the AMD FidelityFX-SDK v2.3.0
     (API headers plus the signed loader/upscaler binaries; nothing is redistributed
     from our tree),
  4. configures it with the win-amd64 preset plus -DREXGLUE_ENABLE_FIDELITYFX=ON
     (the SDK's cmake/rexglue_fidelityfx.cmake fetches its FidelityFX fork itself)
     and the GOW2_DLSS_SDK / GOW2_FFX_SDK paths for the temporal upscalers
     (sdk\patches\0002-temporal-upscalers.patch),
  5. builds the rexruntime and rexgpu-xenos targets with the release build preset,
  6. copies rexruntime.dll, rexgpu-xenos.dll, nvngx_dlss.dll and the
     amd_fidelityfx_loader/upscaler_dx12.dll files into out\sdk\.

  scripts\build.ps1 then picks the DLLs up automatically (see CMakeLists.txt).
  Without them the game still builds and runs with the stock SDK DLLs.

  The first run downloads the SDK with its third-party libraries and compiles it;
  expect 10 to 20 minutes and several GB of disk space in out\sdk-build\.

.PARAMETER Dev
  Also build the SDK development tools (frame tracing, dumps, the D3D12 debug
  layer switch: -DGOW2_DEV_TOOLS=ON).

.PARAMETER NoDlss
  Build the DLLs a binary release ships: no NVIDIA DLSS SDK is fetched or linked,
  nvngx_dlss.dll is not installed, the development tools stay off (not allowed with
  -Dev), and the DLLs go to out\sdk-nodlss\, which scripts\package.ps1 packages.
  out\sdk\ is left as it is. NVIDIA's DLSS SDK license keeps NGX out of an
  open-source release (THIRD_PARTY_NOTICES.md, "NVIDIA DLSS"); the build fails
  if an NGX name is still found in an installed DLL.
  The SDK build tree in out\sdk-build\ is shared: the next run without -NoDlss
  compiles rexgpu-xenos.dll again with DLSS.

.PARAMETER Clean
  Delete out\sdk-build\ and the output folder (out\sdk\, or out\sdk-nodlss\ with
  -NoDlss) first.

.PARAMETER Vulkan
  Build without DLSS in out\sdk-build-vulkan\ and install the development DLLs in
  out\sdk-vulkan\ (every build has both the Direct3D 12 and the Vulkan backend). Can be
  combined with -Dev.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\build-sdk.ps1

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\build-sdk.ps1 -NoDlss
#>
param(
    [switch]$Dev,
    [switch]$NoDlss,
    [switch]$Vulkan,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

function Fail($msg) { Write-Host "ERROR: $msg" -ForegroundColor Red; exit 1 }
function Step($msg) { Write-Host "`n== $msg" -ForegroundColor Cyan }
function Run($what, [scriptblock]$cmd) {
    & $cmd
    if ($LASTEXITCODE -ne 0) { Fail "$what failed (exit code $LASTEXITCODE)." }
}

function Find-VsWithMsvcHeaders {
    # Some VS installs ship cl.exe without the toolset headers (excpt.h); pick one that has them.
    # NOTE: do not use `vswhere -latest` here; the latest VS install on this machine
    # has a broken MSVC toolset.
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { return $null }
    $paths = & $vswhere -all -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    foreach ($p in $paths) {
        if (Get-ChildItem -Path (Join-Path $p 'VC\Tools\MSVC\*\include\excpt.h') -ErrorAction SilentlyContinue) { return $p }
    }
    return $null
}

function Import-VcVars($vsPath) {
    $vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path $vcvars)) { Fail "vcvars64.bat not found in $vsPath." }
    cmd /c "`"$vcvars`" >nul && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
    }
}

# The pinned SDK commit ($SdkCommit), the patched checkout and the symlink repair.
. (Join-Path $PSScriptRoot 'sdk-source.ps1')
# Find-NgxBinaries: NVIDIA NGX names left in a built DLL.
. (Join-Path $PSScriptRoot 'ngx-scan.ps1')

if ($NoDlss -and $Dev) { Fail '-NoDlss builds the release DLLs, which leave the development tools out; drop -Dev.' }
$withoutDlss = $NoDlss -or $Vulkan

function Find-ClangDir($vsPath) {
    foreach ($dir in @((Join-Path $vsPath 'VC\Tools\Llvm\x64\bin'), (Join-Path $env:ProgramFiles 'LLVM\bin'))) {
        if (Test-Path (Join-Path $dir 'clang++.exe')) { return $dir }
    }
    $onPath = Get-Command clang++ -ErrorAction SilentlyContinue
    if ($onPath) { return Split-Path $onPath.Source }
    return $null
}

foreach ($tool in 'git', 'cmake', 'ninja') {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { Fail "$tool was not found on PATH." }
}

Step 'Locating the C++ toolchain'
$vsPath = Find-VsWithMsvcHeaders
if (-not $vsPath) { Fail 'No Visual Studio / Build Tools install with a complete MSVC toolset (excpt.h) was found.' }
Import-VcVars $vsPath
$llvm = Find-ClangDir $vsPath
if (-not $llvm) { Fail 'clang++ was not found. Install LLVM or the VS "C++ Clang tools for Windows" component.' }
$env:PATH = "$llvm;$env:PATH"
Write-Host "Visual Studio: $vsPath"
Write-Host "clang:         $llvm"

$work = Join-Path $root $(if ($Vulkan) { 'out\sdk-build-vulkan' } else { 'out\sdk-build' })
$outDir = Join-Path $root $(if ($Vulkan) { 'out\sdk-vulkan' } elseif ($NoDlss) { 'out\sdk-nodlss' } else { 'out\sdk' })
if ($Clean) {
    Step 'Cleaning'
    foreach ($d in $work, $outDir) { if (Test-Path $d) { Remove-Item $d -Recurse -Force } }
}
New-Item -ItemType Directory -Force $work, $outDir | Out-Null

# ---- the SDK ---------------------------------------------------------------------------------
$sdk = Join-Path $work 'rexglue-sdk'
$marker = Join-Path $work 'sdk-patched.txt'
Step "The ReXGlue SDK ($SdkCommit) with sdk\patches"
try {
    if (-not (Initialize-SdkSource $sdk $marker (Join-Path $root 'sdk\patches'))) {
        Write-Host "Already prepared: $sdk"
    }
} catch { Fail $_.Exception.Message }

# ---- public upscaler SDKs (not redistributed) --------------------------------------------------
$dlss = Join-Path $work 'dlss-sdk'
# The SDK turns DLSS on only when this folder holds include\nvsdk_ngx.h; no-DLSS builds name one that never
# exists, so a DLSS SDK fetched by an earlier run is not picked up.
$dlssForCmake = if ($withoutDlss) { Join-Path $work 'dlss-sdk-left-out' } else { $dlss }
if ($withoutDlss -and (Test-Path $dlssForCmake)) { Fail "$dlssForCmake must not exist for a build without DLSS; delete it." }
if (-not $withoutDlss -and -not (Test-Path (Join-Path $dlss 'include\nvsdk_ngx.h'))) {
    Step 'Fetching the NVIDIA DLSS SDK headers'
    Run 'git clone DLSS' { git clone --depth 1 https://github.com/NVIDIA/DLSS.git $dlss }
}
$ffx = Join-Path $work 'ffx-sdk'
if (-not (Test-Path (Join-Path $ffx 'Kits\FidelityFX\signedbin\amd_fidelityfx_upscaler_dx12.dll'))) {
    Step 'Fetching the AMD FidelityFX-SDK (v2.3.0: API headers and signed binaries)'
    if (-not (Test-Path (Join-Path $ffx '.git'))) {
        Run 'git clone FidelityFX' { git clone --depth 1 --branch v2.3.0 --filter=blob:none --sparse https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK.git $ffx }
    }
    Push-Location $ffx
    try {
        Run 'sparse checkout' { git sparse-checkout set Kits/FidelityFX/api/include Kits/FidelityFX/upscalers/include Kits/FidelityFX/signedbin }
    } finally { Pop-Location }
}

# ---- build -------------------------------------------------------------------------------------
# Also on reuse: a previous run may have prepared the tree before this repair existed.
Repair-SymlinkStubs $sdk

Step 'Configuring'
$devFlag = if ($Dev) { 'ON' } else { 'OFF' }
# Both backends in every build: gpu_backend = "any" still picks Direct3D 12 first on Windows, and
# Vulkan is the player's choice in PC Options > Graphics > Graphics API (issue #34).
$vulkanFlag = 'ON'
Push-Location $sdk
try {
    Run 'cmake configure' {
        cmake --preset win-amd64 -DREXGLUE_ENABLE_FIDELITYFX=ON "-DGOW2_DEV_TOOLS=$devFlag" `
            -DREXGLUE_USE_D3D12=ON "-DREXGLUE_USE_VULKAN=$vulkanFlag" `
            "-DGOW2_DLSS_SDK=$($dlssForCmake.Replace([string][char]92, '/'))" `
            "-DGOW2_FFX_SDK=$($ffx.Replace([string][char]92, '/'))/Kits/FidelityFX"
    }
    Step 'Building rexruntime.dll and rexgpu-xenos.dll (this takes a while)'
    Run 'cmake build' { cmake --build --preset win-amd64-release --target rexruntime rexgpu-xenos }
} finally { Pop-Location }

# ---- install -----------------------------------------------------------------------------------
Step 'Installing the DLLs'
$sdkOut = Join-Path $sdk 'out'
# The game build copies every DLL found here, so drop what an older run left behind.
Get-ChildItem $outDir -Filter *.dll | Remove-Item -Force
foreach ($dll in 'rexruntime.dll', 'rexgpu-xenos.dll') {
    # The SDK is multi-config (out\...\<Config>\); prefer the Release output.
    $src = Get-ChildItem $sdkOut -Recurse -Filter $dll -ErrorAction SilentlyContinue |
        Sort-Object { $_.FullName -notlike '*Release*' } |
        Select-Object -First 1
    if (-not $src) { Fail "$dll was not produced under $sdkOut." }
    Copy-Item $src.FullName $outDir -Force
    Write-Host "  $dll <- $($src.FullName)"
}
# The official, signed temporal-upscaler libraries the game loads at run time (see
# THIRD_PARTY_NOTICES.md; nvngx_dlss.dll is for local builds only and never ships in a release).
if (-not $withoutDlss) {
    $ngx = Join-Path $dlss 'lib\Windows_x86_64\rel\nvngx_dlss.dll'
    if (-not (Test-Path $ngx)) { Fail "nvngx_dlss.dll was not produced in the DLSS SDK ($ngx)." }
    Copy-Item $ngx $outDir -Force
    Write-Host "  nvngx_dlss.dll <- $ngx"
}
foreach ($n in 'loader', 'upscaler') {
    $f = Join-Path $ffx "Kits\FidelityFX\signedbin\amd_fidelityfx_${n}_dx12.dll"
    if (-not (Test-Path $f)) { Fail "$(Split-Path $f -Leaf) was not found in the FidelityFX-SDK ($f)." }
    Copy-Item $f $outDir -Force
    Write-Host "  $(Split-Path $f -Leaf) <- $f"
}
# rexruntime.dll imports amd_fidelityfx_dx12.dll (FSR 1 / CAS present effects); the other signed
# FidelityFX libraries (frame generation, denoiser, radiance cache, Vulkan) are not used.
# The one the SDK builds next to its import library (bin\), not AMD's prebuilt copy under _deps\.
$ffxRuntime = Get-Item (Join-Path $sdk 'bin\amd_fidelityfx_dx12.dll') -ErrorAction SilentlyContinue
if (-not $ffxRuntime) { Fail "amd_fidelityfx_dx12.dll was not built in $sdk\bin (REXGLUE_ENABLE_FIDELITYFX build)." }
Copy-Item $ffxRuntime.FullName $outDir -Force
Write-Host "  amd_fidelityfx_dx12.dll <- $($ffxRuntime.FullName)"

if ($withoutDlss) {
    Step 'Checking that no NVIDIA NGX code was built in'
    $withNgx = @(Find-NgxBinaries $outDir)
    foreach ($b in $withNgx) { Write-Host "  $($b.Path): $($b.Markers)" -ForegroundColor Red }
    if ($withNgx.Count) { Fail "NGX names found in $($withNgx.Count) DLL(s) of a build without DLSS." }
    Write-Host "  No NGX names in $outDir"
}

Step 'Done'
Write-Host "Self-built SDK DLLs: $outDir" -ForegroundColor Green
Get-ChildItem $outDir -Filter *.dll | ForEach-Object { Write-Host "  $($_.Name)" }
if ($Vulkan) { Write-Host 'Run scripts\make-vulkan-run.ps1, then out\vulkan-run\gow2.exe --gpu_backend=vulkan.' }
elseif ($NoDlss) { Write-Host 'Run scripts\package.ps1 -Version <version> to package them.' }
else { Write-Host 'Run scripts\build.ps1 to build the game with them.' }
