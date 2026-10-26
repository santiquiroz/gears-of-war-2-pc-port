// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// First-launch install from the player's own disc image: the pure checks live here so a host
// test can run them (src/gow2_disc_install.cpp does the Windows extraction).
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace rex::glue::disc_install {

// A folder with only default.xex (like the repo's codegen input) boots to a black screen.
inline bool IsDiscFolder(const std::filesystem::path& dir) {
  std::error_code ec;
  return std::filesystem::exists(dir / "default.xex", ec) &&
         std::filesystem::is_directory(dir / "GearGame", ec);
}

enum class PickKind { kIso, kGameExecutable, kOther };

inline bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    char x = a[i], y = b[i];
    if (x >= 'A' && x <= 'Z') x = char(x - 'A' + 'a');
    if (y >= 'A' && y <= 'Z') y = char(y - 'A' + 'a');
    if (x != y) return false;
  }
  return true;
}

inline bool PathEqualsIgnoreCase(const std::filesystem::path& a, std::string_view b) {
  const std::wstring wide = a.wstring();
  if (wide.size() != b.size()) return false;
  std::string narrow(wide.size(), '\0');
  for (size_t i = 0; i < wide.size(); ++i) {
    if (wide[i] > 0x7f) return false;
    narrow[i] = static_cast<char>(wide[i]);
  }
  return EqualsIgnoreCase(narrow, b);
}

inline PickKind ClassifyPick(const std::filesystem::path& picked) {
  if (PathEqualsIgnoreCase(picked.extension(), ".iso")) return PickKind::kIso;
  if (PathEqualsIgnoreCase(picked.filename(), "default.xex")) return PickKind::kGameExecutable;
  return PickKind::kOther;
}

inline bool IsReservedDeviceName(std::string_view name) {
  const std::string_view stem = name.substr(0, name.find('.'));
  for (const char* device : {"CON", "PRN", "AUX", "NUL"}) {
    if (EqualsIgnoreCase(stem, device)) return true;
  }
  if (stem.size() != 4 || stem[3] < '1' || stem[3] > '9') return false;
  return EqualsIgnoreCase(stem.substr(0, 3), "COM") || EqualsIgnoreCase(stem.substr(0, 3), "LPT");
}

// Disc entry names are untrusted: this blocks path traversal and names Windows cannot create.
inline bool IsSafeEntryName(std::string_view name) {
  if (name.empty() || name == "." || name == "..") return false;
  // Windows strips a trailing dot or space, so two entries could land on one file.
  if (name.back() == '.' || name.back() == ' ') return false;
  if (IsReservedDeviceName(name)) return false;
  for (const char c : name) {
    if (static_cast<unsigned char>(c) < 0x20) return false;
    if (std::string_view("<>:\"/\\|?*").find(c) != std::string_view::npos) return false;
  }
  return true;
}

constexpr uint64_t kFreeSpaceMargin = 256ull << 20;

inline uint64_t RequiredFreeBytes(uint64_t total_file_bytes) {
  return total_file_bytes + kFreeSpaceMargin;
}

inline constexpr char kInstallFolderName[] = "game";
inline constexpr char kStagingFolderName[] = "game.partial";

#ifdef _WIN32
struct InstallResult {
  std::filesystem::path game_folder;
  std::wstring error;
  bool cancelled = false;
};

InstallResult InstallFromIso(const std::filesystem::path& iso, const std::filesystem::path& app_home);
#endif

}  // namespace rex::glue::disc_install
