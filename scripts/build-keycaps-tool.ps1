<#
.SYNOPSIS
  Builds keycaps-pack.exe: tools\keycaps\pack.py frozen with PyInstaller, so players of the release zip
  can build their keycap pack (scripts\build-keycaps.ps1) without installing Python.

.DESCRIPTION
  Creates a virtual environment in out\keycaps-tool\venv with pinned versions of the packages pack.py
  needs, freezes pack.py with its data (font_info.json, atlas_info.json, the DejaVu font, the PlayStation
  button art of assets\glyphs\playstation) into
  out\keycaps-tool\dist\keycaps-pack.exe, and copies the license texts of everything the exe bundles
  into out\keycaps-tool\dist\licenses. The exe holds no game data: it reads the player's own texture
  dump at run time. scripts\package.ps1 runs this.
#>
param(
    [string]$OutDir = 'out\keycaps-tool'
)

$ErrorActionPreference = 'Stop'
function Fail($msg) { Write-Host "ERROR: $msg" -ForegroundColor Red; exit 1 }

$root = Split-Path -Parent $PSScriptRoot
$out = if ([IO.Path]::IsPathRooted($OutDir)) { $OutDir } else { Join-Path $root $OutDir }
$keycaps = Join-Path $root 'tools\keycaps'
$glyphs = Join-Path $root 'assets\glyphs'
$venv = Join-Path $out 'venv'
$python = Join-Path $venv 'Scripts\python.exe'
$dist = Join-Path $out 'dist'
$packages = @('pillow==13.0.0', 'numpy==2.5.4', 'resvg-py==0.5.0', 'fonttools==4.67.0', 'pyinstaller==6.22.3')

if (-not (Get-Command py -ErrorAction SilentlyContinue)) { Fail 'Python 3 (the py launcher) was not found.' }
if (-not (Test-Path $python)) {
    py -m venv $venv
    if ($LASTEXITCODE -ne 0) { Fail 'python -m venv failed.' }
}
& $python -m pip install --quiet --disable-pip-version-check @packages
if ($LASTEXITCODE -ne 0) { Fail 'pip could not install the packages.' }

if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
& $python -m PyInstaller --noconfirm --onefile --name keycaps-pack --log-level WARN `
    --distpath $dist --workpath (Join-Path $out 'build') --specpath (Join-Path $out 'build') `
    --paths (Join-Path $root 'tools') --paths $keycaps `
    --add-data "$(Join-Path $keycaps 'font_info.json');." `
    --add-data "$(Join-Path $keycaps 'atlas_info.json');." `
    --add-data "$(Join-Path $keycaps 'fonts');fonts" `
    --add-data "$(Join-Path $glyphs 'playstation');glyphs\playstation" `
    (Join-Path $keycaps 'pack.py')
if ($LASTEXITCODE -ne 0) { Fail 'PyInstaller failed.' }

# License texts of what the exe bundles: Python, the packages (their dist-info licenses), PyInstaller's
# bootloader, the DejaVu font and the Kenney button art.
$licenses = Join-Path $dist 'licenses'
New-Item -ItemType Directory -Force $licenses | Out-Null
$basePrefix = & $python -c 'import sys; print(sys.base_prefix)'
Copy-Item -Force (Join-Path $basePrefix 'LICENSE.txt') (Join-Path $licenses 'Python-LICENSE.txt')
$sitePackages = Join-Path $venv 'Lib\site-packages'
foreach ($name in 'pillow', 'numpy', 'resvg_py', 'fonttools', 'pyinstaller') {
    $info = Get-ChildItem $sitePackages -Directory -Filter "$name-*.dist-info" | Select-Object -First 1
    if (-not $info) { Fail "$name is not installed in $venv." }
    $source = Join-Path $info.FullName 'licenses'
    if (-not (Test-Path $source)) { Fail "$($info.Name) has no licenses folder." }
    Copy-Item -Recurse -Force $source (Join-Path $licenses $name)
}
Copy-Item -Force (Join-Path $keycaps 'fonts\LICENSE_DEJAVU') (Join-Path $licenses 'DejaVu-LICENSE.txt')
Copy-Item -Force (Join-Path $glyphs 'playstation\LICENSE-Kenney.txt') (Join-Path $licenses 'Kenney-LICENSE.txt')

$exe = Join-Path $dist 'keycaps-pack.exe'
& $exe --help | Out-Null
if ($LASTEXITCODE -ne 0) { Fail "$exe does not start." }
Write-Host "keycaps-pack.exe: $exe ($([math]::Round((Get-Item $exe).Length / 1MB, 1)) MB)"
