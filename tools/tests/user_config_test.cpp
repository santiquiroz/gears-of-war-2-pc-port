// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_user_config.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace fs = std::filesystem;
namespace user_config = rex::glue::user_config;

namespace {

int g_failures = 0;

void Expect(bool ok, const char* what) {
  if (ok) return;
  ++g_failures;
  std::fprintf(stderr, "FAIL: %s\n", what);
}

void WriteFile(const fs::path& path, const std::string& content) {
  std::ofstream file(path, std::ios::binary);
  file << content;
}

std::string ReadFile(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void TestChooseConfigDir() {
  user_config::ConfigDirInputs in;
  in.exe_dir = "exe";
  in.portable = true;
  in.gow2_home = "gow2-home";
  in.local_app_data = "local-app-data";
  in.xdg_config_home = "xdg";
  in.home = "home";
  in.windows = true;
  Expect(user_config::ChooseConfigDir(in) == in.exe_dir, "portable folder wins over all overrides");

  in.portable = false;
  Expect(user_config::ChooseConfigDir(in) == in.gow2_home, "GOW2_HOME wins over LOCALAPPDATA");

  in.gow2_home.clear();
  Expect(user_config::ChooseConfigDir(in) == in.local_app_data / "GearsOfWar2PC",
         "Windows uses LOCALAPPDATA/GearsOfWar2PC");

  in.windows = false;
  Expect(user_config::ChooseConfigDir(in) == in.xdg_config_home / "gow2",
         "Linux uses XDG_CONFIG_HOME/gow2");

  in.xdg_config_home.clear();
  Expect(user_config::ChooseConfigDir(in) == in.home / ".config" / "gow2",
         "Linux falls back to HOME/.config/gow2");

  in.windows = true;
  in.local_app_data.clear();
  Expect(user_config::ChooseConfigDir(in) == in.exe_dir, "Windows ignores XDG_CONFIG_HOME and HOME");

  in.windows = false;
  in.home.clear();
  Expect(user_config::ChooseConfigDir(in) == in.exe_dir, "no environment path falls back to exe folder");
}

void TestKeepPlayerChanges() {
  const std::string defaults =
      "equal = \"a#b\"  # default comment\r\n"
      "changed = 1\r\n"
      "a = 1\r\n"
      "dup = old\r\n"
      "k = \"E\"\r\n";
  const std::string player =
      "# drop this comment\r\n"
      "\r\n"
      "equal = \"a#b\" # same value\r\n"
      "changed = 2 # keep this comment\r\n"
      "missing = true\r\n"
      "a=1\r\n"
      "dup=first\r\n"
      "dup = last # last duplicate\r\n"
      "k = \"#\"\r\n"
      "not an assignment\r\n"
      "[table]\r\n"
      "x = 1\r\n"
      "# table comment stays\r\n";
  const std::string expected =
      std::string(user_config::kPlayerFileHeader) +
      "changed = 2 # keep this comment\n"
      "missing = true\n"
      "dup = last # last duplicate\n"
      "k = \"#\"\n"
      "\n"
      "[table]\n"
      "x = 1\n"
      "# table comment stays\n";
  Expect(user_config::KeepPlayerChanges(player, defaults) == expected,
         "player settings are reduced, preserved verbatim and normalize CRLF");

  const std::string single_quote_player = "x = '#' # literal hash\n";
  Expect(user_config::KeepPlayerChanges(single_quote_player, "x = 'E'\n") ==
             std::string(user_config::kPlayerFileHeader) + "x = '#' # literal hash\n",
         "hash inside single-quoted literal is part of the value");

  const std::string escaped_player = "x = \"a\\\"#b\"\n";
  Expect(user_config::KeepPlayerChanges(escaped_player, "x = \"a\\\"#b\" # note\n") ==
             std::string(user_config::kPlayerFileHeader),
         "escaped quote and hash inside a basic string are parsed correctly");

  Expect(user_config::KeepPlayerChanges("a=1\n", "a = 1\n") ==
             std::string(user_config::kPlayerFileHeader),
         "assignment spacing does not change the value");
}

void TestPrepareConfigFile(const fs::path& root) {
  const auto existing_dir = root / "existing";
  fs::create_directories(existing_dir);
  const auto existing_path = existing_dir / "gow2.toml";
  WriteFile(existing_path, "existing bytes\r\n");
  const auto existing = user_config::PrepareConfigFile(existing_dir, root / "unused-exe");
  Expect(existing.config_path == existing_path && !existing.migrated && !existing.created &&
             existing.error.empty(),
         "existing config is left untouched with flags false");
  Expect(ReadFile(existing_path) == "existing bytes\r\n", "existing config stays byte-identical");

  const auto migration_exe = root / "migration-exe";
  const auto migration_config = root / "migration-config";
  fs::create_directories(migration_exe);
  WriteFile(migration_exe / "gow2.default.toml", "same = 1\nchanged = 1\n");
  WriteFile(migration_exe / "gow2.toml", "same=1\nchanged=2 # mine\nunknown=true\n");
  WriteFile(migration_exe / "gow2.toml.moved", "older moved file");
  const auto migrated = user_config::PrepareConfigFile(migration_config, migration_exe);
  const std::string migrated_expected = std::string(user_config::kPlayerFileHeader) +
                                        "changed=2 # mine\nunknown=true\n";
  Expect(migrated.migrated && !migrated.created && migrated.error.empty(), "old config is migrated");
  Expect(ReadFile(migrated.config_path) == migrated_expected, "migration writes only player changes");
  Expect(!fs::exists(migration_exe / "gow2.toml") &&
             ReadFile(migration_exe / "gow2.toml.moved") == "same=1\nchanged=2 # mine\nunknown=true\n",
         "migration renames old config and replaces an existing moved file");

  const auto new_exe = root / "new-exe";
  const auto new_config = root / "new-config";
  fs::create_directories(new_exe);
  const auto created = user_config::PrepareConfigFile(new_config, new_exe);
  Expect(created.created && !created.migrated && created.error.empty(), "missing config is created");
  Expect(ReadFile(created.config_path) == user_config::kPlayerFileHeader, "new config contains only header");

  const auto portable = root / "portable";
  fs::create_directories(portable);
  const auto portable_created = user_config::PrepareConfigFile(portable, portable);
  Expect(portable_created.created && !portable_created.migrated &&
             !fs::exists(portable / "gow2.toml.moved"),
         "portable config in exe folder is created without migration");

  const auto no_defaults_exe = root / "no-defaults-exe";
  const auto no_defaults_config = root / "no-defaults-config";
  fs::create_directories(no_defaults_exe);
  WriteFile(no_defaults_exe / "gow2.toml", "first=1\nsecond=2\n");
  const auto no_defaults = user_config::PrepareConfigFile(no_defaults_config, no_defaults_exe);
  Expect(no_defaults.migrated && ReadFile(no_defaults.config_path) ==
                                     std::string(user_config::kPlayerFileHeader) + "first=1\nsecond=2\n",
         "migration without shipped defaults keeps every player assignment");

  const auto blocking_file = root / "not-a-directory";
  WriteFile(blocking_file, "file");
  const auto failed = user_config::PrepareConfigFile(blocking_file / "child", root / "other-exe");
  Expect(failed.config_path == blocking_file / "child" / "gow2.toml" && !failed.error.empty(),
         "directory operation failure returns config path and error");
}

void TestMigrateGamePathFile(const fs::path& root) {
  const auto old_dir = root / "old-game-path";
  const auto config_dir = root / "new-game-path";
  fs::create_directories(old_dir);
  fs::create_directories(config_dir);
  WriteFile(old_dir / "gow2_game_path.txt", "game folder\n");
  Expect(user_config::MigrateGamePathFile(config_dir, old_dir), "remembered game path is copied");
  Expect(ReadFile(config_dir / "gow2_game_path.txt") == "game folder\n" &&
             fs::exists(old_dir / "gow2_game_path.txt"),
         "remembered game path copy keeps old file");

  WriteFile(config_dir / "gow2_game_path.txt", "keep me");
  Expect(!user_config::MigrateGamePathFile(config_dir, old_dir), "existing remembered path is not overwritten");
  Expect(ReadFile(config_dir / "gow2_game_path.txt") == "keep me", "existing remembered path stays unchanged");
  Expect(!user_config::MigrateGamePathFile(old_dir, old_dir), "same folder does not migrate remembered path");
}

void TestSameFolderGamePath(const fs::path& root) {
  const auto same = root / "same-game-path";
  fs::create_directories(same);
  WriteFile(same / "gow2_game_path.txt", "game folder");
  Expect(!user_config::MigrateGamePathFile(same, same), "same folder returns false");
}

}  // namespace

int main() {
  const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto root = fs::temp_directory_path() / ("gow2-user-config-test-" + std::to_string(nonce));
  std::error_code error;
  fs::create_directories(root, error);
  if (error) {
    std::fprintf(stderr, "FAIL: create temporary directory: %s\n", error.message().c_str());
    return 1;
  }
  TestChooseConfigDir();
  TestKeepPlayerChanges();
  TestPrepareConfigFile(root);
  TestMigrateGamePathFile(root);
  TestSameFolderGamePath(root);
  fs::remove_all(root, error);
  if (error) {
    std::fprintf(stderr, "FAIL: remove temporary directory: %s\n", error.message().c_str());
    return 1;
  }
  std::printf("user_config_test: %d failures\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}