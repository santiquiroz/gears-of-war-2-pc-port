// gow2 - ReXGlue Recompiled Project
//
// Native mouse look, see gow2_mouse_aim.h. A window listener banks captured
// mouse motion on the UI thread; a ProcessInput hook injects the banked counts
// through the client's InputAxis as MouseX/MouseY before the original runs.

#include "gow2_mouse_aim.h"
#include "gow2_host_ui.h"

#if !defined(__ANDROID__)
#include <windows.h>
#endif

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <vector>

#include <rex/cvar.h>
#include <rex/input/mnk/mnk_input_driver.h>
#include <rex/logging.h>
#include <rex/ppc.h>
#include <rex/runtime.h>
#include <rex/system/function_dispatcher.h>
#include <rex/ui/window.h>
#include <rex/ui/window_listener.h>

REXCVAR_DEFINE_BOOL(gow2_mouse_aim, true, "input",
                    "Mouse look through the game's own PC mouse axes: no stick emulation and no "
                    "aim assist for the mouse. Off: the mouse drives the right stick");
REXCVAR_DEFINE_DOUBLE(gow2_mouse_scale, 1.0, "input", "Multiplier on native mouse-look counts.")
    .range(0.05, 10);
REXCVAR_DEFINE_BOOL(gow2_mouse_invert_y, false, "input", "Invert native mouse-look Y.");
REXCVAR_DEFINE_BOOL(gow2_mouse_aim_assist, false, "input",
                    "Keep the game's aim friction/adhesion for the native mouse too.");
REXCVAR_DEFINE_BOOL(gow2_mouse_aim_log, false, "input",
                    "Log native mouse-look stats once per second while the mouse is used.");

namespace rex::glue {

namespace {

constexpr uint32_t kProcessInputFn = 0x827FC468;
constexpr uint32_t kProcessInputSlot = 0x821763D0;
constexpr uint32_t kFrictionFn = 0x828E3830;
constexpr uint32_t kFrictionFnSlot = 0x830C8504;
constexpr uint32_t kAxisFn = 0x82640560;
constexpr uint32_t kAxisFnSlot = 0x821CBE60;

constexpr uint32_t kMouseXName = 0x8313AA24;
constexpr uint32_t kMouseYName = 0x8313AA2C;
constexpr uint32_t kRightXName = 0x8313AD94;
constexpr uint32_t kRightYName = 0x8313AD9C;

constexpr uint32_t kViewportClientOff = 52;
constexpr uint32_t kClientInputAxisSlot = 24;
constexpr uint32_t kUsingGamepadBit = 0x80000000;

uint32_t LoadGuestU32(const uint8_t* base, uint32_t addr) {
  const uint8_t* p = base + addr;
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

void StoreGuestU32(uint8_t* base, uint32_t addr, uint32_t v) {
  uint8_t* p = base + addr;
  p[0] = uint8_t(v >> 24);
  p[1] = uint8_t(v >> 16);
  p[2] = uint8_t(v >> 8);
  p[3] = uint8_t(v);
}

uint64_t LoadFNameValue(const uint8_t* base, uint32_t addr) {
  return (uint64_t(LoadGuestU32(base, addr)) << 32) | LoadGuestU32(base, addr + 4);
}

bool FNameIndexIs(const uint8_t* base, uint32_t name_addr, uint32_t key) {
  const uint32_t index = LoadGuestU32(base, name_addr);
  return index != 0 && index == key;
}

bool TablePointsAt(const uint8_t* base, uint32_t slot, uint32_t fn) {
  return LoadGuestU32(base, slot) == fn;
}

int64_t SteadyTicksNow() {
  return static_cast<int64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
}

constexpr int64_t kLogWindowTicks =
    std::chrono::steady_clock::duration(std::chrono::seconds(1)).count();

std::mutex g_bank_mutex;
double g_bank_dx = 0;
double g_bank_dy = 0;

std::atomic<int64_t> g_last_mouse_tick{0};
std::atomic<int64_t> g_last_pad_tick{0};
std::atomic<uint64_t> g_friction_suppressed{0};

struct MouseCounts {
  double dx = 0;
  double dy = 0;
};

MouseCounts DrainMouseBank() {
  std::lock_guard<std::mutex> lock(g_bank_mutex);
  MouseCounts out{g_bank_dx, g_bank_dy};
  g_bank_dx = 0;
  g_bank_dy = 0;
  return out;
}

bool MouseLookedLast() {
  const int64_t mouse = g_last_mouse_tick.load(std::memory_order_relaxed);
  return mouse != 0 && mouse >= g_last_pad_tick.load(std::memory_order_relaxed);
}

class MouseBankListener : public rex::ui::WindowInputListener {
 public:
  void OnMouseMove(rex::ui::MouseEvent& e) override {
    if (HostUiOpen()) {
      DiscardNativeMouseInput();
      return;
    }
    rex::ui::Window* window = e.target();
    if (!window || !window->IsMouseCaptureRequested()) return;
    if (!rex::input::mnk::IsMouseLookActive()) return;
    if (e.dx() == 0 && e.dy() == 0) return;
    {
      std::lock_guard<std::mutex> lock(g_bank_mutex);
      if (HostUiOpen()) return;
      g_bank_dx += e.dx();
      g_bank_dy += e.dy();
    }
    g_last_mouse_tick.store(SteadyTicksNow(), std::memory_order_relaxed);
  }
};

MouseBankListener g_mouse_listener;

bool SafeInvoke(PPCFunc* fn, PPCContext* call, uint8_t* base) {
#if defined(__ANDROID__)
  // No SEH on Android: a guest fault in here reaches the runtime's signal handler instead.
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

struct AxisCall {
  bool ran = false;      // the guest call returned without a fault
  bool handled = false;  // InputAxis returned nonzero
};

struct AxisSent {
  double x = 0;
  double y = 0;
  uint32_t calls = 0;
  uint32_t handled = 0;
};

AxisCall CallClientInputAxis(PPCContext& ctx, uint8_t* base, PPCFunc* fn, uint32_t client,
                             uint32_t viewport, uint64_t key, double value, double delta_time) {
  PPCContext call = ctx;
  call.r3.u64 = client;
  call.r4.u64 = viewport;
  call.r5.u64 = 0;
  call.r6.u64 = key;
  call.r9.u64 = 0;
  call.f1.f64 = double(float(value));
  call.f2.f64 = delta_time;
  AxisCall out;
  out.ran = SafeInvoke(fn, &call, base);
  out.handled = out.ran && call.r3.u32 != 0;
  return out;
}

void CountAxisCall(const AxisCall& call, AxisSent& sent) {
  if (!call.ran) return;
  ++sent.calls;
  if (call.handled) ++sent.handled;
}

std::atomic<ProcessInputListener> g_process_input_listener{nullptr};
PPCFunc* g_process_input_orig = nullptr;
PPCFunc* g_friction_orig = nullptr;
PPCFunc* g_axis_orig = nullptr;
rex::runtime::FunctionDispatcher* g_dispatcher = nullptr;

AxisSent InjectMouseLook(PPCContext& ctx, uint8_t* base, uint32_t viewport, MouseCounts counts,
                         double delta_time) {
  AxisSent sent;
  const uint32_t client = LoadGuestU32(base, viewport + kViewportClientOff);
  if (client == 0) return sent;
  const uint64_t mouse_x = LoadFNameValue(base, kMouseXName);
  const uint64_t mouse_y = LoadFNameValue(base, kMouseYName);
  if ((mouse_x >> 32) == 0 || (mouse_y >> 32) == 0) return sent;
  const uint32_t vtable = LoadGuestU32(base, client);
  if (vtable == 0) return sent;
  const uint32_t axis_fn = LoadGuestU32(base, vtable + kClientInputAxisSlot);
  if (axis_fn == 0 || !g_dispatcher) return sent;
  PPCFunc* fn = g_dispatcher->GetFunction(axis_fn);
  if (!fn) return sent;
  const double scale = REXCVAR_GET(gow2_mouse_scale);
  const double x = counts.dx * scale;
  double y = -counts.dy * scale;
  if (REXCVAR_GET(gow2_mouse_invert_y)) y = -y;
  if (x != 0) {
    const AxisCall cx = CallClientInputAxis(ctx, base, fn, client, viewport, mouse_x, x, delta_time);
    if (cx.ran) sent.x = x;
    CountAxisCall(cx, sent);
  }
  if (y != 0) {
    const AxisCall cy = CallClientInputAxis(ctx, base, fn, client, viewport, mouse_y, y, delta_time);
    if (cy.ran) sent.y = y;
    CountAxisCall(cy, sent);
  }
  return sent;
}

void MaybeLogMouseAim(AxisSent sent) {
  if (!REXCVAR_GET(gow2_mouse_aim_log)) return;
  static double acc_x = 0;
  static double acc_y = 0;
  static uint64_t calls = 0;
  static uint64_t handled = 0;
  static uint64_t frames = 0;
  static uint64_t last_suppressed = 0;
  static int64_t window_start = 0;
  const int64_t now = SteadyTicksNow();
  if (sent.x != 0 || sent.y != 0) {
    acc_x += sent.x;
    acc_y += sent.y;
    calls += sent.calls;
    handled += sent.handled;
    ++frames;
  }
  if (window_start == 0) window_start = now;
  if (now - window_start < kLogWindowTicks) return;
  if (frames > 0) {
    const uint64_t suppressed = g_friction_suppressed.load(std::memory_order_relaxed);
    REXLOG_INFO("MOUSEAIM sent x={:.1f} y={:.1f} frames={} handled={}/{} friction_suppressed={} last={}",
                acc_x, acc_y, frames, handled, calls, suppressed - last_suppressed,
                MouseLookedLast() ? "mouse" : "pad");
    last_suppressed = suppressed;
  }
  acc_x = 0;
  acc_y = 0;
  calls = 0;
  handled = 0;
  frames = 0;
  window_start = now;
}

void ProcessInputHook(PPCContext& ctx, uint8_t* base) {
  const double delta_time = ctx.f1.f64;
  const uint32_t viewport = ctx.r3.u32;
  const MouseCounts counts = DrainMouseBank();
  AxisSent sent;
  if (!HostUiOpen() && (counts.dx != 0 || counts.dy != 0) && viewport != 0)
    sent = InjectMouseLook(ctx, base, viewport, counts, delta_time);
  MaybeLogMouseAim(sent);
  if (ProcessInputListener listener = g_process_input_listener.load(std::memory_order_acquire))
    listener(ctx, base, viewport, delta_time);
  g_process_input_orig(ctx, base);
}

void FrictionTargetHook(PPCContext& ctx, uint8_t* base) {
  const uint32_t result = ctx.r5.u32;
  g_friction_orig(ctx, base);
  if (REXCVAR_GET(gow2_mouse_aim_assist)) return;
  if (!MouseLookedLast()) return;
  if (result == 0 || LoadGuestU32(base, result) == 0) return;
  StoreGuestU32(base, result, 0);
  g_friction_suppressed.fetch_add(1, std::memory_order_relaxed);
}

void AxisAccumulateHook(PPCContext& ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32;
  const double value = ctx.f1.f64;
  const uint32_t key = self != 0 ? LoadGuestU32(base, self + 220) : 0;
  const uint32_t flags_before = self != 0 ? LoadGuestU32(base, self + 216) : 0;
  g_axis_orig(ctx, base);
  if (self == 0) return;
  if (FNameIndexIs(base, kMouseXName, key) || FNameIndexIs(base, kMouseYName, key)) {
    const uint32_t flags_now = LoadGuestU32(base, self + 216);
    const uint32_t merged = (flags_now & ~kUsingGamepadBit) | (flags_before & kUsingGamepadBit);
    if (merged != flags_now) StoreGuestU32(base, self + 216, merged);
  } else if (value != 0 && (FNameIndexIs(base, kRightXName, key) ||
                            FNameIndexIs(base, kRightYName, key))) {
    g_last_pad_tick.store(SteadyTicksNow(), std::memory_order_relaxed);
  }
}

std::atomic<bool> g_installed{false};
std::atomic<bool> g_hooks_installed{false};

}  // namespace

void SetProcessInputListener(ProcessInputListener listener) {
  g_process_input_listener.store(listener, std::memory_order_release);
}

bool NativeMouseAimInstalled() {
  return g_hooks_installed.load(std::memory_order_acquire);
}

void DiscardNativeMouseInput() {
  DrainMouseBank();
}

bool NativeMouseAimWanted() {
  if (!REXCVAR_GET(gow2_mouse_aim)) return false;
  if (!rex::cvar::Query<bool>("mnk_mouse")) return false;
  for (const rex::cvar::FlagEntry& entry : rex::cvar::GetRegistry()) {
    if (entry.name == "mnk_mouse_stick") return true;
  }
  return false;
}

void PrepareNativeMouseAim() {
  if (!NativeMouseAimWanted()) return;
  rex::cvar::SetFlagByName("mnk_mouse_stick", "false");
}

void InstallNativeMouseAim(rex::ui::Window* window) {
  if (!NativeMouseAimWanted()) return;
  if (g_installed.exchange(true, std::memory_order_acq_rel)) return;
  rex::Runtime* runtime = rex::Runtime::instance();
  rex::runtime::FunctionDispatcher* dispatcher =
      runtime ? runtime->function_dispatcher() : nullptr;
  uint8_t* base = (runtime && runtime->memory()) ? runtime->memory()->virtual_membase() : nullptr;
  const char* why = nullptr;
  if (!runtime || !dispatcher || !base || !window) {
    why = "runtime, dispatcher, memory or window missing";
  } else if (!TablePointsAt(base, kProcessInputSlot, kProcessInputFn)) {
    why = "ProcessInput table entry mismatch";
  } else if (!TablePointsAt(base, kFrictionFnSlot, kFrictionFn)) {
    why = "friction native table entry mismatch";
  } else if (!TablePointsAt(base, kAxisFnSlot, kAxisFn)) {
    why = "axis accumulate table entry mismatch";
  }
  PPCFunc* process_input = nullptr;
  PPCFunc* friction = nullptr;
  PPCFunc* axis = nullptr;
  if (!why) {
    process_input = dispatcher->GetFunction(kProcessInputFn);
    friction = dispatcher->GetFunction(kFrictionFn);
    axis = dispatcher->GetFunction(kAxisFn);
    if (!process_input || !friction || !axis) why = "guest function missing from dispatcher";
  }
  if (why) {
    rex::cvar::SetFlagByName("mnk_mouse_stick", "true");
    REXLOG_WARN("MOUSEAIM native mouse look off ({}); the mouse drives the right stick", why);
    return;
  }
  g_dispatcher = dispatcher;
  g_process_input_orig = process_input;
  g_friction_orig = friction;
  g_axis_orig = axis;
  if (!dispatcher->SetFunction(kProcessInputFn, &ProcessInputHook) ||
      !dispatcher->SetFunction(kFrictionFn, &FrictionTargetHook) ||
      !dispatcher->SetFunction(kAxisFn, &AxisAccumulateHook)) {
    dispatcher->SetFunction(kProcessInputFn, g_process_input_orig);
    dispatcher->SetFunction(kFrictionFn, g_friction_orig);
    dispatcher->SetFunction(kAxisFn, g_axis_orig);
    rex::cvar::SetFlagByName("mnk_mouse_stick", "true");
    REXLOG_WARN("MOUSEAIM native mouse look off (hook install failed); the mouse drives the right stick");
    return;
  }
  window->AddInputListener(&g_mouse_listener, 1);
  g_hooks_installed.store(true, std::memory_order_release);
  REXLOG_INFO("MOUSEAIM native mouse look on (ProcessInput, friction target, axis hooks installed)");
}

}  // namespace rex::glue
