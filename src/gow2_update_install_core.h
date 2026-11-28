// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rex::glue::update_install {

std::optional<std::string> ParseChecksum(std::string_view sidecar, std::string_view archive_name);
bool SafeRelativePath(std::string_view relative);
std::string StageFolderName(std::string_view tag);
std::filesystem::path FindPayloadRoot(const std::filesystem::path& extracted);

struct Replacement {
  std::filesystem::path staged;
  std::filesystem::path target;
  std::filesystem::path backup;
};

struct Plan {
  std::vector<Replacement> items;
  std::string error;
};

Plan PlanApply(const std::filesystem::path& payload_root,
               const std::filesystem::path& install_dir);

struct ApplyResult {
  bool ok = false;
  std::string error;
  std::vector<std::filesystem::path> backups;
};

ApplyResult ApplyPlan(const Plan& plan);

bool WriteCleanupList(const std::filesystem::path& list_file,
                      const std::vector<std::filesystem::path>& backups);
std::vector<std::filesystem::path> ReadCleanupList(const std::filesystem::path& list_file);
void RunCleanup(const std::filesystem::path& list_file);

}  // namespace rex::glue::update_install