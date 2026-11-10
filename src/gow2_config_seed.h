// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#pragma once

#include <filesystem>

namespace rex::glue {

bool SeedConfigFromDefault(const std::filesystem::path& config_path);

}  // namespace rex::glue
