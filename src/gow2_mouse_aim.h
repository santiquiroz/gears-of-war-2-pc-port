// gow2 - ReXGlue Recompiled Project
//
// Native mouse look. Feeds captured mouse motion into the engine's own PC
// mouse axes (MouseX/MouseY) instead of the emulated right stick, and keeps
// the game's aim friction/adhesion off the mouse. The pad path is untouched.
#pragma once

#include <cstdint>

#include <rex/ppc.h>

namespace rex::ui {
class Window;
}

namespace rex::glue {

bool NativeMouseAimWanted();
bool NativeMouseAimInstalled();
void DiscardNativeMouseInput();
void PrepareNativeMouseAim();
void InstallNativeMouseAim(rex::ui::Window* window);

// Called on the game thread at every ProcessInput, after the mouse counts went in and before the
// original runs: viewport is the FViewport, dt the frame time. One listener.
using ProcessInputListener = void (*)(PPCContext& ctx, uint8_t* base, uint32_t viewport, double dt);
void SetProcessInputListener(ProcessInputListener listener);

}  // namespace rex::glue
