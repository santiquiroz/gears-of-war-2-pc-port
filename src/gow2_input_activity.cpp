// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_input_activity.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <atomic>
#include <cmath>

#include <rex/chrono/clock.h>
#include <rex/ui/window.h>
#include <rex/ui/window_listener.h>

namespace rex::glue {
namespace {

std::atomic<uint64_t> g_kbm_ms{0};

// Watches only: it never marks an event handled, so the other listeners see every one.
class ActivityListener : public rex::ui::WindowInputListener {
 public:
  void OnKeyDown(rex::ui::KeyEvent& e) override {
    if (!e.prev_state()) NoteKeyboardMouseActivity();  // prev_state: the OS repeating a held key
  }
  void OnMouseDown(rex::ui::MouseEvent&) override { NoteKeyboardMouseActivity(); }
  void OnMouseWheel(rex::ui::MouseEvent&) override { NoteKeyboardMouseActivity(); }
  void OnMouseMove(rex::ui::MouseEvent& e) override {
    // A desk bump or sensor jitter is not the player switching to the mouse.
    if (std::fabs(e.dx()) > 3.0f || std::fabs(e.dy()) > 3.0f) NoteKeyboardMouseActivity();
  }
};

ActivityListener g_listener;
std::atomic<bool> g_installed{false};

}  // namespace

void NoteKeyboardMouseActivity() {
  g_kbm_ms.store(rex::chrono::Clock::QueryHostUptimeMillis(), std::memory_order_relaxed);
}

uint64_t LastKeyboardMouseActivityMs() { return g_kbm_ms.load(std::memory_order_relaxed); }

uint64_t LastPadActivityMs(int* sdl_type) {
  int type = 0;
  uint64_t ms = 0;
#ifdef _WIN32
  using Fn = uint64_t (*)(int*);
  // Exported by the patched SDK (sdk/patches/0060-pad-activity.patch); older DLLs lack it.
  static const Fn fn = [] {
    HMODULE m = GetModuleHandleA("rexruntime.dll");
    return m ? reinterpret_cast<Fn>(GetProcAddress(m, "rex_input_get_last_physical_activity")) : nullptr;
  }();
  if (fn) ms = fn(&type);
#endif
  if (sdl_type) *sdl_type = type;
  return ms;
}

void InstallInputActivityWatch(rex::ui::Window* window) {
  if (!window || g_installed.exchange(true, std::memory_order_acq_rel)) return;
  window->AddInputListener(&g_listener, 3);
}

}  // namespace rex::glue
