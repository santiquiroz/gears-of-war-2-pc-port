// gow2 - ReXGlue Recompiled Project
//
// Which GPU the game renders on. By default it is the first GPU with its own memory in the order
// Windows uses for high performance (Settings > System > Display > Graphics, per app or system-wide),
// so an integrated GPU with a large shared-memory reservation no longer wins over the graphics card.
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

#include "gow2_vulkan_probe.h"

namespace rex::glue {

struct GpuChoice {
  int index = -1;  // DXGI adapter index on Windows, Vulkan device position on Linux, -1 when nothing was chosen
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

inline std::string GpuLower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return char(std::tolower(c)); });
  return text;
}

// Case-insensitive; true when both are non-empty and one contains the other.
inline bool GpuNamesMatch(const std::string& a, const std::string& b) {
  if (a.empty() || b.empty()) return false;
  const std::string al = GpuLower(a);
  const std::string bl = GpuLower(b);
  return al.find(bl) != std::string::npos || bl.find(al) != std::string::npos;
}

// True when a Vulkan device whose name matches `name` (GpuNamesMatch) is an integrated GPU.
inline bool VulkanSaysIntegrated(const std::vector<VulkanDevice>& devices, const std::string& name) {
  for (const VulkanDevice& device : devices) {
    if (device.type == kVulkanIntegratedGpu && GpuNamesMatch(device.name, name)) return true;
  }
  return false;
}

inline GpuChoice VulkanChoiceAt(const std::vector<VulkanDevice>& devices, size_t pos, const char* reason) {
  GpuChoice choice;
  choice.index = int(pos);
  choice.name = devices[pos].name;
  choice.reason = reason;
  choice.integrated = devices[pos].type == kVulkanIntegratedGpu;
  return choice;
}

inline GpuChoice ChooseVulkanAuto(const std::vector<VulkanDevice>& devices) {
  for (size_t i = 0; i < devices.size(); ++i) {
    if (devices[i].type == kVulkanDiscreteGpu) return VulkanChoiceAt(devices, i, "first discrete GPU in Vulkan's order");
  }
  for (size_t i = 0; i < devices.size(); ++i) {
    if (devices[i].type == kVulkanIntegratedGpu) return VulkanChoiceAt(devices, i, "first integrated GPU (no discrete GPU)");
  }
  return VulkanChoiceAt(devices, 0, "first Vulkan device");
}

inline bool GpuSettingIsAuto(const std::string& setting) {
  return setting.empty() || GpuLower(setting) == "auto";
}

inline bool GpuSettingIsIndex(const std::string& setting) {
  return !setting.empty() && std::all_of(setting.begin(), setting.end(),
                                         [](unsigned char c) { return std::isdigit(c) != 0; });
}

inline int VulkanFindByName(const std::vector<VulkanDevice>& devices, const std::string& setting) {
  const std::string needle = GpuLower(setting);
  for (size_t i = 0; i < devices.size(); ++i) {
    if (GpuLower(devices[i].name).find(needle) != std::string::npos) return int(i);
  }
  return -1;
}

inline int VulkanParseIndex(const std::string& setting, size_t count) {
  size_t pos = 0;
  for (char c : setting) {
    pos = pos * 10 + size_t(c - '0');
    if (pos >= count) return -1;
  }
  return int(pos);
}

inline GpuChoice ChooseVulkanIndex(const std::vector<VulkanDevice>& devices, const std::string& setting) {
  const int pos = VulkanParseIndex(setting, devices.size());
  if (pos < 0) return {};
  return VulkanChoiceAt(devices, size_t(pos), "gow2_gpu index");
}

inline GpuChoice ChooseVulkanGpu(const std::vector<VulkanDevice>& devices, const std::string& setting) {
  if (devices.empty()) return {};
  if (GpuSettingIsAuto(setting)) return ChooseVulkanAuto(devices);
  if (GpuSettingIsIndex(setting)) {
    if (GpuChoice by_index = ChooseVulkanIndex(devices, setting); by_index.index >= 0) return by_index;
  } else if (const int found = VulkanFindByName(devices, setting); found >= 0) {
    return VulkanChoiceAt(devices, size_t(found), "gow2_gpu name");
  }
  GpuChoice fallback = ChooseVulkanAuto(devices);
  fallback.reason = "no adapter matches gow2_gpu '" + setting + "', " + fallback.reason;
  return fallback;
}

// Reads the gow2_gpu cvar: "auto", part of an adapter's name (e.g. "7800") or an adapter index.
GpuChoice ChooseRenderGpu();

// gow2_native_renderer decided against the GPU and the render target path (render_target_path_d3d12, or
// render_target_path_vulkan on Android), computed once.
bool NativeRendererActive();

struct GpuAdapterInfo {
  int index;
  std::string name;
  uint64_t dedicated_bytes;
};

// Every hardware adapter in DXGI order. Empty outside Windows.
std::vector<GpuAdapterInfo> ListRenderGpus();

}  // namespace rex::glue
