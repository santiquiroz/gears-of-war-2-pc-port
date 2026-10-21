// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, Santiago Quiroz Upegui and the gow2-recomp contributors
//
// Custom Vulkan driver (for example Turnip) through libadrenotools, chosen in the launch options.
// The app extracts the driver into its files folder, copies the adrenotools hook libraries beside
// it and names both in the environment before the game starts (GpuDriver.java). Without them, or
// when the driver does not load, the game uses the phone's own driver.

#include <dlfcn.h>

#include <cstdlib>

#include <adrenotools/driver.h>

#include <rex/logging.h>
#include <rex/ui/vulkan/instance.h>

namespace {

const char* Env(const char* name) {
  const char* value = std::getenv(name);
  return value != nullptr && *value != '\0' ? value : nullptr;
}

void* OpenCustomDriver() {
  const char* driver_dir = Env("GOW2_GPU_DRIVER_DIR");
  const char* driver_name = Env("GOW2_GPU_DRIVER_NAME");
  const char* hook_dir = Env("GOW2_GPU_HOOK_DIR");
  if (driver_dir == nullptr || driver_name == nullptr || hook_dir == nullptr) {
    return nullptr;
  }
  void* loader = adrenotools_open_libvulkan(RTLD_NOW, ADRENOTOOLS_DRIVER_CUSTOM, nullptr, hook_dir,
                                            driver_dir, driver_name, nullptr, nullptr);
  if (loader == nullptr) {
    REXLOG_ERROR("GPUDRIVER the custom Vulkan driver {}{} did not load; using the phone's driver",
                 driver_dir, driver_name);
    return nullptr;
  }
  REXLOG_INFO("GPUDRIVER custom Vulkan driver {}{}", driver_dir, driver_name);
  return loader;
}

[[maybe_unused]] const bool kOpenerRegistered =
    (rex::ui::vulkan::VulkanInstance::SetLoaderOpener(OpenCustomDriver), true);

}  // namespace
