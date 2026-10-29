// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Implements pure menu navigation and value presentation without platform dependencies.
#include "gow2_options_model.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

namespace rex::glue::options {
namespace {

bool NeedSatisfied(Need need, const Context& ctx) {
  switch (need) {
    case Need::kTouch: return ctx.touch;
    case Need::kFidelityFx: return ctx.fidelityfx;
    case Need::kDlss: return ctx.dlss;
    case Need::kNoDlss: return !ctx.dlss;
    case Need::kNativeMouse: return ctx.native_mouse;
    case Need::kNoNativeMouse: return !ctx.native_mouse;
    case Need::kGameRunning: return ctx.game_running;
    default: return true;
  }
}

bool ConditionSatisfied(const Setting& s, const Values& values) {
  if (s.visible_if_id.empty()) return true;
  const auto expected = s.visible_if_value;
  const auto actual = values.Get(s.visible_if_id);
  if (expected.starts_with('!')) return actual != expected.substr(1);
  return actual == expected;
}

double ParseNumber(std::string_view text, double fallback) {
  if (text.empty()) return fallback;
  std::istringstream stream{std::string(text)};
  stream.imbue(std::locale::classic());
  double value = 0;
  if (!(stream >> std::noskipws >> value) || !stream.eof()) return fallback;
  return std::isfinite(value) ? value : fallback;
}

std::string FixedNumber(double value, int decimals) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::fixed << std::setprecision(std::max(0, decimals)) << value;
  return stream.str();
}

std::string ShortNumber(double value) {
  auto text = FixedNumber(value, 6);
  while (text.ends_with('0')) text.pop_back();
  if (text.ends_with('.')) text.pop_back();
  if (text == "-0") return "0";
  return text;
}

std::string StepNumber(const Setting& s, std::string_view current, int direction) {
  double value = ParseNumber(current, s.min);
  if (s.step > 0 && std::isfinite(s.step)) {
    value += direction * s.step;
    value = s.min + std::round((value - s.min) / s.step) * s.step;
  }
  return ShortNumber(std::clamp(value, s.min, s.max));
}

std::string StepChoice(const Setting& s, std::string_view current, int direction, const Context& ctx) {
  auto choices = ChoicesOf(s, ctx);
  if (s.id == "gow2_gfx_preset") {
    std::erase_if(choices, [](const Choice& choice) { return choice.value == "custom"; });
  }
  if (choices.empty()) return std::string(current);
  const auto found = std::find_if(choices.begin(), choices.end(),
                                [current](const Choice& c) { return c.value == current; });
  if (found == choices.end()) return std::string(direction > 0 ? choices.front().value : choices.back().value);
  const auto count = static_cast<int>(choices.size());
  const auto index = static_cast<int>(found - choices.begin());
  return std::string(choices[(index + direction + count) % count].value);
}

std::string FormatChoice(const Setting& s, std::string_view value, const Context& ctx) {
  for (const auto& choice : ChoicesOf(s, ctx)) {
    if (choice.value == value) return std::string(ChoiceLabel(choice, ctx.lang));
  }
  return std::string(value);
}

std::string FormatKeys(std::string_view value) {
  std::string result;
  for (const char ch : value) {
    if (ch == ',') result += " / ";
    else result += ch;
  }
  return result;
}

bool Selectable(const Setting& s) { return s.kind != Kind::kInfo; }

std::vector<const Setting*> CurrentRows(const State& state, const std::vector<Setting>& catalog,
                                      const Values& values, const Context& ctx) {
  const auto tabs = VisibleTabs(catalog, values, ctx);
  if (state.tab < 0 || state.tab >= static_cast<int>(tabs.size())) return {};
  return PageRows(catalog, tabs[state.tab], state.page, values, ctx);
}

int FindSelectable(const std::vector<const Setting*>& rows, int start, int direction) {
  for (int index = start; index >= 0 && index < static_cast<int>(rows.size()); index += direction) {
    if (Selectable(*rows[index])) return index;
  }
  return -1;
}

void ValidateSelection(State& state, const std::vector<Setting>& catalog, const Values& values, const Context& ctx) {
  const auto tabs = VisibleTabs(catalog, values, ctx);
  state.tab = tabs.empty() ? 0 : std::clamp(state.tab, 0, static_cast<int>(tabs.size()) - 1);
  const auto rows = CurrentRows(state, catalog, values, ctx);
  if (rows.empty()) { state.row = 0; return; }
  state.row = std::clamp(state.row, 0, static_cast<int>(rows.size()) - 1);
  const int below = FindSelectable(rows, state.row, 1);
  if (below >= 0) { state.row = below; return; }
  state.row = std::max(0, FindSelectable(rows, state.row, -1));
}

void SelectFirst(State& state, const std::vector<Setting>& catalog, const Values& values, const Context& ctx) {
  state.row = 0;
  ValidateSelection(state, catalog, values, ctx);
}

const Setting* SelectedSetting(const State& state, const std::vector<Setting>& catalog,
                               const Values& values, const Context& ctx) {
  const auto rows = CurrentRows(state, catalog, values, ctx);
  if (state.row < 0 || state.row >= static_cast<int>(rows.size())) return nullptr;
  if (!Selectable(*rows[state.row])) return nullptr;
  return rows[state.row];
}

const Setting* FindRow(const std::vector<Setting>& catalog, std::string_view id) {
  const auto found = std::find_if(catalog.begin(), catalog.end(),
                                [id](const Setting& s) { return s.id == id; });
  return found == catalog.end() ? nullptr : &*found;
}

void TrackChangedValue(State& state, std::string_view id, const Values& values) {
  const std::string owned_id(id);
  if (values.Get(id) != state.opened[owned_id]) state.touched.insert(owned_id);
  else state.touched.erase(owned_id);
}

void TrackQualityChanges(State& state, const Values& values) {
  for (const auto& [quality_id, original] : state.opened) {
    if (!quality_id.starts_with("gow2_gfx_") || quality_id == "gow2_gfx_preset") continue;
    TrackChangedValue(state, quality_id, values);
  }
}

bool SetValue(State& state, std::string_view id, std::string_view value, Values& values) {
  if (!values.Set(id, value)) return false;
  if (id != "gow2_gfx_preset") TrackChangedValue(state, id, values);
  TrackQualityChanges(state, values);
  return true;
}

Outcome ChangeValue(State& state, const Setting& s, std::string_view value,
                    const std::vector<Setting>& catalog, Values& values, const Context& ctx) {
  const std::string id(s.id);
  const auto previous = values.Get(id);
  const auto previously_touched = state.touched;
  if (!SetValue(state, id, value, values)) return {};
  ValidateSelection(state, catalog, values, ctx);
  if (values.Get(id) == previous && state.touched == previously_touched) return {};
  return {Outcome::Kind::kChanged, id};
}

bool CanStep(Kind kind) {
  return kind == Kind::kToggle || kind == Kind::kChoice || kind == Kind::kNumber;
}

bool CanReset(Kind kind) {
  return CanStep(kind) || kind == Kind::kText || kind == Kind::kKeybind;
}

Outcome ChangeSelected(State& state, int direction, const std::vector<Setting>& catalog,
                       Values& values, const Context& ctx) {
  const auto* s = SelectedSetting(state, catalog, values, ctx);
  if (!s || !CanStep(s->kind)) return {};
  return ChangeValue(state, *s, StepValue(*s, values.Get(s->id), direction, ctx), catalog, values, ctx);
}

void MoveSelection(State& state, int direction, const std::vector<Setting>& catalog,
                   const Values& values, const Context& ctx) {
  const auto rows = CurrentRows(state, catalog, values, ctx);
  const auto next = FindSelectable(rows, state.row + direction, direction);
  if (next >= 0) state.row = next;
}

struct PageAction {
  std::string_view action;
  Page page;
};

constexpr PageAction kPageActions[] = {
    {"keybindings", Page::kKeybinds},
    {"pad_buttons", Page::kPadButtons},
    {"host_online", Page::kHost},
    {"gow2_build_texture_pack", Page::kTexturePackBuilder},
};

const PageAction* PageActionFor(std::string_view action) {
  for (const auto& entry : kPageActions) {
    if (entry.action == action) return &entry;
  }
  return nullptr;
}

std::string_view ActionForPage(Page page) {
  for (const auto& entry : kPageActions) {
    if (entry.page == page) return entry.action;
  }
  return {};
}

Outcome AcceptAction(State& state, const Setting& s, const std::vector<Setting>& catalog,
                     const Values& values, const Context& ctx) {
  if (ctx.disabled.contains(s.id)) return {};
  const auto* opens = PageActionFor(s.id);
  if (!opens) return {Outcome::Kind::kRunAction, std::string(s.id)};
  state.page = opens->page;
  SelectFirst(state, catalog, values, ctx);
  return {};
}

Outcome AcceptSelected(State& state, const std::vector<Setting>& catalog, Values& values, const Context& ctx) {
  const auto* s = SelectedSetting(state, catalog, values, ctx);
  if (!s) return {};
  switch (s->kind) {
    case Kind::kToggle:
    case Kind::kChoice: return ChangeSelected(state, 1, catalog, values, ctx);
    case Kind::kText: return {Outcome::Kind::kEditText, std::string(s->id)};
    case Kind::kKeybind: return {Outcome::Kind::kCaptureKey, std::string(s->id)};
    case Kind::kAction: return AcceptAction(state, *s, catalog, values, ctx);
    default: return {};
  }
}

void ReturnToMain(State& state, const std::vector<Setting>& catalog, const Values& values, const Context& ctx) {
  const auto action = ActionForPage(state.page);
  state.page = Page::kMain;
  const auto rows = CurrentRows(state, catalog, values, ctx);
  const auto found = std::find_if(rows.begin(), rows.end(),
                                [action](const Setting* s) { return s->id == action; });
  state.row = found == rows.end() ? 0 : static_cast<int>(found - rows.begin());
  ValidateSelection(state, catalog, values, ctx);
}

Outcome GoBack(State& state, const std::vector<Setting>& catalog, const Values& values, const Context& ctx) {
  if (state.page != Page::kMain) { ReturnToMain(state, catalog, values, ctx); return {}; }
  if (state.touched.empty()) return {Outcome::Kind::kClose, {}};
  state.prompt = Prompt::kUnsaved;
  state.prompt_choice = 0;
  return {};
}

Outcome HandlePrompt(State& state, Command command, Values& values) {
  if (command == Command::kLeft) state.prompt_choice = std::clamp(state.prompt_choice - 1, 0, 2);
  if (command == Command::kRight) state.prompt_choice = std::clamp(state.prompt_choice + 1, 0, 2);
  if (command == Command::kBack) state.prompt = Prompt::kNone;
  if (command != Command::kAccept) return {};
  state.prompt = Prompt::kNone;
  if (state.prompt_choice == 0) return {Outcome::Kind::kSaveAndClose, {}};
  if (state.prompt_choice != 1) return {};
  Discard(state, values);
  return {Outcome::Kind::kClose, {}};
}

void CycleTab(State& state, int direction, const std::vector<Setting>& catalog,
              const Values& values, const Context& ctx) {
  state.page = Page::kMain;
  const auto count = static_cast<int>(VisibleTabs(catalog, values, ctx).size());
  state.tab = count == 0 ? 0 : (state.tab + direction + count) % count;
  SelectFirst(state, catalog, values, ctx);
}

bool SelectRow(State& state, int row, const std::vector<Setting>& catalog,
               const Values& values, const Context& ctx) {
  const auto rows = CurrentRows(state, catalog, values, ctx);
  if (row < 0 || row >= static_cast<int>(rows.size())) return false;
  if (!Selectable(*rows[row])) return false;
  state.row = row;
  return true;
}

bool Differs(const std::map<std::string, std::string, std::less<>>& snapshot,
             std::string_view id, std::string_view value) {
  const auto found = snapshot.find(id);
  return found != snapshot.end() && found->second != value;
}

bool Pending(const State& state, const Setting& s, std::string_view value) {
  return s.apply == Apply::kRestart && Differs(state.launch, s.id, value);
}

void SetStepAvailability(RowView& row, std::string_view value, const Context& ctx) {
  const auto& s = *row.setting;
  if (s.kind == Kind::kToggle) { row.can_decrease = row.can_increase = true; return; }
  if (s.kind == Kind::kChoice) { row.can_decrease = row.can_increase = ChoicesOf(s, ctx).size() >= 2; return; }
  if (s.kind != Kind::kNumber) return;
  const double number = ParseNumber(value, s.min);
  row.can_decrease = number > s.min;
  row.can_increase = number < s.max;
}

}

bool IsVisible(const Setting& s, const Values& values, const Context& ctx) {
  if ((s.platforms & ctx.platform) == 0 || !NeedSatisfied(s.need, ctx)) return false;
  if (s.advanced && !ctx.advanced) return false;
  if (ctx.unavailable.contains(s.id)) return false;
  if (s.dynamic_choices && ChoicesOf(s, ctx).empty()) return false;
  return ConditionSatisfied(s, values);
}

std::vector<Tab> VisibleTabs(const std::vector<Setting>& catalog, const Values& values, const Context& ctx) {
  std::set<Tab> tabs;
  for (const auto& s : catalog) {
    if (s.page == Page::kMain && IsVisible(s, values, ctx)) tabs.insert(s.tab);
  }
  return {tabs.begin(), tabs.end()};
}

std::vector<const Setting*> PageRows(const std::vector<Setting>& catalog, Tab tab, Page page,
                                   const Values& values, const Context& ctx) {
  std::vector<const Setting*> rows;
  for (const auto& s : catalog) {
    if (s.tab == tab && s.page == page && IsVisible(s, values, ctx)) rows.push_back(&s);
  }
  return rows;
}

std::vector<Choice> ChoicesOf(const Setting& s, const Context& ctx) {
  if (!s.dynamic_choices) return s.choices;
  const auto found = ctx.dynamic_choices.find(s.id);
  return found == ctx.dynamic_choices.end() ? std::vector<Choice>{} : found->second;
}

std::string StepValue(const Setting& s, std::string_view current, int direction, const Context& ctx) {
  if (direction == 0) return std::string(current);
  direction = direction < 0 ? -1 : 1;
  switch (s.kind) {
    case Kind::kToggle: return current == "true" ? "false" : "true";
    case Kind::kChoice: return StepChoice(s, current, direction, ctx);
    case Kind::kNumber: return StepNumber(s, current, direction);
    default: return std::string(current);
  }
}

std::string FormatValue(const Setting& s, std::string_view value, const Context& ctx) {
  switch (s.kind) {
    case Kind::kToggle: return std::string(Text(value == "true" ? TextId::kOn : TextId::kOff, ctx.lang));
    case Kind::kChoice: return FormatChoice(s, value, ctx);
    case Kind::kNumber: return FixedNumber(ParseNumber(value, s.min) * s.display_scale, s.decimals) + std::string(s.unit);
    case Kind::kText: return value.empty() ? "\xE2\x80\x94" : std::string(value);
    case Kind::kKeybind: return FormatKeys(value);
    case Kind::kAction: return {};
    default: return std::string(value);
  }
}

std::string NormalizeText(const Setting& s, std::string_view text) {
  const auto begin = text.find_first_not_of(" \t");
  if (begin == std::string_view::npos) return {};
  text = text.substr(begin, text.find_last_not_of(" \t") - begin + 1);
  if (s.max_length == 0 || text.size() <= s.max_length) return std::string(text);
  auto size = s.max_length;
  while (size > 0 && (static_cast<unsigned char>(text[size]) & 0xC0) == 0x80) --size;
  return std::string(text.substr(0, size));
}

void CaptureLaunch(State& state, const std::vector<Setting>& catalog, const Values& values) {
  for (const auto& s : catalog) {
    if (s.apply == Apply::kRestart) state.launch[std::string(s.id)] = values.Get(s.id);
  }
}

void Open(State& state, const std::vector<Setting>& catalog, const Values& values, const Context& ctx) {
  state.opened.clear();
  for (const auto& s : catalog) {
    if (s.kind != Kind::kAction) state.opened[std::string(s.id)] = values.Get(s.id);
  }
  state.touched.clear();
  state.prompt = Prompt::kNone;
  state.page = Page::kMain;
  SelectFirst(state, catalog, values, ctx);
}

Outcome Handle(State& state, Command command, const std::vector<Setting>& catalog, Values& values, const Context& ctx) {
  if (state.prompt == Prompt::kUnsaved) {
    const auto outcome = HandlePrompt(state, command, values);
    if (outcome.kind == Outcome::Kind::kClose) ValidateSelection(state, catalog, values, ctx);
    return outcome;
  }
  ValidateSelection(state, catalog, values, ctx);
  switch (command) {
    case Command::kUp: MoveSelection(state, -1, catalog, values, ctx); return {};
    case Command::kDown: MoveSelection(state, 1, catalog, values, ctx); return {};
    case Command::kLeft: return ChangeSelected(state, -1, catalog, values, ctx);
    case Command::kRight: return ChangeSelected(state, 1, catalog, values, ctx);
    case Command::kAccept: return AcceptSelected(state, catalog, values, ctx);
    case Command::kBack: return GoBack(state, catalog, values, ctx);
    case Command::kTabPrev: CycleTab(state, -1, catalog, values, ctx); return {};
    case Command::kTabNext: CycleTab(state, 1, catalog, values, ctx); return {};
    case Command::kSave: return {Outcome::Kind::kSave, {}};
    case Command::kResetRow: {
      const auto* s = SelectedSetting(state, catalog, values, ctx);
      if (!s || !CanReset(s->kind)) return {};
      return ChangeValue(state, *s, values.Default(s->id), catalog, values, ctx);
    }
  }
  return {};
}

Outcome Click(State& state, int row, const std::vector<Setting>& catalog, Values& values, const Context& ctx) {
  if (state.prompt != Prompt::kNone || !SelectRow(state, row, catalog, values, ctx)) return {};
  return Handle(state, Command::kAccept, catalog, values, ctx);
}

Outcome Step(State& state, int row, int direction, const std::vector<Setting>& catalog, Values& values, const Context& ctx) {
  if (direction == 0 || state.prompt != Prompt::kNone || !SelectRow(state, row, catalog, values, ctx)) return {};
  return Handle(state, direction < 0 ? Command::kLeft : Command::kRight, catalog, values, ctx);
}

void SelectTab(State& state, int tab, const std::vector<Setting>& catalog, const Values& values, const Context& ctx) {
  if (tab < 0 || tab >= static_cast<int>(VisibleTabs(catalog, values, ctx).size())) return;
  state.tab = tab;
  state.page = Page::kMain;
  SelectFirst(state, catalog, values, ctx);
}

bool CommitText(State& state, std::string_view id, std::string_view text,
                const std::vector<Setting>& catalog, Values& values) {
  const auto* s = FindRow(catalog, id);
  return s && s->kind == Kind::kText && SetValue(state, id, NormalizeText(*s, text), values);
}

bool CommitKey(State& state, std::string_view id, std::string_view key,
               const std::vector<Setting>& catalog, Values& values) {
  const auto* s = FindRow(catalog, id);
  return s && s->kind == Kind::kKeybind && !key.empty() && SetValue(state, id, key, values);
}

void MarkSaved(State& state, const std::vector<Setting>&, const Values& values) {
  for (const auto& id : state.touched) state.opened[id] = values.Get(id);
  if (state.opened.contains("gow2_gfx_preset")) state.opened["gow2_gfx_preset"] = values.Get("gow2_gfx_preset");
  state.touched.clear();
}

void Discard(State& state, Values& values) {
  for (const auto& id : state.touched) values.Set(id, state.opened[id]);
  state.touched.clear();
}

std::vector<RowView> Rows(const State& state, const std::vector<Setting>& catalog, const Values& values, const Context& ctx) {
  std::vector<RowView> views;
  const auto rows = CurrentRows(state, catalog, values, ctx);
  for (int index = 0; index < static_cast<int>(rows.size()); ++index) {
    const auto* s = rows[index];
    const auto value = values.Get(s->id);
    RowView view;
    view.setting = s;
    view.value_text = FormatValue(*s, value, ctx);
    view.selectable = Selectable(*s);
    view.disabled = ctx.disabled.contains(s->id);
    view.selected = view.selectable && index == state.row;
    view.changed = s->kind != Kind::kInfo && s->kind != Kind::kAction && Differs(state.opened, s->id, value);
    view.restart_pending = Pending(state, *s, value);
    SetStepAvailability(view, value, ctx);
    views.push_back(view);
  }
  return views;
}

std::vector<std::string> RestartPending(const State& state, const std::vector<Setting>& catalog,
                                        const Values& values, const Context& ctx) {
  std::vector<std::string> labels;
  std::set<std::string> seen;
  for (const auto& s : catalog) {
    if (!IsVisible(s, values, ctx) || !Pending(state, s, values.Get(s.id))) continue;
    const std::string label(Label(s, ctx.lang));
    if (seen.insert(label).second) labels.push_back(label);
  }
  return labels;
}

std::string_view CurrentHelp(const State& state, const std::vector<Setting>& catalog,
                             const Values& values, const Context& ctx) {
  const auto* s = SelectedSetting(state, catalog, values, ctx);
  if (!s) return {};
  return ctx.disabled.contains(s->id) ? DisabledHelp(*s, ctx.lang) : Help(*s, ctx.lang);
}

}
