# Building gow2-recomp

## Requirements

| Requirement | Version / notes |
|---|---|
| OS | Windows 10/11 x64 |
| Visual Studio | Visual Studio 2022+ or Build Tools with "Desktop development with C++" and a Windows 10/11 SDK (the script picks an install whose MSVC toolset includes `excpt.h`) |
| clang / clang++ | 18+ (either the VS "C++ Clang tools for Windows" component or LLVM from [llvm.org](https://llvm.org); tested with LLVM clang 23.1.3) |
| CMake | 3.25+ |
| Ninja | any recent version |
| Python | 3.9+ |
| ReXGlue SDK | Windows x64 build, version 0.10.0.9-dev (commit `923c1a5`, GitHub release tag `nightly-20260921-923c1a59`) — the folder containing `bin\rexglue.exe` |
| Game executable | the player's own `default.xex` from the retail Xbox 360 disc (see below) |

Use only the retail `default.xex`: title ID `4D53082D`, media ID `63AF211E`,
XEX version `0.0.0.5`, file time 2008-09-23. Title-update builds have different
addresses and are not supported.

## Steps

1. Dump your disc / extract your ISO, e.g. with [xdvdfs](https://github.com/antangelo/xdvdfs):

   ```powershell
   xdvdfs unpack "<your iso>" <folder>
   ```

2. Copy `default.xex` to `game\default.xex` (git-ignored).

3. Run the build script:

   ```powershell
   powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Sdk C:\path\to\rexglue-sdk
   ```

   Instead of `-Sdk`, you may set the `REXGLUE_SDK` environment variable to the
   SDK folder.

   Options:

   - `-Config Release|RelWithDebInfo|Debug` (default: `Release`)
   - `-SkipCodegen` (reuse the existing `generated\` output)
   - `-Clean` (delete the output directory before building)

4. The output is `out\build\release\gow2.exe`.

## What the script does

1. Runs code generation:

   ```text
   rexglue codegen gow2_manifest.toml
   ```

   This takes about a minute and produces about 230 MB of C++ in `generated\`.
   That directory is never committed.

2. Configures the project with CMake (Ninja generator, clang toolset) and builds it.

3. After codegen and before compiling, the post-codegen patcher
   `tools\patch_switch_traps.py` runs over the generated code.

## Tests

`scripts\test-host.ps1` runs the tests that need no game data, phone or second PC: the host
test targets `gow2_test_input_test`, `gow2_script_patch_test` and `gow2_save_dir_test` (after
`scripts\build.ps1`), the touch controls model test, the SDK's System Link and session unit tests
(after `scripts\build-sdk.ps1`) and `python -m pytest tools`. The exit code is the number of
failed steps.

```powershell
powershell -ExecutionPolicy Bypass -File scripts\test-host.ps1 -LowPriority
```

## Release zip

The release zip holds `out\build\release\gow2.exe` and SDK DLLs built without NVIDIA DLSS: the
DLSS SDK license keeps NVIDIA's NGX library out of an open-source release (THIRD_PARTY_NOTICES.md,
"NVIDIA DLSS").

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-sdk.ps1 -NoDlss
pwsh -ExecutionPolicy Bypass -File scripts\package.ps1 -Version 0.1.2
```

`-NoDlss` installs the DLLs (both the Direct3D 12 and the Vulkan backend, as every SDK build) into `out\sdk-nodlss\` and leaves `out\sdk\`, the DLLs
`scripts\build.ps1` puts next to `gow2.exe`, as they are. It shares the SDK build tree with the
everyday build, so the next `scripts\build-sdk.ps1` without it compiles `rexgpu-xenos.dll` again.
`package.ps1` writes `out\package\gow2-recomp-<version>-win64.zip` and its `.sha256`. It refuses
game files and any `.dll` or `.exe` that holds an NVIDIA NGX name (`nvsdk_ngx`, `nvngx`).

## gow2_manifest.toml

The manifest drives code generation. Two sections worth knowing about:

- `[entrypoint.functions]` holds function entry hints for addresses that
  discovery misses.
- `end = ...` overrides mark the end of catch funclets that finish with a
  rethrow (a call to `_CxxThrowException` at `0x82CD54A0`, which never
  returns), so discovery does not fall through into the next funclet.
