// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_config_seed.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void Expect(bool ok, const char* what) {
  if (ok) return;
  ++g_failures;
  std::fprintf(stderr, "FAIL: %s\n", what);
}

void WriteFile(const fs::path& path, const std::string& content) {
  std::ofstream file(path, std::ios::binary);
  file << content;
}

std::string ReadFile(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void TestConfigSeed(const fs::path& root) {
  const auto seed = root / "seed";
  fs::create_directories(seed);
  const auto config = seed / "gow2.toml";
  WriteFile(seed / "gow2.default.toml", "default = true\n");
  Expect(rex::glue::SeedConfigFromDefault(config), "missing config copies default");
  Expect(ReadFile(config) == "default = true\n", "seeded config has default content");

  const auto existing = root / "existing";
  fs::create_directories(existing);
  WriteFile(existing / "gow2.default.toml", "default = true\n");
  WriteFile(existing / "gow2.toml", "mine");
  Expect(!rex::glue::SeedConfigFromDefault(existing / "gow2.toml"), "existing config is not replaced");
  Expect(ReadFile(existing / "gow2.toml") == "mine", "existing config content stays mine");

  const auto empty = root / "empty";
  fs::create_directories(empty);
  const auto missing = empty / "gow2.toml";
  Expect(!rex::glue::SeedConfigFromDefault(missing), "missing default does not seed config");
  Expect(!fs::exists(missing), "missing config stays absent without default");
  Expect(!rex::glue::SeedConfigFromDefault({}), "empty config path is rejected");
}

}  // namespace

int main() {
  const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto root = fs::temp_directory_path() / ("gow2-config-seed-test-" + std::to_string(nonce));
  std::error_code error;
  fs::create_directories(root, error);
  if (error) {
    std::fprintf(stderr, "FAIL: create temporary directory: %s\n", error.message().c_str());
    return 1;
  }
  TestConfigSeed(root);
  fs::remove_all(root, error);
  if (error) {
    std::fprintf(stderr, "FAIL: remove temporary directory: %s\n", error.message().c_str());
    return 1;
  }
  std::printf("config_seed_test: %d failures\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}
