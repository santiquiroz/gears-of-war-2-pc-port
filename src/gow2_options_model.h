// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Pure menu state, navigation, and setting value transformations.
#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "gow2_options_catalog.h"

namespace rex::glue::options {

class Values {
 public:
  virtual ~Values() = default;
  virtual std::string Get(std::string_view id) const = 0;
  virtual bool Set(std::string_view id, std::string_view value) = 0;
  virtual std::string Default(std::string_view id) const = 0;
};

struct Context {
  uint8_t platform = kWindows;
  bool touch = false, fidelityfx = false, dlss = false, native_mouse = false;
  bool advanced = false;
  bool game_running = true;  // false in the launcher, before the runtime exists
  Lang lang = Lang::kEn;
  std::map<std::string, std::vector<Choice>, std::less<>> dynamic_choices;
  std::set<std::string, std::less<>> unavailable;  // rows whose cvar this runtime does not have
};

enum class Command : uint8_t { kUp, kDown, kLeft, kRight, kAccept, kBack, kTabPrev, kTabNext, kResetRow, kSave };
enum class Prompt : uint8_t { kNone, kUnsaved };

struct State {
  int tab = 0;
  Page page = Page::kMain;
  int row = 0;
  Prompt prompt = Prompt::kNone;
  int prompt_choice = 0;
  std::map<std::string, std::string, std::less<>> launch;
  std::map<std::string, std::string, std::less<>> opened;
  std::set<std::string, std::less<>> touched;
};

struct Outcome {
  enum class Kind : uint8_t { kNone, kChanged, kClose, kSave, kSaveAndClose, kEditText, kCaptureKey, kRunAction };
  Kind kind = Kind::kNone;
  std::string id;
};

struct RowView {
  const Setting* setting = nullptr;
  std::string value_text;
  bool selected = false;
  bool selectable = true;
  bool can_decrease = false;
  bool can_increase = false;
  bool changed = false;
  bool restart_pending = false;
};

bool IsVisible(const Setting& s, const Values& values, const Context& ctx);
std::vector<Tab> VisibleTabs(const std::vector<Setting>& catalog, const Values& values, const Context& ctx);
std::vector<const Setting*> PageRows(const std::vector<Setting>& catalog, Tab tab, Page page, const Values& values, const Context& ctx);
std::vector<Choice> ChoicesOf(const Setting& s, const Context& ctx);
std::string StepValue(const Setting& s, std::string_view current, int direction, const Context& ctx);
std::string FormatValue(const Setting& s, std::string_view value, const Context& ctx);
std::string NormalizeText(const Setting& s, std::string_view text);
void CaptureLaunch(State& state, const std::vector<Setting>& catalog, const Values& values);
void Open(State& state, const std::vector<Setting>& catalog, const Values& values, const Context& ctx);
Outcome Handle(State& state, Command command, const std::vector<Setting>& catalog, Values& values, const Context& ctx);
Outcome Click(State& state, int row, const std::vector<Setting>& catalog, Values& values, const Context& ctx);
Outcome Step(State& state, int row, int direction, const std::vector<Setting>& catalog, Values& values, const Context& ctx);
void SelectTab(State& state, int tab, const std::vector<Setting>& catalog, const Values& values, const Context& ctx);
bool CommitText(State& state, std::string_view id, std::string_view text, const std::vector<Setting>& catalog, Values& values);
bool CommitKey(State& state, std::string_view id, std::string_view key, const std::vector<Setting>& catalog, Values& values);
void MarkSaved(State& state, const std::vector<Setting>& catalog, const Values& values);
void Discard(State& state, Values& values);
std::vector<RowView> Rows(const State& state, const std::vector<Setting>& catalog, const Values& values, const Context& ctx);
std::vector<std::string> RestartPending(const State& state, const std::vector<Setting>& catalog, const Values& values, const Context& ctx);
std::string_view CurrentHelp(const State& state, const std::vector<Setting>& catalog, const Values& values, const Context& ctx);

}
