// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Host-side test of the integrated-GPU decision (no game data, no SDK).
#include "gow2_gpu_pick.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

int g_checks = 0;
int g_failures = 0;
void Expect(bool ok, std::string_view what) { ++g_checks; if (ok) return; ++g_failures; std::printf("FAIL: %.*s\n", int(what.size()), what.data()); }

int main() {
  constexpr uint64_t kMiB = 1024ULL * 1024;
  Expect(rex::glue::LooksIntegrated(true, true, 16ULL * 1024 * kMiB), "known UMA wins over a big shared-memory reservation");
  Expect(!rex::glue::LooksIntegrated(true, false, 0), "known non-UMA is dedicated even with no dedicated memory");
  Expect(rex::glue::LooksIntegrated(false, false, 511 * kMiB), "unknown UMA under 512 MiB is integrated");
  Expect(rex::glue::LooksIntegrated(false, true, 511 * kMiB), "UMA flag is ignored when unknown");
  Expect(!rex::glue::LooksIntegrated(false, false, 512 * kMiB), "unknown UMA at exactly 512 MiB is dedicated");
  Expect(!rex::glue::LooksIntegrated(false, false, 8ULL * 1024 * kMiB), "unknown UMA with gigabytes is dedicated");
  Expect(!rex::glue::GpuChoice{}.integrated, "a fresh GpuChoice is not integrated");

  Expect(rex::glue::GpuNamesMatch("AMD Ryzen Z1 Extreme (RADV PHOENIX)", "amd ryzen z1 extreme (radv phoenix)"),
         "GpuNamesMatch ignores case");
  Expect(rex::glue::GpuNamesMatch("Radeon RX 7800 XT", "AMD Radeon RX 7800 XT (RADV NAVI32)"),
         "GpuNamesMatch matches when one name contains the other");
  Expect(!rex::glue::GpuNamesMatch("", "x"), "GpuNamesMatch is false when a name is empty");
  Expect(!rex::glue::GpuNamesMatch("NVIDIA", "AMD"), "GpuNamesMatch is false for unrelated names");

  const std::vector<rex::glue::VulkanDevice> z1 = {{"AMD Ryzen Z1 Extreme (RADV PHOENIX)", 1}};
  Expect(rex::glue::VulkanSaysIntegrated(z1, "AMD Ryzen Z1 Extreme (RADV PHOENIX)"),
         "VulkanSaysIntegrated is true for a matching integrated GPU");
  const std::vector<rex::glue::VulkanDevice> z1_discrete = {{"AMD Ryzen Z1 Extreme (RADV PHOENIX)", 2}};
  Expect(!rex::glue::VulkanSaysIntegrated(z1_discrete, "AMD Ryzen Z1 Extreme (RADV PHOENIX)"),
         "VulkanSaysIntegrated is false for a matching discrete GPU");
  Expect(!rex::glue::VulkanSaysIntegrated(z1, "NVIDIA GeForce RTX 4060"),
         "VulkanSaysIntegrated is false without a name match");
  Expect(!rex::glue::VulkanSaysIntegrated({}, "AMD Ryzen Z1 Extreme (RADV PHOENIX)"),
         "VulkanSaysIntegrated is false for an empty device list");

  Expect(!rex::glue::HasArmGpu({}), "HasArmGpu is false for an empty device list");
  Expect(!rex::glue::HasArmGpu({{"Adreno (TM) 740", 2, 0x5143}}), "HasArmGpu is false for an Adreno GPU");
  Expect(rex::glue::HasArmGpu({{"Mali-G720 MC7", 2, 0x13B5}}), "HasArmGpu is true for a Mali GPU");

  const std::vector<rex::glue::VulkanDevice> mixed = {
      {"llvmpipe", 4}, {"AMD Radeon 780M", 1}, {"NVIDIA GeForce RTX 4060", 2}};
  const rex::glue::GpuChoice auto_discrete = rex::glue::ChooseVulkanGpu(mixed, "auto");
  Expect(auto_discrete.index == 2 && !auto_discrete.integrated &&
             auto_discrete.reason == "first discrete GPU in Vulkan's order",
         "ChooseVulkanGpu auto picks the first discrete GPU");
  const std::vector<rex::glue::VulkanDevice> no_discrete = {
      {"llvmpipe", 4}, {"AMD Ryzen Z1 Extreme (RADV PHOENIX)", 1}};
  const rex::glue::GpuChoice auto_integrated = rex::glue::ChooseVulkanGpu(no_discrete, "auto");
  Expect(auto_integrated.index == 1 && auto_integrated.integrated,
         "ChooseVulkanGpu auto falls back to the first integrated GPU");
  const rex::glue::GpuChoice auto_single = rex::glue::ChooseVulkanGpu({{"llvmpipe", 4}}, "");
  Expect(auto_single.index == 0, "ChooseVulkanGpu with an empty setting picks device 0 when alone");
  Expect(rex::glue::ChooseVulkanGpu({}, "auto").index == -1, "ChooseVulkanGpu picks nothing without devices");
  const rex::glue::GpuChoice by_index = rex::glue::ChooseVulkanGpu(mixed, "1");
  Expect(by_index.index == 1 && by_index.reason == "gow2_gpu index", "ChooseVulkanGpu picks a device position");
  const rex::glue::GpuChoice by_name = rex::glue::ChooseVulkanGpu(mixed, "780m");
  Expect(by_name.index == 1 && by_name.reason == "gow2_gpu name", "ChooseVulkanGpu matches part of a name");
  const rex::glue::GpuChoice no_match = rex::glue::ChooseVulkanGpu(mixed, "intel arc");
  Expect(no_match.index == 2 &&
             no_match.reason.rfind("no adapter matches gow2_gpu 'intel arc', ", 0) == 0,
         "ChooseVulkanGpu falls back to auto without a match");

  std::printf("gpu_pick_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
