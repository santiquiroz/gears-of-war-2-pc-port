# PC Options: the in-game settings menu

PC Options is the port's own settings menu. It lives inside the game. It
changes the same settings as `gow2.toml` and the command line.

On Android, the menu and its entry on the game's Options screen are called
"Device Options" ("Opciones del dispositivo" in Spanish). Windows keeps
"PC Options" ("Opciones de PC" in Spanish). Everything else is the same menu.

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

- The game's Options screen (main menu, RB) lists "PC Options" on Windows
  or "Device Options" on Android where
  "Storage Location" was. Selecting it opens the menu.
- F10 opens and closes it from anywhere.
- Back + Start, held together on a controller, opens and closes it.
- On Android, the gear in the top-right corner opens and closes it.

Turn the Options entry off with `gow2_pc_options_entry = false`. It takes
effect on the next start.

## Controls

Controller:

- D-pad or left stick: move between rows and change values.
- A: change. B or Esc: back. X: default. Y: save.
- LB / RB: switch tabs.

Keyboard:

- Arrows: move between rows and change values.
- Enter or Space: change. Esc or Backspace: back. Del: default.
- Q / E or PgUp / PgDn: switch tabs. Ctrl+S: save.

Mouse and touch:

- Click or tap tabs, rows, and the arrows beside a value.
- Three buttons at the bottom: Save, Default, Back (Close when clean).

Text rows open an editor. Controllers get an on-screen keyboard there.

## Saving

Only the rows you changed are written into `gow2.toml` next to `gow2.exe`.
On Android the file is
`Android/data/com.santiquiroz.gow2recomp/files/gow2.toml`. Every comment in
the file is kept. Windows paths are written as literal strings.

A build from source copies `config/gow2.toml` over the build folder's file.
The release zip is not affected: your saved settings stay.

Leaving with unsaved changes asks Save / Discard / Cancel. Discard restores
the values the menu opened with.

## Applies now, next level, on restart

Changes apply at once where the setting allows it. Rows that need a restart
say so under the label. A banner lists the changed rows that still need a
restart, with a "Restart now" button on Windows.

The menu language follows the system (English or Spanish), or
`gow2_menu_language`. Advanced rows appear with "Show advanced options" on.
Defaults keep the original game and crossplay working.

## Display

| Row | Key(s) | Values | Applies | Platforms |
|---|---|---|---|---|
| Window mode | `fullscreen` + `fullscreen_exclusive` | windowed, borderless, exclusive | now | Windows |
| Display | `monitor` | detected monitors | now | Windows |
| Window size | `window_width` + `window_height` | game resolution, 1280x720 ... 3840x2160 | now | Windows |
| Graphics card | `gow2_gpu` | automatic, detected GPUs | restart | Windows |
| VSync | `vsync` | on, off | now | both |
| Unlock frame rate | `gow2_unlock_fps` | on, off | now | both |
| Frame rate limit | `gow2_fps_cap` | off, 30 ... 240 | now | both |
| Display refresh hint | `present_android_frame_rate` | phone default, 30 ... 120 | restart | Android |

## Graphics

| Row | Key(s) | Values | Applies | Platforms |
|---|---|---|---|---|
| Internal resolution | `resolution_scale` | 720p, 1440p, 4K | restart | both |
| Upscaler | `gow2_temporal` + `gow2_upscaler` | off, FSR 3.1, FSR 4 (DLSS on DLSS builds) | now | Windows |
| Upscaling | info | internal -> screen ratio | now | Windows |
| Final image filter | `present_effect` | bilinear, FSR 1, CAS (SGSR on Android) | restart | both |
| FSR 1 softening | `present_fsr_sharpness_reduction` | 0 ... 2 | restart | both |
| CAS sharpness | `present_cas_additional_sharpness` | 0 ... 1 | restart | both |
| Dithering | `present_dither` | on, off | restart | both |
| Anisotropic filtering | `anisotropic_override` | game default, off, 1x ... 16x | next level | both |
| Texture cache | `texture_cache_memory_limit_soft` + `texture_cache_memory_limit_hard` | 384 MB, 1 GB, 2 GB, 4 GB | now | both |
| Character lighting fix | `gow2_black_shading_fix` | on, off | restart | both |
| Bigger texture pool | `gow2_texture_streaming_fix` | on, off | restart | both |
| 16x filtering patch | `gow2_aniso16` | on, off | restart | both |
| Ambient occlusion fix | `gow2_ao_upscaling_fix` | on, off | now | both |
| Depth of field fix | `gow2_dof_upscaling_fix` | on, off | now | both |
| Texture packs | `texture_replace` | on, off | restart | Windows |
| Dump textures | `texture_dump` | on, off (advanced) | restart | Windows |
| Sustained performance | `gow2_android_sustained_performance` | on, off | restart | Android |
| Engine settings | `gow2_system_settings` | text (advanced) | restart | both |


### Game quality

Graphics includes **Game quality** (**Calidad del juego**) on Windows and Android.
The **Quality preset** row offers Low / Medium / High (retail) / Custom. Low
reduces optional effects and shadow/detail settings; Medium is intermediate;
High sets all 24 controls to retail values. Individual changes display Custom
when they no longer match a preset. Each row needs a restart. These new rows
have host-test coverage; the earlier in-game checks at the top of this page do
not validate the new quality controls or their visual/performance effects.

| Row | Key | Values | Applies | Platforms |
|---|---|---|---|---|
| Quality preset | menu action over the 24 keys below | Low, Medium, High (retail), Custom | restart | both |
| Object shadows | `gow2_gfx_light_environment_shadows` | auto, on, off | restart | both |
| Dynamic shadows | `gow2_gfx_dynamic_shadows` | auto, on, off | restart | both |
| Dynamic lights | `gow2_gfx_dynamic_lights` | auto, on, off | restart | both |
| Composite dynamic lights | `gow2_gfx_composite_dynamic_lights` | auto, on, off | restart | both |
| Bloom | `gow2_gfx_bloom` | auto, on, off | restart | both |
| High quality bloom | `gow2_gfx_use_high_quality_bloom` | auto, on, off | restart | both |
| Motion blur | `gow2_gfx_motion_blur` | auto, on, off | restart | both |
| Depth of field | `gow2_gfx_depth_of_field` | auto, on, off | restart | both |
| Ambient occlusion | `gow2_gfx_ambient_occlusion` | auto, on, off | restart | both |
| Lens flares | `gow2_gfx_lens_flares` | auto, on, off | restart | both |
| Distortion | `gow2_gfx_distortion` | auto, on, off | restart | both |
| Static decals | `gow2_gfx_static_decals` | auto, on, off | restart | both |
| Dynamic decals | `gow2_gfx_dynamic_decals` | auto, on, off | restart | both |
| Fog volumes | `gow2_gfx_fog_volumes` | auto, on, off | restart | both |
| Tree leaves | `gow2_gfx_speed_tree_leaves` | auto, on, off | restart | both |
| Tree fronds | `gow2_gfx_speed_tree_fronds` | auto, on, off | restart | both |
| Shadow filter bias | `gow2_gfx_shadow_filter_quality_bias` | auto, -1, 0, 1 | restart | both |
| World detail | `gow2_gfx_detail_mode` | auto, 0, 1, 2 | restart | both |
| Character LOD bias | `gow2_gfx_skeletal_mesh_lod_bias` | auto, 0, 1, 2 | restart | both |
| Particle LOD bias | `gow2_gfx_particle_lod_bias` | auto, 0, 1, 2 | restart | both |
| Foliage draw radius | `gow2_gfx_foliage_draw_radius_multiplier` | auto, 0.25, 0.5, 1.000000 | restart | both |
| Maximum shadow resolution | `gow2_gfx_max_shadow_resolution` | auto, 256, 512, 800 | restart | both |
| Shadow texels per pixel | `gow2_gfx_shadow_texels_per_pixel` | auto, 0.5, 1, 1.27324 | restart | both |
| Decal draw distance | `gow2_gfx_decal_cull_distance_scale` | auto, 0.4, 0.7, 1.0 | restart | both |

`auto` keeps platform defaults: Windows retains retail quality; Android disables
Object shadows only. High explicitly enables Object shadows on Android. The
advanced Engine settings row (`gow2_system_settings`, empty by default) applies
last, so existing raw `Key=Value;OtherKey=Value` pairs override quality rows and
presets. Clear conflicting raw pairs to give control to the menu. Quality cvars
are strings in TOML, including numbers (e.g. `gow2_gfx_detail_mode = "1"`).

See [SYSTEM_SETTINGS.md](SYSTEM_SETTINGS.md) for the complete 76-key retail
inventory, every preset value, scope of verification, and the bundle size rule.

## Controls

| Row | Key(s) | Values | Applies | Platforms |
|---|---|---|---|---|
| Keyboard and mouse | `mnk_mode` | on, off | now | both |
| Native mouse aim | `gow2_mouse_aim` | on, off | restart | both |
| Mouse sensitivity | `gow2_mouse_scale` or `mnk_sensitivity` | 0.05x ... 10x | now | both |
| Invert mouse Y | `gow2_mouse_invert_y` | on, off | now | both |
| Aim assist with the mouse | `gow2_mouse_aim_assist` | on, off | now | both |
| Button prompts | `gow2_prompts` | automatic, keyboard, controller | restart | both |
| Keycap pictures | `gow2_keycaps` | on, off | restart | Windows |
| Controller look dead zone | `pad_right_stick_deadzone` | 0 ... 0.9 | now | both |
| Key bindings | opens the page below | | now | both |
| Controller layout | info | in the game's own Options | now | both |

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
| Your name | `user_name` | text, up to 15 characters | restart | both |
| System Link (LAN and internet) | `net_link_active` | on, off | restart | both |
| Co-op players | `gow2_coop_players` | 2, 3, 4 | restart | both |
| Co-op check code | info | the combined hash | now | both |
| Join a host (name or IP) | `net_peers` | text | restart | both |
| Host over the internet | opens the page below | | now | Windows |
| This device's LAN address | `net_local_ip` | text (advanced) | restart | both |
| Port offset | `net_port_offset` | 1024 ... 64512 | restart | both |
| Co-op check | `net_beacon_gate` | on, off (advanced) | restart | both |
| Network log | `system_link_trace` | on, off (advanced) | now | both |

The "Host over the internet" page holds: Status (info), Keep open for
(`gow2_host_lease_hours`: 2, 6, 12, 24 h), Open ports now (action), Close
ports (action), Address friends use (`net_public_address`: text), Friends
allowed (`net_allow_peers`: text), Close ports when the game exits
(`gow2_host_close_on_exit`: toggle). All Windows. See below.

## Saves

| Row | Key(s) | Values | Applies | Platforms |
|---|---|---|---|---|
| Save location | `gow2_save_dir` | default, portable, detected cloud folders | restart | both |
| Choose a folder | picks `gow2_save_dir` | folder picker | now | Windows |
| Folder path | `gow2_save_dir` | text (advanced) | restart | both |
| Saves are in | info | the folder this session uses | now | both |
| Open the saves folder | action | opens File Explorer | now | Windows |

## Audio

| Row | Key(s) | Values | Applies | Platforms |
|---|---|---|---|---|
| Volume | `gow2_audio_volume` | 0% ... 100% | now | both |
| Mute | `audio_mute` | on, off | now | both |
| Audio buffer | `audio_maxqframes` | 4 ... 64 (advanced) | restart | both |

## System

| Row | Key(s) | Values | Applies | Platforms |
|---|---|---|---|---|
| Menu language | `gow2_menu_language` | automatic, English, Spanish | now | both |
| Show advanced options | `gow2_menu_advanced` | on, off | now | both |
| All runtime settings | action | opens the F4 list | now | both |
| Restart the game | action | save first: it saves nothing | now | Windows |
| Quit the game | action | quit, then open the game again | now | Android |
| Settings file | info | where the menu saves | now | both |
| Version | info | this build of the port | now | both |
| Open this menu | info | F10, Back + Start | now | both |

## New settings this release

`gow2_fps_cap`, `gow2_audio_volume`, `gow2_menu_language`,
`gow2_menu_advanced`, `gow2_host_lease_hours`, `gow2_host_close_on_exit`,
`gow2_pc_options_entry`, `bind_pc_options` (F10).

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
