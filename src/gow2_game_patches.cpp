// Adapted from gears-judgement-recomp (BSD-3-Clause).
// gow2 - ReXGlue Recompiled Project
//
// Host switches for the ported xenia-canary game patches
// (tools/patch_game_patches.py). The generated code asks Gow2GamePatch(id)
// at each patched instruction, so every patch is a live cvar. Patched
// lines in the generated code carry the marker "GOW2 GAMEPATCH <id>".

#include <rex/cvar.h>

#include "gow2_widescreen.h"

REXCVAR_DEFINE_BOOL(gow2_black_shading_fix, true, "GameFixes",
                    "Black Shading Fix: effectively turns off MSAA (GUseTilingMode=FALSE), "
                    "solving the dynamic lighting artifacts caused by MSAA resolve imperfections.");
REXCVAR_DEFINE_BOOL(gow2_texture_streaming_fix, true, "GameFixes",
                    "Texture Streaming Fix: PoolSize=196 and RHIGetTextureMemoryStats=FALSE "
                    "(locks FudgeFactor=1.0), minimizing texture pop-in in heavy scenes.");
REXCVAR_DEFINE_BOOL(gow2_aniso16, true, "GameFixes",
                    "16x Anisotropic Filtering.");
REXCVAR_DEFINE_BOOL(gow2_unlock_fps, false, "GameFixes",
                    "Unlock FPS: bypasses the engine framerate cap (frame limiter "
                    "load skipped, frame mode forced).");
REXCVAR_DEFINE_BOOL(gow2_ao_upscaling_fix, true, "GameFixes",
                    "Ambient Occlusion - Option A (Upscaling Fix): forces the AO "
                    "working height to 704, forces AO enable flags, disables one AO "
                    "occluder helper, and keeps the aspect-ratio branch always taken, "
                    "mitigating post-processing ghosting, shimmer, and checkerboard "
                    "artifacts when rendering above native resolution.");
REXCVAR_DEFINE_BOOL(gow2_dof_upscaling_fix, true, "GameFixes",
                    "Depth of Field, Option A (Upscaling Fix): disables the gaussian "
                    "blur factor (f1=0) in the DoF sampling path, reducing blur "
                    "ghosting at higher internal resolutions.");

extern "C" double Gow2WidescreenHalfFov(double half_fov, int site) {
  return rex::glue::WidescreenHalfFov(half_fov, site);
}

extern "C" int Gow2GamePatch(int id) {
  switch (id) {
    case 1: return REXCVAR_GET(gow2_black_shading_fix);
    case 2: return REXCVAR_GET(gow2_texture_streaming_fix);
    case 3: return REXCVAR_GET(gow2_aniso16);
    case 4: return REXCVAR_GET(gow2_unlock_fps);
    case 5: return REXCVAR_GET(gow2_ao_upscaling_fix);
    case 6: return REXCVAR_GET(gow2_dof_upscaling_fix);
    case 7: return rex::glue::WidescreenBackBufferWidth();  // 0 = the game's own 16:9
    default: return 0;
  }
}
