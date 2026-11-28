// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_user_config.h"

#include "gow2_toml_edit.h"

#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rex::glue::user_config {

const std::string_view kPlayerFileHeader =
    "# Your Gears of War 2 PC settings. The options menu saves here and you can edit it by hand.\n"
    "# Every setting and its default is in gow2.default.toml next to the game's executable:\n"
    "# copy a line here and change it. A line here wins over gow2.default.toml.\n";

namespace {

struct AssignmentLine {
  std::string key;
  std::string value;
  std::string_view line;
};

bool IsBlank(char c) { return c == ' ' || c == '\t'; }

bool IsKeyChar(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
         c == '_' || c == '-';
}

std::string_view TrimBlanks(std::string_view text) {
  while (!text.empty() && IsBlank(text.front())) text.remove_prefix(1);
  while (!text.empty() && IsBlank(text.back())) text.remove_suffix(1);
  return text;
}

std::string_view WithoutCarriageReturn(std::string_view line) {
  if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
  return line;
}

std::vector<std::string_view> SplitLines(std::string_view text) {
  std::vector<std::string_view> lines;
  size_t start = 0;
  while (start < text.size()) {
    const size_t end = text.find('\n', start);
    if (end == std::string_view::npos) {
      lines.push_back(WithoutCarriageReturn(text.substr(start)));
      break;
    }
    lines.push_back(WithoutCarriageReturn(text.substr(start, end - start)));
    start = end + 1;
  }
  return lines;
}

bool IsTableHeader(std::string_view line) {
  line = TrimBlanks(line);
  return !line.empty() && line.front() == '[';
}

bool ParseAssignment(std::string_view line, AssignmentLine* assignment) {
  size_t i = 0;
  while (i < line.size() && IsBlank(line[i])) ++i;
  const size_t key_start = i;
  while (i < line.size() && IsKeyChar(line[i])) ++i;
  if (i == key_start) return false;
  const size_t key_end = i;
  while (i < line.size() && IsBlank(line[i])) ++i;
  if (i == line.size() || line[i] != '=') return false;
  ++i;

  const size_t value_start = i;
  size_t value_end = line.size();
  char quote = 0;
  while (i < line.size()) {
    const char c = line[i];
    if (quote == '"' && c == '\\') {
      i += 2;
      continue;
    }
    if (quote != 0) {
      if (c == quote) quote = 0;
    } else if (c == '#') {
      value_end = i;
      break;
    } else if (c == '"' || c == '\'') {
      quote = c;
    }
    ++i;
  }

  const std::string_view key = line.substr(key_start, key_end - key_start);
  const std::string_view value = TrimBlanks(line.substr(value_start, value_end - value_start));
  assignment->key.assign(key);
  assignment->value.assign(value);
  assignment->line = line;
  return true;
}

size_t TopLevelEnd(const std::vector<std::string_view>& lines) {
  for (size_t i = 0; i < lines.size(); ++i) {
    if (IsTableHeader(lines[i])) return i;
  }
  return lines.size();
}

std::unordered_map<std::string, std::string> CollectDefaultValues(
    const std::vector<std::string_view>& lines, size_t top_level_end) {
  std::unordered_map<std::string, std::string> values;
  for (size_t i = 0; i < top_level_end; ++i) {
    AssignmentLine assignment;
    if (ParseAssignment(lines[i], &assignment)) values[assignment.key] = std::move(assignment.value);
  }
  return values;
}

std::string PathText(const std::filesystem::path& path) {
  const auto text = path.u8string();
  return {text.begin(), text.end()};
}

bool TryExists(const std::filesystem::path& path, bool* exists, std::string* error) {
  std::error_code ec;
  *exists = std::filesystem::exists(path, ec);
  if (!ec) return true;
  *error = "cannot inspect " + PathText(path) + ": " + ec.message();
  return false;
}

bool SameFolder(const std::filesystem::path& left, const std::filesystem::path& right) {
  std::error_code left_error;
  std::error_code right_error;
  const bool left_exists = std::filesystem::exists(left, left_error);
  const bool right_exists = std::filesystem::exists(right, right_error);
  if (left_exists && right_exists && !left_error && !right_error) {
    std::error_code equivalent_error;
    const bool equivalent = std::filesystem::equivalent(left, right, equivalent_error);
    if (!equivalent_error) return equivalent;
  }
  return left.lexically_normal() == right.lexically_normal();
}

bool ReadOptionalFile(const std::filesystem::path& path, std::string* text, std::string* error) {
  bool exists = false;
  if (!TryExists(path, &exists, error)) return false;
  if (!exists) {
    text->clear();
    return true;
  }
  if (toml_edit::ReadFile(path, text)) return true;
  *error = "cannot read " + PathText(path);
  return false;
}

bool WriteConfig(const std::filesystem::path& path, std::string_view text, std::string* error) {
  if (toml_edit::WriteFileAtomic(path, text, error)) return true;
  if (error->empty()) *error = "cannot write " + PathText(path);
  return false;
}

bool MigrateOldConfig(const std::filesystem::path& config_path,
                      const std::filesystem::path& exe_dir, std::string* error) {
  const auto old_path = exe_dir / "gow2.toml";
  std::string player_text;
  if (!ReadOptionalFile(old_path, &player_text, error)) return false;
  std::string default_text;
  if (!ReadOptionalFile(exe_dir / "gow2.default.toml", &default_text, error)) return false;
  if (!WriteConfig(config_path, KeepPlayerChanges(player_text, default_text), error)) return false;

  const auto moved_path = exe_dir / "gow2.toml.moved";
  std::error_code ec;
  std::filesystem::remove(moved_path, ec);
  if (ec) {
    *error = "cannot remove " + PathText(moved_path) + ": " + ec.message();
    return false;
  }
  std::filesystem::rename(old_path, moved_path, ec);
  if (ec) {
    *error = "cannot move " + PathText(old_path) + ": " + ec.message();
    return false;
  }
  return true;
}

}  // namespace

std::filesystem::path ChooseConfigDir(const ConfigDirInputs& in) {
  if (in.portable) return in.exe_dir;
  if (!in.gow2_home.empty()) return in.gow2_home;
  if (in.windows) {
    if (!in.local_app_data.empty()) return in.local_app_data / "GearsOfWar2PC";
    return in.exe_dir;
  }
  if (!in.xdg_config_home.empty()) return in.xdg_config_home / "gow2";
  if (!in.home.empty()) return in.home / ".config" / "gow2";
  return in.exe_dir;
}

std::string KeepPlayerChanges(std::string_view player_text, std::string_view default_text) {
  const std::vector<std::string_view> player_lines = SplitLines(player_text);
  const std::vector<std::string_view> default_lines = SplitLines(default_text);
  const size_t player_top_end = TopLevelEnd(player_lines);
  const size_t default_top_end = TopLevelEnd(default_lines);
  const auto defaults = CollectDefaultValues(default_lines, default_top_end);

  std::vector<AssignmentLine> assignments;
  std::unordered_map<std::string, size_t> last_occurrence;
  for (size_t i = 0; i < player_top_end; ++i) {
    AssignmentLine assignment;
    if (!ParseAssignment(player_lines[i], &assignment)) continue;
    last_occurrence[assignment.key] = assignments.size();
    assignments.push_back(std::move(assignment));
  }

  std::string output(kPlayerFileHeader);
  for (size_t i = 0; i < assignments.size(); ++i) {
    const AssignmentLine& assignment = assignments[i];
    if (last_occurrence[assignment.key] != i) continue;
    const auto default_value = defaults.find(assignment.key);
    if (default_value != defaults.end() && default_value->second == assignment.value) continue;
    output.append(assignment.line);
    output.push_back('\n');
  }

  if (player_top_end < player_lines.size()) {
    output.push_back('\n');
    for (size_t i = player_top_end; i < player_lines.size(); ++i) {
      output.append(player_lines[i]);
      output.push_back('\n');
    }
  }
  return output;
}

PrepareResult PrepareConfigFile(const std::filesystem::path& config_dir,
                                const std::filesystem::path& exe_dir) {
  PrepareResult result;
  result.config_path = config_dir / "gow2.toml";

  std::error_code ec;
  std::filesystem::create_directories(config_dir, ec);
  if (ec) {
    result.error = "cannot create directory " + PathText(config_dir) + ": " + ec.message();
    return result;
  }

  bool config_exists = false;
  if (!TryExists(result.config_path, &config_exists, &result.error) || config_exists) return result;

  const auto old_path = exe_dir / "gow2.toml";
  if (!SameFolder(config_dir, exe_dir)) {
    bool old_exists = false;
    if (!TryExists(old_path, &old_exists, &result.error)) return result;
    if (old_exists) {
      if (!MigrateOldConfig(result.config_path, exe_dir, &result.error)) return result;
      result.migrated = true;
      return result;
    }
  }

  if (WriteConfig(result.config_path, kPlayerFileHeader, &result.error)) result.created = true;
  return result;
}

bool MigrateGamePathFile(const std::filesystem::path& config_dir, const std::filesystem::path& old_dir) {
  if (SameFolder(config_dir, old_dir)) return false;
  const auto new_path = config_dir / "gow2_game_path.txt";
  const auto old_path = old_dir / "gow2_game_path.txt";
  std::error_code ec;
  if (std::filesystem::exists(new_path, ec) || ec) return false;
  if (!std::filesystem::exists(old_path, ec) || ec) return false;
  return std::filesystem::copy_file(old_path, new_path, std::filesystem::copy_options::none, ec) && !ec;
}

}  // namespace rex::glue::user_config