// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#pragma once

#include "gow2_update_check_core.h"

#include <filesystem>
#include <string>

namespace rex::glue::update {

struct Snapshot {
  Release release;
  std::string dismissed;
  bool checked = false;
  bool failed = false;
  bool busy = false;
};

void Start(const std::filesystem::path& user_data_root);
void CheckNow();
void Dismiss();
std::string DownloadPageUrl();
Snapshot GetSnapshot();
std::string CurrentVersion();
bool TakeNotice();

}
