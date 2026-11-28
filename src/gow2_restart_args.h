// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#pragma once

#include <string>
#include <vector>

namespace rex::glue::restart_args {

std::vector<std::string> RestartArgs(std::vector<std::string> argv, int pid);

}  // namespace rex::glue::restart_args