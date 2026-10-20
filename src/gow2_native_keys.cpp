// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_native_keys.h"

#if !defined(__ANDROID__)
#include <windows.h>
#endif

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc.h>
#include <rex/runtime.h>
#include <rex/system/function_dispatcher.h>
#include <rex/ui/window.h>
#include <rex/ui/window_listener.h>

#include "gow2_dev_commands.h"
#include "gow2_dev_guest_calls.h"
#include "gow2_engine_tick.h"
#include "gow2_guest.h"
#include "gow2_host_ui.h"
#include "gow2_key_names.h"
#include "gow2_mouse_aim.h"
#include "gow2_native_bindings.h"

REXCVAR_DEFINE_BOOL(gow2_native_keys, true, "input",
                    "While a level is played, send keys and mouse buttons to the game as keys "
                    "(its own bindings) instead of through the emulated pad. Menus and the pause "
                    "menu still use the pad emulation. Takes effect on the next start.");

namespace rex::glue {
namespace {

using Clock = std::chrono::steady_clock;

constexpr uint32_t kViewportClientOff = 52;   // FViewport -> FViewportClient
constexpr uint32_t kClientInputKeySlot = 16;  // vtable offset of InputKey(viewport, controller, key, event, amount, gamepad)
constexpr uint32_t kWorldInfoTimeSeconds = 708;
constexpr uint32_t kWorldInfoRealTimeSeconds = 712;
constexpr int kEventPressed = 0, kEventReleased = 1;

// Keys the window saw, in order. A wheel notch is a press and a release.
struct KeyEvent {
  uint16_t vk;
  bool down;
};
constexpr uint16_t kWheelUp = 0x1001, kWheelDown = 0x1002, kReleaseAll = 0x1FFF;

std::mutex g_queue_mutex;
std::vector<KeyEvent> g_queue;
std::atomic<bool> g_installed{false};
std::atomic<bool> g_active{false};
std::atomic<bool> g_gameplay{false};  // written by the engine tick, read by the input hook
std::atomic<std::thread::id> g_game_thread{};

void Push(uint16_t vk, bool down) {
  std::lock_guard<std::mutex> lock(g_queue_mutex);
  if (g_queue.size() < 4096) g_queue.push_back({vk, down});
}

class KeyListener : public rex::ui::WindowInputListener {
 public:
  void OnKeyDown(rex::ui::KeyEvent& e) override {
    if (e.prev_state()) return;  // the OS repeating a held key
    Push(uint16_t(e.virtual_key()), true);
  }
  void OnKeyUp(rex::ui::KeyEvent& e) override { Push(uint16_t(e.virtual_key()), false); }
  void OnMouseDown(rex::ui::MouseEvent& e) override { MouseButton(e, true); }
  void OnMouseUp(rex::ui::MouseEvent& e) override { MouseButton(e, false); }
  void OnMouseWheel(rex::ui::MouseEvent& e) override {
    if (e.scroll_y() > 0) Push(kWheelUp, true);
    if (e.scroll_y() < 0) Push(kWheelDown, true);
  }

 private:
  static void MouseButton(rex::ui::MouseEvent& e, bool down) {
    using Button = rex::ui::MouseEvent::Button;
    uint16_t vk = 0;
    switch (e.button()) {
      case Button::kLeft: vk = 0x01; break;
      case Button::kRight: vk = 0x02; break;
      case Button::kMiddle: vk = 0x04; break;
      case Button::kX1: vk = 0x05; break;
      case Button::kX2: vk = 0x06; break;
      default: return;
    }
    // A click outside the mouse capture (menus, the overlay) is not the game's.
    rex::ui::Window* window = e.target();
    if (down && (!window || !window->IsMouseCaptureRequested())) return;
    Push(vk, down);
  }
};

class FocusListener : public rex::ui::WindowListener {
 public:
  void OnLostFocus(rex::ui::UISetupEvent&) override { Push(kReleaseAll, false); }
};

KeyListener g_key_listener;
FocusListener g_focus_listener;

// ---- engine side -------------------------------------------------------------------------------

std::map<std::string, uint64_t> g_names;  // engine key name -> FName value (game thread)
std::vector<NativeBinding> g_baked;       // the bindings the game's input bundle was made with

// The engine key a physical key stands for; the wheel has no virtual key of its own.
const char* EngineNameOf(uint16_t vk) {
  if (vk == kWheelUp) return "MouseScrollUp";
  if (vk == kWheelDown) return "MouseScrollDown";
  return keynames::EngineNameForVk(vk);
}

uint64_t NameFor(const PPCContext& ctx, uint8_t* base, const std::string& engine_name) {
  const auto it = g_names.find(engine_name);
  if (it != g_names.end()) return it->second;
  uint64_t value = 0;
  if (const auto n = guest_call::MakeName(ctx, base, engine_name, true)) value = *n;
  g_names[engine_name] = value;
  return value;
}

bool SafeCall(PPCFunc* fn, PPCContext* call, uint8_t* base) {
#if defined(__ANDROID__)
  fn(*call, base);
  return true;
#else
  __try {
    fn(*call, base);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
#endif
}

struct Delivery {
  PPCContext* ctx;
  uint8_t* base;
  uint32_t viewport;
  PPCFunc* fn = nullptr;
  uint32_t client = 0;
};

bool PrepareDelivery(Delivery* d) {
  rex::Runtime* runtime = rex::Runtime::instance();
  auto* dispatcher = runtime ? runtime->function_dispatcher() : nullptr;
  if (!dispatcher || d->viewport == 0) return false;
  d->client = guest::LoadU32(d->base, d->viewport + kViewportClientOff);
  if (d->client == 0) return false;
  const uint32_t vtable = guest::LoadU32(d->base, d->client);
  if (vtable == 0) return false;
  const uint32_t fn = guest::LoadU32(d->base, vtable + kClientInputKeySlot);
  if (fn == 0) return false;
  d->fn = dispatcher->GetFunction(fn);
  return d->fn != nullptr;
}

bool SendKey(Delivery& d, const std::string& engine_name, int event) {
  const uint64_t name = NameFor(*d.ctx, d.base, engine_name);
  if (name == 0) return false;
  PPCContext call = *d.ctx;
  call.r3.u64 = d.client;
  call.r4.u64 = d.viewport;
  call.r5.u64 = 0;  // controller id
  call.r6.u64 = name;
  call.r7.u64 = uint64_t(event);
  call.r8.u64 = 0;
  call.r9.u64 = 0;  // not a gamepad
  call.f1.f64 = 1.0;  // amount depressed
  return SafeCall(d.fn, &call, d.base);
}

std::set<uint16_t> g_held;                        // keys the user holds (every mode; game thread)
std::map<uint16_t, std::vector<std::string>> g_routed;  // physical key -> engine keys sent for it
std::map<std::string, int> g_down;                // engine key -> physical keys holding it down

bool Held(uint16_t left, uint16_t right) { return g_held.count(left) || g_held.count(right); }

// The keys the player set in the options now, read when a key goes down so a change made while
// playing applies at once (the bundle itself keeps the bindings of the start).
void Press(Delivery& d, uint16_t vk) {
  std::vector<std::string> routes;
  if (const char* name = EngineNameOf(vk)) {
    const auto current = NativeKeyBindings([](const char* n) { return rex::cvar::Query<std::string>(n); });
    routes = RouteKey(g_baked, current, name, Held(0xA0, 0xA1), Held(0xA2, 0xA3), Held(0xA4, 0xA5));
  }
  if (const char* name = EngineNameOf(vk); name && (routes.size() != 1 || routes[0] != name))
    REXLOG_INFO("NATIVEKEYS rebound key {} sends {} key(s){}{}", name, routes.size(),
                routes.empty() ? "" : ": ", routes.empty() ? "" : routes[0]);
  for (const std::string& name : routes)
    if (++g_down[name] == 1) SendKey(d, name, kEventPressed);
  g_routed[vk] = std::move(routes);
}

void Release(Delivery& d, uint16_t vk) {
  const auto it = g_routed.find(vk);
  if (it == g_routed.end()) return;
  for (const std::string& name : it->second) {
    const auto down = g_down.find(name);
    if (down != g_down.end() && --down->second == 0) {
      g_down.erase(down);
      SendKey(d, name, kEventReleased);
    }
  }
  g_routed.erase(it);
}

void ReleaseSent(Delivery& d) {
  for (const auto& [name, count] : g_down) SendKey(d, name, kEventReleased);
  g_down.clear();
  g_routed.clear();
}

void OnProcessInput(PPCContext& ctx, uint8_t* base, uint32_t viewport, double) {
  // Only the game thread's input step: other threads call it for other viewports.
  if (std::this_thread::get_id() != g_game_thread.load(std::memory_order_acquire)) return;
  std::vector<KeyEvent> events;
  {
    std::lock_guard<std::mutex> lock(g_queue_mutex);
    events.swap(g_queue);
  }
  const bool native = g_gameplay.load(std::memory_order_acquire) && !HostUiOpen();
  Delivery d{&ctx, base, viewport};
  const bool ready = PrepareDelivery(&d);
  for (const KeyEvent& e : events) {
    if (e.vk == kReleaseAll) {
      g_held.clear();
      if (ready) ReleaseSent(d);
      continue;
    }
    if (e.vk == kWheelUp || e.vk == kWheelDown) {
      if (native && ready) {
        SendKey(d, EngineNameOf(e.vk), kEventPressed);
        SendKey(d, EngineNameOf(e.vk), kEventReleased);
      }
      continue;
    }
    if (e.down) g_held.insert(e.vk);
    else g_held.erase(e.vk);
    if (!native || !ready) continue;
    if (e.down) {
      if (!g_routed.count(e.vk)) Press(d, e.vk);
    } else {
      Release(d, e.vk);
    }
  }
  if (!ready) return;
  if (native) {
    // Entering native mode with keys already held: the game learns of them now.
    for (uint16_t vk : g_held)
      if (!g_routed.count(vk)) Press(d, vk);
  } else if (!g_routed.empty()) {
    ReleaseSent(d);
  }
}

// ---- the gate ----------------------------------------------------------------------------------

float LoadFloat(const uint8_t* base, uint32_t addr) {
  const uint32_t bits = guest::LoadU32(base, addr);
  float f;
  std::memcpy(&f, &bits, 4);
  return f;
}

// A level is being played: the local player controller has a pawn and the game clock runs (it
// stops in the pause menu while the real-time clock goes on).
bool GameplayNow(const PPCContext& ctx, uint8_t* base) {
  static float last_time = -1, last_real = -1;
  static Clock::time_point last_advance{};
  const uint32_t engine = ctx.r3.u32;
  const uint32_t pc = dev_commands::LocalPlayerController(base, dev_commands::kGameGlobals, engine);
  if (pc == 0 || guest::LoadU32(base, pc + dev_commands::layout::kControllerPawn) == 0) return false;
  const uint32_t wi = dev_commands::WorldInfoOf(base, dev_commands::kGameGlobals);
  if (wi == 0) return false;
  const float time = LoadFloat(base, wi + kWorldInfoTimeSeconds);
  const float real = LoadFloat(base, wi + kWorldInfoRealTimeSeconds);
  const auto now = Clock::now();
  if (time != last_time) last_advance = now;
  last_time = time;
  last_real = real;
  return now - last_advance < std::chrono::milliseconds(400);
}

void GateTick(const PPCContext& ctx, uint8_t* base) {
  if (!g_installed.load(std::memory_order_acquire)) return;
  g_game_thread.store(std::this_thread::get_id(), std::memory_order_release);
  static Clock::time_point next{};
  const auto now = Clock::now();
  if (now < next) return;
  next = now + std::chrono::milliseconds(100);
  bool gameplay = false;
  // Reads guest objects: a fault while the world is being torn down means "not gameplay".
  std::pair<const PPCContext*, uint8_t*> args{&ctx, base};
  struct Run {
    std::pair<const PPCContext*, uint8_t*>* a;
    bool* out;
  } run{&args, &gameplay};
  guest_call::GuardedRun(
      [](void* arg) {
        auto* r = static_cast<Run*>(arg);
        *r->out = GameplayNow(*r->a->first, r->a->second);
      },
      &run);
  const bool was = g_gameplay.exchange(gameplay, std::memory_order_acq_rel);
  if (was == gameplay) return;
  // The pad emulation and the native path never run together.
  (void)rex::cvar::SetFlagByName("mnk_keys", gameplay ? "false" : "true");
  g_active.store(gameplay, std::memory_order_release);
  REXLOG_INFO("NATIVEKEYS {}", gameplay ? "on (gameplay): keys go to the game as keys"
                                        : "off: the pad emulation takes the keys");
}

[[maybe_unused]] const bool kGateRegistered = (AddEngineTickListener(&GateTick), true);

}  // namespace

bool NativeKeysWanted() {
  return rex::cvar::Query<bool>("gow2_native_keys") && NativeMouseAimInstalled();
}

bool NativeKeysActive() { return g_active.load(std::memory_order_acquire); }

void RememberBakedBindings(std::vector<NativeBinding> bindings) { g_baked = std::move(bindings); }

void InstallNativeKeys(rex::ui::Window* window) {
  if (!window || !NativeKeysWanted()) return;
  if (g_installed.exchange(true, std::memory_order_acq_rel)) return;
  SetProcessInputListener(&OnProcessInput);
  window->AddInputListener(&g_key_listener, 2);
  window->AddListener(&g_focus_listener);
  REXLOG_INFO("NATIVEKEYS installed (InputKey slot {})", kClientInputKeySlot);
}

}  // namespace rex::glue
