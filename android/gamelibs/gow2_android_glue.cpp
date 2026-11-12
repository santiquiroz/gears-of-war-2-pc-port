// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, Santiago Quiroz Upegui and the gow2-recomp contributors
//
// Android counterparts of the Windows-only guards that gow2_app.h installs; the desktop Linux build
// (docs/LINUX.md) compiles this file too, since it also loads librexruntime.so. The Windows versions
// rewrite x86 export thunks in rexruntime.dll or rely on SEH, so they are not built for Android:
//
// - XamInput serialization and the non-throwing UTF-8 decoder live in the SDK's Android patch
//   (sdk/patches/0007-android-arm64-port.patch, enabled on desktop Linux by 0075: InputSystem holds
//   one recursive lock across its entry points, and utf8::next for string views decodes an invalid
//   lead byte as one byte instead of throwing).
// - The disc-error box is replaced here: libgow2.so defines the runtime's own
//   __imp__XamShowDirtyDiscErrorUI, and the recompiled code and the function-mapping table bind
//   to this definition inside libgow2.so instead of the runtime's export.
// - XamInputGetState is wrapped the same way, so the gow2_test_input pad file
//   (src/gow2_test_input.h) also drives the phone.

#include "gow2_crash_watch.h"
#include "gow2_test_input.h"
#include "gow2_utf8_guard.h"
#include "gow2_xam_input_guard.h"

#include <dlfcn.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <rex/logging.h>
#include <rex/ppc/context.h>
#include <rex/ppc/func.h>

namespace {

constexpr const char* kDiscErrorExport = "__imp__XamShowDirtyDiscErrorUI";
constexpr const char* kInputStateExport = "__imp__XamInputGetState";
constexpr uint32_t kErrorDeviceNotConnected = 0x48F;

// GOW2_DISC_ERROR_UI=1 keeps the runtime's own box, as on Windows.
bool RuntimeDiscErrorUiWanted() {
  const char* gate = std::getenv("GOW2_DISC_ERROR_UI");
  return gate && std::strcmp(gate, "1") == 0;
}

PPCFunc* RuntimeExport(const char* name) {
  void* runtime = dlopen("librexruntime.so", RTLD_NOW | RTLD_NOLOAD);
  if (!runtime) {
    return nullptr;
  }
  return reinterpret_cast<PPCFunc*>(dlsym(runtime, name));
}

PPCFunc* RuntimeDiscErrorUi() {
  return RuntimeExport(kDiscErrorExport);
}

PPCFunc* RuntimeInputGetState() {
  static PPCFunc* const original = RuntimeExport(kInputStateExport);
  return original;
}

void LogMissingInputStateOnce() {
  static std::atomic<bool> logged{false};
  if (!logged.exchange(true)) {
    REXLOG_ERROR("TESTINPUT the runtime's {} export was not found; pads read as disconnected",
                 kInputStateExport);
  }
}

void LogDiscErrorSuppressedOnce() {
  static std::atomic<bool> logged{false};
  if (!logged.exchange(true)) {
    REXLOG_INFO("DISCERR the game's 'Disc Read Error' box was suppressed (no disc drive is mounted by design)");
  }
}

// The guest treats this call as one that never returns (the runtime's version shows the box and
// exits the process), and the title shuts down if it does return, so the calling thread is parked:
// the state the game runs in while the box is up. Every other thread carries on.
[[noreturn]] void ParkCallingThread() {
  for (;;) {
    pause();
  }
}

}  // namespace

extern "C" REX_FUNC(__imp__XamShowDirtyDiscErrorUI) {
  if (RuntimeDiscErrorUiWanted()) {
    if (PPCFunc* original = RuntimeDiscErrorUi()) {
      original(ctx, base);
      return;
    }
  }
  LogDiscErrorSuppressedOnce();
  ParkCallingThread();
}

// XamInputGetState(user_index r3, flags r4, XINPUT_STATE* r5); r3 holds the result afterwards.
extern "C" REX_FUNC(__imp__XamInputGetState) {
  const uint32_t user_index = ctx.r3.u32;
  const uint32_t state_addr = ctx.r5.u32;
  PPCFunc* original = RuntimeInputGetState();
  if (!original) {
    LogMissingInputStateOnce();
    ctx.r3.u64 = kErrorDeviceNotConnected;
    return;
  }
  original(ctx, base);
  uint32_t result = ctx.r3.u32;
  rex::glue::ApplyTestInput(base, user_index, state_addr, &result);
  ctx.r3.u64 = result;
}

namespace rex::glue {

void InstallUtf8Guard() {
  REXLOG_INFO("UTF8GUARD on Android and Linux the runtime decodes invalid UTF-8 one byte at a time (SDK patches)");
}

void InstallXamInputGuard() {
  REXLOG_INFO(
      "XAMINPUTGUARD on Android and Linux the runtime serializes its input system (SDK patches); "
      "gow2_test_input is merged in the game's own {}",
      kInputStateExport);
}

void InstallDiscErrorSilencer() {
  if (RuntimeDiscErrorUiWanted()) {
    REXLOG_INFO("DISCERR GOW2_DISC_ERROR_UI=1: the runtime's disc error box is kept");
    return;
  }
  REXLOG_INFO("DISCERR disc error box silenced ({} defined in the game's own code)", kDiscErrorExport);
}

void InstallCrashWatch() {
  REXLOG_INFO("CRASHWATCH not on Android or Linux: faults go to the runtime's signal handler");
}

}  // namespace rex::glue
