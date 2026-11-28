// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#pragma once

#include "gow2_update_check_core.h"

#include <filesystem>
#include <optional>
#include <string>

namespace rex::glue::update_install {

enum class InstallState { kIdle, kDownloading, kVerifying, kExtracting, kReady, kFailed };

struct InstallSnapshot {
  InstallState state = InstallState::kIdle;
  std::string tag;
  int percent = -1;
  std::string error;
};

std::filesystem::path InstallDir();
void StartInstall(const update::Release& release);
InstallSnapshot GetInstallSnapshot();
bool ApplyStagedUpdate(std::string* error);
std::optional<std::string> ReadyStagedTag();
void CleanupAfterUpdate();

}  // namespace rex::glue::update_install