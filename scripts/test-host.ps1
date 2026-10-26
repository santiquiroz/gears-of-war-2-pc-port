<#
.SYNOPSIS
  Runs the tests that need no game data, phone or second PC, on this Windows PC.

.DESCRIPTION
  1. The port's host test targets gow2_test_input_test, gow2_script_patch_test,
     gow2_script_patch_survival_test, gow2_save_dir_test, gow2_dev_commands_test,
     gow2_coop_leash_test, gow2_toml_edit_test, gow2_options_catalog_test,
     gow2_options_model_test, gow2_options_input_test, gow2_options_values_test,
     gow2_options_actions_test, gow2_fps_cap_test, gow2_audio_volume_test,
     gow2_coalesced_text_test, gow2_keycaps_test, gow2_gpu_pick_test, gow2_frame_stats_test, gow2_update_check_test, gow2_prompt_device_test, gow2_disc_install_test and gow2_key_names_test in
     out\build\<config> (configure it with scripts\build.ps1 first).
     gow2_toml_edit_test takes config\gow2.toml as its first argument; the others
     take none.
  2. android\gamelibs\tests\touch_model_test.cpp: pure C++, so it also runs on the PC.
  3. The SDK's System Link and session unit tests of 0010 and 0011
     (tests\unit\system\system_link_test.cpp, tests\unit\kernel\xsession_test.cpp) in the tree
     scripts\build-sdk.ps1 prepared (out\sdk-build\rexglue-sdk), compiled alone against the SDK's
     own Catch2: the SDK's whole unit_tests target does not build at 923c1a5 (hash_test.cpp
     cannot find xxhash.h).
  4. The SDK's header-only unit tests of 0007, each compiled alone: the guest FPSCR rounding mode
     on x64 and arm64 (tests\unit\ppc\fpscr_rounding_test.cpp) and the Android draw diagnostics
     (tests\unit\graphics\android_draw_experiments_test.cpp) and driver pipeline cache files
     (tests\unit\graphics\pipeline_cache_file_test.cpp).
  5. python -m pytest tools, when pytest is installed.

  Test binaries go to out\host-tests. The exit code is the number of failed steps.

.PARAMETER Config
  The build folder of step 1: Release (default), RelWithDebInfo or Debug.

.PARAMETER LowPriority
  Run below normal priority (the compilers and tests inherit it).

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\test-host.ps1 -LowPriority
#>
param(
    [ValidateSet('Release', 'RelWithDebInfo', 'Debug')][string]$Config = 'Release',
    [switch]$LowPriority
)

$ErrorActionPreference = 'Stop'
if ($LowPriority) { (Get-Process -Id $PID).PriorityClass = 'BelowNormal' }
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

function Fail($msg) { Write-Host "ERROR: $msg" -ForegroundColor Red; exit 1 }
function Step($msg) { Write-Host "`n== $msg" -ForegroundColor Cyan }

function Find-VsWithMsvcHeaders {
    # Some VS installs ship cl.exe without the toolset headers (excpt.h); pick one that has them.
    $pf86 = ${env:ProgramFiles(x86)}
    if (-not $pf86) { return $null }
    $vswhere = Join-Path $pf86 'Microsoft Visual Studio\Installer\vswhere.exe'
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
    cmd /c "`"$vcvars`" >nul 2>nul && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
    }
}

function Find-ClangDir($vsPath) {
    $roots = @()
    if ($vsPath) { $roots += Join-Path $vsPath 'VC\Tools\Llvm\x64\bin' }
    if ($env:ProgramFiles) { $roots += Join-Path $env:ProgramFiles 'LLVM\bin' }
    foreach ($dir in $roots) {
        if (Test-Path (Join-Path $dir 'clang++.exe')) { return $dir }
    }
    $onPath = Get-Command clang++ -ErrorAction SilentlyContinue
    if ($onPath) { return Split-Path $onPath.Source }
    return $null
}

# Native tools write progress to stderr; under 'Stop' Windows PowerShell 5.1 would turn that into
# an error. The exit code decides.
function Invoke-Native([scriptblock]$cmd) {
    $ErrorActionPreference = 'Continue'
    & $cmd 2>&1 | ForEach-Object { Write-Host "  $_" }
    return $LASTEXITCODE
}

function Test-Exe([string]$exe, [string[]]$ExeArgs) {
    if ($ExeArgs) { $code = Invoke-Native { & $exe @ExeArgs } }
    else { $code = Invoke-Native { & $exe } }
    Write-Host "  exit $code"
    return $code -eq 0
}

function Test-HostTargets([string]$build) {
    if (-not (Test-Path (Join-Path $build 'build.ninja'))) {
        Write-Host "  $build is not configured; run scripts\build.ps1 first."
        return $false
    }
    $targets = 'gow2_test_input_test', 'gow2_script_patch_test', 'gow2_script_patch_survival_test',
        'gow2_save_dir_test', 'gow2_dev_commands_test', 'gow2_coop_leash_test',
        'gow2_toml_edit_test', 'gow2_options_catalog_test', 'gow2_options_model_test',
        'gow2_options_input_test', 'gow2_options_values_test', 'gow2_options_actions_test',
        'gow2_fps_cap_test', 'gow2_audio_volume_test', 'gow2_coalesced_text_test', 'gow2_keycaps_test',
        'gow2_gpu_pick_test', 'gow2_widescreen_math_test', 'gow2_frame_stats_test', 'gow2_update_check_test',
        'gow2_native_renderer_math_test', 'gow2_native_frontend_test', 'gow2_prompt_device_test',
        'gow2_disc_install_test', 'gow2_key_names_test'
    if ((Invoke-Native { cmake --build $build --target $targets }) -ne 0) { return $false }
    $ok = $true
    foreach ($t in $targets) {
        Write-Host "  $t"
        $exeArgs = @()
        if ($t -eq 'gow2_toml_edit_test') { $exeArgs = @("$root\config\gow2.toml") }
        if (-not (Test-Exe (Join-Path $build "$t.exe") $exeArgs)) { $ok = $false }
    }
    return $ok
}

function Test-TouchModel([string]$outDir) {
    $exe = Join-Path $outDir 'touch_model_test.exe'
    $code = Invoke-Native { clang++ -std=c++23 -O1 -Wall -D_CRT_SECURE_NO_WARNINGS -I src `
        android\gamelibs\tests\touch_model_test.cpp src\gow2_touch_model.cpp -o $exe }
    if ($code -ne 0) { return $false }
    return Test-Exe $exe
}

function Test-SdkSourcePrepared([string]$sdk) {
    $marker = Join-Path $root 'out\sdk-build\sdk-patched.txt'
    $stamp = Get-SdkSourceStamp (Get-SdkPatchFiles (Join-Path $root 'sdk\patches'))
    return (Test-Path (Join-Path $sdk 'include')) -and (Test-SdkSourceCurrent $marker $stamp)
}

function Build-Catch2([string]$sdk, [string]$catchBuild) {
    $code = Invoke-Native { cmake -S (Join-Path $sdk 'thirdparty\catch2') -B $catchBuild -G Ninja `
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ `
        -DCMAKE_CXX_STANDARD=20 -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL }
    if ($code -ne 0) { return $false }
    return (Invoke-Native { cmake --build $catchBuild --target Catch2WithMain }) -eq 0
}

function Test-SdkSystemLink([string]$outDir) {
    $sdk = Join-Path $root 'out\sdk-build\rexglue-sdk'
    if (-not (Test-SdkSourcePrepared $sdk)) {
        Write-Host "  $sdk is missing or older than sdk\patches; run scripts\build-sdk.ps1 first."
        return $false
    }
    $catchBuild = Join-Path $outDir 'catch2'
    if (-not (Build-Catch2 $sdk $catchBuild)) { return $false }
    $exe = Join-Path $outDir 'system_link_tests.exe'
    $sources = 'src\system\system_link.cpp', 'src\kernel\xam\xam_session.cpp',
        'tests\unit\system\system_link_test.cpp', 'tests\unit\kernel\xsession_test.cpp'
    Push-Location $sdk
    try {
        $code = Invoke-Native { clang++ -std=c++20 -O1 -fms-runtime-lib=dll -D_CRT_SECURE_NO_WARNINGS -Wall `
            -I include -I thirdparty\catch2\src -I (Join-Path $catchBuild 'generated-includes') `
            $sources (Get-Catch2Libraries $catchBuild) -o $exe }
    } finally { Pop-Location }
    if ($code -ne 0) { return $false }
    return Test-Exe $exe
}

function Get-Catch2Libraries([string]$catchBuild) {
    $main = Get-ChildItem $catchBuild -Recurse -Filter 'Catch2Main.lib' | Select-Object -First 1
    $core = Get-ChildItem $catchBuild -Recurse -Filter 'Catch2.lib' | Select-Object -First 1
    return @($main.FullName, $core.FullName)
}

# Header-only: the SDK headers and simde, no SDK library. -O2 so the FPSCR test's divisions run
# under the rounding mode it sets, not folded at compile time.
function Test-SdkHeaderUnit([string]$sdk, [string]$catchBuild, [string]$test, [string]$outDir) {
    Write-Host "  $test"
    $exe = Join-Path $outDir ([IO.Path]::GetFileNameWithoutExtension($test) + '.exe')
    Push-Location $sdk
    try {
        $code = Invoke-Native { clang++ -std=c++23 -O2 -fms-runtime-lib=dll -D_CRT_SECURE_NO_WARNINGS -Wall `
            -I include -I thirdparty\simde -I thirdparty\xxHash -I thirdparty\catch2\src -I (Join-Path $catchBuild 'generated-includes') `
            $test (Get-Catch2Libraries $catchBuild) -o $exe }
    } finally { Pop-Location }
    if ($code -ne 0) { return $false }
    return Test-Exe $exe
}

function Test-SdkHeaderUnits([string]$outDir) {
    $sdk = Join-Path $root 'out\sdk-build\rexglue-sdk'
    if (-not (Test-SdkSourcePrepared $sdk)) {
        Write-Host "  $sdk is missing or older than sdk\patches; run scripts\build-sdk.ps1 first."
        return $false
    }
    $catchBuild = Join-Path $outDir 'catch2'
    if (-not (Build-Catch2 $sdk $catchBuild)) { return $false }
    $tests = 'tests\unit\ppc\fpscr_rounding_test.cpp',
        'tests\unit\graphics\android_draw_experiments_test.cpp',
        'tests\unit\graphics\pipeline_cache_file_test.cpp',
        'tests\unit\input\pad_remap_test.cpp',
        'tests\unit\graphics\native_surfaces_test.cpp'
    $ok = $true
    foreach ($test in $tests) {
        if (-not (Test-SdkHeaderUnit $sdk $catchBuild $test $outDir)) { $ok = $false }
    }
    return $ok
}

function Test-PythonTools {
    $python = Get-Command python -ErrorAction SilentlyContinue
    if (-not $python) { Write-Host '  skipped: python was not found'; return $true }
    if ((Invoke-Native { python -c 'import pytest' }) -ne 0) { Write-Host '  skipped: pytest is not installed'; return $true }
    return (Invoke-Native { python -m pytest tools -q -p no:cacheprovider }) -eq 0
}

# The pinned SDK commit and the patch stamp of the prepared SDK tree.
. (Join-Path $PSScriptRoot 'sdk-source.ps1')

Step 'Locating the C++ toolchain'
$vsPath = Find-VsWithMsvcHeaders
if (-not $vsPath) { Fail 'No Visual Studio / Build Tools install with a complete MSVC toolset (excpt.h) was found.' }
Import-VcVars $vsPath
$llvm = Find-ClangDir $vsPath
if (-not $llvm) { Fail 'clang++ was not found. Install LLVM or the VS "C++ Clang tools for Windows" component.' }
$env:PATH = "$llvm;$env:PATH"
$outDir = Join-Path $root 'out\host-tests'
New-Item -ItemType Directory -Force $outDir | Out-Null

$steps = [ordered]@{
    'Host test targets'          = { Test-HostTargets (Join-Path $root "out\build\$($Config.ToLower())") }
    'Touch model test'           = { Test-TouchModel $outDir }
    'SDK System Link unit tests' = { Test-SdkSystemLink $outDir }
    'SDK header unit tests'      = { Test-SdkHeaderUnits $outDir }
    'Python tests in tools'      = { Test-PythonTools }
}
$failed = @()
foreach ($name in $steps.Keys) {
    Step $name
    if (-not (& $steps[$name])) { $failed += $name }
}

Write-Host ''
if ($failed.Count) {
    Write-Host "FAILED: $($failed -join ', ')" -ForegroundColor Red
} else {
    Write-Host "All $($steps.Count) steps passed." -ForegroundColor Green
}
exit $failed.Count
