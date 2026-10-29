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
- [x] FSR 4 and FSR 3.1 temporal upscaling (PC Options > Graphics > Upscaler; `gow2_temporal`, `gow2_upscaler`; off by default). The release has no DLSS; DLSS shows up in the menu with SDK DLLs you build yourself (0.2.8)
- [x] Keyboard keys and mouse buttons handled as on a PC while a level is played (`gow2_native_keys`, SDK patch 0022); menus, the pause menu and cutscenes still pass through the emulated controller
- [x] Button prompts that follow the device you used last, live: keycap pictures of your keybinds for keyboard and mouse, the Xbox buttons, or PlayStation buttons (PS4: Share/Options, PS5: Create/Options) for a DualShock or DualSense (idea and SDK patch from @Flippage's Gears of War 3 recompilation, issue #25) (`gow2_keycaps`; build the pack from your own copy with `Build keycap pack.cmd` in the release zip, no Python needed since 0.2.9, or `scripts/build-keycaps.ps1` from source, see [tools/keycaps](tools/keycaps/README.md))
- [x] Ultrawide: a really wider picture (Hor+) instead of 16:9 with bars, up to 1680 pixels wide (`gow2_widescreen`, see [docs/WIDESCREEN.md](docs/WIDESCREEN.md)); works with the native renderer (AMD and NVIDIA by default) and with the ROV render target path (Intel by default)
- [x] Native renderer (0.3.0; draws taken from the game's Direct3D calls since 0.3.4): render targets are host surfaces and resolves write textures directly instead of emulating the Xbox 360 EDRAM. On by default on AMD, NVIDIA (0.3.8) and Android; Graphics > Renderer switches it (`gow2_native_renderer`)
- [x] Anti-aliasing: FXAA and FXAA Extreme (Graphics > Anti-aliasing, 0.3.4), on PC and Android
- [x] Graphics API on PC: Direct3D 12 (default) or Vulkan, experimental (PC Options > Graphics > Graphics API, `gpu_backend`, 0.3.8). On Vulkan the temporal upscalers are not available; FSR 1 and CAS are
- [x] Install from your own `.iso` on PC (0.3.6) and on Android (0.2.6)
- [x] Voices and subtitles in separate languages (0.3.7): English, Latin American Spanish or French voices; text in 9 languages ([docs/LANGUAGES.md](docs/LANGUAGES.md))
- [ ] Full campaign play-through
- [x] System Link campaign co-op for up to four players (`gow2_coop_players`, off by default): four copies on one PC (loopback), and a PC with an Android phone on one LAN for two players, verified 2026-10-14 and 2026-10-15; Stage 2 (chapter changes, deaths, leave/rejoin, checkpoints, cinematics) verified with four copies on one PC on 2026-10-16; a phone and a PC on 0.2.0 joined one System Link lobby on 2026-10-16; 8 and 10 players are experimental (0.3.0, eight copies joined on one PC)
- [x] Internet play, experimental (`net_peers`, `net_public_address`, `net_allow_peers`): a phone on mobile data joined a PC-hosted campaign through ports opened by UPnP, verified 2026-10-15; a PC friend outside the network has not been tried yet
- [x] In-game options menu (PC Options; Device Options on Android) ([docs/PC_OPTIONS.md](docs/PC_OPTIONS.md)): verified on a PC and on a Galaxy S25 Ultra on 2026-10-16
- [x] Game quality options: Graphics > Game quality with Low/Medium/High/Custom presets and 24 of the game's own engine settings (shadows, lights, bloom, motion blur, depth of field, ambient occlusion, flares, distortion, decals, fog, foliage, level of detail); all 76 keys in [docs/SYSTEM_SETTINGS.md](docs/SYSTEM_SETTINGS.md)
- [x] Update checker on PC and Android (GitHub Releases, at most twice a day, `gow2_update_check`)
- [x] Automatic settings for integrated GPUs (720p internal, 2x anisotropic filtering, no per-frame guest memory re-upload) for handhelds and laptops
- [x] Android: the white objects/effects of the Vulkan renderer are fixed (0.2.3); object shadows are an option, off by default on Android
- [x] VSync waits for the display on D3D12 and the 30 fps cap holds with VSync off (0.2.4)
- [x] Split-screen campaign co-op on one PC (PC Options > Multiplayer > Split-screen players, then Co-op Campaign > Split-screen Campaign; two players, the game's own limit; the second player needs a controller, or the keyboard as player 1), verified 2026-10-22 with two players
- [x] System Link Versus: two copies on one PC joined a party and played Warzone on Avalanche on 2026-10-22
- [x] System Link Horde: two copies on one PC played wave 1 on Avalanche on 2026-10-22 (SDK patches 0049 and 0050)
- [ ] Android: a build from the repository reaches gameplay and joins a PC's System Link campaign, but is not finished yet; an experimental APK has been attached to every release since v0.1.2 (see [Android](#android-experimental)). The native renderer is on by default since 0.3.1. On a Galaxy S25 Ultra (720p internal, frame rate unlocked) the hospital runs at about 46-53 fps from a cool start and about 37 fps once the phone is warm (38 °C battery); the GPU is the limit, and it slows down as the phone heats up. Mali GPUs without the Vulkan feature `vertexPipelineStoresAndAtomics` (for example the Dimensity 8300's Mali-G720) start from 0.3.8 (issue #27) ([docs/ANDROID.md](docs/ANDROID.md))

See [ROADMAP.md](ROADMAP.md) for the full plan.

## Download and play

Nothing of the game is included in the release. You need your own retail Xbox 360 copy of Gears of War 2 (title ID `4D53082D`): a disc image (`.iso`) of it, or a folder that has `default.xex` and the `GearGame` folder, which you can still make by extracting your ISO with [xdvdfs](https://github.com/antangelo/xdvdfs):

```text
xdvdfs unpack "Gears of War 2.iso" C:\Games\GoW2
```

The port was recompiled from the US retail disc (its default.xex); other releases such as the European disc are untested and can crash, and the log says at start which one you have (`XEX default.xex ...`).

Do not pirate the game.

### PC

Requirements: Windows 10/11 x64 and a DirectX 12 GPU, or a Vulkan GPU for the experimental Vulkan option (developed on an AMD Radeon RX 7800 XT).

1. Download `gow2-recomp-<version>-win64.zip` from the [latest release](https://github.com/santiquiroz/gears-of-war-2-pc-port/releases/latest) (the `.apk` is the Android build).
2. Unzip it anywhere and run `gow2.exe`. The exe is not signed, so Windows may show "Windows protected your PC": choose **More info**, then **Run anyway**.
3. The first launch asks for your `.iso`, or for `default.xex` in an extracted game folder (one that also has `GearGame`), and remembers the game folder in `gow2_game_path.txt` next to the exe. With an `.iso` it extracts it once into a `game` folder next to `gow2.exe`, which needs about 8 GB free on that drive; you can delete the `.iso` afterwards if you want. To pick another folder later, delete `gow2_game_path.txt`.

It does not ask when the game folder is inside the exe's folder or right next to it, when the exe's folder is inside the game folder, or when you pass the folder yourself (`--game_data_root` also takes the .iso):

```text
gow2.exe --game_data_root="C:\Games\GoW2"
```

The first time you play System Link co-op, Windows asks whether `gow2.exe` may use the network: allow it, or the other players will not find your game.

The first start, and the first start after an update that changes the graphics, compiles the shaders again: the window can sit on the title screen for a few minutes and Windows may call it "Not responding", and the first level takes a little longer to load. Leave it running; later starts use the saved shaders.

### Android (experimental)

Requirements: an arm64 phone with Android 10 or newer and Vulkan, and free space for a full copy of the game files.

1. Download `gow2-recomp-<version>-android-arm64.apk` from the [latest release](https://github.com/santiquiroz/gears-of-war-2-pc-port/releases/latest) and install it (Android asks you to allow installing apps from your browser or file manager).
2. Copy your extracted game folder, or the `.iso` of your disc, to the phone.
3. Open the app and tap **Choose game folder** (and pick that folder) or **Choose .iso file** (the app extracts the image itself; plain, XGD2 and XGD3 images work). The app copies the files into its own storage, which needs as much free space as the game takes, because the game cannot read files through the folder picker. With the folder from step 2 that is two copies; delete yours once the app has finished.

Instead of steps 2-3 you can copy the folder over USB from a PC, so that `default.xex` ends up in `Android/data/com.santiquiroz.gow2recomp/files/game/`, then tap **Check again**.

The settings menu is called Device Options on Android (the gear in the top-right corner). If the game closes unexpectedly, the next launch opens **Launch options** first: a few settings, a **GPU driver** choice (on Adreno, a Turnip .zip made for AdrenoTools in place of the phone's driver), **Safe mode** (no sparse GPU memory, 1x resolution, bilinear filter, the phone's driver), **Reset settings** and **Share last log**. To open it on purpose, long-press the app icon and choose **Launch options**. Status and known limits: [docs/ANDROID.md](docs/ANDROID.md). In Device Options, drag a finger over the list to scroll it (0.3.8).

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

A controller works at the same time. Its buttons can be remapped in PC Options > Controls > Controller buttons (every controller SDL knows, Xbox included; saved as `pad_map_*`). All bindings are in `gow2.toml`.

## Settings

Everything here can be changed in game from Options > PC Options (or F10, or Back+Start); see [docs/PC_OPTIONS.md](docs/PC_OPTIONS.md). `gow2.toml` sits next to the exe. It is read at start-up; command-line flags (`--name=value`) override it. Main keys:

The Graphics tab includes **Game quality**: Low / Medium / High (retail) presets and 24 individual engine quality controls, all applied after a restart. High restores retail quality; Auto keeps platform defaults (retail on PC, which is already the highest choice in every row; Android turns object shadows off). See [the key inventory and presets](docs/SYSTEM_SETTINGS.md).

Voices (English, Latin American Spanish or French; `gow2_voice_language`) and subtitles and text (9 languages; `user_language`) are chosen separately in PC Options > System and apply after a restart.

- `fullscreen` — run fullscreen or windowed.
- `resolution_scale` — internal render resolution as a multiple of 720p: 1 = 720p, 2 = 1440p, 3 = 4K.
- `present_effect` — final image scaling: `bilinear`, `fsr` (AMD FSR 1 with sharpening, from a lower `resolution_scale`) or `cas` (sharpening only).
- `vsync` — vertical sync.
- `gow2_native_renderer` — `auto` (native on AMD, NVIDIA and Android), `on` or `off` (emulated EDRAM).
- `gpu_backend` — `any` (Direct3D 12), `d3d12` or `vulkan` (experimental); needs a restart.
- `gow2_native_frontend` — draw source: `auto` (console commands, faster on PC and on the phones measured so far), `on` or `off`.
- `gow2_skip_intro_movies` — skips the Epic Games and Microsoft Game Studios logo videos at start.
- `present_letterbox` — Android: `false` fills a wider screen (cropped up to 10 % at the top and bottom, stretched sideways for the rest).
- `gow2_unlock_fps` — lifts the game's 30 fps cap.
- `gow2_fps_cap` — frame rate limit while unlocked (0 = no limit).
- `gow2_audio_volume` — master volume in percent.
- `gow2_black_shading_fix` — correct character lighting.
- `gow2_texture_streaming_fix` — bigger texture pool, less pop-in.
- `gow2_aniso16` — 16x anisotropic filtering.
- `gow2_ao_upscaling_fix` — ambient occlusion without artifacts when scaled.
- `gow2_dof_upscaling_fix` — depth of field without artifacts when scaled.
- `gow2_temporal`, `gow2_upscaler` — temporal upscaling: `gow2_temporal = true` with `gow2_upscaler = "fsr4"` (Radeon RX 7000/9000) or `"fsr3"`; off by default (Direct3D 12 only).
- `mnk_sensitivity` — mouse sensitivity.
- `gow2_save_dir` — folder for saves, profile settings and achievements; point it at a OneDrive, Google Drive or Dropbox folder to keep the campaign in the cloud ([docs/SAVES.md](docs/SAVES.md)).

The community patches come from [xenia-canary/game-patches](https://github.com/xenia-canary/game-patches) (authors illusion and boma); the patched sites and values for this XEX are applied to the generated code, each behind its `gow2_*` setting.

On Windows, the game checks GitHub for a new release at most twice a day and sends nothing else. Turn this off in PC Options > System, or set `gow2_update_check = false` in `gow2.toml`.

## Performance

Chapter 2 hospital, frame rate unlocked: about 260 fps at 1440p internal on a Ryzen 9 7900X3D + Radeon RX 7800 XT with the native renderer (0.3.8; 0.3.3-0.3.7 ran at about 150 because the native front end was on by default); 60 fps (the limit) with the GPU at about 55 % on a GTX 1070; about 46-53 fps from a cool start on a Galaxy S25 Ultra at 720p. Where the frame time goes on PC and Android, why the port needs more GPU than "Hollow", and the ranked plan to cut it: [docs/PERFORMANCE.md](docs/PERFORMANCE.md). For bug reports, `gow2_frame_log = true` logs FRAMESTATS frame times every 5 s and `d3d12_stats_log = true` logs D3D12 counters.

## Modern enhancements

- Native resolution via `resolution_scale` (see Settings).
- FSR: the release runtime is built with AMD FidelityFX. `present_effect = "fsr"` renders at a lower `resolution_scale` and upscales with FSR 1 plus sharpening; `"cas"` only sharpens. Measured with `resolution_scale = 1` and FSR 1 to 1440p: about 85 fps in Act 1.
- Temporal upscaling: FSR 4.1 on Radeon RX 7000/9000 and FSR 3.1 on other GPUs, through AMD's signed FidelityFX SDK 2.3 DLLs, ported from the Gears of War: Judgment recompilation. Off by default: choose it in PC Options > Graphics > Upscaler (or set `gow2_temporal = true` and `gow2_upscaler = "fsr4"` or `"fsr3"`), and lower `resolution_scale` so the upscaler has room to work. FSR 4 was checked in game on a Radeon RX 7800 XT (1280x704 upscaled to 2560x1408).
- No DLSS in the release: NVIDIA's DLSS SDK license does not allow it in an open-source release (see THIRD_PARTY_NOTICES.md). Only builds made from source with your own copy of NVIDIA's DLSS SDK can use it. To use it with the release `gow2.exe`, build the SDK DLLs with `scripts/build-sdk.ps1` (without `-NoDlss`) and copy `rexruntime.dll`, `rexgpu-xenos.dll` and `nvngx_dlss.dll` next to it; the PC Options menu then offers DLSS. Confirmed on an RTX 5060 Ti (issue #18).
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

Versus works over System Link: in Multiplayer set Match Type to System Link, create the party on one copy and join it from the System Link Browser on the others (two copies on one PC played Warzone on 2026-10-22). Horde works the same way from the Horde menu (two copies played wave 1 on 2026-10-22). See [ROADMAP.md](ROADMAP.md).

## Known issues

- **"Bad Image ... rexruntime.dll ... 0xc0e90002" on Windows 11.** That is Smart App Control blocking the release's DLLs because they are not code-signed yet; the files are not damaged. Turning Smart App Control off (Windows Security → App & browser control) lets the game start, but Windows cannot turn it back on without a reinstall, so decide with that in mind. Signing the releases is being looked into.

- **Mouse aim changed in 0.1.1.** In 0.1.0 the mouse was turned into a right-stick movement, and the game ignored any stick deflection under 30% (its `DeadZone=0.3` binding). Now the mouse goes through the game's own PC mouse axes (`gow2_mouse_aim`), with no dead zone, while controllers keep the game's own aiming. Keys still pass through an emulated controller. Reports of how it feels are welcome in an issue.
- **Hints show Xbox button pictures.** Menu prompts follow the device you used last, switching live: keyboard keys ([Space], [E]...) after keyboard or mouse input, the Xbox buttons after an Xbox controller, PlayStation buttons (PS4: Share/Options, PS5: Create/Options) after a DualShock or DualSense (`gow2_prompts` = `auto`; `keyboard`, `controller` and `playstation` fix one choice at launch; on Android the choice is still made at launch and there is no `playstation`). PlayStation controllers are now detected through SDL (before, only XInput pads counted); their buttons come from the keycap pack, so rebuild it with `Build keycap pack.cmd` (without a 0.3.5 pack they show Xbox buttons). Idea and SDK patch from @Flippage's Gears of War 3 recompilation (issue #25). A menu already on screen changes the next time it opens; the tutorial hint pictures have their Xbox button circles removed while keyboard prompts show (the hint text names your key), and any other art with buttons painted in still shows Xbox buttons. Gameplay prompts (pick up, reload, take cover) draw the button straight from the game's button font (issue #21): with the keycap pack they show your keys (confirmed in game in #21 with v0.3.4), and with text labels they keep the Xbox buttons ([docs/PROMPTS_GAMEPLAY_FINDINGS.md](docs/PROMPTS_GAMEPLAY_FINDINGS.md)).
- **Green-looking lighting** has been reported on skin, lights and the menu flames; it has not been reproduced on the development PC yet. Screenshots are welcome in an issue.
- **Co-op with three or four players is new.** Stage 2 has been tested with four copies on one PC: players 3 and 4 keep their bodies and weapons through chapter changes and travel, respawn, leave and rejoin, return after checkpoint loads, hide during cinematics, stay unharmed on foot while a hero rides, and are moved next to Marcus or Dom by the host when left far behind. Co-op splits, getting out of a vehicle, the Ride-Reaver section, joining during a cinematic, long real combat, four PCs over a LAN or the internet, and Android with 3+ players have not been tried ([docs/COOP_N_PLAYERS.md](docs/COOP_N_PLAYERS.md)).
- **Every player needs the same release.** Everyone must run the same version; with three or more players, campaigns with different co-op hashes stay hidden, and two-player co-op has no check, so mixed versions can crash.
- Only Act 1 has been played through. On 2026-10-22 `scripts/chapter-sweep.ps1` travelled to each of the 29 chapters and played a minute of scripted movement and fire in each with no missing-function crash; one run hit a GPU timeout (TDR) in chapter 23 that three later runs did not repeat. Playing acts 2-5 start to finish is still open (issue #8).
- **Native renderer on some AMD cards:** on a Radeon RX 7900 XT, blocky dark patches can show on the floor where characters cast shadows (issue #26); Graphics > Renderer > Emulated (EDRAM) avoids them while it is investigated.

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
