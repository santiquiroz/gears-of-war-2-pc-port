// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_restart_args.h"

#include <cstddef>
#include <string_view>
#include <utility>

namespace rex::glue::restart_args {
namespace {

constexpr std::string_view kWaitPidPrefix = "--gow2_wait_pid=";

}  // namespace

std::vector<std::string> RestartArgs(std::vector<std::string> argv, int pid) {
  std::vector<std::string> result;
  result.reserve(argv.size() + 1);
  if (!argv.empty()) {
    result.push_back(std::move(argv.front()));
    for (std::size_t i = 1; i < argv.size(); ++i) {
      if (!std::string_view(argv[i]).starts_with(kWaitPidPrefix)) result.push_back(std::move(argv[i]));
    }
  }
  result.push_back(std::string(kWaitPidPrefix) + std::to_string(pid));
  return result;
}

}  // namespace rex::glue::restart_args