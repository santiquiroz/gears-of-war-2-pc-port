// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Voice language at boot: reads the disc's TOCs, writes the edited copies and the VFS overrides file.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace rex::glue {

struct VoiceLanguageSetup {
  std::filesystem::path overrides_file;
  std::string summary;
  bool text_missing = false;
  bool failed = false;
};

VoiceLanguageSetup PrepareVoiceLanguage(const std::filesystem::path& game_root, const std::filesystem::path& cache_dir,
                                        uint32_t user_language, std::string_view voice_choice);

}  // namespace rex::glue
