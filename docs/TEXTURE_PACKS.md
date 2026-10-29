# Texture packs

A texture pack replaces the game's textures with larger versions. PC Options can build one for you
from the textures your own copy dumps, upscaled on your GPU with Real-ESRGAN (Windows only).

## Dump, build, pick

1. **Dump.** PC Options → Graphics: set **Texture pack** to **Off**, turn on **Dump textures** (an advanced
   row: System → Show advanced options), restart and play the parts you want. Every texture the game
   draws lands once in `textures\4D53082D\dump` next to `gow2.exe`. A dump goes into the active pack
   root, so with a pack chosen it ends up inside that pack and the builder does not see it.
2. **Build.** PC Options → Graphics → **Build texture pack** (works in the launcher, before the game
   loads). Choose the scale (2x or 4x) and the quality, then **Start**. The status row counts the
   textures (`Building 120 / 724`). You can press Play and keep it running in the background; quitting
   the game cancels it. Cancel and Start again picks up where it stopped, and a later build only adds the
   textures you dumped since. The builder runs Real-ESRGAN on the same GPU as the game.
3. **Pick.** When it says `Done`, choose the new pack (`dump-2x`, `dump-4x`, `dump-2x-bc7` or
   `dump-4x-bc7`) in **Texture pack** and restart.

Only colored BC1/BC2/BC3 textures of 128 pixels or more on a side are upscaled. Normal maps (recognised
by their blue-violet average color), grayscale textures (paint-damage masks, specular maps, light cones
and gradients, emissive masks), noise (the rgba8 dither/jitter textures), single-channel masks and
textures smaller than 128 pixels keep the disc version: Real-ESRGAN would invent detail in them and a
plain resize only costs memory. In the chapter 2 hospital, 161 of the 431 color-format textures that
were upscaled were grayscale: a paint-damage mask came back with harder, darker blobs and drew black
splotches on a door, two rgba8 noise textures were smoothed flat, and 64-pixel textures came out darker
and invented. No side goes above 4096.

## Video memory

2x has about 4 times the pixels, 4x about 16 times; **BC7** is sharper than the disc's own formats and
takes about twice the memory. Measured on 40 textures:

| Pack | Size |
| --- | --- |
| Disc textures | 5.5 MB |
| 2x, BC7 | 54 MB |
| 4x, BC7 | 214 MB |

If the game stutters or runs out of video memory with a 4x pack, use 2x or raise PC Options → Graphics
→ Texture cache.

## Where packs go

Each pack is a folder in `texture_packs` next to `gow2.exe` holding `4D53082D\replace\*.dds`, with the
same file names as the dumps (the game matches the 16-digit hash at the start of the name). **Open
texture packs folder** opens it. The builder works in `texture_packs\.build` and deletes it when it
finishes. It needs `tools\texconv` and `tools\realesrgan` next to `gow2.exe`, which the release zip
includes; a source build gets them from `scripts\package.ps1`. A pack you make yourself by hand goes in
the same place (README, "Texture packs").

## Keep them on your PC

A pack built from the game's textures is derived from the game's copyrighted art. It is for your own
copy: do not share it. This project does not host or link packs made from the game's art.

## Credits

- [Real-ESRGAN](https://github.com/xinntao/Real-ESRGAN) by Xintao Wang et al. (realesr-animevideov3-x4 model, BSD 3-Clause)
  and [Real-ESRGAN-ncnn-vulkan](https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan) (MIT), on
  [ncnn](https://github.com/Tencent/ncnn) (BSD 3-Clause).
- [DirectXTex](https://github.com/microsoft/DirectXTex) texconv (MIT) for the DDS and PNG conversion and
  the mip maps.

License texts: `THIRD_PARTY_NOTICES.md`.
