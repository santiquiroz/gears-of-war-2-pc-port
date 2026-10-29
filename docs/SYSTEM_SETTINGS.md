# Retail engine SystemSettings and game quality

## Source and scope

Read from the user's retail `GearGame/CookedXenon/Coalesced_int.bin` with the project's read-only decoder:

```powershell
python -I tools/ue3/coalesced_dump.py C:\personal\gow2-gamedata\retail\GearGame\CookedXenon\Coalesced_int.bin SystemSettings
```

The decoder matches section substrings. The inventory below filters its output to
exactly `..\GearGame\Config\Xe-GearEngine.ini` and exactly `[SystemSettings]`;
`SystemSettingsEditor` and `SystemSettingsSplitScreen2` are excluded. There are
**76 keys: 57 scalars and 19 texture-group structs**. Retail spellings, letter case
and numeric formatting are preserved, including `ansio` in EffectsNotFiltered.
`ShadowFilterQuality` is absent; the actual key is `ShadowFilterQualityBias`.

Types and purposes describe the generic UE3 setting, using Epic's
[legacy UE3 System Settings reference](https://docs.unrealengine.com/udk/Three/SystemSettings.html)
and [Texture Support and Settings](https://docs.unrealengine.com/udk/Three/TextureSupportAndSettings.html).
Presence and retail values are verified by decoding this bundle. Visual effects,
performance gains and whether every Xbox 360 rendering path consumes each setting
are **not verified in game**. No game launch was performed for this change. In
particular, legacy resolution, VSync, MSAA and D3D10 requests do not establish that
they control the host D3D12 runtime; use the existing display/runtime rows for that.

## Complete retail inventory

| Key | Retail value | Type | UE3 purpose |
|---|---|---|---|
| `StaticDecals` | `True` | bool | Static surface marks. |
| `DynamicDecals` | `True` | bool | Dynamic surface marks. |
| `UnbatchedDecals` | `True` | bool | Decals outside static draw lists. |
| `DecalCullDistanceScale` | `1.0` | float | Decal visibility distance multiplier. |
| `DynamicLights` | `True` | bool | Dynamic lighting. |
| `DynamicShadows` | `True` | bool | Dynamic shadow rendering. |
| `LightEnvironmentShadows` | `True` | bool | Shadows from dynamic light environments (object shadows). |
| `CompositeDynamicLights` | `True` | bool | Combines dynamic lights into light environments. |
| `DirectionalLightmaps` | `True` | bool | Directional baked lighting, including normal/specular response. |
| `MotionBlur` | `True` | bool | Motion blur effect. |
| `DepthOfField` | `True` | bool | Focus blur effect. |
| `AmbientOcclusion` | `True` | bool | Contact/occlusion shading effect. |
| `Bloom` | `True` | bool | Bright-area glow effect. |
| `UseHighQualityBloom` | `True` | bool | Higher-quality bloom path. |
| `Distortion` | `True` | bool | Scene distortion effects. |
| `DropParticleDistortion` | `False` | bool | Permits omitting particle distortion under reduced detail. |
| `SpeedTreeLeaves` | `True` | bool | SpeedTree leaf rendering. |
| `SpeedTreeFronds` | `True` | bool | SpeedTree frond rendering. |
| `OnlyStreamInTextures` | `False` | bool | Prevents texture streaming out. |
| `LensFlares` | `True` | bool | Lens flare rendering. |
| `FogVolumes` | `True` | bool | Volumetric fog regions. |
| `FloatingPointRenderTargets` | `True` | bool | Floating-point render surfaces. |
| `OneFrameThreadLag` | `True` | bool | Allows render thread to trail simulation by one frame. |
| `UseVsync` | `False` | bool | Engine vertical synchronization request. |
| `UpscaleScreenPercentage` | `True` | bool | Expands a reduced view to the output surface. |
| `Fullscreen` | `True` | bool | Engine fullscreen request. |
| `AllowD3D10` | `False` | bool | Permits the original engine D3D10 path. |
| `SkeletalMeshLODBias` | `0` | int | Skeletal mesh detail bias; positive selects coarser levels. |
| `ParticleLODBias` | `0` | int | Particle-system detail bias; positive selects coarser levels. |
| `DetailMode` | `2` | int | Component detail tier (menu uses 0 low, 1 medium, 2 high). |
| `ShadowFilterQualityBias` | `0` | int | Shadow filtering quality bias; negative reduces quality. |
| `MaxAnisotropy` | `4` | int | Maximum anisotropic texture filtering level. |
| `MaxMultisamples` | `1` | int | Maximum requested MSAA sample count. |
| `MinShadowResolution` | `64` | int | Minimum shadow-map dimension in texels. |
| `MaxShadowResolution` | `800` | int | Maximum shadow-map dimension in texels. |
| `ShadowFadeResolution` | `128` | int | Shadow resolution below which fading begins. |
| `ShadowFadeExponent` | `2.2` | float | Shadow fading curve exponent. |
| `ResX` | `1280` | int | Engine horizontal rendering resolution request. |
| `ResY` | `720` | int | Engine vertical rendering resolution request. |
| `ScreenPercentage` | `100.000000` | float | Main-view rendering percentage. |
| `SceneCaptureStreamingMultiplier` | `1.000000` | float | Texture-streaming distance multiplier for scene captures. |
| `FoliageDrawRadiusMultiplier` | `1.000000` | float | Foliage visibility radius multiplier. |
| `ShadowTexelsPerPixel` | `1.27324` | float | Shadow-map density relative to projected subject size. |
| `bEnableVSMShadows` | `False` | bool | Variance shadow map path selection. |
| `bEnableBranchingPCFShadows` | `TRUE` | bool | Branching percentage-closer shadow filtering. |
| `bAllowBetterModulatedShadows` | `True` | bool | Extra pass to suppress shadows on emissive/back-facing geometry. |
| `bEnableForegroundShadowsOnWorld` | `True` | bool | Allows foreground objects to shadow world geometry. |
| `bEnableForegroundSelfShadowing` | `False` | bool | Allows foreground self-shadowing. |
| `ShadowFilterRadius` | `2` | float | Shadow filter disk radius in texels. |
| `ShadowDepthBias` | `.012` | float | Projected shadow depth offset (excluding VSM). |
| `ShadowVolumeLightRadiusThreshold` | `1000` | float | Minimum light radius for shadow volumes. |
| `ShadowVolumePrimitiveScreenSpacePercentageThreshold` | `0.25` | float | Minimum projected primitive size for shadow volumes. |
| `bAllowFracturedDamage` | `True` | bool | Allows damage to fractured meshes. |
| `NumFracturedPartsScale` | `1.0` | float | Multiplier for allowed fractured physics pieces. |
| `FractureDirectSpawnChanceScale` | `1.0` | float | Scales piece spawning after direct damage. |
| `FractureRadialSpawnChanceScale` | `1.0` | float | Scales piece spawning after blast damage. |
| `FractureCullDistanceScale` | `1.0` | float | Distance multiplier for damage-induced fracture. |
| `TEXTUREGROUP_World` | `(MinLODSize=256,MaxLODSize=1024,LODBias=1,MinMagFilter=aniso,MipFilter=point)` | struct | Texture size bounds, mip bias and filters for World textures. |
| `TEXTUREGROUP_WorldNormalMap` | `(MinLODSize=256,MaxLODSize=1024,LODBias=1,MinMagFilter=aniso,MipFilter=point)` | struct | Texture size bounds, mip bias and filters for WorldNormalMap textures. |
| `TEXTUREGROUP_WorldSpecular` | `(MinLODSize=256,MaxLODSize=1024,LODBias=2,MinMagFilter=linear,MipFilter=point)` | struct | Texture size bounds, mip bias and filters for WorldSpecular textures. |
| `TEXTUREGROUP_Character` | `(MinLODSize=256,MaxLODSize=1024,LODBias=0,MinMagFilter=aniso,MipFilter=point)` | struct | Texture size bounds, mip bias and filters for Character textures. |
| `TEXTUREGROUP_CharacterNormalMap` | `(MinLODSize=256,MaxLODSize=1024,LODBias=0,MinMagFilter=aniso,MipFilter=point)` | struct | Texture size bounds, mip bias and filters for CharacterNormalMap textures. |
| `TEXTUREGROUP_CharacterSpecular` | `(MinLODSize=256,MaxLODSize=1024,LODBias=2,MinMagFilter=linear,MipFilter=point)` | struct | Texture size bounds, mip bias and filters for CharacterSpecular textures. |
| `TEXTUREGROUP_Weapon` | `(MinLODSize=256,MaxLODSize=1024,LODBias=0,MinMagFilter=aniso,MipFilter=point)` | struct | Texture size bounds, mip bias and filters for Weapon textures. |
| `TEXTUREGROUP_WeaponNormalMap` | `(MinLODSize=256,MaxLODSize=1024,LODBias=0,MinMagFilter=aniso,MipFilter=point)` | struct | Texture size bounds, mip bias and filters for WeaponNormalMap textures. |
| `TEXTUREGROUP_WeaponSpecular` | `(MinLODSize=256,MaxLODSize=1024,LODBias=2,MinMagFilter=linear,MipFilter=point)` | struct | Texture size bounds, mip bias and filters for WeaponSpecular textures. |
| `TEXTUREGROUP_Vehicle` | `(MinLODSize=512,MaxLODSize=1024,LODBias=0,MinMagFilter=aniso,MipFilter=point)` | struct | Texture size bounds, mip bias and filters for Vehicle textures. |
| `TEXTUREGROUP_VehicleNormalMap` | `(MinLODSize=512,MaxLODSize=1024,LODBias=0,MinMagFilter=aniso,MipFilter=point)` | struct | Texture size bounds, mip bias and filters for VehicleNormalMap textures. |
| `TEXTUREGROUP_VehicleSpecular` | `(MinLODSize=512,MaxLODSize=1024,LODBias=2,MinMagFilter=linear,MipFilter=point)` | struct | Texture size bounds, mip bias and filters for VehicleSpecular textures. |
| `TEXTUREGROUP_Cinematic` | `(MinLODSize=4096,MaxLODSize=4096,LODBias=0,MinMagFilter=aniso,MipFilter=point)` | struct | Texture size bounds, mip bias and filters for Cinematic textures. |
| `TEXTUREGROUP_Effects` | `(MinLODSize=128,MaxLODSize=512,LODBias=1,MinMagFilter=linear,MipFilter=point)` | struct | Texture size bounds, mip bias and filters for Effects textures. |
| `TEXTUREGROUP_EffectsNotFiltered` | `(MinLODSize=128,MaxLODSize=512,LODBias=1,MinMagFilter=ansio,MipFilter=point)` | struct | Texture size bounds, mip bias and filters for EffectsNotFiltered textures. |
| `TEXTUREGROUP_Skybox` | `(MinLODSize=512,MaxLODSize=2048,LODBias=0,MinMagFilter=linear,MipFilter=point)` | struct | Texture size bounds, mip bias and filters for Skybox textures. |
| `TEXTUREGROUP_UI` | `(MinLODSize=512,MaxLODSize=1024,LODBias=0,MinMagFilter=linear,MipFilter=point)` | struct | Texture size bounds, mip bias and filters for UI textures. |
| `TEXTUREGROUP_LightAndShadowMap` | `(MinLODSize=512,MaxLODSize=4096,LODBias=0,MinMagFilter=aniso,MipFilter=point)` | struct | Texture size bounds, mip bias and filters for LightAndShadowMap textures. |
| `TEXTUREGROUP_RenderTarget` | `(MinLODSize=1,MaxLODSize=4096,LODBias=0,MinMagFilter=aniso,MipFilter=point)` | struct | Texture size bounds, mip bias and filters for RenderTarget textures. |

Lowering these LODBias values below retail is not offered: tried on 2026-10-28 (one level and all to 0, with and without a bigger texture pool), some objects render black, because the disc's cooked textures do not carry the mips the retail values skip. Sharper textures come from texture packs (Graphics → Texture packs).

## Game quality in Graphics

The new **Game quality** group (**Calidad del juego** in Spanish) is available on
Windows and Android. Its 24 controls edit existing retail keys for shadows,
lighting, effects, decals, foliage and mesh/particle detail. They have a bounded
choice list rather than accepting arbitrary engine values. Every row needs a
restart because Coalesced bundles are prepared at boot. The new controls have
host-test coverage; their appearance, visual effects and performance have not
been tested by launching the game as part of this change.

All 24 cvars are strings, including numeric choices: use e.g.
`gow2_gfx_max_shadow_resolution = "512"`. `"auto"` keeps platform defaults:
Windows uses retail values; Android disables only `LightEnvironmentShadows` and
keeps the rest retail. Object shadows uses
`gow2_gfx_light_environment_shadows = "auto" | "on" | "off"`, shared with the
Android launcher.

**Quality preset** sets all 24 controls to Low, Medium or High (retail). Low
reduces shadows and optional effects; Medium restores some effects and uses
intermediate detail. These are intended quality/performance tradeoffs, not
measured performance promises. High explicitly restores the retail values on
both platforms, including object shadows on Android. The initial all-Auto state displays High on Windows and Custom on Android
(the latter keeps object shadows off). Other combinations that do not exactly
match a preset display Custom.
Changing an individual row away from its preset displays Custom. Custom is a
status/choice that preserves individual values. The preset does not clear raw
advanced overrides.

The table gives the exact cvar choices and preset mappings. All controls also
accept `auto`; boolean controls accept `on` and `off`.

| Row | Cvar | Explicit choices | Low | Medium | High (retail) |
|---|---|---|---|---|---|
| Object shadows | `gow2_gfx_light_environment_shadows` | `on`, `off` | `off` | `off` | `on` |
| Dynamic shadows | `gow2_gfx_dynamic_shadows` | `on`, `off` | `off` | `on` | `on` |
| Dynamic lights | `gow2_gfx_dynamic_lights` | `on`, `off` | `on` | `on` | `on` |
| Composite dynamic lights | `gow2_gfx_composite_dynamic_lights` | `on`, `off` | `on` | `on` | `on` |
| Bloom | `gow2_gfx_bloom` | `on`, `off` | `off` | `on` | `on` |
| High quality bloom | `gow2_gfx_use_high_quality_bloom` | `on`, `off` | `off` | `off` | `on` |
| Motion blur | `gow2_gfx_motion_blur` | `on`, `off` | `off` | `off` | `on` |
| Depth of field | `gow2_gfx_depth_of_field` | `on`, `off` | `off` | `on` | `on` |
| Ambient occlusion | `gow2_gfx_ambient_occlusion` | `on`, `off` | `off` | `off` | `on` |
| Lens flares | `gow2_gfx_lens_flares` | `on`, `off` | `off` | `on` | `on` |
| Distortion | `gow2_gfx_distortion` | `on`, `off` | `off` | `on` | `on` |
| Static decals | `gow2_gfx_static_decals` | `on`, `off` | `on` | `on` | `on` |
| Dynamic decals | `gow2_gfx_dynamic_decals` | `on`, `off` | `off` | `on` | `on` |
| Fog volumes | `gow2_gfx_fog_volumes` | `on`, `off` | `off` | `on` | `on` |
| Tree leaves | `gow2_gfx_speed_tree_leaves` | `on`, `off` | `off` | `on` | `on` |
| Tree fronds | `gow2_gfx_speed_tree_fronds` | `on`, `off` | `off` | `on` | `on` |
| Shadow filter bias | `gow2_gfx_shadow_filter_quality_bias` | `-1`, `0`, `1` | `-1` | `0` | `0` |
| World detail | `gow2_gfx_detail_mode` | `0`, `1`, `2` | `0` | `1` | `2` |
| Character LOD bias | `gow2_gfx_skeletal_mesh_lod_bias` | `0`, `1`, `2` | `2` | `1` | `0` |
| Particle LOD bias | `gow2_gfx_particle_lod_bias` | `0`, `1`, `2` | `2` | `1` | `0` |
| Foliage draw radius | `gow2_gfx_foliage_draw_radius_multiplier` | `0.25`, `0.5`, `1.000000` | `0.25` | `0.5` | `1.000000` |
| Maximum shadow resolution | `gow2_gfx_max_shadow_resolution` | `256`, `512`, `800` | `256` | `512` | `800` |
| Shadow texels per pixel | `gow2_gfx_shadow_texels_per_pixel` | `0.5`, `1`, `1.27324` | `0.5` | `1` | `1.27324` |
| Decal draw distance | `gow2_gfx_decal_cull_distance_scale` | `0.4`, `0.7`, `1.0` | `0.4` | `0.7` | `1.0` |

## Advanced overrides and bundle limits

`gow2_system_settings` is an advanced text row on both platforms, defaulting to
an empty string. Its semicolon-separated `Key=Value` pairs override the platform
defaults and individual quality rows, with case-insensitive key matching. Example:
`gow2_system_settings = "Bloom=0;MaxShadowResolution=512"`. Existing saved raw
values still override presets; remove the relevant raw pair to let its menu row
control it again. The layer applies to Windows as well as Android.

Recognized UE3 booleans are written as short `0`/`1` equivalents, including raw
`True`/`False` or `on`/`off`, to avoid growth such as `True` becoming `False`.
An edited bundle larger than its TOC size is rejected with the existing
Coalesced preparation diagnostic; the game's original values remain available.
A shorter edited bundle is zero-padded to exactly its TOC size. Presets and raw
overrides cannot bypass that boot-safety limit.
