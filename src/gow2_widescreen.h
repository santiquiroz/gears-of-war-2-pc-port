// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Aspect-aware back buffer sizing and field-of-view adjustments.
#pragma once

#include <cstdint>
#include <string>

#include <rex/ppc.h>

namespace rex::glue {

// ---- widescreen -----------------------------------------------------------------------------
// Wider aspects extend the horizontal view. Narrower aspects keep 1280 pixels of width and extend the
// vertical view by increasing the render height.

// The aspect ratio wanted (width / height), or 0 when the game keeps its own 16:9.
double WidescreenAspect();
// The back buffer width, or 0 when the game keeps its native width.
int WidescreenBackBufferWidth();
// The back buffer height, including any Vert+ extension and EDRAM clamp.
int WidescreenBackBufferHeight();
// The denominator used by the guest aspect calculation; wider modes retain the original 720 value.
int WidescreenAspectHeight();
// Runs from OnPreSetup: gives the runtime's video mode the same aspect so the presenter shows the
// frame unstretched.
void PrepareWidescreen();
// The largest shadow map (pixels per side) that still fits the EDRAM next to the scene buffers, or
// 0 when the game's own 800 is fine.
int WidescreenMaxShadowResolution();
// The camera half-FOV widens for Hor+ and stays unchanged for Vert+.
double WidescreenHalfFov(double half_fov, int site);


// The first live (not Default__) guest object whose class is exactly class_name and, when scene_only is set,
// that has a scene client (offset 424), or 0.
uint32_t FindGuestObject(const uint8_t* base, const char* class_name, bool scene_only);


}  // namespace rex::glue
