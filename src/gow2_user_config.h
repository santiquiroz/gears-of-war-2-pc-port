// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace rex::glue::user_config {

struct ConfigDirInputs {
  std::filesystem::path exe_dir;
  bool portable = false;
  std::filesystem::path gow2_home;
  std::filesystem::path local_app_data;
  std::filesystem::path xdg_config_home;
  std::filesystem::path home;
  bool windows = false;
};

extern const std::string_view kPlayerFileHeader;

std::filesystem::path ChooseConfigDir(const ConfigDirInputs& in);
std::string KeepPlayerChanges(std::string_view player_text, std::string_view default_text);

struct PrepareResult {
  std::filesystem::path config_path;
  bool migrated = false;
  bool created = false;
  std::string error;
};

PrepareResult PrepareConfigFile(const std::filesystem::path& config_dir,
                                const std::filesystem::path& exe_dir);
bool MigrateGamePathFile(const std::filesystem::path& config_dir, const std::filesystem::path& old_dir);

}  // namespace rex::glue::user_config