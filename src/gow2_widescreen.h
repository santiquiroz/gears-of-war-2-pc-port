// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Widescreen support: the back buffer width and field of view for wider than 16:9, and the UI fixes that go with it.
#pragma once

#include <cstdint>
#include <string>

#include <rex/ppc.h>

namespace rex::glue {

// ---- widescreen -----------------------------------------------------------------------------
// gow2_widescreen = off | auto | W:H | number. The game builds its back buffer as 704 lines (720
// without the AO fix) times an aspect it fixes at 16:9 and clamps to 1280 wide; the widescreen patch
// (game patch id 7) lets that width be larger, a multiple of 80 so the console's EDRAM tiles fit,
// and widens the field of view so the vertical view stays what it was.

// The aspect ratio wanted (width / height), or 0 when the game keeps its own 16:9.
double WidescreenAspect();
// The back buffer width the game will build (a multiple of 80), or 0 when off.
int WidescreenBackBufferWidth();
// The back buffer height the game uses (704, or 720 without the AO fix).
int WidescreenBackBufferHeight();
// Runs from OnPreSetup: gives the runtime's video mode the same aspect so the presenter shows the
// frame unstretched.
void PrepareWidescreen();
// The largest shadow map (pixels per side) that still fits the EDRAM next to the wider scene buffers, or
// 0 when the game's own 800 is fine.
int WidescreenMaxShadowResolution();
// The camera's half field of view (radians) widened for the wider back buffer: the horizontal
// tangent grows by W / 1280, so the vertical field of view is the one of the 16:9 game.
double WidescreenHalfFov(double half_fov, int site);


// The first live (not Default__) guest object whose class is exactly class_name and, when scene_only is set,
// that has a scene client (offset 424), or 0.
uint32_t FindGuestObject(const uint8_t* base, const char* class_name, bool scene_only);


}  // namespace rex::glue
