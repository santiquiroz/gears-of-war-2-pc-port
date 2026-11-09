// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Pure text conversions for PC Options values.
#pragma once

#include "gow2_options_actions.h"

#include <optional>
#include <string>
#include <string_view>

namespace rex::glue::options::values_text {

struct Pair {
  std::string first;
  std::string second;
};

std::optional<Pair> ParseWindowSize(std::string_view value);
std::optional<Pair> ParseTextureMemory(std::string_view value);
std::string FloatText(std::string_view value);
std::string WindowMode(std::string_view fullscreen, std::string_view exclusive);
std::string WindowSize(std::string_view width, std::string_view height);
std::string UpscaleRatio(std::string_view scale, std::string_view target_w,
                         std::string_view target_h);
std::string LeaseTime(std::string_view lease_end);
std::string HostResultText(const options_actions::HostResult& result, bool spanish,
                           std::string_view lease_time);

}
