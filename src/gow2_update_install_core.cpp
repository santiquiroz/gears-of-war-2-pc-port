// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_update_install_core.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <system_error>
#include <utility>

namespace rex::glue::update_install {

namespace {

bool IsBlank(char value) { return value == ' ' || value == '\t'; }

std::string_view Trim(std::string_view text) {
  while (!text.empty() && IsBlank(text.front())) text.remove_prefix(1);
  while (!text.empty() && IsBlank(text.back())) text.remove_suffix(1);
  return text;
}

int HexValue(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

bool IsSafeTagCharacter(char value) {
  return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
      (value >= '0' && value <= '9') || value == '.' || value == '_' || value == '-';
}

bool EntryExists(const std::filesystem::path& path, std::error_code* error) {
  const auto status = std::filesystem::symlink_status(path, *error);
  if (*error == std::errc::no_such_file_or_directory || *error == std::errc::not_a_directory) {
    error->clear();
    return false;
  }
  return !*error && status.type() != std::filesystem::file_type::not_found;
}

std::string PathText(const std::filesystem::path& path) {
  const std::u8string text = path.u8string();
  return {text.begin(), text.end()};
}

std::string FailureText(const std::filesystem::path& path, const std::error_code& error) {
  return PathText(path) + ": " + error.message();
}

struct AppliedReplacement {
  const Replacement* replacement = nullptr;
  bool backup_made = false;
  bool staged_moved = false;
};

void RollBack(const std::vector<AppliedReplacement>& applied) {
  for (auto it = applied.rbegin(); it != applied.rend(); ++it) {
    std::error_code error;
    if (it->staged_moved) {
      std::filesystem::rename(it->replacement->target, it->replacement->staged, error);
    }
    if (it->backup_made) {
      error.clear();
      std::filesystem::rename(it->replacement->backup, it->replacement->target, error);
    }
  }
}

void ClearFailedResult(ApplyResult* result, const std::vector<AppliedReplacement>& applied) {
  RollBack(applied);
  result->ok = false;
  result->backups.clear();
}

bool CreateTargetParents(const Replacement& replacement, std::filesystem::path* failing_path,
                         std::error_code* error) {
  *failing_path = replacement.target.parent_path();
  if (!failing_path->empty()) std::filesystem::create_directories(*failing_path, *error);
  return !*error;
}

bool RemoveStaleBackup(const Replacement& replacement, std::filesystem::path* failing_path,
                       std::error_code* error) {
  *failing_path = replacement.backup;
  if (EntryExists(replacement.backup, error)) std::filesystem::remove(replacement.backup, *error);
  return !*error;
}

bool MoveTargetToBackup(const Replacement& replacement, AppliedReplacement* applied,
                        std::filesystem::path* failing_path, std::error_code* error) {
  *failing_path = replacement.target;
  if (!EntryExists(replacement.target, error)) return !*error;
  std::filesystem::rename(replacement.target, replacement.backup, *error);
  if (*error) return false;
  applied->backup_made = true;
  return true;
}

bool MoveStagedToTarget(const Replacement& replacement, AppliedReplacement* applied,
                        std::filesystem::path* failing_path, std::error_code* error) {
  *failing_path = replacement.staged;
  std::filesystem::rename(replacement.staged, replacement.target, *error);
  if (*error) return false;
  applied->staged_moved = true;
  return true;
}

bool ApplyReplacement(const Replacement& replacement, AppliedReplacement* applied,
                      std::filesystem::path* failing_path, std::error_code* error) {
  return CreateTargetParents(replacement, failing_path, error) &&
      RemoveStaleBackup(replacement, failing_path, error) &&
      MoveTargetToBackup(replacement, applied, failing_path, error) &&
      MoveStagedToTarget(replacement, applied, failing_path, error);
}

std::filesystem::path BackupPath(const std::filesystem::path& target) {
  std::filesystem::path backup = target;
  backup += ".gow2old";
  return backup;
}

Plan FailedPlan(std::string error) {
  return {{}, std::move(error)};
}

std::optional<std::filesystem::path> UniqueCleanupPath(const std::filesystem::path& list_file,
                                                       std::string_view suffix) {
  static std::atomic_uint64_t sequence{0};
  const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
  for (int attempt = 0; attempt < 128; ++attempt) {
    std::filesystem::path candidate = list_file;
    candidate += ".";
    candidate += suffix;
    candidate += "." + std::to_string(timestamp) + "." +
        std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
    std::error_code error;
    if (!EntryExists(candidate, &error) && !error) return candidate;
  }
  return std::nullopt;
}

bool WriteCleanupContents(const std::filesystem::path& path,
                          const std::vector<std::filesystem::path>& backups) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) return false;
  for (const auto& backup : backups) {
    const std::u8string text = backup.u8string();
    file.write(reinterpret_cast<const char*>(text.data()), static_cast<std::streamsize>(text.size()));
    file.put('\n');
    if (!file) return false;
  }
  file.flush();
  if (!file) return false;
  file.close();
  return !file.fail();
}

bool ReplaceCleanupList(const std::filesystem::path& temporary,
                        const std::filesystem::path& list_file) {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(list_file, error);
  if (error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory) {
    error.clear();
  } else if (error) {
    return false;
  } else if (status.type() != std::filesystem::file_type::not_found &&
             status.type() != std::filesystem::file_type::regular) {
    return false;
  }

  std::filesystem::rename(temporary, list_file, error);
  if (!error) return true;

  const auto old_list = UniqueCleanupPath(list_file, "gow2-list-old");
  if (!old_list) return false;
  error.clear();
  std::filesystem::rename(list_file, *old_list, error);
  if (error) return false;

  std::filesystem::rename(temporary, list_file, error);
  if (error) {
    std::error_code restore_error;
    std::filesystem::rename(*old_list, list_file, restore_error);
    return false;
  }

  std::error_code remove_error;
  std::filesystem::remove(*old_list, remove_error);
  return true;
}

bool HasBackupSuffix(const std::filesystem::path& path) {
  return path.filename().u8string().ends_with(u8".gow2old");
}

bool RemoveListedBackup(const std::filesystem::path& path) {
  if (!HasBackupSuffix(path)) return false;
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  if (error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory) return true;
  if (error) return false;
  if (status.type() == std::filesystem::file_type::not_found) return true;
  if (status.type() != std::filesystem::file_type::regular) return false;
  std::filesystem::remove(path, error);
  return !error;
}

std::vector<std::filesystem::path> RemoveListedBackups(
    const std::vector<std::filesystem::path>& paths) {
  std::vector<std::filesystem::path> remaining;
  for (const auto& path : paths) {
    if (!RemoveListedBackup(path)) remaining.push_back(path);
  }
  return remaining;
}

}  // namespace

bool IsHttpsUrl(std::string_view url) {
  constexpr std::string_view prefix = "https://";
  if (!url.starts_with(prefix)) return false;
  url.remove_prefix(prefix.size());
  const size_t end = url.find_first_of("/?#");
  std::string_view authority = url.substr(0, end);
  if (authority.empty()) return false;
  for (const char value : authority) {
    if (value <= ' ' || value == '\\') return false;
  }
  const size_t user = authority.rfind('@');
  if (user != std::string_view::npos) authority.remove_prefix(user + 1);
  if (authority.empty()) return false;
  if (authority.front() == '[') {
    const size_t close = authority.find(']');
    return close > 1 && close != std::string_view::npos &&
        (close + 1 == authority.size() || authority[close + 1] == ':');
  }
  const size_t port = authority.find(':');
  return !authority.substr(0, port).empty();
}

std::optional<std::string> ArchiveNameFromUrl(std::string_view url) {
  if (!IsHttpsUrl(url)) return std::nullopt;
  url.remove_prefix(std::string_view("https://").size());
  const size_t path = url.find_first_of("/?#");
  if (path == std::string_view::npos || url[path] != '/') return std::nullopt;
  const size_t suffix = url.find_first_of("?#", path);
  if (suffix != std::string_view::npos) url = url.substr(0, suffix);
  const size_t slash = url.rfind('/');
  const std::string_view name = url.substr(slash + 1);
  if (name.empty() || name == "." || name == ".." ||
      name.find_first_of("\\:") != std::string_view::npos) {
    return std::nullopt;
  }
  return std::string(name);
}

int DownloadPercent(uint64_t have, uint64_t total) {
  if (total == 0) return -1;
  if (have >= total) return 100;
  const uint64_t whole = total / 100;
  const uint64_t remainder = total % 100;
  for (int percent = 99; percent > 0; --percent) {
    const uint64_t threshold = whole * percent + (remainder * percent + 99) / 100;
    if (have >= threshold) return percent;
  }
  return 0;
}

std::optional<std::string> ParseChecksum(std::string_view sidecar, std::string_view archive_name) {
  try {
    if (sidecar.ends_with("\r\n")) {
      sidecar.remove_suffix(2);
    } else if (sidecar.ends_with('\n')) {
      sidecar.remove_suffix(1);
    }
    sidecar = Trim(sidecar);
    if (sidecar.find('\r') != std::string_view::npos || sidecar.find('\n') != std::string_view::npos ||
        sidecar.size() < 64) {
      return std::nullopt;
    }

    std::string checksum;
    checksum.reserve(64);
    for (std::size_t i = 0; i < 64; ++i) {
      if (HexValue(sidecar[i]) < 0) return std::nullopt;
      const char value = sidecar[i];
      checksum.push_back(value >= 'A' && value <= 'F' ? static_cast<char>(value - 'A' + 'a') : value);
    }

    std::string_view suffix = sidecar.substr(64);
    if (suffix.empty()) return checksum;
    if (suffix.starts_with("  ")) {
      suffix.remove_prefix(2);
    } else if (suffix.starts_with(" *")) {
      suffix.remove_prefix(2);
    } else {
      return std::nullopt;
    }
    if (suffix.empty() || suffix != archive_name) return std::nullopt;
    return checksum;
  } catch (...) {
    return std::nullopt;
  }
}

bool SafeRelativePath(std::string_view relative) {
  if (relative.empty() || relative.front() == '/' || relative.front() == '\\') return false;
  if (relative.find(':') != std::string_view::npos) return false;
  std::size_t start = 0;
  while (start <= relative.size()) {
    const std::size_t end = relative.find_first_of("/\\", start);
    const std::size_t component_end = end == std::string_view::npos ? relative.size() : end;
    if (relative.substr(start, component_end - start) == "..") return false;
    if (end == std::string_view::npos) break;
    start = end + 1;
  }
  return true;
}

std::string StageFolderName(std::string_view tag) {
  try {
    std::string result;
    result.reserve(tag.size());
    for (char value : tag) result.push_back(IsSafeTagCharacter(value) ? value : '_');
    if (result.empty() || result == "." || result == "..") return "unknown";
    return result;
  } catch (...) {
    return {};
  }
}

std::filesystem::path FindPayloadRoot(const std::filesystem::path& extracted) {
  try {
    std::error_code error;
    std::filesystem::directory_iterator it(extracted, error);
    if (error || it == std::filesystem::directory_iterator()) return extracted;
    const std::filesystem::path only_entry = it->path();
    it.increment(error);
    if (error || it != std::filesystem::directory_iterator()) return extracted;
    const bool is_directory = std::filesystem::is_directory(only_entry, error);
    return !error && is_directory ? only_entry : extracted;
  } catch (...) {
    return extracted;
  }
}

Plan PlanApply(const std::filesystem::path& payload_root,
               const std::filesystem::path& install_dir) {
  struct PlannedFile {
    std::string relative;
    Replacement replacement;
  };

  std::vector<PlannedFile> files;
  try {
    std::error_code error;
    std::filesystem::recursive_directory_iterator it(payload_root, error);
    const std::filesystem::recursive_directory_iterator end;
    if (error) return FailedPlan(FailureText(payload_root, error));
    while (!error && it != end) {
      std::error_code type_error;
      const auto status = it->symlink_status(type_error);
      if (type_error) return FailedPlan(FailureText(it->path(), type_error));
      if (status.type() == std::filesystem::file_type::regular) {
        const auto relative_path = it->path().lexically_relative(payload_root);
        const std::string relative = relative_path.generic_string();
        const auto filename = it->path().filename();
        if (!relative_path.empty() && SafeRelativePath(relative) && filename != "gow2.toml" &&
            filename != "gow2_game_path.txt") {
          const auto target = install_dir / relative_path;
          files.push_back({relative, {it->path(), target, BackupPath(target)}});
        }
      }
      it.increment(error);
    }
    if (error) return FailedPlan(FailureText(payload_root, error));
    std::sort(files.begin(), files.end(), [](const PlannedFile& left, const PlannedFile& right) {
      return left.relative < right.relative;
    });
    Plan plan;
    plan.items.reserve(files.size());
    for (auto& file : files) plan.items.push_back(std::move(file.replacement));
    return plan;
  } catch (...) {
    return FailedPlan("payload scan failed");
  }
}

ApplyResult ApplyPlan(const Plan& plan) {
  ApplyResult result;
  if (!plan.error.empty()) {
    result.error = plan.error;
    return result;
  }
  std::vector<AppliedReplacement> applied;
  try {
    applied.reserve(plan.items.size());
    result.backups.reserve(plan.items.size());
    for (const Replacement& replacement : plan.items) {
      applied.push_back({&replacement, false, false});
      AppliedReplacement& current = applied.back();
      std::error_code error;
      std::filesystem::path failing_path;
      if (!ApplyReplacement(replacement, &current, &failing_path, &error)) {
        result.error = FailureText(failing_path, error);
        ClearFailedResult(&result, applied);
        return result;
      }
      if (current.backup_made) result.backups.push_back(replacement.backup);
    }
    result.ok = true;
    return result;
  } catch (...) {
    RollBack(applied);
    result.backups.clear();
    result.error = "filesystem operation failed";
    return result;
  }
}

bool WriteCleanupList(const std::filesystem::path& list_file,
                      const std::vector<std::filesystem::path>& backups) {
  std::optional<std::filesystem::path> temporary;
  try {
    temporary = UniqueCleanupPath(list_file, "gow2-list-new");
    if (!temporary || !WriteCleanupContents(*temporary, backups) ||
        !ReplaceCleanupList(*temporary, list_file)) {
      if (temporary) {
        std::error_code error;
        std::filesystem::remove(*temporary, error);
      }
      return false;
    }
    return true;
  } catch (...) {
    if (temporary) {
      std::error_code error;
      std::filesystem::remove(*temporary, error);
    }
    return false;
  }
}

std::vector<std::filesystem::path> ReadCleanupList(const std::filesystem::path& list_file) {
  std::vector<std::filesystem::path> paths;
  try {
    std::ifstream file(list_file, std::ios::binary);
    if (!file) return paths;
    std::string line;
    while (std::getline(file, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.empty()) continue;
      std::u8string text;
      text.reserve(line.size());
      for (unsigned char value : line) text.push_back(static_cast<char8_t>(value));
      paths.emplace_back(std::move(text));
    }
    if (file.bad()) return {};
  } catch (...) {
    return {};
  }
  return paths;
}

void RunCleanup(const std::filesystem::path& list_file) {
  try {
    std::ifstream readable(list_file, std::ios::binary);
    if (!readable) return;
    readable.close();

    const auto paths = ReadCleanupList(list_file);
    const auto remaining = RemoveListedBackups(paths);
    if (remaining.empty()) {
      std::error_code error;
      std::filesystem::remove(list_file, error);
    } else {
      if (!WriteCleanupList(list_file, remaining)) return;
    }
  } catch (...) {
  }
}

}  // namespace rex::glue::update_install