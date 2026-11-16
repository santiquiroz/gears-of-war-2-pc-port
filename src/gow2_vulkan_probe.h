// gow2 - ReXGlue Recompiled Project
//
// Lists the Vulkan physical devices in the loader's order, with their type.
// Used to spot an integrated GPU where D3D12's UMA answer cannot be trusted
// (vkd3d-proton reports UMA = false) and to pick a GPU on native Linux.
#pragma once

#include <string>
#include <vector>

namespace rex::glue {

struct VulkanDevice {
  std::string name;
  int type = 0;  // VkPhysicalDeviceType: 1 integrated, 2 discrete, 3 virtual, 4 cpu, 0 other
};

inline constexpr int kVulkanIntegratedGpu = 1;
inline constexpr int kVulkanDiscreteGpu = 2;

// Every physical device in the loader's order; empty when the Vulkan loader or a driver is missing.
std::vector<VulkanDevice> ListVulkanDevices();

}  // namespace rex::glue
