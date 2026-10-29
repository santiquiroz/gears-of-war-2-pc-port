// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Texture packs: folders under <exe folder>\texture_packs, each a pack root holding 4D53082D\replace.
#pragma once

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace rex::glue::texture_packs {

inline bool IsPackRoot(const std::filesystem::path& dir) {
  std::error_code ec;
  return std::filesystem::is_directory(dir / "4D53082D" / "replace", ec);
}

inline std::string Utf8(const std::filesystem::path& path) {
  const std::u8string text = path.u8string();
  return std::string(text.begin(), text.end());
}

inline std::vector<std::string> PackNames(const std::filesystem::path& packs_root) {
  std::vector<std::string> names;
  std::error_code ec;
  for (std::filesystem::directory_iterator it(
           packs_root, std::filesystem::directory_options::skip_permission_denied, ec);
       !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
    std::error_code e2;
    std::string name = Utf8(it->path().filename());
    // "off" and "textures" are choices of their own; a folder with that name could never be picked.
    if (name != "off" && name != "textures" && it->is_directory(e2) && IsPackRoot(it->path())) names.push_back(name);
  }
  std::sort(names.begin(), names.end());
  return names;
}

struct PackCvars {
  std::optional<bool> replace;
  std::optional<std::string> pack_dir;
};

// The name comes from gow2.toml, which a player can edit: nothing that leaves texture_packs.
inline bool IsPlainName(std::string_view name) {
  return name.find_first_of("/\\:") == std::string_view::npos && name.find("..") == std::string_view::npos;
}

inline PackCvars ResolvePack(std::string_view choice, const std::filesystem::path& exe_dir) {
  if (choice.empty()) return {};
  if (choice == "off" || !IsPlainName(choice)) return {false, std::nullopt};
  if (choice == "textures") return {true, std::string()};
  const std::u8string name(choice.begin(), choice.end());
  return {true, Utf8(exe_dir / "texture_packs" / std::filesystem::path(name))};
}

}  // namespace rex::glue::texture_packs
