// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_texture_packs.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
namespace packs = rex::glue::texture_packs;

int g_checks = 0;
int g_failures = 0;
void Expect(bool ok, std::string_view what) {
  ++g_checks;
  if (ok) return;
  ++g_failures;
  std::printf("FAIL: %.*s\n", int(what.size()), what.data());
}

void ExpectOff(std::string_view choice, std::string_view what) {
  const auto pack = packs::ResolvePack(choice, "C:\\game");
  Expect(pack.replace == false && !pack.pack_dir, what);
}

void TestPackNames(const fs::path& root) {
  fs::create_directories(root / "a" / "4D53082D" / "replace");
  fs::create_directories(root / "b" / "4D53082D" / "replace");
  fs::create_directories(root / "c");
  fs::create_directories(root / "off" / "4D53082D" / "replace");
  fs::create_directories(root / "textures" / "4D53082D" / "replace");
  std::ofstream(root / "d.txt") << "not a pack";
  Expect(packs::PackNames(root) == std::vector<std::string>{"a", "b"}, "only folders with 4D53082D/replace, sorted, without the reserved off and textures");
  Expect(packs::PackNames(root / "missing").empty(), "a missing packs folder has no packs");
}

void TestResolvePack() {
  const fs::path exe_dir = "C:\\game";
  const auto keep = packs::ResolvePack("", exe_dir);
  Expect(!keep.replace && !keep.pack_dir, "empty keeps texture_replace and texture_pack_dir");
  ExpectOff("off", "off turns texture replacement off");
  const auto old = packs::ResolvePack("textures", exe_dir);
  Expect(old.replace == true && old.pack_dir == std::string(), "textures uses the old folder next to the exe");
  const auto named = packs::ResolvePack("a", exe_dir);
  Expect(named.replace == true && named.pack_dir == packs::Utf8(exe_dir / "texture_packs" / "a"),
         "a pack name points under texture_packs");
  ExpectOff("../x", "a name with .. is rejected");
  ExpectOff("a\\b", "a name with a backslash is rejected");
  ExpectOff("C:x", "a name with a colon is rejected");
  ExpectOff("a/b", "a name with a slash is rejected");
}

int main() {
  const fs::path root = fs::temp_directory_path() / "gow2_texture_packs_test";
  std::error_code ec;
  fs::remove_all(root, ec);
  try {
    TestPackNames(root);
    TestResolvePack();
  } catch (const std::exception& e) {
    ++g_checks;
    ++g_failures;
    std::printf("FAIL: exception: %s\n", e.what());
  }
  fs::remove_all(root, ec);
  std::printf("texture_packs_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
