// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace rex::glue::process {

struct Result {
  bool started = false;
  int exit_code = -1;
  std::string output;
  bool output_truncated = false;
  std::string error;
};

Result Run(const std::vector<std::string>& argv_utf8, size_t max_output = 0);
bool StartDetached(const std::vector<std::string>& argv_utf8);
std::string QuoteWindowsArgument(std::string_view argument);

}  // namespace rex::glue::process