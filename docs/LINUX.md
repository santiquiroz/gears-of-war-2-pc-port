# Linux and SteamOS (experimental)

A native x86_64 Linux build of the port: the same recompiled game code as the Windows build, on the
SDK's Vulkan backend, with SDL for the window, controllers and audio. It runs on SteamOS (Steam Deck,
Legion Go) and on desktop distributions with a Vulkan driver.

It needs an x86-64-v3 CPU (AVX2, FMA, BMI2, MOVBE): the Steam Deck and Legion Go, Intel Core from
2013 (Haswell) and AMD from 2015 (Excavator, every Ryzen). On an older CPU `gow2.sh` stops with a
message instead of crashing.

## Play

You need your own retail Xbox 360 copy of Gears of War 2 as an extracted game folder: one that has
`default.xex` and the `GearGame` folder. The Linux build cannot extract an `.iso` itself; extract it
with [xdvdfs](https://github.com/antangelo/xdvdfs) (`xdvdfs unpack gow2.iso gow2-game`), or copy the
folder the Windows build made.

1. Extract `gow2-recomp-<version>-linux-x86_64.tar.gz` and copy everything inside its folder into the
   game folder, next to `default.xex`.
2. Run `./gow2.sh` from that folder.

The game folder is found without asking when the gow2 files are inside it or right next to it. Any
other place works with `--game_data_root=/path/to/game`, or with that path written on the first line of
`gow2_game_path.txt` next to `gow2`.

### Steam Deck and Legion Go (SteamOS)

1. In Desktop Mode, extract the package on the device itself (Dolphin: right-click the `.tar.gz` >
   Extract > Extract archive here) and copy its files into the game folder as above. Files extracted
   on Windows and copied over lose their "executable" permission, and Steam then refuses `gow2.sh`
   with "Unsupported file type".
2. If the files came from Windows anyway: in the game folder, right-click an empty area > Open
   Terminal Here, and run `chmod +x gow2 gow2.sh`.
3. Steam > Games > Add a Non-Steam Game to My Library > Browse, set File type to All Files, pick
   `gow2.sh` and add it.
4. In the shortcut's Properties, leave Compatibility unchecked: it is a native Linux program, not one
   for Proton.
5. Start it from Gaming Mode. The built-in controller works as an Xbox controller.

If Steam still says "Unsupported file type" after step 2, the game folder is on a drive formatted on
Windows (FAT32, exFAT or NTFS), which does not keep that permission: move the folder to the internal
storage or to a card formatted by SteamOS.

## Where things live

| What | Where |
|---|---|
| Settings (`gow2.toml`), logs | next to `gow2` |
| Saves, profile, achievements | `~/.local/share/gow2` (or `$XDG_DATA_HOME/gow2`) |
| Shader cache, edited Coalesced copies | `~/.local/share/gow2/cache` |

`gow2_save_dir` in `gow2.toml` moves the saves, as on Windows (see [SAVES.md](SAVES.md)).

## What is different from Windows

- Graphics always run on Vulkan, which is experimental on Windows too. The Direct3D 12 features are
  not available: FSR and DLSS upscaling, frame generation, HDR10 and texture packs.
- No in-game disc image extraction and no automatic update download.
- The PC Options page "Host over the internet" opens ports with a PowerShell script, so on Linux open
  UDP 14001 and 41000 on your router yourself (see [ONLINE.md](ONLINE.md)).
- The game needs an audio output (PipeWire, PulseAudio or ALSA). Without one it stops at the first
  movie.
- SteamOS ships RenderDoc, a graphics debugger. Until 0.4.2 the game loaded it on every start, which
  showed "Capturing Vulkan" and the frame time in the top-left corner and cost some performance. Now
  RenderDoc is only used when you start the game from RenderDoc; `load_renderdoc = true` in
  `gow2.toml` brings the overlay back (its F12 and Print Screen keys take captures). For a frame
  counter, turn on PC Options > Display > Frame counter, or use Steam's Performance overlay.
- Up to 0.4.2, when the game crashed or was closed by force (Steam's Exit Game, `kill -9`), it left
  its memory behind in RAM as a `xenia_memory_*` file in `/dev/shm`, several hundred MB each, until
  a reboot. Now the system frees it however the game ends (SDK patch 0084). With an older version,
  reboot, or run `rm /dev/shm/xenia_memory_*` while the game is not running.
- Since 0.4.4 each Vulkan render pass covers only the area its draws and clears can write, instead
  of the whole render target (SDK patch 0089). If part of the picture looks missing or stale, set
  `vulkan_tight_render_area = false` in `gow2.toml` and open an issue with the scene.
- While it waits for the GPU, the game's render thread spins a CPU core. `gow2_gpu_wait_sleep_us =
  200` in `gow2.toml` makes it sleep between checks: on a phone that freed about half a core for
  about 7 % fewer frames per GPU clock. It is off by default and not measured on a Steam Deck; try it
  if battery life matters more than the last frames.

## Build

On Ubuntu 24.04 (or any distribution with clang 20 or later and CMake 3.25 or later):

```sh
sudo apt install cmake ninja-build pkg-config python3 git lld \
  libx11-dev libx11-xcb-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev \
  libxfixes-dev libxtst-dev libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev \
  libasound2-dev libpulse-dev libpipewire-0.3-dev libdbus-1-dev libudev-dev libdrm-dev
# clang 20 or later, for example from apt.llvm.org:  sudo ./llvm.sh 23
cp /path/to/your/default.xex game/default.xex
scripts/build-linux.sh
```

The script clones ReXGlue at the pinned commit into `out/sdk-linux-src`, applies every
`sdk/patches/*.patch`, builds the SDK's `rexglue` tool and runs codegen on `game/default.xex` when
`generated/` does not exist yet (later builds re-run it when its inputs change), configures
`out/linux`, builds `gow2` and writes `out/linux/package/gow2-recomp-<version>-linux-x86_64.tar.gz`. The
binaries target `x86-64-v3` (since 0.4.2: the byte-swapped guest loads and stores become `movbe` and
every PowerPC multiply-add an inline FMA) and link the system `libstdc++`, so build on a distribution no newer than
the one you run on (Ubuntu 24.04 builds run on current SteamOS).

The script links with lld (`-fuse-ld=lld` in the linker flags; `CMAKE_LINKER_TYPE` needs CMake 3.29 and
3.28 ignores it). With lld the SDK's shared libraries call their own functions directly instead of
through their PLT (`-Wl,-Bsymbolic-non-weak-functions`, SDK patch 0083); with GNU ld they still build,
without that. The SDK and the game use the small code model, like every other target (SDK patch
0081).
