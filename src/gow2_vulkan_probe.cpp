// gow2 - ReXGlue Recompiled Project
#include "gow2_vulkan_probe.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <cstdint>
#include <cstring>

namespace rex::glue {
namespace {

// Mirrors vulkan_core.h: the SDK ships no Vulkan headers and only these few are needed.
namespace vk {
using Instance = struct Instance_T*;
using PhysicalDevice = struct PhysicalDevice_T*;
using Result = int32_t;
using VoidFunction = void (*)();
constexpr int32_t kStructureTypeApplicationInfo = 0;
constexpr int32_t kStructureTypeInstanceCreateInfo = 1;
constexpr uint32_t kApiVersion10 = 1u << 22;
constexpr size_t kDeviceTypeOffset = 16;
constexpr size_t kDeviceNameOffset = 20;
constexpr size_t kDeviceNameSize = 256;
struct ApplicationInfo {
  int32_t sType;
  const void* pNext;
  const char* pApplicationName;
  uint32_t applicationVersion;
  const char* pEngineName;
  uint32_t engineVersion;
  uint32_t apiVersion;
};
struct InstanceCreateInfo {
  int32_t sType;
  const void* pNext;
  uint32_t flags;
  const ApplicationInfo* pApplicationInfo;
  uint32_t enabledLayerCount;
  const char* const* ppEnabledLayerNames;
  uint32_t enabledExtensionCount;
  const char* const* ppEnabledExtensionNames;
};
using GetInstanceProcAddr = VoidFunction (*)(Instance, const char*);
using CreateInstance = Result (*)(const InstanceCreateInfo*, const void*, Instance*);
using DestroyInstance = void (*)(Instance, const void*);
using EnumeratePhysicalDevices = Result (*)(Instance, uint32_t*, PhysicalDevice*);
using GetPhysicalDeviceProperties = void (*)(PhysicalDevice, void*);
}  // namespace vk

template <typename Fn>
Fn VulkanProc(vk::GetInstanceProcAddr get, vk::Instance instance, const char* name) {
  return reinterpret_cast<Fn>(get(instance, name));
}

VulkanDevice VulkanDeviceAt(vk::GetPhysicalDeviceProperties get_properties, vk::PhysicalDevice device) {
  // The real VkPhysicalDeviceProperties is 824 bytes; the spare room covers a driver writing a newer layout.
  alignas(8) uint8_t properties[1024] = {};
  get_properties(device, properties);
  int32_t type = 0;
  std::memcpy(&type, properties + vk::kDeviceTypeOffset, sizeof(type));
  const char* name = reinterpret_cast<const char*>(properties + vk::kDeviceNameOffset);
  return {std::string(name, strnlen(name, vk::kDeviceNameSize)), int(type)};
}

std::vector<VulkanDevice> VulkanDevices(vk::GetInstanceProcAddr get, vk::Instance instance) {
  const auto enumerate = VulkanProc<vk::EnumeratePhysicalDevices>(get, instance, "vkEnumeratePhysicalDevices");
  const auto properties = VulkanProc<vk::GetPhysicalDeviceProperties>(get, instance, "vkGetPhysicalDeviceProperties");
  if (!enumerate || !properties) return {};
  uint32_t count = 0;
  if (enumerate(instance, &count, nullptr) < 0 || count == 0) return {};
  std::vector<vk::PhysicalDevice> devices(count);
  if (enumerate(instance, &count, devices.data()) < 0) return {};
  devices.resize(count);
  std::vector<VulkanDevice> out;
  for (const auto device : devices) out.push_back(VulkanDeviceAt(properties, device));
  return out;
}

std::vector<VulkanDevice> VulkanDevices(vk::GetInstanceProcAddr get) {
  const auto create = VulkanProc<vk::CreateInstance>(get, nullptr, "vkCreateInstance");
  if (!create) return {};
  const vk::ApplicationInfo app{vk::kStructureTypeApplicationInfo, nullptr, "gow2", 0, nullptr, 0, vk::kApiVersion10};
  const vk::InstanceCreateInfo info{vk::kStructureTypeInstanceCreateInfo, nullptr, 0, &app, 0, nullptr, 0, nullptr};
  vk::Instance instance = nullptr;
  if (create(&info, nullptr, &instance) < 0 || !instance) return {};
  auto devices = VulkanDevices(get, instance);
  if (const auto destroy = VulkanProc<vk::DestroyInstance>(get, instance, "vkDestroyInstance")) destroy(instance, nullptr);
  return devices;
}

}  // namespace

std::vector<VulkanDevice> ListVulkanDevices() {
  try {
#ifdef _WIN32
    const HMODULE loader = LoadLibraryW(L"vulkan-1.dll");
    if (!loader) return {};
    std::vector<VulkanDevice> devices;
    if (const auto get = reinterpret_cast<vk::GetInstanceProcAddr>(GetProcAddress(loader, "vkGetInstanceProcAddr"))) {
      devices = VulkanDevices(get);
    }
    FreeLibrary(loader);
    return devices;
#else
    void* loader = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!loader) loader = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!loader) return {};
    std::vector<VulkanDevice> devices;
    if (const auto get = reinterpret_cast<vk::GetInstanceProcAddr>(dlsym(loader, "vkGetInstanceProcAddr"))) {
      devices = VulkanDevices(get);
    }
    // No dlclose: the renderer opens the same loader right after, and unloading it would unload the drivers too.
    return devices;
#endif
  } catch (...) {
    return {};
  }
}

}  // namespace rex::glue
