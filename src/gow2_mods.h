// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "gow2_sha_table.h"
#include "gow2_voice_language.h"

namespace rex::glue::mods {

struct Mod {
  std::string folder, name, author, version, description, url;
  bool textures = false;
};

struct File {
  std::string mod_folder, relative_path, source;
  uint64_t size = 0, uncompressed = 0, modified = 0;
  std::string md5;
  std::array<uint8_t, 20> sha1{};
  bool ignored = false;
  std::string warning;
};

struct Plan {
  std::vector<voice_language::TocFile> edited_tocs;
  std::vector<std::string> override_lines, texture_folders, warnings;
  std::vector<std::pair<std::string, const File*>> winners;
};

std::vector<Mod> FindMods(const std::filesystem::path& root);
std::vector<File> FindFiles(const std::filesystem::path& root, const std::vector<Mod>& mods);

Plan BuildPlan(const std::vector<Mod>& mods, const std::vector<File>& files,
               const std::vector<voice_language::TocFile>& tocs);

struct Setup {
  std::filesystem::path overrides_file;
  std::vector<HashSecEntry> hashes;
  std::vector<std::string> texture_folders, log_lines, warnings;
  bool active = false;
};

Setup Prepare(const std::filesystem::path& game_root, const std::filesystem::path& cache_root,
              const std::filesystem::path& mods_root,
              const std::filesystem::path& voice_overrides_file);

}  // namespace rex::glue::mods
