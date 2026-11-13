# Android shell

On Android, the settings menu and its entry on the game's Options screen are
called "Device Options" ("Opciones del dispositivo" in Spanish). Windows keeps
"PC Options" ("Opciones de PC" in Spanish); everything else is the same menu.

The Android application of the port (step 3 of [docs/ANDROID.md](../docs/ANDROID.md)): an SDL3
activity, the game-folder setup and either a placeholder native library (the default) or the
game's own libraries, built by `scripts/build-android.ps1` (see [Game build](#game-build)).

This directory is part of gow2-recomp and is released under its BSD-3-Clause license
(see [`../LICENSE`](../LICENSE)). The Java files in `app/src/main/java/org/libsdl/app/`,
`app/proguard-rules.pro` and parts of `AndroidManifest.xml` come from SDL3's `android-project`
template (SDL 3.4.14), unchanged, under SDL's zlib license ([`LICENSE.SDL3.txt`](LICENSE.SDL3.txt)).
They must match the SDL3 version that is built: SDL refuses to start on a Java/C version mismatch,
so copy them again from `thirdparty/sdl3/android-project` whenever the SDK's SDL3 changes.

**No game files are ever packaged.** The APK holds only the port's own code. Players bring their
own extracted copy of the game and the app copies it into its storage on the phone.

## What it does

`Gow2Activity` extends `SDLActivity`:

1. Before any native library loads it sets `HOME` and `XDG_DATA_HOME` to the app's files folder
   (`Android/data/com.santiquiroz.gow2recomp/files`; ReXGlue keeps user data in
   `$XDG_DATA_HOME/<app name>`) and `TMPDIR` to its cache folder (Android has no `/tmp`).
2. If `default.xex` is not in `Android/data/com.santiquiroz.gow2recomp/files/game`, it shows a
   setup screen instead of SDL's surface; while that surface is detached SDL never starts its
   native thread. The player picks the folder of their extracted game (Storage Access Framework)
   and the app copies it, with progress, into `files/game.incoming` and then renames that to
   `files/game`. The copy is needed because the runtime memory-maps the game files and a
   `content://` URI cannot be memory-mapped. The player can also copy the folder over USB from a
   PC and press *Check again*. File names from the picker are validated (no `..`, separators or
   NUL) and the free space is checked before copying.
3. It loads the libraries listed in `NativeLibraries.java` and passes
   `--game_data_root=<files/game>` to `SDL_main`. It also sets `REX_EXECUTABLE_FOLDER` to the files
   folder, where the runtime keeps its config and logs (an app's process image is
   `/system/bin/app_process64`).

The placeholder `libgow2shell.so` (`app/src/main/cpp`) opens an SDL3 window with Vulkan, logs the
GPU, its driver, the Vulkan features and extensions the port cares about to logcat (tag
`Gow2Shell`) and clears the screen to a solid dark red.

## Requirements

- JDK 17.
- Android SDK with platform 36, NDK `30.0.14904198` and CMake `4.1.2` (SDK manager).
- An SDL3 source tree: `thirdparty/sdl3` of a ReXGlue SDK checkout, for example
  `out/sdk-android-src/thirdparty/sdl3` once `scripts/build-android.ps1` has run. It is only read;
  the build files go to `app/.cxx`.

The scripts look for the NDK pinned in `app/build.gradle` (`ndkVersion`) under `ANDROID_HOME`,
`ANDROID_SDK_ROOT`, `sdk.dir` of `local.properties` or `%LOCALAPPDATA%\Android\Sdk`, then
`ANDROID_NDK_HOME`, then the newest installed NDK; a pre-release NDK whose base revision is the
pinned one (for example `30.0.14904198-beta1`) counts as the pinned one.

## Build (placeholder shell)

Create `android/local.properties` (it is not in git; paths are examples):

```properties
sdk.dir=C:/Users/you/AppData/Local/Android/Sdk
gow2.sdl3.dir=C:/path/to/rexglue-sdk/thirdparty/sdl3
```

Then, from `android/`:

```sh
./gradlew assembleDebug testDebugUnitTest      # gradlew.bat on Windows
```

The APK is `app/build/outputs/apk/debug/app-debug.apk` (arm64-v8a only, minSdk 29, targetSdk 36).
Native libraries are stored uncompressed and 16 KB aligned in the APK (`useLegacyPackaging =
false`, NDK r28+ default linker alignment), so the app also runs on 16 KB-page devices.

## Licenses in the APK

Every APK carries the license texts in `assets/licenses/`: the repository's [`LICENSE`](../LICENSE),
[`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md), [`LICENSE.SDL3.txt`](LICENSE.SDL3.txt)
and [`LICENSE.SGSR.txt`](LICENSE.SGSR.txt). The Gradle task `copyLicenseAssets` (`app/build.gradle`)
copies them from the repository at build time for every variant, so there is one copy of each in
git; it stops the build when one of them is missing. `tools/check_android_libs.py` fails an APK
without `assets/licenses/LICENSE`, `assets/licenses/THIRD_PARTY_NOTICES.md` or
`assets/licenses/LICENSE.SGSR.txt`.
`THIRD_PARTY_NOTICES.md` has an "Android APK" section with what each of the five libraries
contains (and a "Windows release zip" section for the PC package), every component's license, and
how to get the FFmpeg and libmspack sources (LGPL 2.1, linked statically into `librexruntime.so`
and the Windows `rexruntime.dll`) and rebuild and relink them.

## Install and run

```sh
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.santiquiroz.gow2recomp/.Gow2Activity
adb logcat -s Gow2Shell Gow2Activity SDL
```

Debug builds also accept two launch extras for unattended runs; release builds ignore both.
`SKIP_GAME_SETUP` starts the native side without game files, which is how the placeholder is
tested, and `SHOW_WHEN_LOCKED` shows the activity over a locked screen and turns the screen on:

```sh
adb shell am start -n com.santiquiroz.gow2recomp/.Gow2Activity \
    --ez com.santiquiroz.gow2recomp.extra.SKIP_GAME_SETUP true \
    --ez com.santiquiroz.gow2recomp.extra.SHOW_WHEN_LOCKED true
```

## Verified on the Galaxy S25 Ultra

October 2026, Android 16, stock Qualcomm driver (build f61dec9117, 03/26/26): the APK installs
(`extractNativeLibs=false`, all libraries 16 KB aligned), the setup screen appears when there are no
game files and keeps the native side from starting, and the placeholder reports `Adreno (TM) 830`,
Vulkan 1.3.284, all 22 core features it checks (among them `vertexPipelineStoresAndAtomics`,
`geometryShader`, `textureCompressionBC`, `shaderStorageImageWriteWithoutFormat`), no
`VK_EXT_fragment_shader_interlock` or `VK_EXT_memory_budget`, and presents frames through SDL's
Vulkan renderer.

With the game's libraries the phone plays Act 1 with touch only: the touch Start button passes the
PRESS START screen, the D-pad, A, B and Y drive the menus, Solo Campaign > New Campaign plays the
intro movie and the opening cutscene of "Tip of the Spear", and in gameplay the floating stick moves
Marcus, a drag on the right side turns the camera, LB opens Tac/Com and RT fires the Lancer (the
ammo count drops). Checkpoints saved there load again from the save slot screen.

With `gow2_touch_log` the game thread read the pad 135 to 660 ms apart in that area, while another
thread read it every 16 ms. Five quick taps of the D-pad, one of R3 and one of X each reached the
game thread; a steady 4 s drag across the look area gave the game thread 13 reads of 15226 to 21346
(about 21100 while the finger moved at a constant speed, none of them 0); a drag that started on
RT kept firing and turned the camera on every read. The gear opens the Device Options menu, a quick
tap selects in it, and saving wrote `gow2.toml` beside the logs.

Frame rate in that first gameplay area: 2.6 to 5.2 game frames per second (`gow2_frame_log`), with
the GPU held at 160 to 222 MHz of its 1.2 GHz by the phone's thermal limit (thermal status 2 to 3,
skin 42 to 45 C, after hours of testing while charging; `thermal_pwrlevel` 13 to 14 in
`/sys/class/kgsl/kgsl-3d0`). The GPU is 99% busy, so these numbers say little about a cool phone:
after a few minutes' rest (thermal status 2, GPU allowed up to 734 MHz) the same place ran at 13.6
to 16.8 fps for about ten seconds, then fell back to 3 to 6 fps as the cap returned. The menus run
at 30 to 50 fps. On October 15, at the same 222 MHz cap, the same place ran at 4.6 fps with the
SDK's per-frame page invalidation and at 9.4 fps without it (`clear_memory_page_state`, now off on
Android; see [GPU diagnostics and switches](#gpu-diagnostics-and-switches)). From a cool start it
runs at the game's 30 fps cap (29.7 fps at 734 MHz), then follows the falling thermal cap: 15.8 fps
at 342 MHz and 12.7 at 222 MHz by the end of a six-minute run; warm (GPU capped at 160-222 MHz) it
runs at about 10-12.5 fps (10.1 at 160 MHz, 12.5 at 222 MHz).
The black regions were fixed on October 15 with the Android color depth bias default of 16.
The Android default `LightEnvironmentShadows=0` removes the solid-white lamp posts and hanging
banner that light environment shadows produced. The phone's warm colour grade also differs from
the PC's teal grade, with the console match unknown.

## Game build

`scripts/build-android.ps1` builds the APK with the game's libraries from a clean clone of this
repository. It needs, besides the requirements above, Python 3, `git`, `cmake` and `ninja` on
`PATH`, network access the first time (it clones the SDK), and your own `game\default.xex` (only
while `generated\default` does not exist yet, together with the Windows ReXGlue SDK for
`rexglue codegen`, `-Sdk`). The Windows ReXGlue SDK used for codegen must be the one the Windows
build uses, the Windows x64 build of version 0.10.0.9-dev (commit `923c1a5`, GitHub release tag
`nightly-20260921-923c1a59`, see [`BUILDING.md`](../BUILDING.md)): code generated by another SDK
version does not match the runtime built from `923c1a5`. `gow2.sdl3.dir` is not used in this mode.

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-android.ps1 -LowPriority
```

The steps, each skipped when its output is current:

0. Clones ReXGlue `923c1a5` with its submodules into `out\sdk-android-src` and applies every
   `sdk\patches\*.patch` in order (0002-0006 like the Windows SDK build, then
   `0007-android-arm64-port.patch`), with the same clone, marker and symlink repair as
   `scripts\build-sdk.ps1` (shared in `scripts\sdk-source.ps1`). `out\sdk-android-patched.txt`
   records the commit and the patch hashes; when a patch changes (or that file is deleted), the
   tree is reset and patched again. `-Clean` leaves the clone alone. The tree is then compared with
   `923c1a5` plus the series (see [Building your own SDK tree](#building-your-own-sdk-tree)).
1. `rexglue codegen` on every build when `-Sdk` and `game\default.xex` are available (it skips the
   work when `generated\default` is current), and whenever `generated\default` is missing or with
   `-Codegen`. Without them, `generated\default\codegen.d` (the inputs codegen read: the xex, the
   manifest and `hints\*.toml`) decides: inputs that were read in another worktree must equal this
   tree's files (line endings aside), otherwise the build stops, and inputs newer than
   `codegen.build.stamp` print a warning and the existing code is built. A `generated\` folder
   shared between worktrees of different branches would mix their code; give each its own.
2. `tools/prepare_android_sources.py` copies `generated\default` to `out\android\generated\default`
   and applies the switch-trap, game and ARM memory-barrier patches to the copy; files whose
   bytes did not change keep their timestamps, so ninja recompiles only the changed ones. The
   Windows tree is never patched for ARM.
3. CMake configures [`gamelibs/`](gamelibs/CMakeLists.txt) with the NDK toolchain (arm64-v8a,
   API = `minSdk` of `app/build.gradle` (29; `-Api` must agree with it),
   `-march=armv8.2-a`, `-fno-emulated-tls`, `-Wl,-z,max-page-size=16384`) into
   `out\android\build`. The SDK source tree is part of that build (its binaries go to
   `out\android\build\sdk`, not to the SDK's own `out\android-arm64`). The script prints the
   SDK version the configure step derived from the SDK tree's git history (`0.10.0.9-dev.g923c1a5`
   for the clean clone) and warns when it names a commit other than that tree's `HEAD`.
4. Builds `libSDL3.so`, `librexruntime.so` and `librexgpu-xenos.so`, then `libgow2.so`, and stages
   them with the NDK's `libc++_shared.so` in `out\android\jniLibs\arm64-v8a`.
5. `tools/check_android_libs.py` checks the staged libraries, then the old APK is deleted (Gradle
   packs a debug APK incrementally into the existing file, which kept every rebuild's dead space),
   Gradle builds `assembleDebug -Pgow2.gameLibraries=true` and the APK is checked again.

Every source is compiled with `-ffile-prefix-map` for the repository (`gow2-recomp/...`), the SDK
tree (`rexglue-sdk/...`) and the NDK (`android-ndk/...`), so `__FILE__` strings and debug info hold
no absolute paths: the libraries do not depend on where the checkout lives and carry none of the
builder's folders. For debug info Clang applies the last matching map, so the SDK tree's map comes
last. The configure step also sets `SOURCE_DATE_EPOCH` to the time of the SDK commit (unless it is
already set), which CMake uses for the build stamp the SDK writes into `librexruntime.so`, and the
license files the APK copies are checked out with LF everywhere (`.gitattributes`). Two builds of
one commit, from checkouts in `C:\personal\gow2-recomp` and `C:\personal\gow2-recomp-repro`, gave
byte-identical libraries, stripped and unstripped, and byte-identical APKs (same SHA-256, both
signed with the same debug key).

Logs are in `out\android\logs`. A first build took 10 minutes on a busy 24-thread PC, plus the SDK
clone; a rebuild after a source change takes about a minute.

### Building your own SDK tree

`-SdkSource <folder>` (or `REXGLUE_SDK_ANDROID`) builds that rexglue-sdk tree instead of
`out\sdk-android-src`, for example the worktree you change the Android port in: ReXGlue `923c1a5`
with all of `sdk\patches` applied. Before anything else the script compares the tree's files with
`923c1a5` plus every patch, through temporary git indexes, so the tree's own index and commits
stay as they are (about 2 s). An identical tree passes. A tree with changes beyond the series
passes with the list of the extra paths. A tree that lacks part of the series, or changes lines it
adds, stops the build with the list of paths that differ; when your tree is the newer one,
regenerate `0007-android-arm64-port.patch` from it first, as
[`sdk/patches/README.md`](../sdk/patches/README.md) describes.

`libgow2.so` holds the recompiled code, the port's code from `src/` and the SDK's SDL entry point
(`windowed_app_main_sdl.cpp`, which exports `SDL_main` on Android). The version script
`gamelibs/libgow2.map` keeps the recompiled code's ~98,000 symbols local, so the library exports
only `SDL_main` (which `SDLActivity` finds with `dlsym`). That shrank `libgow2.so` in the APK from
62.2 MB to 52.0 MB and its load time from 10-15 ms to 2-7 ms; the whole APK, packed from scratch, is 67.6 MB. Every file
in `src/` is built except the Windows-only ones listed in `gamelibs/CMakeLists.txt`
(`gow2_xam_input_guard.cpp`, `gow2_utf8_guard.cpp`, `gow2_crash_watch.cpp`);
`gamelibs/gow2_android_glue.cpp` stands in for them: it suppresses the disc-error box itself, logs
that the XamInput serialization and the non-throwing UTF-8 decoder (an invalid byte decodes as a
one-byte code point instead of throwing) come from patch 0007, and that there is no crash watch on
Android.

Gradle alone, once the libraries are staged:

```sh
./gradlew assembleDebug -Pgow2.gameLibraries=true    # gradlew.bat on Windows
```

It packages `../out/android/jniLibs` (or `gow2.gameLibs.dir`, as a `-P` property or in
`local.properties`) and stops with a clear message if one of the five libraries is missing.

### Checking the libraries

```sh
python tools/check_android_libs.py out/android/jniLibs/arm64-v8a
python tools/check_android_libs.py android/app/build/outputs/apk/debug/app-debug.apk
```

It uses the NDK's `llvm-readelf` and `llvm-nm` and fails when: an expected library is missing or is
not an AArch64 shared object; a `PT_LOAD` segment is not 16 KB aligned (and, in an APK, a library is
compressed or not 16 KB aligned in the zip); a `DT_NEEDED` entry is neither packaged nor an allowed
system library; an undefined symbol is not defined by the libraries the linker will search for it
(the library's own `DT_NEEDED` closure, with the NDK's API-29 stubs for system libraries, which is
how Android's linker resolves app libraries); a library other than `libSDL3.so` defines `SDL_*` or
`Java_org_libsdl_app_*` symbols; or `libgow2.so` does not export `SDL_main`; or `libgow2.so` exports
anything besides `SDL_main` (`--main-exports` lists the allowed names); or an APK lacks
`assets/licenses/LICENSE`, `assets/licenses/THIRD_PARTY_NOTICES.md` or
`assets/licenses/LICENSE.SGSR.txt` (`--no-notices` skips that
check); or a library contains an absolute build path: the repository root, the NDK folder or a
`--forbid-path` (the build script passes the SDK tree), with either slash and in any case
(`--allow-build-paths` skips that check).

### Self-test on the phone

`scripts/test-android-device.ps1` (`-Serial`, `-SkipBuild`, `-LowPriority`) builds
`gow2_runtime_selftest` (`gamelibs/tests/runtime_selftest.cpp`, not part of the APK) in
`out\android\build` (configured by `scripts\build-android.ps1` first), pushes it with `libSDL3.so`,
`librexruntime.so` and `libc++_shared.so` to `/data/local/tmp/gow2-selftest` over adb, runs it there
and exits with its exit code; the app and its data are untouched. It checks patch 0007 through the
runtime's public API: the UTF-8 helpers (`utf8_count`, `to_utf16`, `utf8_hash_fnv1a`,
`utf8_lower_ascii`, `utf8_equal_case`, `utf8_starts_with_case`) never throw on invalid, truncated,
overlong, surrogate and out-of-range sequences and decode each invalid byte as a one-byte code point
(`0xFF` becomes U+00FF, as in `src/gow2_utf8_guard.cpp`, not U+FFFD, so names hash alike on both
platforms). 8 threads calling `InputSystem`'s `GetState`, `GetCapabilities`, `SetState` and
`GetKeystroke` over a counting driver never have more than one thread inside the driver, while the
same driver called without `InputSystem` does see overlapping threads (so the check can detect the
race). The guest `XamInputGetState`, `XamInputGetCapabilities(Ex)`, `XamInputSetState` and
`XamInputGetKeystroke(Ex)` call exactly those four methods. On the S25 Ultra it printed
`runtime_selftest: 18 checks, 0 failed`. A 19th check (October 15) sets d8-d15 to known values,
switches to a fiber that overwrites them and back, and expects them unchanged: AAPCS64 makes them
callee-saved, and patch 0007's fiber switch did not keep them before. On the phone:
`runtime_selftest: 19 checks, 0 failed`. The 20th and 21st (added later on October 15) write each
guest FPSCR rounding mode, read it back as `mffs` does and round 1/3 with it: FPCR.RMode orders
+infinity and -infinity unlike the PC's MXCSR.RC, and the runtime used to read Up as Down and Down
as Up on ARM.
The script then runs `gow2_touch_model_test`
(`gamelibs/tests/touch_model_test.cpp`, pure C++): the touch layout, finger ownership, the stick,
taps held until each reader has seen them (also while a second reader polls every 16 ms), look
speed between reads 300 and 900 ms apart, the look curve's direction and smoothness, RT and LT
drags that turn the camera, R3, the settings control and the aim toggle
(`touch_model_test: 105 checks, 0 failed`).

```powershell
powershell -ExecutionPolicy Bypass -File scripts\test-android-device.ps1 -LowPriority
```

## Native libraries

`gow2.gameLibraries` in `gradle.properties` (or `-Pgow2.gameLibraries=true`) selects the list in
`NativeLibraries.java`:

| Value | Loaded, in order (the last one holds `SDL_main`) |
|---|---|
| `false` (default) | `libSDL3.so`, `libgow2shell.so` (placeholder) |
| `true` | `libSDL3.so`, `librexruntime.so`, `librexgpu-xenos.so`, `libgow2.so` |

There is exactly one SDL in the process: the SDK's Android build makes SDL3 a shared library
(`SDL_SHARED`), `librexruntime.so` and `librexgpu-xenos.so` link that `libSDL3.so`, and SDLActivity
loads it first, so its `JNI_OnLoad` sets up the JNI state the runtime's SDL calls use. A static SDL
inside the runtime would be a second copy without that state.

## Upscaling and display

Android now defaults `present_effect` to `"fsr"` (previously only `"bilinear"`). The choices are
`"bilinear"`, `"cas"`, `"fsr"` (AMD FSR 1) and `"sgsr"` (Snapdragon Game Super Resolution 1).
FSR 2 and FSR 3 are not available on Android. Set a choice in `gow2.toml` beside the logs, then
relaunch, for example:

```toml
present_effect = "sgsr"
```

Use `present_effect = "bilinear"` for the old filter. At launch, only the selected effect's shader
modules and pipelines are created (bilinear is always available); a driver-rejected module or
pipeline makes that effect unavailable with one warning, and painting falls back to bilinear with
one warning. FSR 1 runs EASU from 1280x720 to 2560x1440 into an intermediate image at that output
size, then RCAS into the swapchain. SGSR draws once straight into the swapchain, with no
intermediate image and no dithering. `present_dither` is
true on Android (false elsewhere) for the final pass of the other effects.
`present_fsr_sharpness_reduction` is 0.3 stops on Android (0.2 elsewhere).
`present_cas_additional_sharpness` (0) and `present_fsr_max_upsampling_passes` (4) are now available
on Android too; their defaults are unchanged from elsewhere.

The runtime still renders the game at 1280x720. These upscalers change the presented image, not
the GPU work of the game. On the phone, FSR 1 and SGSR 1 drew correctly on the Adreno 830 and were
sharper than bilinear (SGSR a little crisper); their cost was within run-to-run noise (one run
each). `present_effect` stays `fsr` ([ANDROID_EXPERIMENTS.md](../docs/ANDROID_EXPERIMENTS.md), P7).

Android defaults `vulkan_allow_present_mode_immediate`, `vulkan_allow_present_mode_mailbox` and
`vulkan_allow_present_mode_fifo_relaxed` to false, selecting plain FIFO (the October 15 logs used
MAILBOX). With FIFO the presenter paints on the UI thread, never on the GPU command thread.
`present_android_frame_rate` defaults to 30: the window asks Android 11+ for this rate through
`ANativeWindow_setFrameRate`; P8/C3 confirmed the panel left 120 Hz for 30 Hz with FIFO and the
30 Hz request. It is a request, not a forced refresh rate. 0 makes no request. If `gow2_unlock_fps` is
on and `gow2.toml` does not set the rate, the port uses 0 for that launch (`DISPLAY frame rate hint`
in the log). The runtime also logs `VulkanPresenter: ANativeWindow_setFrameRate(30 Hz) returned 0`
when that call succeeds. These changes are Android-only; Windows behaviour is unchanged.

## Controls

### Touch

`gow2_touch=auto` (the default on Android) turns the on-screen controls on when the phone has a
touchscreen (`Configuration.touchscreen`). Fingers drive a virtual Xbox 360 pad
(`src/gow2_touch_controls.cpp`, layout and pointer logic in `src/gow2_touch_model.cpp`) that the
SDK merges with real pads for the first player, so a controller and the touch controls work at the
same time.

| Where (landscape) | Control |
|---|---|
| Left 40% of the screen, below the top 15% | Floating left stick: it is centred where the finger lands, full tilt at 10% of the screen height |
| Rest of the right side | Look: the right stick follows the finger's speed between two reads of the pad (its travel since that reader's previous read over the time since then), full tilt at one screen height per second (`gow2_touch_look_gain` scales it). The deflection keeps the drag's direction and gets a square root and a minimum of about 0.4 so slow drags get past the game's look dead zone; within 8 degrees of an axis it snaps to the axis, blending back to the drag's angle by 16 degrees. The game's own aim assist stays on |
| Right side, large | RT (fire); LT (aim, held, or toggled with `gow2_touch_aim_toggle`). A finger on RT or LT that slides also turns the camera |
| Bottom right | A (cover, run), B (melee), X (reload, use) |
| Right side | Y, RB (active reload) |
| Left of RB, RT and LT | R3 (right-stick click: zoom while aiming, for example the Longshot's scope) |
| Top left | LB (Tac/Com) |
| Left, above the stick | D-pad (weapons, menus) |
| Top centre | Back, Start |
| Top right corner (gear) | Opens and closes the Device Options menu (the runtime's raw settings are in Device Options > System > All runtime settings, F4 on a keyboard) |

Each finger owns the control it touched first until it lifts. A tap stays pressed until the game has
read it, and for at least 100 ms. Every thread that reads the pad (`XamInputGetState`) and the
key-event reader (`XamInputGetKeystroke`) count as separate readers, because in gameplay the game
thread reads once per slow frame while another thread reads every 16 ms; each reader sees every tap
and the look travel since its own previous read. A second tap of the same control between two
reads gets a release in between, and a tap no read sees within 2 s is dropped. Losing focus
releases everything. Fingers reach the controls through an SDL event watch as the activity hands
them over, with SDL's event times, instead of after the UI thread's next present, which can take a
whole slow game frame. There is no L3 control. `gow2_touch_log=true` logs every read that is not
neutral (`TOUCHPAD` lines, with the reading thread).

The controls are a translucent ImGui overlay (`gow2_touch_opacity`, `gow2_touch_scale`) drawn with
each game frame, not at the display's refresh rate (patch 0007's
`ImGuiDialog::WantsContinuousPaint`). While a controller is connected, using it (any button, or an
axis past about a quarter) hides them until the next touch; unplugging the last controller shows
them at once (not yet tried with a real controller on the phone). A key or a mouse hides them for
`gow2_touch_hide_seconds` (10). The first touch on hidden controls only shows them.

The gear opens the Device Options menu, whose Touch tab holds
the `gow2_touch_*` settings. ImGui takes touches like a mouse there
(patch 0007 turns a quick tap into a click), and the game's pad reads
neutral while the menu is open. Saving writes `gow2.toml` in
`Android/data/com.santiquiroz.gow2recomp/files`, beside the `logs` folder.
A keyboard or mouse found at launch makes `mnk_mode` / `mnk_mouse` that
launch's default value instead of a changed one, so saving does not keep
them for a later launch without the device.

Patch 0007 makes the SDK's window deliver Android finger events to its listeners and drop the mouse
events SDL makes up from fingers and pens. On Windows the SDK does not deliver finger events, the
touch files are not built and `gow2_touch` defaults to `off`.

### Controllers, keyboard and mouse

Controllers (USB or Bluetooth) go through the SDK's SDL driver as before. A physical keyboard or an
external mouse attached when the game starts turns on the keyboard/mouse driver (`mnk_mode`, with
the same key binds as on Windows); a mouse also sets `mnk_mouse`, so the native mouse look of
`src/gow2_mouse_aim.cpp` installs. The runtime log records the choice in an `INPUTDEV` line. The
devices are found through JNI (`src/gow2_android_devices.cpp`): the keyboard from
`Configuration.keyboard`, the mouse from `InputDevice` entries that are external, not virtual and
have `SOURCE_MOUSE` (the S25 Ultra's built-in `sec_touchpad` reports `SOURCE_MOUSE` too). A keyboard
or mouse plugged in after the start is not picked up. Only the keyboard path was exercised on the
phone, with `adb shell input keyevent` and `mnk_mode=true` in `gow2.toml`; no real keyboard or mouse
was at hand.

### Network

Solo campaign opens a UDP socket for its session layer, so the app asks for `INTERNET` (without it
the campaign stops at "Error initializing network layer"). Apps may not bind ports below 1024: patch
0011 adds `net_port_offset` (40000) to every guest port below 1024, on the phone and the PC alike, so
the game's port 1000 binds as 41000 (with `system_link_trace=true`, a
`[netlink] bind port 1000 -> 41000 (net_port_offset)` log line), and it maps the socket options and
`FIONBIO`/`FIONREAD` the game passes with Winsock numbers to the host's. Patch 0007 alone shifted
those ports by 30000 (31000); 0011 replaces that code, so a phone and a PC meet on 41000. For System
Link the app also asks for the Wi-Fi permissions, holds Wi-Fi locks and passes its Wi-Fi address as
`REX_DETECTED_LOCAL_IP`; see [docs/NETWORK_PLAY.md](../docs/NETWORK_PLAY.md).

### Frame rate log

`gow2_frame_log=true` (in `gow2.toml` beside the logs) logs the game's own frame rate every five
seconds as `FRAMES` lines, counted at the player's input step. Presented frames do not measure it on
a phone: Android lowers the display's refresh rate, and runtime overlays repaint without new game
frames. Each line ends with the GPU clock and its current ceiling, busy percentage and
`thermal_pwrlevel` from `/sys/class/kgsl/kgsl-3d0`, and the thermal status and headroom Android
reports (`n/a` for whatever the app may not read), so a rate can be read against the clock it ran
at. Android 16 on the S25 Ultra hides `gpuclk` from apps and from `adb shell`; the clock comes from
`devfreq/cur_freq` (or `clock_mhz`) instead, as in `scripts/android-gpu-clocks.sh`. `tools/android_log_summary.py` turns a log into one row per window with GPU megacycles per
game frame.

### GPU diagnostics and switches

Patch 0007's Vulkan side has Android-only diagnostics and switches for the black scene and the
frame rate; the diagnostics keep the default behaviour unless set in `gow2.toml`, while
`vulkan_android_color_depth_bias` defaults to 16 on Qualcomm GPUs (Vulkan vendor ID `0x5143`),
and 0 on other GPUs unless set, as described below.
[`docs/ANDROID_EXPERIMENTS.md`](../docs/ANDROID_EXPERIMENTS.md) lists them and the order to try
them in. `vulkan_android_stats_log=true` adds a `GPUSTATS` line every five seconds (draws by kind,
render passes, submissions, waits for the whole GPU, occlusion queries, time the GPU command thread
spends building pipelines, why render passes ended, guest memory uploads by the draw step that
needed them, CPU writes caught by the memory watches, texture loads and all-zero texture data) and
a `TEXFMT` line per texture format; a texture whose data cannot be loaded logs `TEXFAIL` once per
format. `gow2_system_settings` sets keys of the game's own `[SystemSettings]` (for example
`DynamicShadows=0;AmbientOcclusion=0`) in the cached Coalesced copy, so the game's own
effects can be switched off without a rebuild. On Android it defaults to
`LightEnvironmentShadows=0`; setting the key replaces that default. A value longer than the
original is not applied (`False` is longer than `True`; `0` reads as false).
`vulkan_android_depth_experiment` (diagnostic bits:
depth test forced to pass, depth prepass skipped, stencil off, whole-target scissor, scene draws
painted magenta, culling off, no clip planes, prepass meshes skipped without their depth clears,
every float24 color draw passing its depth test, mid-frame depth clears skipped, prepass painted),
the shader hash lists `vulkan_android_skip_shader_hashes` and
`vulkan_android_paint_shader_hashes`, `vulkan_android_skip_draw_range` and
`vulkan_android_trace_frame_after_seconds` (every draw of one frame logged as `FRAMETRACE` with its
render targets, viewport, scissor and polygon offset, plus the frame's render target transfers,
resolves and resolve clears as `FRAMETRACE-RT`) remain diagnostics for the black scene.
`vulkan_android_color_depth_bias` defaults to 16 on Qualcomm GPUs (Vulkan vendor ID `0x5143`)
unless set, and 0 on other GPUs. Sixteen units are 16 float32 ULPs of the D32 host depth (float24
is hosted as `D32_SFLOAT_S8_UINT`), about 2 float24 steps; the measured prepass/base-pass mismatch
is up to about one float24 step. It adds bias to color draws with a pixel shader on float24 depth
that test GREATER or GEQUAL; 0 turns it off. The runtime logs
`Android: color depth bias 16 (GPU vendor 0x5143, default for this vendor)`.
`ResolveColorDepthBias` in `include/rex/graphics/vulkan/android_draw_experiments.h` is checked by
`tests/unit/graphics/android_draw_experiments_test.cpp`. `docs/ANDROID_EXPERIMENTS.md` lists them.

`clear_memory_page_state` is off on Android (patch 0007; the SDK's default, on, stays elsewhere).
On, it marks every page the CPU uploaded as stale at each frame end, so the whole working set was
copied to the GPU again every frame: about 1,850 uploads and 21 MB a frame in Act 1, each upload a
transfer that ends the Vulkan render pass (about 1,600 render passes a frame). Off, the memory
watches still invalidate the pages the CPU writes; the frame rate doubled at the same GPU clock.

`vulkan_tight_render_area` (SDK 0089, on by default on every Vulkan platform) begins each render
pass with a render area that covers only the scissors of its draws and its clear rectangles. The
render targets are sized for the whole EDRAM (1520x3456 in the phone's logs) and every pass loads
and stores them, so the tile GPU moved about 5.7 times the 1280x720 a 720p pass draws. It is not
measured on the phone yet; `false` takes effect at once and goes back to the whole target.

The Vulkan shader and pipeline storage survives a relaunch: Bionic starts reading an `"a+"`
stream at the end of the file, so the storage found no header and truncated itself at every
launch, and every pipeline was built again during play. Each new pipeline held the GPU command
thread for 40-48 ms, and those bursts made the slowest windows of the October 14-15 runs. Patch
0007 rewinds such streams; on the phone the storage now loads its pipelines at boot (`Created 295
graphics pipelines from Vulkan storage` once the first gameplay has filled it).

`vulkan_pipeline_cache_file=true` also keeps the driver's pipeline cache in
`cache/shaders/shareable/4D53082D.vkpc`, beside `4D53082D.xsh` and `4D53082D.fbo.vk.xpso` under
the game's storage. At boot it checks vendor, device, driver version, the driver's pipeline cache
UUID, size and an XXH3 hash; a different or damaged file is ignored and rewritten. The storage
writer saves it after the boot-time pipelines are built, every two minutes while new pipelines
appear, and at shutdown, by writing `.tmp` then renaming it. Look for
`VulkanPipelineCache: pipeline cache file <path>: <status> (<n> bytes of driver data)` and
`VulkanPipelineCache: saved <n> bytes of driver pipeline cache to <path>`.
Reusing driver data avoided repeating compilation in P10: 852 stored pipelines built in 2.7 s with
the driver cache against 27.5 s without, on a warm phone.

### CPU waits

`gpu_worker_spin_us` defaults to -1, the SDK's 500 yield polls before sleeping.
Setting it to 50 makes the GPU command thread poll for up to 50 microseconds with ISB
pauses, then sleep on the write pointer event in 5 ms waits; 0 sleeps at once.
P9 measured no difference on the warm phone at the same GPU clock, so the default returned to the
SDK behaviour.

The game's render thread waits for room in the GPU ring buffer by polling the read pointer the
command processor writes back (`sub_8298C2A0` through `sub_82977688`). On the phone that wait takes
about 80 % of the thread's time, spinning a whole core. `gow2_gpu_wait_sleep_us` (default 0, the
console's spin) makes it sleep that many microseconds between polls, at most 1 ms before the game's
own check runs again; `gow2_gpu_wait_spin_us` spins that long first. Both take effect at once, and
`gow2_gpu_wait_stats` logs a GPUWAIT line every five seconds. `gow2_swap_queue_limit` (default 0, the
game's 15) caps the frames D3D Swap lets the render thread queue ahead of the GPU command thread; it
is not measured yet. On the S25 Ultra (chapter 2, A B B A,
November 13), 200 µs took the render thread from 99 % to 43 % of a core and the process from 200 %
to 155 %, with 1.5 fps less once the heat drift is removed: inside the spread, but about 7 % fewer
frames per GPU GHz in two sessions, so it stays off until it is measured with the read pointer
written back block by block (SDK 0088, `gpu_read_pointer_writeback_blocks`, also off by default:
with the spin it changed nothing).

At build time `tools/prepare_android_sources.py` runs `tools/patch_spin_hints.py` on the Android
copy of the generated code. It adds a guarded `__builtin_arm_isb(15)` after Xenon delay hints, which
the recompiler otherwise emits nothing for: `#if defined(__aarch64__)` / `__builtin_arm_isb(15);` /
`#endif`. The patched text also compiles on x64. The script refuses paths outside `out/android` and
takes no default path. GoW2 has 12 `db16cyc` sites: eight in `sub_82977688` and the others in the
wait loops of `sub_829925B0`, `sub_82955768` and `sub_829531F8`. The original `generated/` and the
Windows build do not receive these pauses. The pauses were present in both P9 arms, so their own
effect was not isolated; separating it needs a build without them.

`gow2_android_sustained_performance=false` leaves sustained performance mode opt-in. True asks
for the window's mode where `PowerManager.isSustainedPerformanceModeSupported()` allows it; the
runtime logs `SUSTAINED performance mode requested: ...`. Sustained clocks and frame rate remain
to be measured (P11).

## Menu prompts

With `gow2_prompts=auto` the menus show the controller glyphs unless a physical keyboard is attached
and `mnk_mode` is on, in which case they show the keyboard labels. The keyboard is detected through
SDL's JNI bridge (`Configuration.keyboard` of the activity), because SDL 3.4.14 keeps no keyboard
list on Android. The runtime log records the choice in a `PROMPTS` line. On Windows the rule is
unchanged: keyboard labels unless an XInput controller is connected at launch.

## Not done yet

- Speed: about 10-12.5 game frames per second once the phone is warm (GPU capped at 160-222 MHz),
  about 30 for the first minutes from a cool start (see above); the Xenos
  path's remaining cost on the Adreno 830 is next
  ([`docs/ANDROID_EXPERIMENTS.md`](../docs/ANDROID_EXPERIMENTS.md), steps 1 and P1-P6).
- Rendering: the black regions were fixed on October 15. The Android default
  `LightEnvironmentShadows=0` also removes the solid-white lamp posts and hanging banner;
  the cause in the Vulkan backend or driver remains open. The phone's
  warm colour grade differs from the PC's teal grade; which matches the console is not established
  (same document, [black scene results](../docs/ANDROID_EXPERIMENTS.md#black-scene-found-october-15-second-session)).
- A checkpoint written by an older build (before the network fixes) crashes on load: the main
  thread's guest stack overflows in `sub_8255E4A8`, which calls itself through an object listed as
  its own child (`Unhandled guest access violation: write of guest 0x7014FFD0`). New campaigns and
  their checkpoints load.
- Keyboard and mouse hot-plug, a test with a real keyboard and mouse, and a separate `:game`
  process.
