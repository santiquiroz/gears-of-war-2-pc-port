// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Voice language: which _LOC_ voice packages to serve under the names the game opens, and the TOC
// lines that go with them. Pure standard C++ so the host test builds alone.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rex::glue::voice_language {

struct TocEntry {
  std::string size, uncompressed, third, path, md5;
};

std::optional<TocEntry> ParseTocLine(std::string_view line);
std::string FormatTocLine(const TocEntry& entry);

std::string TextLanguageCode(uint32_t user_language);
std::string VoiceChoiceCode(std::string_view choice);

struct TocFile {
  std::string name;
  std::string text;
};

struct FileSwap {
  std::string guest;
  std::string source;
};

struct VoicePlan {
  std::vector<FileSwap> swaps;
  std::vector<TocFile> edited_tocs;
  std::string summary;
};

VoicePlan PlanVoiceSwap(const std::vector<TocFile>& tocs, std::string_view text_code, std::string_view voice_code);

}  // namespace rex::glue::voice_language
