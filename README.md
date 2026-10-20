# Gears of War 2 — native PC port (unofficial)

[![Latest release](https://img.shields.io/github/v/release/santiquiroz/gears-of-war-2-pc-port?label=release)](https://github.com/santiquiroz/gears-of-war-2-pc-port/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/santiquiroz/gears-of-war-2-pc-port/total)](https://github.com/santiquiroz/gears-of-war-2-pc-port/releases)
[![License: BSD-3-Clause](https://img.shields.io/github/license/santiquiroz/gears-of-war-2-pc-port)](LICENSE)
[![GitHub stars](https://img.shields.io/github/stars/santiquiroz/gears-of-war-2-pc-port?style=social)](https://github.com/santiquiroz/gears-of-war-2-pc-port/stargazers)

**Website:** [santiquiroz.github.io/gears-of-war-2-pc-port](https://santiquiroz.github.io/gears-of-war-2-pc-port/)

Retail Xbox 360 Gears of War 2 statically recompiled from the player's own `default.xex` to native x64 code with the ReXGlue SDK. This is not emulation. Players must own the game. An earlier fan port ("Hollow") was built from a leaked 2008 development build; this one runs the complete retail game.

The project's short name, `gow2-recomp`, names the release zip; the repository is `gears-of-war-2-pc-port`.

Leer en español: [README.es.md](README.es.md)

## Screenshots

![Title screen](docs/screenshots/title.jpg)
![Main menu](docs/screenshots/main-menu.jpg)
![PC Options menu](docs/screenshots/pc-options.jpg)
![Act 1, Jacinto Med](docs/screenshots/act1-jacintomed.jpg)
![Act 1 with 4K internal rendering (resolution_scale = 3)](docs/screenshots/act1-4k-internal.jpg)

*Act 1 with 4K internal rendering (resolution_scale = 3).*

![Original 720p rendering (left) vs 4K internal rendering (right)](docs/screenshots/scale-1x-vs-3x.jpg)

*Original 720p rendering (left) vs 4K internal rendering (right).*

## Status

- [x] Act 1 playable with keyboard and mouse or a controller
- [x] Native high resolution (`resolution_scale` 2 = 1440p, 3 = 4K internal)
- [x] Community game patches
- [x] FSR 1 upscaling and CAS sharpening (`present_effect`)
- [x] Texture packs: dump the game's textures, load your own higher-resolution versions
- [x] Mouse aim through the game's own PC mouse axes, with no stick dead zone (`gow2_mouse_aim`, on by default)
- [x] FSR 4 and FSR 3.1 temporal upscaling (`gow2_temporal`, `gow2_upscaler`; off by default). The release has no DLSS
- [x] Keyboard keys and mouse buttons handled as on a PC while a level is played (`gow2_native_keys`, SDK patch 0022); menus, the pause menu and cutscenes still pass through the emulated controller
- [x] Keyboard prompts as keycap pictures that follow your keybinds and switch back to the pad's buttons while a controller is connected (`gow2_keycaps`; build the pack with `scripts/build-keycaps.ps1`, see [tools/keycaps](tools/keycaps/README.md))
- [ ] Full campaign play-through
- [x] System Link campaign co-op for up to four players (`gow2_coop_players`, off by default): four copies on one PC (loopback), and a PC with an Android phone on one LAN for two players, verified 2026-10-14 and 2026-10-15; Stage 2 (chapter changes, deaths, leave/rejoin, checkpoints, cinematics) verified with four copies on one PC on 2026-10-16; a phone and a PC on 0.2.0 joined one System Link lobby on 2026-10-16
- [x] Internet play, experimental (`net_peers`, `net_public_address`, `net_allow_peers`): a phone on mobile data joined a PC-hosted campaign through ports opened by UPnP, verified 2026-10-15; a PC friend outside the network has not been tried yet
- [x] In-game options menu (PC Options; Device Options on Android) ([docs/PC_OPTIONS.md](docs/PC_OPTIONS.md)): verified on a PC and on a Galaxy S25 Ultra on 2026-10-16
- [x] Game quality options: Graphics > Game quality with Low/Medium/High/Custom presets and 24 of the game's own engine settings (shadows, lights, bloom, motion blur, depth of field, ambient occlusion, flares, distortion, decals, fog, foliage, level of detail); all 76 keys in [docs/SYSTEM_SETTINGS.md](docs/SYSTEM_SETTINGS.md)
- [x] Update checker on PC and Android (GitHub Releases, at most twice a day, `gow2_update_check`)
- [x] Automatic settings for integrated GPUs (720p internal, 2x anisotropic filtering, no per-frame guest memory re-upload) for handhelds and laptops
- [x] Android: the white objects/effects of the Vulkan renderer are fixed (0.2.3); object shadows are an option, off by default on Android
- [x] VSync waits for the display on D3D12 and the 30 fps cap holds with VSync off (0.2.4)
- [ ] System Link Horde and Versus
- [ ] Android: a build from the repository reaches gameplay and joins a PC's System Link campaign, but is not finished yet; an experimental APK has been attached to every release since v0.1.2 (see [Android](#android-experimental)). The whole scene has drawn since October 15; it runs at about 30 fps for the first minutes from a cool start, falling to about 10-12.5 fps once the phone is warm ([docs/ANDROID.md](docs/ANDROID.md))

See [ROADMAP.md](ROADMAP.md) for the full plan.

## Download and play

Nothing of the game is included in the release. You need your own retail Xbox 360 copy of Gears of War 2 (title ID `4D53082D`), dumped to a folder that has `default.xex` and the `GearGame` folder, for example by extracting your ISO with [xdvdfs](https://github.com/antangelo/xdvdfs):

```text
xdvdfs unpack "Gears of War 2.iso" C:\Games\GoW2
```

Do not pirate the game.

### PC

Requirements: Windows 10/11 x64 and a DirectX 12 GPU (developed on an AMD Radeon RX 7800 XT).

1. Download `gow2-recomp-<version>-win64.zip` from the [latest release](https://github.com/santiquiroz/gears-of-war-2-pc-port/releases/latest) (the `.apk` is the Android build).
2. Unzip it anywhere and run `gow2.exe`. The exe is not signed, so Windows may show "Windows protected your PC": choose **More info**, then **Run anyway**.
3. The first launch asks for the game folder (the one with `default.xex` and `GearGame`) and remembers it in `gow2_game_path.txt` next to the exe. To pick another folder later, delete that file.

It does not ask when the game folder is inside the exe's folder or right next to it, when the exe's folder is inside the game folder, or when you pass the folder yourself:

```text
gow2.exe --game_data_root="C:\Games\GoW2"
```

The first time you play System Link co-op, Windows asks whether `gow2.exe` may use the network: allow it, or the other players will not find your game.

### Android (experimental)

Requirements: an arm64 phone with Android 10 or newer and Vulkan, and free space for a full copy of the game files.

1. Download `gow2-recomp-<version>-android-arm64.apk` from the [latest release](https://github.com/santiquiroz/gears-of-war-2-pc-port/releases/latest) and install it (Android asks you to allow installing apps from your browser or file manager).
2. Copy your extracted game folder to the phone.
3. Open the app, tap **Choose game folder** and pick that folder. The app copies the files into its own storage, which needs as much free space as the game takes, because the game cannot read files through the folder picker. With the folder from step 2 that is two copies; delete yours once the app has finished.

Instead of steps 2-3 you can copy the folder over USB from a PC, so that `default.xex` ends up in `Android/data/com.santiquiroz.gow2recomp/files/game/`, then tap **Check again**.

The settings menu is called Device Options on Android (the gear in the top-right corner). If the game closes unexpectedly, the next launch opens **Launch options** first: a few settings, **Safe mode** (no sparse GPU memory, 1x resolution, bilinear filter), **Reset settings** and **Share last log**. To open it on purpose, long-press the app icon and choose **Launch options**. Status and known limits: [docs/ANDROID.md](docs/ANDROID.md).

## Controls

| Action | Key |
|---|---|
| Move | W / A / S / D |
| Look | Mouse (or arrow keys) |
| Fire | Left mouse |
| Aim | Right mouse |
| Cover / roadie run / roll | Space |
| Melee / chainsaw (hold) | F or V |
| Use / pick up / revive | E |
| Point of interest | Q |
| Reload | R |
| Tactical display | G |
| Zoom | Middle mouse or Z |
| Weapons | 1–4 |
| Pause | Enter or Esc |
| Back | Tab |
| PC Options menu | F10 |

A controller works at the same time. All bindings are in `gow2.toml`.

## Settings

Everything here can be changed in game from Options > PC Options (or F10, or Back+Start); see [docs/PC_OPTIONS.md](docs/PC_OPTIONS.md). `gow2.toml` sits next to the exe. It is read at start-up; command-line flags (`--name=value`) override it. Main keys:

The Graphics tab includes **Game quality**: Low / Medium / High (retail) presets and 24 individual engine quality controls, all applied after a restart. High restores retail quality; Auto keeps platform defaults. See [the key inventory and presets](docs/SYSTEM_SETTINGS.md).

- `fullscreen` — run fullscreen or windowed.
- `resolution_scale` — internal render resolution as a multiple of 720p: 1 = 720p, 2 = 1440p, 3 = 4K.
- `present_effect` — final image scaling: `bilinear` with the stock runtime; `fsr`, `cas` and experimental temporal paths with a source-built runtime (see below).
- `vsync` — vertical sync.
- `gow2_unlock_fps` — lifts the game's 30 fps cap.
- `gow2_fps_cap` — frame rate limit while unlocked (0 = no limit).
- `gow2_audio_volume` — master volume in percent.
- `gow2_black_shading_fix` — correct character lighting.
- `gow2_texture_streaming_fix` — bigger texture pool, less pop-in.
- `gow2_aniso16` — 16x anisotropic filtering.
- `gow2_ao_upscaling_fix` — ambient occlusion without artifacts when scaled.
- `gow2_dof_upscaling_fix` — depth of field without artifacts when scaled.
- `gow2_temporal`, `gow2_upscaler` — temporal upscaling: `gow2_temporal = true` with `gow2_upscaler = "fsr4"` (Radeon RX 7000/9000) or `"fsr3"`; off by default.
- `mnk_sensitivity` — mouse sensitivity.
- `gow2_save_dir` — folder for saves, profile settings and achievements; point it at a OneDrive, Google Drive or Dropbox folder to keep the campaign in the cloud ([docs/SAVES.md](docs/SAVES.md)).

The community patches come from [xenia-canary/game-patches](https://github.com/xenia-canary/game-patches) (authors illusion and boma); the patched sites and values for this XEX are applied to the generated code, each behind its `gow2_*` setting.

On Windows, the game checks GitHub for a new release at most twice a day and sends nothing else. Turn this off in PC Options > System, or set `gow2_update_check = false` in `gow2.toml`.

## Performance

Measured on a Ryzen 9 7900X3D + Radeon RX 7800 XT, Act 1, `resolution_scale = 2` (1440p internal): about 90 presented frames per second on average, 1% low 55, GPU about 6 ms per frame (PresentMon 2.6, 30 s capture). These numbers are presented frames. For bug reports, `gow2_frame_log = true` logs FRAMESTATS frame times every 5 s and `d3d12_stats_log = true` logs D3D12 counters.

## Modern enhancements

- Native resolution via `resolution_scale` (see Settings).
- FSR: the release runtime is built with AMD FidelityFX. `present_effect = "fsr"` renders at a lower `resolution_scale` and upscales with FSR 1 plus sharpening; `"cas"` only sharpens. Measured with `resolution_scale = 1` and FSR 1 to 1440p: about 85 fps in Act 1.
- Temporal upscaling: FSR 4.1 on Radeon RX 7000/9000 and FSR 3.1 on other GPUs, through AMD's signed FidelityFX SDK 2.3 DLLs, ported from the Gears of War: Judgment recompilation. Off by default: set `gow2_temporal = true` and `gow2_upscaler = "fsr4"` or `"fsr3"`, and lower `resolution_scale` so the upscaler has room to work. FSR 4 was checked in game on a Radeon RX 7800 XT (1280x704 upscaled to 2560x1408).
- No DLSS in the release: NVIDIA's DLSS SDK license does not allow it in an open-source release (see THIRD_PARTY_NOTICES.md). Only builds made from source with your own copy of NVIDIA's DLSS SDK can use it.
- Texture packs: see the next section. Tested with a texture replaced by a version twice its size:

  ![A title-screen texture replaced by a recoloured 2048x2048 version](docs/screenshots/texture-pack-test.jpg)

## Texture packs

Replace game textures with higher-resolution versions (for example upscaled 4x) for your own use. Each player builds packs from their own copy. An upscaled or recoloured texture is still derived from the game's copyrighted art, so do not share packs made from the game's textures; this project does not host or link them.

1. Set `texture_dump = true` in `gow2.toml` and play. Dumps land in `textures\4D53082D\dump` next to `gow2.exe`, one `.dds` file per unique texture, named `<hash>_<format>_<width>x<height>.dds`. Videos and other textures that change every frame are skipped.
2. Convert the dumps to PNG with `tools\texpack.py` (included in the release; needs Python 3 and Pillow, `pip install pillow`): `python tools\texpack.py to-png textures\4D53082D\dump work` converts the whole folder.
3. Upscale the PNG with any tool (Upflow on AMD GPUs, chaiNNer, Upscayl).
4. Convert back with a full mip chain: `python tools\texpack.py from-png work textures\4D53082D\replace`. File names are kept; the game matches each file by the hash at the start of its name. This writes uncompressed DDS; for files about 4x smaller compress with Microsoft texconv instead: `texconv -f BC7_UNORM -m 0 file.dds`.
5. Set `texture_replace = true` in `gow2.toml` and play. Before upscaling everything, check that replacement works with a solid color: `python tools/texpack.py tint <dump.dds> <replace.dds>` and look for the tinted texture in game.

A replacement is picked up when it has at least as many mip levels as the game texture (a full chain down to 1x1, which `from-png` writes), keeps the same aspect ratio, and is at least as large as the original.

## System Link

Campaign co-op over System Link works between PCs and between several copies on one PC; an Android build joined a PC for two-player co-op. It is off by default: set `net_link_active = true` in `gow2.toml` (or pass `--net_link_active=true`) on every player's copy, and use the same build everywhere; `gow2_coop_players` 3 or more turns it on by itself. The game's UDP port 1000 becomes 41000 on every platform (`net_port_offset`, because Android cannot bind ports below 1024) and the System Link beacon uses UDP 14001, so a PC that hosts needs a Windows Firewall rule for those two ports. `system_link_trace = true` logs every network and session call the game makes (`[netlink]` lines).

- [docs/NETWORK_PLAY.md](docs/NETWORK_PLAY.md): a PC and a phone on one network, the settings, several copies on one PC, troubleshooting.
- [docs/COOP_N_PLAYERS.md](docs/COOP_N_PLAYERS.md): Stage 2 co-op with more than two players covers chapter changes, deaths, leave/rejoin, checkpoints and cinematics; what is still missing includes co-op splits, vehicle exit, the Ride-Reaver section and four PCs over the internet.
- [docs/ONLINE.md](docs/ONLINE.md): playing over the internet. From the repository (not in the release zip), [`scripts/host-online.ps1`](https://github.com/santiquiroz/gears-of-war-2-pc-port/blob/master/scripts/host-online.ps1) `-Start` opens UDP 14001 and 41000 through UPnP on every router between the PC and the internet, with a lease (6 h by default, 24 h at most), and schedules an automatic `-Stop` for the end of the lease; `-Stop` closes them. Run `-Stop` after every session: some routers ignore the lease. Without UPnP: forward by hand; with no open ports: Tailscale or ZeroTier. Verified on 2026-10-15: a phone on mobile data joined a PC host behind two routers in a row.

Horde and Versus over System Link have not been tried. See [ROADMAP.md](ROADMAP.md).

## Known issues

- **Mouse aim changed in 0.1.1.** In 0.1.0 the mouse was turned into a right-stick movement, and the game ignored any stick deflection under 30% (its `DeadZone=0.3` binding). Now the mouse goes through the game's own PC mouse axes (`gow2_mouse_aim`), with no dead zone, while controllers keep the game's own aiming. Keys still pass through an emulated controller. Reports of how it feels are welcome in an issue.
- **Hints show Xbox button pictures.** Menu prompts show keyboard keys ([Space], [E]...) while no controller is connected and switch to the Xbox buttons when you plug one in, and back when you unplug it (`gow2_prompts` = `auto`; `keyboard` and `controller` fix one choice at launch; on Android the choice is still made at launch). A menu already on screen changes the next time it opens; the tutorial hint pictures have their Xbox button circles removed while no controller is connected (the hint text names your key), and any other art with buttons painted in still shows Xbox buttons.
- **Green-looking lighting** has been reported on skin, lights and the menu flames; it has not been reproduced on the development PC yet. Screenshots are welcome in an issue.
- **Co-op with three or four players is new.** Stage 2 has been tested with four copies on one PC: players 3 and 4 keep their bodies and weapons through chapter changes and travel, respawn, leave and rejoin, return after checkpoint loads, hide during cinematics, stay unharmed on foot while a hero rides, and are moved next to Marcus or Dom by the host when left far behind. Co-op splits, getting out of a vehicle, the Ride-Reaver section, joining during a cinematic, long real combat, four PCs over a LAN or the internet, and Android with 3+ players have not been tried ([docs/COOP_N_PLAYERS.md](docs/COOP_N_PLAYERS.md)).
- **Every player needs the same release.** Everyone must run v0.1.2; with three or more players, campaigns with different co-op hashes stay hidden, while two-player co-op has no check and v0.1.1 is expected to crash at the end of Act 1 chapter 2.
- Only Act 1 has been played through; the co-op tests reached chapters 13, 16 and 27 by chapter travel.

## Building from source

See [BUILDING.md](BUILDING.md). The main scripts are `scripts/build.ps1`, `scripts/build-sdk.ps1` (optional — adds FidelityFX/FSR, temporal upscalers and system link tracing) and `scripts/package.ps1`.

## Finding the functions codegen misses

`tools/harvest_hints.py` scans the decrypted image (exported from Ghidra with `tools/ghidra/ExportImage.java`) against a baseline codegen without hints (`scripts/codegen-baseline.sh`) and emits `hints/harvested.toml`: adjustor thunks, `.pdata` entries, code pointers stored in `.rdata`/`.data` and addresses built in code with `lis`+`addi`/`ori`. Hints that split an existing function are detected from the codegen log by `tools/hint_fixpoint.py` and kept out through `hints/denylist.txt`. Anything still missing is found at runtime by `scripts/run-harvest-loop.sh` (boot, read the "unregistered function" FATAL, add it to `hints/runtime.toml`, rebuild). `scripts/chapter-sweep.ps1` does the same for all 29 chapters in one pass by travelling to each (see [docs/TEST_TOOLING.md](docs/TEST_TOOLING.md)).

## Contributing

Contributions are welcome through pull requests on GitHub, one pull request per feature or fix. See [CONTRIBUTING.md](CONTRIBUTING.md) for the rules. Bug reports go through issues with the newest `gow2_*.log` attached (on Android: Launch options > Share last log).

## Credits

- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) (BSD-3-Clause, derived from Xenia) — runtime and code generator.
- [OverkillLabs3/gears-judgement-recomp](https://github.com/OverkillLabs3/gears-judgement-recomp) (Gears of War: Judgment on ReXGlue, BSD-3-Clause) — this repo copies `tools/patch_switch_traps.py` and `tools/check_objects_fresh.py` from it and adapts `scripts/build.ps1` from its `scripts/build.ps1`. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
- [XenonRecomp](https://github.com/hedge-dev/xenonrecomp) (hedge-dev).
- [Xenia](https://github.com/xenia-project/xenia).
- AMD FidelityFX SDK: FSR 1 and CAS (MIT). The FSR 4 and FSR 3.1 DLLs in the release are AMD's signed binaries, redistributed under AMD's license, not MIT (see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)).
- NVIDIA DLSS SDK (builds from source only; not in the release).
- [PresentMon](https://github.com/GameTechDev/PresentMon) (used for measurements).

## Legal

Unofficial project. Not affiliated with or endorsed by Microsoft, Xbox Game Studios, The Coalition, or Epic Games. All trademarks belong to their owners. This repository's own code is licensed under the BSD-3-Clause License — see [LICENSE](LICENSE).

The screenshots (`docs/screenshots`, the `docs/assets/img` files other than `icon-320.webp`, `docs/assets/og-image*.jpg` and `assets/icon/og-image*.png`) show Gears of War 2 content, © Microsoft Corporation (originally © 2008 Epic Games, Inc.), captured from the game running in this port. They are not covered by the BSD-3-Clause license. The release zip also carries AMD and Microsoft runtime DLLs under their own licenses ([THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)).
