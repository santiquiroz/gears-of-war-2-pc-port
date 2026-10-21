// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Host tests of option values, visibility, navigation, and editing.
#include "gow2_options_model.h"
#include "gow2_graphics_quality.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <string_view>
#include <vector>

using namespace rex::glue::options;

int g_checks = 0;
int g_failures = 0;

void Expect(bool ok, std::string_view what) {
  ++g_checks;
  if (ok) return;
  ++g_failures;
  std::printf("FAIL: %.*s\n", static_cast<int>(what.size()), what.data());
}

class FakeValues final : public Values {
 public:
  std::map<std::string, std::string, std::less<>> current;
  std::map<std::string, std::string, std::less<>> defaults;
  std::string rejected;

  std::string Get(std::string_view id) const override {
    const auto found = current.find(id);
    return found == current.end() ? "" : found->second;
  }

  bool Set(std::string_view id, std::string_view value) override {
    if (id == rejected) return false;
    current[std::string(id)] = value;
    return true;
  }

  std::string Default(std::string_view id) const override {
    const auto found = defaults.find(id);
    return found == defaults.end() ? "" : found->second;
  }
};

Setting MakeRow(std::string_view id, Kind kind, Tab tab = Tab::kDisplay) {
  Setting row;
  row.id = id;
  row.kind = kind;
  row.tab = tab;
  row.label_en = id;
  row.label_es = "Etiqueta";
  row.help_en = "English help";
  row.help_es = "Ayuda";
  return row;
}

std::vector<Setting> MakeCatalog() {
  std::vector<Setting> rows;
  rows.push_back(MakeRow("info", Kind::kInfo));
  rows.push_back(MakeRow("toggle", Kind::kToggle));
  rows.push_back(MakeRow("dependent", Kind::kText));
  rows.back().visible_if_id = "toggle";
  rows.back().visible_if_value = "true";
  rows.push_back(MakeRow("choice", Kind::kChoice));
  rows.back().choices = {{"a", "Alpha", "Alfa"}, {"b", "Beta", "Beta"}, {"c", "Gamma", "Gama"}};
  rows.push_back(MakeRow("number", Kind::kNumber));
  rows.back().min = 0;
  rows.back().max = 1;
  rows.back().step = 0.05;
  rows.back().display_scale = 100;
  rows.back().unit = "%";
  rows.push_back(MakeRow("text", Kind::kText));
  rows.back().max_length = 6;
  rows.push_back(MakeRow("key", Kind::kKeybind));
  rows.push_back(MakeRow("keybindings", Kind::kAction));
  rows.push_back(MakeRow("host_online", Kind::kAction));
  rows.push_back(MakeRow("pad_buttons", Kind::kAction));
  rows.push_back(MakeRow("run", Kind::kAction));
  rows.push_back(MakeRow("advanced", Kind::kToggle));
  rows.back().advanced = true;
  rows.push_back(MakeRow("android", Kind::kToggle));
  rows.back().platforms = kAndroid;
  rows.push_back(MakeRow("page_info", Kind::kInfo));
  rows.back().page = Page::kKeybinds;
  rows.push_back(MakeRow("page_key", Kind::kKeybind));
  rows.back().page = Page::kKeybinds;
  rows.push_back(MakeRow("host_text", Kind::kText));
  rows.back().page = Page::kHost;
  rows.push_back(MakeRow("pad_choice", Kind::kChoice));
  rows.back().page = Page::kPadButtons;
  rows.push_back(MakeRow("restart", Kind::kToggle, Tab::kGraphics));
  rows.back().apply = Apply::kRestart;
  rows.back().label_en = "Restart setting";
  rows.push_back(MakeRow("restart_duplicate", Kind::kToggle, Tab::kGraphics));
  rows.back().apply = Apply::kRestart;
  rows.back().label_en = "Restart setting";
  rows.push_back(MakeRow("hidden_restart", Kind::kToggle, Tab::kGraphics));
  rows.back().apply = Apply::kRestart;
  rows.back().advanced = true;
  return rows;
}

struct Fixture {
  std::vector<Setting> catalog = MakeCatalog();
  FakeValues values;
  Context context;
  State state;

  Fixture() {
    for (const auto& row : catalog) values.current[std::string(row.id)] = "false";
    values.current["toggle"] = "true";
    values.current["choice"] = "a";
    values.current["number"] = "0.5";
    values.current["text"] = "start";
    values.current["key"] = "F,V";
    values.current["info"] = "Read only";
    values.defaults = values.current;
    CaptureLaunch(state, catalog, values);
    Open(state, catalog, values, context);
  }
};

int IndexOf(const Fixture& f, std::string_view id) {
  const auto rows = PageRows(f.catalog, VisibleTabs(f.catalog, f.values, f.context)[f.state.tab],
                             f.state.page, f.values, f.context);
  for (size_t index = 0; index < rows.size(); ++index) {
    if (rows[index]->id == id) return static_cast<int>(index);
  }
  return -1;
}

const Setting& SettingOf(const Fixture& f, std::string_view id) {
  for (const auto& row : f.catalog) {
    if (row.id == id) return row;
  }
  return f.catalog.front();
}

void Select(Fixture& f, std::string_view id) { f.state.row = IndexOf(f, id); }

Outcome Send(Fixture& f, Command command) {
  return Handle(f.state, command, f.catalog, f.values, f.context);
}

void ExpectOutcome(const Outcome& outcome, Outcome::Kind kind, std::string_view id,
                   std::string_view what) {
  Expect(outcome.kind == kind && outcome.id == id, what);
}

void TestVisibility() {
  FakeValues values;
  Context context;
  Setting row = MakeRow("visible", Kind::kToggle);
  Expect(IsVisible(row, values, context), "ordinary row visible");
  row.platforms = kAndroid;
  Expect(!IsVisible(row, values, context), "Android row hidden on Windows");
  context.platform = kAndroid;
  Expect(IsVisible(row, values, context), "Android row visible on Android");
  row.platforms = kWindows;
  Expect(!IsVisible(row, values, context), "Windows row hidden on Android");
  context.platform = kWindows;
  row.platforms = kAllPlatforms;
  row.advanced = true;
  Expect(!IsVisible(row, values, context), "advanced hidden by default");
  context.advanced = true;
  Expect(IsVisible(row, values, context), "advanced context exposes row");
  row.advanced = false;
  const std::vector<Need> needs = {Need::kTouch, Need::kFidelityFx, Need::kDlss, Need::kNativeMouse};
  for (const auto need : needs) {
    row.need = need;
    Expect(!IsVisible(row, values, context), "missing capability hides row");
  }
  context.touch = context.fidelityfx = context.dlss = context.native_mouse = true;
  for (const auto need : needs) {
    row.need = need;
    Expect(IsVisible(row, values, context), "present capability exposes row");
  }
  row.need = Need::kNoNativeMouse;
  Expect(!IsVisible(row, values, context), "no-native requirement rejects native mouse");
  context.native_mouse = false;
  Expect(IsVisible(row, values, context), "no-native requirement permits fallback mouse");
  row.need = Need::kNoDlss;
  Expect(!IsVisible(row, values, context), "no-DLSS requirement rejects DLSS runtime");
  context.dlss = false;
  Expect(IsVisible(row, values, context), "no-DLSS requirement permits fallback runtime");
  row.need = Need::kNothing;
  row.dynamic_choices = true;
  Expect(!IsVisible(row, values, context), "missing dynamic choices hides row");
  context.dynamic_choices["visible"] = {};
  Expect(!IsVisible(row, values, context), "empty dynamic choices hides row");
  context.dynamic_choices["visible"] = {{"a", "A", "A"}};
  Expect(IsVisible(row, values, context), "nonempty dynamic choices exposes row");
  row.dynamic_choices = false;
  row.visible_if_id = "condition";
  row.visible_if_value = "enabled";
  Expect(!IsVisible(row, values, context), "equality condition rejects missing value");
  values.current["condition"] = "enabled";
  Expect(IsVisible(row, values, context), "equality condition matches exact value");
  row.visible_if_value = "!enabled";
  Expect(!IsVisible(row, values, context), "inequality condition rejects matching value");
  values.current["condition"] = "other";
  Expect(IsVisible(row, values, context), "inequality condition accepts different value");
  row.visible_if_value = "!";
  Expect(IsVisible(row, values, context), "bare negation compares with empty text");
  values.current["condition"] = "";
  Expect(!IsVisible(row, values, context), "bare negation rejects empty text");
}

void TestUpscalerVisibility() {
  FakeValues values;
  Context context;
  const auto* fallback = FindSetting("upscaler");
  const auto* dlss = FindSetting("upscaler_dlss");
  Expect(fallback && dlss, "both upscaler variants are cataloged");
  if (!fallback || !dlss) return;
  Expect(IsVisible(*fallback, values, context), "fallback upscaler visible without DLSS");
  Expect(!IsVisible(*dlss, values, context), "DLSS upscaler hidden without DLSS");
  context.dlss = true;
  Expect(!IsVisible(*fallback, values, context), "fallback upscaler hidden with DLSS");
  Expect(IsVisible(*dlss, values, context), "DLSS upscaler visible with DLSS");
}

void TestTabAndPageFiltering() {
  Fixture f;
  const auto tabs = VisibleTabs(f.catalog, f.values, f.context);
  Expect(tabs == std::vector<Tab>{Tab::kDisplay, Tab::kGraphics}, "tabs in enum order");
  auto reversed = f.catalog;
  std::reverse(reversed.begin(), reversed.end());
  Expect(VisibleTabs(reversed, f.values, f.context) == tabs, "tab order independent of catalog order");
  auto orphan = MakeRow("orphan", Kind::kText, Tab::kSystem);
  orphan.page = Page::kHost;
  f.catalog.push_back(orphan);
  Expect(VisibleTabs(f.catalog, f.values, f.context) == tabs, "subpage alone does not expose tab");
  const auto rows = PageRows(f.catalog, Tab::kDisplay, Page::kKeybinds, f.values, f.context);
  Expect(rows.size() == 2 && rows[0]->id == "page_info" && rows[1]->id == "page_key",
         "PageRows preserves catalog order and filters page");
  Expect(PageRows(f.catalog, Tab::kAudio, Page::kMain, f.values, f.context).empty(), "missing page empty");
}

void TestStepping() {
  Fixture f;
  const auto& toggle = SettingOf(f, "toggle");
  Expect(StepValue(toggle, "true", 1, f.context) == "false", "toggle true becomes false");
  Expect(StepValue(toggle, "false", -1, f.context) == "true", "toggle false becomes true");
  Expect(StepValue(toggle, "invalid", 20, f.context) == "true", "nontrue toggle treated false");
  const auto& choice = SettingOf(f, "choice");
  Expect(StepValue(choice, "c", 1, f.context) == "a", "choice forward wraps");
  Expect(StepValue(choice, "a", -9, f.context) == "c", "choice backward wraps and uses sign");
  Expect(StepValue(choice, "a", 9, f.context) == "b", "choice positive direction uses sign");
  Expect(StepValue(choice, "missing", 1, f.context) == "a", "unknown choice forward picks first");
  Expect(StepValue(choice, "missing", -1, f.context) == "c", "unknown choice backward picks last");
  auto dynamic = choice;
  dynamic.dynamic_choices = true;
  Expect(ChoicesOf(dynamic, f.context).empty(), "missing dynamic options empty");
  f.context.dynamic_choices["choice"] = {{"x", "Ex", "Equis"}, {"y", "Why", "Ye"}};
  Expect(ChoicesOf(dynamic, f.context).size() == 2, "dynamic choices replace static choices");
  Expect(StepValue(dynamic, "x", 1, f.context) == "y", "step uses dynamic choices");
  dynamic.choices.clear();
  f.context.dynamic_choices.clear();
  Expect(StepValue(dynamic, "unchanged", 1, f.context) == "unchanged", "no choices leaves value");
  Expect(StepValue(toggle, "true", 0, f.context) == "true", "zero step preserves value");
  const auto& number = SettingOf(f, "number");
  Expect(StepValue(number, "0.5", 1, f.context) == "0.55", "number shortest fractional text");
  Expect(StepValue(number, "0.53", 1, f.context) == "0.6", "number snaps to grid");
  Expect(StepValue(number, "invalid", 1, f.context) == "0.05", "invalid number starts at minimum");
  Expect(StepValue(number, "1", 1, f.context) == "1", "number clamps upper bound");
  Expect(StepValue(number, "0", -1, f.context) == "0", "number clamps lower bound");
  auto integer = number;
  integer.min = -2;
  integer.max = 4000;
  integer.step = 1;
  Expect(StepValue(integer, "1", 1, f.context) == "2", "number trims trailing zeros");
  Expect(StepValue(integer, "3999", 1, f.context) == "4000", "large number avoids exponent notation");
  Expect(StepValue(integer, "-2", 1, f.context) == "-1", "negative number text");
  integer.min = -1;
  integer.step = 0.1;
  Expect(StepValue(integer, "-0.1", 1, f.context) == "0", "number never formats negative zero");
  integer.min = 0.03;
  integer.step = 0.1;
  Expect(StepValue(integer, "0.13", 1, f.context) == "0.23", "number grid anchored at minimum");
  integer.min = 0;
  integer.step = 0.000001;
  Expect(StepValue(integer, "0", 1, f.context) == "0.000001", "number retains six fractional decimals");
  integer.step = 0.0000001;
  Expect(StepValue(integer, "0", 1, f.context) == "0", "number limits text to six decimals");
  for (const auto kind : {Kind::kText, Kind::kKeybind, Kind::kAction, Kind::kInfo}) {
    const auto row = MakeRow("other", kind);
    Expect(StepValue(row, "unchanged", 1, f.context) == "unchanged", "nonsteppable kind preserved");
  }
}

void TestFormatting() {
  Fixture f;
  const auto& toggle = SettingOf(f, "toggle");
  Expect(FormatValue(toggle, "true", f.context) == Text(TextId::kOn, Lang::kEn), "toggle English on");
  Expect(FormatValue(toggle, "other", f.context) == Text(TextId::kOff, Lang::kEn), "invalid toggle off");
  f.context.lang = Lang::kEs;
  Expect(FormatValue(toggle, "true", f.context) == Text(TextId::kOn, Lang::kEs), "toggle Spanish on");
  Expect(FormatValue(SettingOf(f, "choice"), "a", f.context) == "Alfa", "choice localized label");
  Expect(FormatValue(SettingOf(f, "choice"), "unknown", f.context) == "unknown", "unknown choice raw text");
  auto dynamic = SettingOf(f, "choice");
  dynamic.dynamic_choices = true;
  f.context.dynamic_choices["choice"] = {{"x", "Ex", "Equis"}};
  Expect(FormatValue(dynamic, "x", f.context) == "Equis", "choice formatting uses dynamic labels");
  Expect(FormatValue(SettingOf(f, "number"), "0.55", f.context) == "55%", "number display scale and percent");
  auto number = SettingOf(f, "number");
  number.display_scale = 1;
  number.decimals = 2;
  number.unit = "x";
  Expect(FormatValue(number, "1.5", f.context) == "1.50x", "number exact decimals");
  number.decimals = 0;
  number.unit = " s";
  Expect(FormatValue(number, "10", f.context) == "10 s", "number retains space in unit");
  Expect(FormatValue(SettingOf(f, "text"), "", f.context) == "\xE2\x80\x94", "empty text displays em dash");
  Expect(FormatValue(SettingOf(f, "text"), "value", f.context) == "value", "text preserved");
  Expect(FormatValue(SettingOf(f, "key"), "F,V,K", f.context) == "F / V / K", "all key commas replaced");
  Expect(FormatValue(SettingOf(f, "run"), "ignored", f.context).empty(), "action formats empty");
  Expect(FormatValue(SettingOf(f, "info"), "information", f.context) == "information", "info formats raw text");
}

void TestNormalizeText() {
  auto row = MakeRow("text", Kind::kText);
  Expect(NormalizeText(row, " \t value \t ") == "value", "trim spaces and tabs");
  Expect(NormalizeText(row, " \t ").empty(), "blank text trimmed empty");
  Expect(NormalizeText(row, "\nvalue\n") == "\nvalue\n", "newlines are not trimmed");
  Expect(NormalizeText(row, "abcdef") == "abcdef", "zero maximum has no limit");
  row.max_length = 3;
  Expect(NormalizeText(row, " abcdef ") == "abc", "trim before byte truncation");
  const std::string accented = "a\xC3\xA9" "b";
  row.max_length = 2;
  Expect(NormalizeText(row, accented) == "a", "cut before partial two byte sequence");
  row.max_length = 3;
  Expect(NormalizeText(row, accented) == "a\xC3\xA9", "complete multibyte sequence retained");
  const std::string emoji = "\xF0\x9F\x98\x80" "z";
  Expect(NormalizeText(row, emoji).empty(), "cut before partial four byte sequence");
  row.max_length = 4;
  Expect(NormalizeText(row, emoji) == emoji.substr(0, 4), "complete four byte sequence retained");
  const std::string euro = "x\xE2\x82\xAC" "y";
  row.max_length = 3;
  Expect(NormalizeText(row, euro) == "x", "cut before partial three byte sequence");
}

void TestOpeningAndNavigation() {
  Fixture f;
  Expect(f.state.row == 1, "open selects first non-info row");
  Expect(f.state.opened.contains("page_key") && !f.state.opened.contains("keybindings"),
         "open captures other pages but excludes actions");
  Expect(f.state.launch.size() == 3, "capture launch includes hidden restart rows");
  f.values.current["restart"] = "true";
  CaptureLaunch(f.state, f.catalog, f.values);
  Expect(f.state.launch["restart"] == "true", "capture launch overwrites prior values");
  Send(f, Command::kUp);
  Expect(f.state.row == 1, "up stops before leading info");
  Send(f, Command::kDown);
  Expect(f.state.row == 2, "down selects next selectable row");
  Send(f, Command::kUp);
  Expect(f.state.row == 1, "up selects previous selectable row");
  f.state.row = 999;
  Send(f, Command::kDown);
  Expect(f.state.row == IndexOf(f, "run"), "invalid selection clamps to last row");
  Send(f, Command::kDown);
  Expect(f.state.row == IndexOf(f, "run"), "down does not wrap");
  f.state.prompt = Prompt::kUnsaved;
  f.state.touched.insert("toggle");
  f.state.tab = 999;
  f.state.page = Page::kHost;
  Open(f.state, f.catalog, f.values, f.context);
  Expect(f.state.tab == 1 && f.state.page == Page::kMain && f.state.row == 0,
         "open clamps tab and selects first row");
  Expect(f.state.prompt == Prompt::kNone && f.state.touched.empty(), "open resets prompt and touched");
  Send(f, Command::kTabNext);
  Expect(f.state.tab == 0 && f.state.row == 1, "next tab wraps and skips info");
  Send(f, Command::kTabPrev);
  Expect(f.state.tab == 1, "previous tab wraps");
  SelectTab(f.state, 0, f.catalog, f.values, f.context);
  Expect(f.state.tab == 0 && f.state.row == 1, "select valid tab");
  SelectTab(f.state, -1, f.catalog, f.values, f.context);
  SelectTab(f.state, 50, f.catalog, f.values, f.context);
  Expect(f.state.tab == 0, "invalid explicit tabs ignored");
  auto info_only = std::vector<Setting>{MakeRow("info", Kind::kInfo)};
  Open(f.state, info_only, f.values, f.context);
  Expect(f.state.row == 0, "only-info page selection is zero");
  Expect(CurrentHelp(f.state, {}, f.values, f.context).empty(), "empty catalog has no help");
  Open(f.state, {}, f.values, f.context);
  Expect(f.state.row == 0 && f.state.tab == 0, "empty catalog opens safely");
}

void TestAcceptAndPages() {
  Fixture f;
  Select(f, "toggle");
  ExpectOutcome(Send(f, Command::kAccept), Outcome::Kind::kChanged, "toggle", "accept toggles");
  Select(f, "choice");
  ExpectOutcome(Send(f, Command::kAccept), Outcome::Kind::kChanged, "choice", "accept cycles choice");
  Expect(f.values.Get("choice") == "b", "accept choice advances forward");
  Select(f, "number");
  ExpectOutcome(Send(f, Command::kAccept), Outcome::Kind::kNone, "", "accept number has no action");
  Select(f, "text");
  ExpectOutcome(Send(f, Command::kAccept), Outcome::Kind::kEditText, "text", "accept edits text");
  Select(f, "key");
  ExpectOutcome(Send(f, Command::kAccept), Outcome::Kind::kCaptureKey, "key", "accept captures key");
  Select(f, "run");
  ExpectOutcome(Send(f, Command::kAccept), Outcome::Kind::kRunAction, "run", "accept runs ordinary action");
  Select(f, "keybindings");
  ExpectOutcome(Send(f, Command::kAccept), Outcome::Kind::kNone, "", "keybindings opens page without action");
  Expect(f.state.page == Page::kKeybinds && f.state.row == 1, "subpage opens on first selectable row");
  Send(f, Command::kBack);
  Expect(f.state.page == Page::kMain && f.state.row == IndexOf(f, "keybindings"), "back returns to keybindings action");
  Select(f, "host_online");
  Send(f, Command::kAccept);
  Expect(f.state.page == Page::kHost && f.state.row == 0, "host action opens host page");
  Send(f, Command::kBack);
  Expect(f.state.page == Page::kMain && f.state.row == IndexOf(f, "host_online"), "back returns to host action");
  Select(f, "pad_buttons");
  ExpectOutcome(Send(f, Command::kAccept), Outcome::Kind::kNone, "", "controller buttons opens page without action");
  Expect(f.state.page == Page::kPadButtons, "controller buttons action opens its page");
  Send(f, Command::kBack);
  Expect(f.state.page == Page::kMain && f.state.row == IndexOf(f, "pad_buttons"), "back returns to controller buttons action");
  Select(f, "host_online");
  Send(f, Command::kAccept);
  Send(f, Command::kTabNext);
  Expect(f.state.page == Page::kMain && f.state.tab == 1, "tab command leaves subpage");
  SelectTab(f.state, 0, f.catalog, f.values, f.context);
  f.context.lang = Lang::kEs;
  Expect(CurrentHelp(f.state, f.catalog, f.values, f.context) == "Ayuda", "current help localized");
}

void TestEditingAndTouched() {
  Fixture f;
  Select(f, "toggle");
  Send(f, Command::kRight);
  Expect(f.state.touched.contains("toggle"), "changed value becomes touched");
  Send(f, Command::kLeft);
  Expect(!f.state.touched.contains("toggle"), "return to opened value clears touched");
  Select(f, "number");
  ExpectOutcome(Send(f, Command::kRight), Outcome::Kind::kChanged, "number", "right steps number");
  Expect(f.values.Get("number") == "0.55", "right stores step result");
  f.values.current["number"] = "1";
  ExpectOutcome(Send(f, Command::kRight), Outcome::Kind::kNone, "", "clamped unchanged step has no outcome");
  f.values.rejected = "number";
  ExpectOutcome(Send(f, Command::kLeft), Outcome::Kind::kNone, "", "rejected step has no outcome");
  Expect(f.values.Get("number") == "1", "rejected value preserved");
  f.values.rejected.clear();
  Expect(CommitText(f.state, "text", " \t abcdefghi \t", f.catalog, f.values), "text commit succeeds");
  Expect(f.values.Get("text") == "abcdef" && f.state.touched.contains("text"), "text commit normalizes and tracks touch");
  Expect(CommitText(f.state, "text", "abcdef", f.catalog, f.values), "unchanged text commit returns Set success");
  Expect(!CommitText(f.state, "key", "value", f.catalog, f.values), "text commit rejects wrong kind");
  Expect(!CommitText(f.state, "missing", "value", f.catalog, f.values), "text commit rejects missing id");
  f.values.rejected = "text";
  Expect(!CommitText(f.state, "text", "new", f.catalog, f.values), "text commit reports rejection");
  f.values.rejected.clear();
  Expect(!CommitKey(f.state, "key", "", f.catalog, f.values), "key commit rejects empty key");
  Expect(!CommitKey(f.state, "text", "K", f.catalog, f.values), "key commit rejects wrong kind");
  Expect(!CommitKey(f.state, "missing", "K", f.catalog, f.values), "key commit rejects missing id");
  Expect(CommitKey(f.state, "key", "Q,E", f.catalog, f.values), "key commit succeeds");
  Expect(f.values.Get("key") == "Q,E" && f.state.touched.contains("key"), "key commit stores and tracks touch");
  f.values.rejected = "key";
  Expect(!CommitKey(f.state, "key", "Z", f.catalog, f.values), "key commit reports rejection");
  f.values.rejected.clear();
  for (const auto id : {"toggle", "choice", "number", "text", "key"}) {
    Select(f, id);
    f.values.current[id] = "different";
    ExpectOutcome(Send(f, Command::kResetRow), Outcome::Kind::kChanged, id, "reset applies default to editable kind");
    Expect(f.values.Get(id) == f.values.Default(id), "reset stores default");
    ExpectOutcome(Send(f, Command::kResetRow), Outcome::Kind::kNone, "", "reset unchanged default no outcome");
  }
  Select(f, "run");
  ExpectOutcome(Send(f, Command::kResetRow), Outcome::Kind::kNone, "", "action reset ignored");
  CommitText(f.state, "text", "saved", f.catalog, f.values);
  MarkSaved(f.state, f.catalog, f.values);
  Expect(f.state.touched.empty() && f.state.opened["text"] == "saved", "mark saved updates baseline and clears touched");
  Expect(f.state.launch["restart"] == "false", "mark saved preserves launch baseline");
  f.values.current["info"] = "externally updated";
  MarkSaved(f.state, f.catalog, f.values);
  Expect(f.state.opened["info"] == "Read only", "mark saved leaves untouched baselines unchanged");
  CommitText(f.state, "text", "later", f.catalog, f.values);
  Discard(f.state, f.values);
  Expect(f.values.Get("text") == "saved" && f.state.touched.empty(), "discard restores saved baseline");
}

void TestSelectionRevalidation() {
  Fixture f;
  Select(f, "dependent");
  Expect(CommitKey(f.state, "key", "Q", f.catalog, f.values), "independent commit succeeds");
  f.values.current["toggle"] = "false";
  CommitKey(f.state, "key", "R", f.catalog, f.values);
  Expect(f.state.row == IndexOf(f, "choice"), "disappearing selected row selects row below at its index");
  auto hiding = MakeRow("hiding", Kind::kToggle);
  hiding.visible_if_id = "hiding";
  hiding.visible_if_value = "true";
  f.catalog = {hiding, MakeRow("between", Kind::kInfo), MakeRow("below", Kind::kToggle)};
  f.values.current["hiding"] = "true";
  Open(f.state, f.catalog, f.values, f.context);
  ExpectOutcome(Send(f, Command::kRight), Outcome::Kind::kChanged, "hiding", "self-hiding toggle changes");
  Expect(f.state.row == 1 && Rows(f.state, f.catalog, f.values, f.context)[1].setting->id == "below",
         "value change skips info at vanished selection and chooses below");
  f.catalog = {MakeRow("above", Kind::kToggle), hiding, MakeRow("trailing", Kind::kInfo)};
  f.values.current["hiding"] = "true";
  f.values.defaults["hiding"] = "false";
  Open(f.state, f.catalog, f.values, f.context);
  f.state.row = 1;
  Send(f, Command::kResetRow);
  Expect(f.state.row == 0, "reset hiding selected row falls back above trailing info");
  f.catalog = {hiding, MakeRow("only_info", Kind::kInfo)};
  f.values.current["hiding"] = "true";
  Open(f.state, f.catalog, f.values, f.context);
  Send(f, Command::kRight);
  Expect(f.state.row == 0, "self-hiding final selectable row leaves zero selection");
  auto info = MakeRow("last_info", Kind::kInfo);
  f.catalog = {MakeRow("only", Kind::kToggle), info};
  f.state.row = 1;
  f.values.defaults["only"] = "true";
  Open(f.state, f.catalog, f.values, f.context);
  f.state.row = 1;
  Send(f, Command::kDown);
  Expect(f.state.row == 0, "selection on trailing info falls back above");
  f.catalog = {MakeRow("head", Kind::kInfo), MakeRow("middle", Kind::kToggle), MakeRow("tail", Kind::kInfo)};
  Open(f.state, f.catalog, f.values, f.context);
  Send(f, Command::kDown);
  Expect(f.state.row == 1, "navigation skips trailing info");
  Send(f, Command::kUp);
  Expect(f.state.row == 1, "navigation skips leading info");
}

void TestUnsavedPrompt() {
  Fixture f;
  ExpectOutcome(Send(f, Command::kSave), Outcome::Kind::kSave, "", "save command emits save");
  ExpectOutcome(Send(f, Command::kBack), Outcome::Kind::kClose, "", "clean main page closes");
  Send(f, Command::kRight);
  ExpectOutcome(Send(f, Command::kBack), Outcome::Kind::kNone, "", "dirty back opens prompt");
  Expect(f.state.prompt == Prompt::kUnsaved && f.state.prompt_choice == 0, "prompt begins with Save");
  Send(f, Command::kLeft);
  Expect(f.state.prompt_choice == 0, "prompt left clamps");
  for (int index = 0; index < 4; ++index) Send(f, Command::kRight);
  Expect(f.state.prompt_choice == 2, "prompt right clamps");
  const int row = f.state.row;
  ExpectOutcome(Send(f, Command::kDown), Outcome::Kind::kNone, "", "prompt ignores navigation");
  Expect(f.state.row == row, "prompt navigation preserves row");
  ExpectOutcome(Send(f, Command::kSave), Outcome::Kind::kNone, "", "prompt ignores save shortcut");
  ExpectOutcome(Send(f, Command::kAccept), Outcome::Kind::kNone, "", "Cancel keeps menu open");
  Expect(f.state.prompt == Prompt::kNone && !f.state.touched.empty(), "Cancel keeps unsaved changes");
  Send(f, Command::kBack);
  ExpectOutcome(Send(f, Command::kBack), Outcome::Kind::kNone, "", "Back dismisses prompt");
  Expect(f.state.prompt == Prompt::kNone && !f.state.touched.empty(), "Back dismissal keeps changes");
  Send(f, Command::kBack);
  ExpectOutcome(Send(f, Command::kAccept), Outcome::Kind::kSaveAndClose, "", "Save prompt emits save and close");
  Expect(f.state.prompt == Prompt::kNone, "Save prompt cleared");
  Send(f, Command::kBack);
  Send(f, Command::kRight);
  ExpectOutcome(Send(f, Command::kAccept), Outcome::Kind::kClose, "", "Discard prompt closes");
  Expect(f.state.prompt == Prompt::kNone && f.state.touched.empty() && f.values.Get("toggle") == "true",
         "Discard restores value and clears prompt");
}

void TestPointerCommandsAndRows() {
  Fixture f;
  const int original = f.state.row;
  ExpectOutcome(Click(f.state, 0, f.catalog, f.values, f.context), Outcome::Kind::kNone, "", "click info ignored");
  ExpectOutcome(Step(f.state, 0, 1, f.catalog, f.values, f.context), Outcome::Kind::kNone, "", "step info ignored");
  Expect(f.state.row == original && f.state.touched.empty(), "info pointer commands preserve state");
  ExpectOutcome(Click(f.state, -1, f.catalog, f.values, f.context), Outcome::Kind::kNone, "", "negative click ignored");
  ExpectOutcome(Step(f.state, 999, 1, f.catalog, f.values, f.context), Outcome::Kind::kNone, "", "out of range step ignored");
  ExpectOutcome(Click(f.state, IndexOf(f, "text"), f.catalog, f.values, f.context), Outcome::Kind::kEditText, "text", "click selects and accepts");
  ExpectOutcome(Step(f.state, IndexOf(f, "number"), -9, f.catalog, f.values, f.context), Outcome::Kind::kChanged, "number", "pointer step uses direction sign");
  Expect(f.values.Get("number") == "0.45", "pointer negative step decreases");
  auto rows = Rows(f.state, f.catalog, f.values, f.context);
  Expect(rows.size() == PageRows(f.catalog, Tab::kDisplay, Page::kMain, f.values, f.context).size(), "one view per page row");
  Expect(!rows[0].selectable && !rows[0].selected && rows[0].value_text == "Read only", "info view read-only");
  const auto& number = rows[IndexOf(f, "number")];
  Expect(number.setting->id == "number" && number.selected && number.changed && number.can_decrease && number.can_increase,
         "number view includes selection change and step flags");
  Expect(rows[IndexOf(f, "toggle")].can_decrease && rows[IndexOf(f, "toggle")].can_increase, "toggle has both directions");
  Expect(rows[IndexOf(f, "choice")].can_decrease && rows[IndexOf(f, "choice")].can_increase, "multiple choices have both directions");
  f.catalog[IndexOf(f, "choice")].choices.resize(1);
  rows = Rows(f.state, f.catalog, f.values, f.context);
  Expect(!rows[IndexOf(f, "choice")].can_increase && !rows[IndexOf(f, "choice")].can_decrease, "single choice has no directions");
  f.values.current["number"] = "0";
  rows = Rows(f.state, f.catalog, f.values, f.context);
  Expect(!rows[IndexOf(f, "number")].can_decrease && rows[IndexOf(f, "number")].can_increase, "number lower bound directions");
  f.values.current["number"] = "1";
  rows = Rows(f.state, f.catalog, f.values, f.context);
  Expect(rows[IndexOf(f, "number")].can_decrease && !rows[IndexOf(f, "number")].can_increase, "number upper bound directions");
  f.state.opened.erase("number");
  rows = Rows(f.state, f.catalog, f.values, f.context);
  Expect(!rows[IndexOf(f, "number")].changed, "absent opened baseline means unchanged");
}

void TestInfoAndActionRowsNeverShowChanged() {
  Fixture f;
  f.state.opened["run"] = f.values.Get("run");
  f.values.current["info"] = "Updated status";
  f.values.current["run"] = "Updated action";

  const auto rows = Rows(f.state, f.catalog, f.values, f.context);
  const auto& info = rows[IndexOf(f, "info")];
  Expect(info.value_text == "Updated status", "info view displays updated computed text");
  Expect(!info.changed, "updated info value never shows changed");
  Expect(!rows[IndexOf(f, "run")].changed, "updated action value never shows changed even with an opened baseline");
}

void TestRestartPending() {
  Fixture f;
  f.values.current["restart"] = "true";
  f.values.current["restart_duplicate"] = "true";
  f.values.current["hidden_restart"] = "true";
  auto pending = RestartPending(f.state, f.catalog, f.values, f.context);
  Expect(pending == std::vector<std::string>{"Restart setting"}, "pending deduplicates labels and excludes invisible rows");
  f.context.advanced = true;
  pending = RestartPending(f.state, f.catalog, f.values, f.context);
  Expect(pending == std::vector<std::string>{"Restart setting", "hidden_restart"}, "pending catalog order and advanced visibility");
  f.context.lang = Lang::kEs;
  Expect(RestartPending(f.state, f.catalog, f.values, f.context) == std::vector<std::string>{"Etiqueta"}, "pending localized labels deduplicated");
  SelectTab(f.state, 1, f.catalog, f.values, f.context);
  auto rows = Rows(f.state, f.catalog, f.values, f.context);
  Expect(rows[0].restart_pending && rows[0].changed, "restart view compares launch and opened independently");
  MarkSaved(f.state, f.catalog, f.values);
  rows = Rows(f.state, f.catalog, f.values, f.context);
  Expect(rows[0].restart_pending, "save does not clear restart pending");
  f.state.launch.erase("restart");
  rows = Rows(f.state, f.catalog, f.values, f.context);
  Expect(!rows[0].restart_pending, "missing launch baseline not pending");
  f.values.current["restart_duplicate"] = "false";
  f.values.current["hidden_restart"] = "false";
  Expect(RestartPending(f.state, f.catalog, f.values, f.context).empty(), "restored values and missing launch produce no pending rows");
  auto subpage = MakeRow("subpage_restart", Kind::kToggle, Tab::kSystem);
  subpage.page = Page::kKeybinds;
  subpage.apply = Apply::kRestart;
  f.catalog.push_back(subpage);
  f.state.launch["subpage_restart"] = "false";
  f.values.current["subpage_restart"] = "true";
  f.context.lang = Lang::kEn;
  Expect(RestartPending(f.state, f.catalog, f.values, f.context) == std::vector<std::string>{"subpage_restart"},
         "pending includes visible subpage row even without main tab rows");
  f.catalog.back().platforms = kAndroid;
  Expect(RestartPending(f.state, f.catalog, f.values, f.context).empty(), "pending excludes incompatible platform");
}

class QualityValues final : public Values {
 public:
  std::map<std::string, std::string, std::less<>> current;

  QualityValues() {
    for (const auto& setting : rex::glue::graphics_quality::Settings()) current[std::string(setting.id)] = "auto";
  }

  std::string Get(std::string_view id) const override {
    if (id == "gow2_gfx_preset")
      return rex::glue::graphics_quality::DetectPreset([this](std::string_view key) { return Get(key); });
    const auto found = current.find(id);
    return found == current.end() ? "" : found->second;
  }

  bool Set(std::string_view id, std::string_view value) override {
    if (id != "gow2_gfx_preset") { current[std::string(id)] = value; return true; }
    if (value != "auto" && value != "low" && value != "medium" && value != "high") return false;
    for (const auto& setting : rex::glue::graphics_quality::Settings()) {
      current[std::string(setting.id)] = value == "auto" ? "auto" :
          rex::glue::graphics_quality::PresetValue(setting, value);
    }
    return true;
  }

  std::string Default(std::string_view) const override { return "auto"; }
};

void TestQualityPresetTransactions() {
  namespace quality = rex::glue::graphics_quality;
  const auto& catalog = Catalog();
  QualityValues values;
  Context context;
  State state;
  values.current["gow2_gfx_bloom"] = "off";
  values.current["gow2_gfx_max_shadow_resolution"] = "512";
  const auto original = values.current;
  CaptureLaunch(state, catalog, values);
  Open(state, catalog, values, context);
  SelectTab(state, 1, catalog, values, context);
  const auto rows = PageRows(catalog, Tab::kGraphics, Page::kMain, values, context);
  const auto index_of = [&](std::string_view id) {
    const auto found = std::find_if(rows.begin(), rows.end(), [id](const Setting* row) { return row->id == id; });
    return static_cast<int>(found - rows.begin());
  };
  const int preset = index_of("gow2_gfx_preset");
  const auto* preset_setting = FindSetting("gow2_gfx_preset");
  Expect(StepValue(*preset_setting, "high", 1, context) == "low", "derived Custom is skipped when cycling presets");
  Expect(StepValue(*preset_setting, "custom", -1, context) == "high", "Custom can step backwards to High");
  Step(state, preset, 1, catalog, values, context);
  Expect(values.Get("gow2_gfx_preset") == "low", "choosing Low replaces a custom combination");
  Expect(!state.touched.contains("gow2_gfx_preset"), "derived preset is never persisted or restored as a separate value");
  for (const auto& setting : quality::Settings()) {
    const auto id = std::string(setting.id);
    Expect(values.Get(id) == quality::PresetValue(setting, "low"), "Low updates every quality constituent");
    Expect(state.touched.contains(id) == (values.Get(id) != original.at(id)), "preset dirty state tracks each actual constituent change");
  }
  Expect(!RestartPending(state, catalog, values, context).empty(), "preset raises restart notice");
  Discard(state, values);
  Expect(values.current == original && state.touched.empty(), "discard restores the exact original custom combination");

  Step(state, preset, 1, catalog, values, context);
  MarkSaved(state, catalog, values);
  const auto saved_low = values.current;
  Expect(state.opened.at("gow2_gfx_preset") == "low", "save refreshes the derived preset baseline");
  Step(state, index_of("gow2_gfx_motion_blur"), 1, catalog, values, context);
  Expect(values.Get("gow2_gfx_preset") == "custom", "manual quality change shows Custom immediately");
  Expect(Rows(state, catalog, values, context)[preset].changed, "derived preset shows unsaved change after manual edit");
  Discard(state, values);
  Expect(values.current == saved_low && values.Get("gow2_gfx_preset") == "low", "discard after saving restores all saved Low values");
  Expect(!RestartPending(state, catalog, values, context).empty(), "save and discard preserve restart notice relative to launch");

  state.row = preset;
  Handle(state, Command::kResetRow, catalog, values, context);
  for (const auto& setting : quality::Settings()) Expect(values.Get(setting.id) == "auto", "reset preset restores platform defaults for every row");
  Expect(!state.touched.empty(), "reset preset tracks changes even when the derived label matches a default");
  Discard(state, values);
  Expect(values.current == saved_low, "discard after default restores the saved explicit values");

  values.Set("gow2_gfx_preset", "auto");
  Open(state, catalog, values, context);
  Step(state, preset, -1, catalog, values, context);
  Expect(values.Get("gow2_gfx_preset") == "medium", "automatic desktop High cycles to Medium");
  Step(state, preset, 1, catalog, values, context);
  Expect(values.Get("gow2_gfx_light_environment_shadows") == "on", "High explicitly enables retail object shadows");
  Discard(state, values);
  for (const auto& setting : quality::Settings()) Expect(values.Get(setting.id) == "auto", "discard explicit High restores all automatic values");
}

int main() {
  TestVisibility();
  TestUpscalerVisibility();
  TestTabAndPageFiltering();
  TestStepping();
  TestFormatting();
  TestNormalizeText();
  TestOpeningAndNavigation();
  TestAcceptAndPages();
  TestEditingAndTouched();
  TestSelectionRevalidation();
  TestUnsavedPrompt();
  TestPointerCommandsAndRows();
  TestInfoAndActionRowsNeverShowChanged();
  TestRestartPending();
  TestQualityPresetTransactions();
  std::printf("options_model_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
