$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$release = Join-Path $root 'out\build\release'
$sdk = Join-Path $root 'out\sdk-vulkan'
$destination = Join-Path $root 'out\vulkan-run'

$files = @('gow2.exe', 'gow2.default.toml', 'gamecontrollerdb.txt') | ForEach-Object {
    Join-Path $release $_
}
$files += @('rexruntime.dll', 'rexgpu-xenos.dll', 'amd_fidelityfx_dx12.dll',
    'amd_fidelityfx_loader_dx12.dll', 'amd_fidelityfx_upscaler_dx12.dll') | ForEach-Object {
    Join-Path $sdk $_
}
foreach ($file in $files) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
        throw "Required file is missing: $file. Build the release game and run scripts\build-sdk.ps1 -Vulkan first."
    }
}

New-Item -ItemType Directory -Path $destination -Force | Out-Null
Copy-Item -LiteralPath $files -Destination $destination -Force
Get-ChildItem -LiteralPath $sdk -Filter *.dll | Copy-Item -Destination $destination -Force
Write-Host "Vulkan run folder: $destination"
Write-Host "Run: & '$destination\gow2.exe' --gpu_backend=vulkan"
