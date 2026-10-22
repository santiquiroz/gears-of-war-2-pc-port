// gow2 - ReXGlue Recompiled Project
//
// Which GPU the game renders on. By default it is the first GPU with its own memory in the order
// Windows uses for high performance (Settings > System > Display > Graphics, per app or system-wide),
// so an integrated GPU with a large shared-memory reservation no longer wins over the graphics card.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rex::glue {

struct GpuChoice {
  int index = -1;     // DXGI adapter index for d3d12_adapter, -1 when nothing was chosen
  std::string name;
  std::string reason;
  bool integrated = false;  // true when the chosen adapter shares memory with the CPU
};

// Whether an adapter looks integrated: the driver's UMA answer wins when it is known,
// otherwise under 512 MiB of dedicated memory counts as integrated (a shared-memory
// reservation can inflate that number, so UMA wins when known).
inline constexpr bool LooksIntegrated(bool uma_known, bool uma, uint64_t dedicated_bytes) {
  if (uma_known) return uma;
  return dedicated_bytes < 512ULL * 1024 * 1024;
}

// Reads the gow2_gpu cvar: "auto", part of an adapter's name (e.g. "7800") or an adapter index.
GpuChoice ChooseRenderGpu();

// gow2_native_renderer decided against the GPU and render_target_path_d3d12, computed once.
bool NativeRendererActive();

struct GpuAdapterInfo {
  int index;
  std::string name;
  uint64_t dedicated_bytes;
};

// Every hardware adapter in DXGI order. Empty outside Windows.
std::vector<GpuAdapterInfo> ListRenderGpus();

}  // namespace rex::glue
