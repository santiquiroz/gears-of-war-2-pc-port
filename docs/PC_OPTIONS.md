# PC Options: the in-game settings menu

PC Options is the port's own settings menu. It lives inside the game. It
changes the same settings as `gow2.toml` and the command line.

On Android, the menu and its entry on the game's Options screen are called
"Device Options" ("Opciones del dispositivo" in Spanish). Windows and Linux
use "PC Options" ("Opciones de PC" in Spanish). Everything else is the same menu.

Verified in game on a PC on 2026-10-16, including Back + Start, which opened
the menu, and the B that closed it, which did not reach the game. With the
menu's 0.2.0 build a phone found a PC host's System Link campaign and joined
its lobby (both logs: `XGISessionJoinRemote ... members=2`). Verified on a Galaxy S25 Ultra
on 2026-10-16: the gear and the game's Options > Device Options open
the menu (log `PCOPTIONS open (gear)` / `(game-menu)`); tabs and
arrows work by touch; Save wrote `gow2_touch_opacity` into
files/gow2.toml keeping its comment; the menu follows the phone's
language (persist.sys.locale).

## Opening it

- The game's Options screen (main menu, RB) lists "PC Options" on Windows and Linux
  or "Device Options" on Android where
  "Storage Location" was. Selecting it opens the menu.
- F10 opens and closes it from anywhere.
- Back + Start, held together on a controller, opens and closes it.
- On Android, the gear in the top-right corner opens and closes it.

Turn the Options entry off with `gow2_pc_options_entry = false`. It takes
effect on the next start.

On Windows the menu also opens before the game loads, on the Display tab,
with **Play** in place of Close (Esc, Back or B also play). Play saves the
changed rows; when one of them needs a restart, the game restarts once with
them and goes straight in. Rows that need the running game are hidden there:
All runtime settings, Co-op check code, and Multiplayer > Host over the
internet with its whole page (status, open, close and the host settings).
Key bindings, Controller buttons and the save folder rows still work: they
only change settings the game reads when it starts. Turn it off in System > Show
options at start (`gow2_launcher = false`); a restart from the menu never
stops at it.

## Controls

Controller:

- D-pad or left stick: move between rows and change values.
- A: change. B or Esc: back. X: default. Y: save.
- LB / RB: switch tabs.

Keyboard:

- Arrows or W/A/S/D: move between rows and change values (Ctrl+S still saves).
- Enter or Space: change. Esc or Backspace: back. Del: default.
- Q / E or PgUp / PgDn: switch tabs. Ctrl+S: save. Ctrl+Q: quit the game.

Mouse and touch:

- Click or tap tabs, rows, and the arrows beside a value.
- Three buttons at the bottom: Save, Default, Back (Close when clean).
  F10 or Back + Start opens the menu.

Quit game sits at the top right of every page with every device: click or tap
it, press Ctrl+Q, or hold View (Back) on a controller for 0.7 s (a bar fills
along the button). It asks first, with Cancel selected; pressing Ctrl+Q or
holding View again confirms. Unsaved changes are saved before the game closes.
System > Quit the game opens the same question.

Text rows open an editor. Controllers get an on-screen keyboard there.

## Saving

Only the rows you changed are written into your `gow2.toml`:
`%LOCALAPPDATA%\GearsOfWar2PC\gow2.toml` on Windows, `~/.config/gow2/gow2.toml`
on Linux (a `portable.txt` file next to the executable keeps it there instead).
On Android the file is
`Android/data/com.santiquiroz.gow2recomp/files/gow2.toml`. Every comment in
the file is kept. Windows paths are written as literal strings.

A build from source and the release zip ship `config/gow2.toml` as
`gow2.default.toml` next to the executable. The game loads it on every start
before your `gow2.toml`, so your choices win and new defaults from an update
reach you. An old `gow2.toml` next to the executable is moved to your folder on
the first start, keeping only the lines that differ from the defaults.

System > Download opens the latest release page in your browser. In fullscreen,
the game minimizes so you can see the browser. "Update and restart" downloads
and installs the update if needed, then restarts the game; automatic installs
apply a downloaded update on the next start.

Leaving with unsaved changes asks Save / Discard / Cancel. Discard restores
the values the menu opened with.

## Applies now, next level, on restart

Changes apply at once where the setting allows it. Rows that need a restart
say so under the label. A banner lists the changed rows that still need a
restart, with a "Restart now" button on Windows and Linux.

The menu language follows the system (English or Spanish), or
`gow2_menu_language`. Advanced rows appear with "Show advanced options" on.
Defaults keep the original game and crossplay working.

## Display

Platforms: `PC` = Windows and Linux, `Windows` = Windows only, `Android`, `all` = every platform.

| Row | Key(s) | Values | Applies | Platforms |
|---|---|---|---|---|
| Window mode | `fullscreen` + `fullscreen_exclusive` | windowed, borderless, exclusive | now | PC |
| Display | `monitor` | detected monitors | now | PC |
| Window size | `window_width` + `window_height` | game resolution, 1280x720 ... 3840x2160 | now | PC |
| Graphics card | `gow2_gpu` | automatic, detected GPUs | restart | Windows |
| VSync | `vsync` | on, off | now | all |
| Unlock frame rate | `gow2_unlock_fps` | on, off | now | all |
| Frame rate limit | `gow2_fps_cap` | off, VRR, 30 ... 240 | now | all |
| Frame counter | `gow2_show_fps` | off, on (game fps and frame time; top-left, top-center on Android) | now | all |
| Keep 16:9 shape | `present_letterbox` | on (black bars), off (fill: crop up to 10 % top and bottom, stretch the rest) | now | Android |
| Display refresh hint | `present_android_frame_rate` | phone default, 30 ... 120 | restart | Android |

## Graphics

| Row | Key(s) | Values | Applies | Platforms |
|---|---|---|---|---|
| Graphics API | `gpu_backend` | automatic (Direct3D 12), Direct3D 12, Vulkan (experimental); Direct3D 12 is the tested path, Vulkan is experimental with only FSR 1 and CAS | restart | Windows |
| Internal resolution | `resolution_scale` | 720p, 1440p, 4K | restart | all |
| Upscaler | `gow2_temporal` + `gow2_upscaler` | off, FSR 3.1, FSR 4 (DLSS on DLSS builds) | now | Windows |
| Upscaling | info | internal -> screen ratio | now | Windows |
| Final image filter | `present_effect` | bilinear, FSR 1, CAS (SGSR on Android; Vulkan list on Linux, as on Android) | restart | all |
| FSR 1 softening | `present_fsr_sharpness_reduction` | 0 ... 2 | now | all |
| CAS sharpness | `present_cas_additional_sharpness` | 0 ... 1 | now | all |
| Dithering | `present_dither` | on, off | now | all |
| Anisotropic filtering | `anisotropic_override` | game default, off, 1x ... 16x | next level | all |
| Texture cache | `texture_cache_memory_limit_soft` + `texture_cache_memory_limit_hard` | 384 MB, 1 GB, 2 GB, 4 GB | now | all |
| Character lighting fix | `gow2_black_shading_fix` | on, off | restart | all |
| Bigger texture pool | `gow2_texture_streaming_fix` | on, off | restart | all |
| 16x filtering patch | `gow2_aniso16` | on, off | restart | all |
| Ambient occlusion fix | `gow2_ao_upscaling_fix` | on, off | now | all |
| Depth of field fix | `gow2_dof_upscaling_fix` | on, off | now | all |
| Texture pack | `gow2_texture_pack` | off, textures folder, the folder names in `texture_packs` next to the exe (each holds `4D53082D\replace`); empty uses `texture_replace`; it overrides `texture_replace` and `texture_pack_dir` in gow2.toml, not on the command line | restart | Windows |
| Open texture packs folder | action | creates `texture_packs` next to the exe and opens it | now | Windows |
| Build texture pack | opens the page below; dimmed when `tools\texconv` or `tools\realesrgan` is missing | upscales the dumped color textures with Real-ESRGAN into `texture_packs\dump-2x` and the like ([Texture packs](TEXTURE_PACKS.md)) | now | Windows |
| Dump textures | `texture_dump` | on, off (advanced); dump with Texture pack set to Off so Build texture pack finds them | restart | Windows |
| Sustained performance | `gow2_android_sustained_performance` | on, off | restart | Android |
| Engine settings | `gow2_system_settings` | text (advanced) | restart | all |

The "Build texture pack" page holds: Scale (`gow2_texture_build_scale`: 2x, 4x), Quality
(`gow2_texture_build_quality`: same as the disc, BC7), the dump count with the expected video memory
(2x about 4 times the textures', 4x about 16 times, double with BC7), Start, Cancel and the status
(`Building 120 / 724`, then the pack to pick or the failure). The build keeps running when you press
Play or close the menu; quitting the game cancels it. A finished pack shows up in Texture pack right
away.


### Game quality

Graphics includes **Game quality** (**Calidad del juego**) on PC and Android.
The **Quality preset** row offers Low / Medium / High (retail) / Custom. Low
reduces optional effects and shadow/detail settings; Medium is intermediate;
High sets all 24 controls to retail values. Individual changes display Custom
when they no longer match a preset. Each row needs a restart. These new rows
have host-test coverage; the earlier in-game checks at the top of this page do
not validate the new quality controls or their visual/performance effects.

| Row | Key | Values | Applies | Platforms |
|---|---|---|---|---|
| Quality preset | menu action over the 24 keys below | Low, Medium, High (retail), Custom | restart | all |
| Render scale | `gow2_render_scale` | 100 % (native), 85 %, 75 %, 50 % (75 % on phones) | restart | all |
| Object shadows | `gow2_gfx_light_environment_shadows` | auto, on, off | restart | all |
| Dynamic shadows | `gow2_gfx_dynamic_shadows` | auto, on, off | restart | all |
| Dynamic lights | `gow2_gfx_dynamic_lights` | auto, on, off | restart | all |
| Composite dynamic lights | `gow2_gfx_composite_dynamic_lights` | auto, on, off | restart | all |
| Bloom | `gow2_gfx_bloom` | auto, on, off | restart | all |
| High quality bloom | `gow2_gfx_use_high_quality_bloom` | auto, on, off | restart | all |
| Motion blur | `gow2_gfx_motion_blur` | auto, on, off | restart | all |
| Depth of field | `gow2_gfx_depth_of_field` | auto, on, off | restart | all |
| Ambient occlusion | `gow2_gfx_ambient_occlusion` | auto, on, off | restart | all |
| Lens flares | `gow2_gfx_lens_flares` | auto, on, off | restart | all |
| Distortion | `gow2_gfx_distortion` | auto, on, off | restart | all |
| Static decals | `gow2_gfx_static_decals` | auto, on, off | restart | all |
| Dynamic decals | `gow2_gfx_dynamic_decals` | auto, on, off | restart | all |
| Fog volumes | `gow2_gfx_fog_volumes` | auto, on, off | restart | all |
| Tree leaves | `gow2_gfx_speed_tree_leaves` | auto, on, off | restart | all |
| Tree fronds | `gow2_gfx_speed_tree_fronds` | auto, on, off | restart | all |
| Shadow filter bias | `gow2_gfx_shadow_filter_quality_bias` | auto, -1, 0, 1 | restart | all |
| World detail | `gow2_gfx_detail_mode` | auto, 0, 1, 2 | restart | all |
| Character LOD bias | `gow2_gfx_skeletal_mesh_lod_bias` | auto, 0, 1, 2 | restart | all |
| Particle LOD bias | `gow2_gfx_particle_lod_bias` | auto, 0, 1, 2 | restart | all |
| Foliage draw radius | `gow2_gfx_foliage_draw_radius_multiplier` | auto, 0.25, 0.5, 1.000000 | restart | all |
| Maximum shadow resolution | `gow2_gfx_max_shadow_resolution` | auto, 256, 512, 800 | restart | all |
| Shadow texels per pixel | `gow2_gfx_shadow_texels_per_pixel` | auto, 0.5, 1, 1.27324 | restart | all |
| Decal draw distance | `gow2_gfx_decal_cull_distance_scale` | auto, 0.4, 0.7, 1.0 | restart | all |

`auto` keeps platform defaults: PC retains retail quality; Android disables
Object shadows only. High explicitly enables Object shadows on Android. The
advanced Engine settings row (`gow2_system_settings`, empty by default) applies
last, so existing raw `Key=Value;OtherKey=Value` pairs override quality rows and
presets. Clear conflicting raw pairs to give control to the menu. Quality cvars
are strings in TOML, including numbers (e.g. `gow2_gfx_detail_mode = "1"`). Render scale stands outside the preset and the Custom detection, and raw `ScreenPercentage=...` still wins over it.

See [SYSTEM_SETTINGS.md](SYSTEM_SETTINGS.md) for the complete 76-key retail
inventory, every preset value, scope of verification, and the bundle size rule.

## Controls

| Row | Key(s) | Values | Applies | Platforms |
|---|---|---|---|---|
| Keyboard and mouse | `mnk_mode` | on, off | now | all |
| Native mouse aim | `gow2_mouse_aim` | on, off | restart | all |
| Mouse sensitivity | `gow2_mouse_scale` or `mnk_sensitivity` | 0.05x ... 10x | now | all |
| Invert mouse Y | `gow2_mouse_invert_y` | on, off | now | all |
| Aim assist with the mouse | `gow2_mouse_aim_assist` | on, off | now | all |
| Button prompts | `gow2_prompts` | automatic (last device used), keyboard, controller (Xbox buttons), playstation (Windows) | restart | all |
| Keycap pictures | `gow2_keycaps` | on, off | restart | Windows |
| Controller look dead zone | `pad_right_stick_deadzone` | 0 ... 0.9 | now | all |
| Key bindings | opens the page below | | now | all |
| Controller layout | info | in the game's own Options | now | all |

The Key bindings page holds one row per action (`keybind_*`, 24 rows).
Accept, then press the new key. Default restores the original keys. Open it
with F10 during a level: a new key works as soon as the menu closes. The game
itself keeps the bindings it was started with, so the port sends each key as
the key the game still binds to the same action (`RouteKey`,
`src/gow2_native_bindings.cpp`). One limit: an action that was bound to a
key combination (`Shift+Up`) at start cannot move to another key until the
next start. Menu prompts update after a restart.

## Touch (Android)

| Row | Key(s) | Values | Applies | Platforms |
|---|---|---|---|---|
| Touch controls | `gow2_touch` | automatic, on, off | restart | Android |
| Opacity | `gow2_touch_opacity` | 0% ... 100% | now | Android |
| Size | `gow2_touch_scale` | 0.5x ... 2x | now | Android |
| Look sensitivity | `gow2_touch_look_gain` | 0.1x ... 5x | now | Android |
| Aim: tap to toggle | `gow2_touch_aim_toggle` | on, off | now | Android |
| Hide after keyboard or mouse | `gow2_touch_hide_seconds` | 0 ... 600 s | now | Android |

## Multiplayer

| Row | Key(s) | Values | Applies | Platforms |
|---|---|---|---|---|
| Your name | `user_name` | text, up to 15 characters | restart | all |
| System Link (LAN and internet) | `net_link_active` | on, off | restart | all |
| Co-op players | `gow2_coop_players` | 2, 3, 4, 8, 10 (8 and 10 experimental) | restart | all |
| Friendly fire (co-op) | `gow2_friendly_fire` | on, off | now | all |
| Go to player 1 | action | | now | all |
| Key: go to player 1 | `bind_coop_go_to_player1` | a key, F6 by default | now | all |
| Controller: go to player 1 | `gow2_coop_go_to_player1_pad` | off, Back + D-pad up, Back + D-pad down, both stick clicks (LS + RS) | now | all |
| Players 3+ dying ends the game | `gow2_coop_extra_players_game_over` | on, off | restart | all |
| Co-op check code | info | the combined hash | now | all |
| Join a host (name or IP) | `net_peers` | text | restart | all |
| Host over the internet | opens the page below | | now | Windows |
| This device's LAN address | `net_local_ip` | text (advanced) | restart | all |
| Port offset | `net_port_offset` | 1024 ... 64512 | restart | all |
| Co-op check | `net_beacon_gate` | on, off (advanced) | restart | all |
| Network log | `system_link_trace` | on, off (advanced) | now | all |

The "Host over the internet" page holds: Status (info), Keep open for
(`gow2_host_lease_hours`: 2, 6, 12, 24 h), Open ports now (action), Close
ports (action), Address friends use (`net_public_address`: text), Friends
allowed (`net_allow_peers`: text), Close ports when the game exits
(`gow2_host_close_on_exit`: toggle). Address friends use and Friends allowed
are available on PC; the port controls are Windows-only. See below.

## Saves

| Row | Key(s) | Values | Applies | Platforms |
|---|---|---|---|---|
| Save location | `gow2_save_dir` | default, portable, detected cloud folders | restart | all |
| Choose a folder | picks `gow2_save_dir` | folder picker | now | Windows |
| Folder path | `gow2_save_dir` | text (advanced) | restart | all |
| Saves are in | info | the folder this session uses | now | all |
| Open the saves folder | action | opens the folder | now | PC |
| All chapters and Insane unlocked | `gow2_unlock_all_chapters` | on, off | restart | all |

## Audio

| Row | Key(s) | Values | Applies | Platforms |
|---|---|---|---|---|
| Volume | `gow2_audio_volume` | 0% ... 100% | now | all |
| Mute | `audio_mute` | on, off | now | all |
| Audio buffer | `audio_maxqframes` | 4 ... 64 (advanced) | restart | all |

## System

| Row | Key(s) | Values | Applies | Platforms |
|---|---|---|---|---|
| Menu language | `gow2_menu_language` | automatic, English, Spanish | now | all |
| Subtitles and text | `user_language` | English, Latin American Spanish, French, German, Italian, Polish, Russian, Korean, Traditional Chinese | restart | all |
| Voices | `gow2_voice_language` | same as subtitles, English, Latin American Spanish, French | restart | all |
| Skip intro videos | `gow2_skip_intro_movies` | on, off | restart | all |
| Show how to skip cinematics | `gow2_skip_hint` | on, off | now | PC |
| Show options at start | `gow2_launcher` | on, off | at start | Windows |
| Show advanced options | `gow2_menu_advanced` | on, off | now | all |
| All runtime settings | action | opens the F4 list | now | all |
| F12 screenshots | `present_screenshot_key_enabled` | on, off (Direct3D 12 only) | now | Windows |
| Update status | info | latest release, or the download/install progress | now | PC |
| Check for updates | `gow2_update_check` | on, off | now | PC |
| Download | action | opens the release page | now | PC |
| Update and restart | action | downloads, checks, installs and restarts | now | PC |
| Install updates automatically | `gow2_update_auto_install` | on, off | now | PC |
| Check now | action | asks GitHub again | now | PC |
| Dismiss | action | hides this release's notice | now | PC |
| Restart the game | action | save first: it saves nothing | now | PC |
| Quit the game | action | quit, then open the game again | now | all |
| Settings file | info | where the menu saves | now | all |
| Version | info | this build of the port | now | all |
| Open this menu | info | F10, Back + Start | now | all |

## New settings this release

0.4.11: `gow2_update_auto_install` and the "Update and restart" action. Linux
shows the PC rows instead of the Android ones.

0.4.2: `gow2_friendly_fire` and the "Go to player 1" action. FSR 1 softening,
CAS sharpness and Dithering now apply at once (SDK patch 0077). Runtime list
only: `gow2_coop_requests` (on; off keeps both features out) and, on Linux,
`load_renderdoc` (off; see [LINUX.md](LINUX.md)).

## Host over the internet

On Windows this page runs the bundled `scripts\host-online.ps1` hidden with
`-Json`. It opens UDP 14001 and 41000 by UPnP on every router on the way,
with a lease. It fills `net_public_address` when empty. It closes the ports
when the game exits if "Close ports when the game exits" is on. The
scheduled `-Stop` still covers a crash. See [ONLINE.md](ONLINE.md).

## Co-op check code

With 3 or 4 players every player must see the same code: the same build and
the same co-op settings. With 2 players it is not used. See
[NETWORK_PLAY.md](NETWORK_PLAY.md).

## Friendly fire and Go to player 1

Friendly fire works in campaign co-op only, and only while every human
player has it on: the host counts its own setting and the vote each guest's
game sends every few seconds, and a guest who stops voting for 15 s counts
as off. Split-screen players on one PC share that PC's setting. Turning it on changes nothing for AI squadmates, and melee never
targets a teammate. While it is on, a teammate's grenade or explosion can
also knock you down, as in the original game's friendly-fire modes.

"Go to player 1" moves you next to player 1 (Marcus, or the host when
nobody plays Marcus) when you are stuck. The host refuses it while either
of you has no live character, rides a vehicle or turret, or watches a
cutscene, and allows it once every 10 s. Both features need 0.4.2 or later
on the host; a guest on an older build sends no vote, so friendly fire stays
off. How it works:
[COOP_N_PLAYERS.md](COOP_N_PLAYERS.md#requests-and-friendly-fire).

## All runtime settings

The raw runtime list stays on F4 ("All runtime settings"). It is also a row
in PC Options > System.

## How it works inside

SDK patch 0013 adds the hooks. `xam_input_block` gives the game a neutral
pad and no keystrokes while a host menu is open. The Coalesced layer renames
the game's Options > Storage Location button to PC Options. The device
selector call with the UI flag (0x200) runs the command cvar
`gow2_pc_options_modal`, which opens this menu and waits; the boot-time call
(flags 0) stays headless. The game is told through the system-UI
notification (XN_SYS_UI) while the menu is open.

## Known limits

- Back + Start also reaches the game during the 300 ms before the menu
  opens, so the game may open its pause menu underneath.
- In two of the twelve languages on the phone, the renamed Options
  entry did not fit its bundle and keeps the game's own text; the
  button still opens the menu.
- Controller remapping is the game's own Options > Controller layouts.
