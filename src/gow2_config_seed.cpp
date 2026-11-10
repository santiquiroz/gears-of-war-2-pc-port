// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_config_seed.h"

#include <system_error>

namespace rex::glue {

bool SeedConfigFromDefault(const std::filesystem::path& config_path) {
  if (config_path.empty()) return false;

  std::error_code error;
  if (std::filesystem::exists(config_path, error) || error) return false;

  const auto default_path = config_path.parent_path() / "gow2.default.toml";
  if (!std::filesystem::exists(default_path, error) || error) return false;
  const bool copied = std::filesystem::copy_file(default_path, config_path, error);
  return copied && !error;
}

}  // namespace rex::glue
