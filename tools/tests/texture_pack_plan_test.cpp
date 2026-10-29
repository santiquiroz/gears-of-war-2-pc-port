// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_texture_pack_plan.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace plan = rex::glue::texture_pack_plan;

int g_checks = 0;
int g_failures = 0;
void Expect(bool ok, std::string_view what) {
  ++g_checks;
  if (ok) return;
  ++g_failures;
  std::printf("FAIL: %.*s\n", int(what.size()), what.data());
}

void TestParseDumpName() {
  const auto parsed = plan::ParseDumpName("035e75eb6ad61933_bc1_512x512.dds");
  Expect(parsed && parsed->hash == "035e75eb6ad61933" && parsed->format == "bc1" && parsed->width == 512 &&
             parsed->height == 512,
         "a dump name parses into hash, format and size");
  const auto rgba = plan::ParseDumpName("ffffffffffffffff_rgba8_1x99999.dds");
  Expect(rgba && rgba->format == "rgba8" && rgba->width == 1 && rgba->height == 99999, "rgba8 and 5-digit sides");
  for (const char* bad : {"035E75EB6AD61933_bc1_512x512.dds", "035e75eb6ad6193_bc1_512x512.dds",
                          "035e75eb6ad619330_bc1_512x512.dds", "035e75eb6ad61933_dxt1_512x512.dds",
                          "035e75eb6ad61933_bc1_0x0.dds", "035e75eb6ad61933_bc1_0512x512.dds",
                          "035e75eb6ad61933_bc1_512x100000.dds", "..\\x.dds", "a\"b", "035e75eb6ad61933_bc1_512x512",
                          "035e75eb6ad61933_bc1_512x512.DDS", "035e75eb6ad61933_bc1_512x512.dds.png",
                          "035e75eb6ad61933_bc1_512x512 .dds", "035e75eb6ad61933_bc1_512.dds",
                          "035e75eb6ad61933-bc1_512x512.dds", ""}) {
    Expect(!plan::ParseDumpName(bad), std::string("rejected: ") + bad);
  }
}

plan::DumpName Dump(std::string format, uint32_t width, uint32_t height) {
  return {"035e75eb6ad61933", std::move(format), width, height};
}

void TestPlanTexture() {
  const auto bc1 = plan::PlanTexture(Dump("bc1", 512, 512), false, 2, false);
  Expect(bc1.step == plan::Step::kUpscale && bc1.width == 1024 && bc1.height == 1024 && bc1.dxgi == "BC1_UNORM",
         "512x512 bc1 x2 -> 1024x1024 BC1_UNORM");
  Expect(plan::PlanTexture(Dump("bc1", 512, 512), false, 2, true).dxgi == "BC7_UNORM", "high quality is BC7");
  Expect(plan::PlanTexture(Dump("bc1", 512, 512), true, 2, false).step == plan::Step::kSkip, "normal maps stay");
  Expect(plan::PlanTexture(Dump("bc1", 32, 128), false, 2, false).step == plan::Step::kSkip, "32x128 stays");
  Expect(plan::PlanTexture(Dump("bc1", 512, 16), false, 2, false).step == plan::Step::kSkip, "512x16 stays");
  const auto big = plan::PlanTexture(Dump("bc3", 2048, 2048), false, 4, false);
  Expect(big.step == plan::Step::kUpscale && big.width == 4096 && big.height == 4096 && big.dxgi == "BC3_UNORM",
         "2048x2048 x4 stops at 4096x4096");
  const auto wide = plan::PlanTexture(Dump("bc2", 2048, 512), false, 4, false);
  Expect(wide.width == 4096 && wide.height == 1024 && wide.dxgi == "BC2_UNORM", "halving keeps the aspect");
  Expect(plan::PlanTexture(Dump("bc1", 4096, 4096), false, 2, false).step == plan::Step::kSkip,
         "4096x4096 x2 is back at its own size");
  for (const char* mask : {"bc4", "bc5", "r8", "rg8"}) {
    Expect(plan::PlanTexture(Dump(mask, 512, 512), false, 2, false).step == plan::Step::kSkip,
           std::string("masks stay: ") + mask);
  }
  const auto rgba = plan::PlanTexture(Dump("rgba8", 256, 256), false, 2, false);
  Expect(rgba.step == plan::Step::kUpscale && rgba.dxgi == "R8G8B8A8_UNORM", "rgba8 x2 -> R8G8B8A8_UNORM");
}

uint16_t Rgb565(uint32_t r, uint32_t g, uint32_t b) {
  return static_cast<uint16_t>(((r * 31 + 127) / 255) << 11 | ((g * 63 + 127) / 255) << 5 | ((b * 31 + 127) / 255));
}

void AppendColorBlock(std::vector<uint8_t>& out, uint16_t color) {
  for (int endpoint = 0; endpoint < 2; ++endpoint) {
    out.push_back(static_cast<uint8_t>(color & 0xff));
    out.push_back(static_cast<uint8_t>(color >> 8));
  }
  for (int i = 0; i < 4; ++i) out.push_back(0x5a);
}

std::vector<uint8_t> Bc1Blocks(uint16_t color, int count) {
  std::vector<uint8_t> out;
  for (int i = 0; i < count; ++i) AppendColorBlock(out, color);
  return out;
}

void TestLooksLikeNormalMap() {
  const uint16_t flat = Rgb565(127, 127, 255);
  Expect(plan::LooksLikeNormalMap("bc1", Bc1Blocks(flat, 4)), "bc1 blocks of (127,127,255) are a normal map");
  Expect(!plan::LooksLikeNormalMap("bc1", Bc1Blocks(Rgb565(200, 120, 60), 4)), "bc1 blocks of (200,120,60) are not");
  std::vector<uint8_t> bc3{0xff, 0x00, 0x13, 0x77, 0xe2, 0x9c, 0x01, 0xfe};
  AppendColorBlock(bc3, flat);
  Expect(plan::LooksLikeNormalMap("bc3", bc3), "bc3 reads the color half after the noisy alpha half");
  Expect(!plan::LooksLikeNormalMap("bc5", Bc1Blocks(flat, 4)), "bc5 is never a normal map here");
  Expect(!plan::LooksLikeNormalMap("bc1", std::vector<uint8_t>{0x10, 0x84, 0x10}), "truncated data is not");
  Expect(!plan::LooksLikeNormalMap("bc3", Bc1Blocks(flat, 1)), "half a bc3 block is not");
  const std::vector<uint8_t> texels{127, 127, 250, 255, 128, 126, 252, 255};
  Expect(plan::LooksLikeNormalMap("rgba8", texels), "rgba8 averages its texels");
}

void TestDdsMip0() {
  std::vector<uint8_t> file(148, 0);
  file[0] = 'D', file[1] = 'D', file[2] = 'S', file[3] = ' ';
  file[84] = 'D', file[85] = 'X', file[86] = '1', file[87] = '0';
  const auto blocks = Bc1Blocks(Rgb565(127, 127, 255), 4);
  file.insert(file.end(), blocks.begin(), blocks.end());
  const auto dump = Dump("bc1", 8, 8);
  const auto mip0 = plan::DdsMip0(file, dump);
  Expect(mip0.size() == 32 && mip0.data() == file.data() + 148, "mip 0 follows the DX10 header");
  file.pop_back();
  Expect(plan::DdsMip0(file, dump).empty(), "a short file has no mip 0");
}

void TestPackNameAndMemory() {
  Expect(plan::BuildScale(4) == 4 && plan::BuildScale(2) == 2, "2x and 4x pass through");
  Expect(plan::BuildScale(3) == 2 && plan::BuildScale(0) == 2 && plan::BuildScale(9) == 4, "other values snap to 2x or 4x");
  Expect(plan::PackName(2, false) == "dump-2x", "dump-2x");
  Expect(plan::PackName(4, false) == "dump-4x", "dump-4x");
  Expect(plan::PackName(2, true) == "dump-2x-bc7", "dump-2x-bc7");
  Expect(plan::PackName(4, true) == "dump-4x-bc7", "dump-4x-bc7");
  Expect(plan::MemoryMultiplier(2, false) == 4 && plan::MemoryMultiplier(4, false) == 16 &&
             plan::MemoryMultiplier(4, true) == 32,
         "2x is 4 times the memory, 4x 16 times, double with BC7");
}

void TestQuoting() {
  Expect(plan::QuoteArgument("plain") == "plain", "plain arguments stay bare");
  Expect(plan::QuoteArgument("") == "\"\"", "an empty argument is quoted");
  Expect(plan::QuoteArgument("C:\\Program Files\\x") == "\"C:\\Program Files\\x\"", "spaces are quoted");
  Expect(plan::QuoteArgument("a\"b") == "\"a\\\"b\"", "an embedded quote is escaped");
  Expect(plan::QuoteArgument("C:\\my dir\\") == "\"C:\\my dir\\\\\"", "a trailing backslash is doubled");
  Expect(plan::QuoteArgument("a\\\\\"b") == "\"a\\\\\\\\\\\"b\"", "backslashes before a quote are doubled");
  Expect(plan::QuoteArgument("in\\*.dds") == "in\\*.dds", "backslashes alone stay literal");
  Expect(plan::CommandLine({"C:\\a b\\t.exe", "-o", "png"}) == "\"C:\\a b\\t.exe\" -o png", "a command line");
}

void TestVulkanIndexForName() {
  const std::vector<std::string> names = {"AMD Radeon(TM) Graphics", "AMD Radeon RX 7800 XT"};
  Expect(plan::VulkanIndexForName(names, "AMD Radeon RX 7800 XT") == 1u, "the exact name finds the card");
  Expect(plan::VulkanIndexForName(names, "rx 7800") == 1u, "part of the name matches case-insensitively");
  Expect(!plan::VulkanIndexForName(names, ""), "no game GPU name, no index");
  Expect(!plan::VulkanIndexForName(names, "NVIDIA"), "no match, no index");
  Expect(plan::VulkanIndexForName({"AMD Radeon RX 7800 XT (2)", "AMD Radeon RX 7800 XT"}, "AMD Radeon RX 7800 XT") ==
             1u,
         "an exact match wins over a containing name");
}

void TestVulkanFailureLine() {
  Expect(plan::VulkanFailureLine("[0 AMD Radeon RX 7800 XT]  queueC=1[8]\nvkAllocateMemory failed -2\n") ==
             std::string("vkAllocateMemory failed -2"),
         "vkAllocateMemory failed is found and trimmed");
  Expect(!plan::VulkanFailureLine("[0 AMD Radeon RX 7800 XT]  queueC=1[8]\npng4\\a.png done\n"),
         "a clean log has no failure");
  Expect(plan::VulkanFailureLine("vkQueueSubmit FAILED") == std::string("vkQueueSubmit FAILED"),
         "the match ignores case");
}

int main() {
  TestParseDumpName();
  TestPlanTexture();
  TestLooksLikeNormalMap();
  TestDdsMip0();
  TestPackNameAndMemory();
  TestQuoting();
  TestVulkanIndexForName();
  TestVulkanFailureLine();
  std::printf("texture_pack_plan_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
