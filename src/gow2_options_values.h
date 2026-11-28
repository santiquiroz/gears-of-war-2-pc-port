// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
//
// The PC Options rows backed by the live cvars: composite rows span several cvars, info rows compute
// their text, and saving writes only the rows the player changed into gow2.toml.
#pragma once

#include <deque>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "gow2_options_model.h"
#include "gow2_toml_edit.h"
#if !defined(__ANDROID__)
#include "gow2_update_install.h"
#endif

namespace rex::glue::options {

struct GamePaths {
  std::filesystem::path config_path;     // gow2.toml the menu saves to
  std::filesystem::path user_data_root;  // where this session's saves are
};

class CvarValues final : public Values {
 public:
  explicit CvarValues(GamePaths paths);
  void set_paths(GamePaths paths);
  std::string Get(std::string_view id) const override;
  bool Set(std::string_view id, std::string_view value) override;
  std::string Default(std::string_view id) const override;
  const GamePaths& paths() const { return paths_; }

 private:
  GamePaths paths_;
};

// The cvars a row is stored in, with their current values and TOML kinds. Empty for actions and info.
std::vector<toml_edit::Assignment> Persist(std::string_view id);

// Writes the rows' cvars into the file, keeping every other line. False with *error on failure.
bool SaveRows(const std::filesystem::path& config_path, const std::set<std::string, std::less<>>& ids,
              std::string* error);

// Context plus the strings its dynamic choices point into.
struct OwnedContext {
  Context ctx;
  std::deque<std::string> text;
};

// touch_controls: the touch controls were created at launch.
void BuildContext(bool touch_controls, OwnedContext* out);

// gow2_menu_language: "en", "es", or "auto" (the system's UI language: Spanish when it starts with "es").
Lang MenuLang();

#if !defined(__ANDROID__)
std::string UpdateInstallStatus(const update_install::InstallSnapshot& snapshot);
#endif

}  // namespace rex::glue::options
