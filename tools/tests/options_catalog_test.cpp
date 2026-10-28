// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Host-side test of the PC Options catalog data (no game data, no SDK).
#include "gow2_options_catalog.h"
#include "gow2_graphics_quality.h"

#include <cstdio>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace options = rex::glue::options;

int g_checks = 0;
int g_failures = 0;
void Expect(bool ok, std::string_view what) {
  ++g_checks;
  if (ok) return;
  ++g_failures;
  std::printf("FAIL: %.*s\n", int(what.size()), what.data());
}

void TestIdsUnique() {
  const std::vector<options::Setting>& catalog = options::Catalog();
  std::unordered_set<std::string_view> seen;
  for (const options::Setting& s : catalog) {
    Expect(!s.id.empty(), "id is non-empty");
    Expect(seen.insert(s.id).second, std::string("unique id: ") + std::string(s.id));
  }
}

void TestFindSetting() {
  for (const options::Setting& s : options::Catalog()) {
    const options::Setting* found = options::FindSetting(s.id);
    Expect(found != nullptr, std::string("FindSetting finds: ") + std::string(s.id));
    Expect(found != nullptr && found->id == s.id, "FindSetting returns the same row");
  }
  Expect(options::FindSetting("nope") == nullptr, "FindSetting returns nullptr for nope");
  Expect(options::FindSetting("") == nullptr, "FindSetting returns nullptr for empty");
}

void TestLabels() {
  for (const options::Setting& s : options::Catalog()) {
    Expect(!s.label_en.empty(), std::string("label_en: ") + std::string(s.id));
    Expect(!s.label_es.empty(), std::string("label_es: ") + std::string(s.id));
  }
}

void TestChoices() {
  for (const options::Setting& s : options::Catalog()) {
    if (s.kind != options::Kind::kChoice || s.dynamic_choices) continue;
    Expect(s.choices.size() >= 2, std::string(">= 2 choices: ") + std::string(s.id));
    std::unordered_set<std::string_view> values;
    for (const options::Choice& c : s.choices) {
      Expect(!c.value.empty(), std::string("choice value: ") + std::string(s.id));
      Expect(values.insert(c.value).second,
             std::string("unique choice value: ") + std::string(s.id) + "=" + std::string(c.value));
    }
  }
}

void TestNumbers() {
  for (const options::Setting& s : options::Catalog()) {
    if (s.kind != options::Kind::kNumber) continue;
    Expect(s.min < s.max, std::string("min < max: ") + std::string(s.id));
    Expect(s.step > 0, std::string("step > 0: ") + std::string(s.id));
  }
}

void TestTexts() {
  for (const options::Setting& s : options::Catalog()) {
    if (s.kind != options::Kind::kText) continue;
    Expect(s.max_length > 0, std::string("max_length > 0: ") + std::string(s.id));
  }
}

void TestVisibleIf() {
  for (const options::Setting& s : options::Catalog()) {
    if (s.visible_if_id.empty()) continue;
    Expect(options::FindSetting(s.visible_if_id) != nullptr,
           std::string("visible_if names an id: ") + std::string(s.id) + " -> " +
               std::string(s.visible_if_id));
  }
}

void TestFpsCapHelp() {
  const options::Setting* cap = options::FindSetting("gow2_fps_cap");
  const options::Setting* unlock = options::FindSetting("gow2_unlock_fps");
  Expect(cap != nullptr, "frame rate limit setting exists");
  Expect(unlock != nullptr, "unlock frame rate setting exists");
  if (!cap || !unlock) return;
  Expect(cap->visible_if_id.empty(), "frame rate limit remains visible while locked");
  Expect(cap->help_en.find(unlock->label_en) != std::string_view::npos,
         "English limit help names the required unlock setting");
  Expect(cap->help_es.find(unlock->label_es) != std::string_view::npos,
         "Spanish limit help names the required unlock setting");
}

void TestPageTabs() {
  for (const options::Setting& s : options::Catalog()) {
    if (s.page == options::Page::kKeybinds) {
      Expect(s.tab == options::Tab::kControls, "keybinds row is on the controls tab");
    }
    if (s.page == options::Page::kHost) {
      Expect(s.tab == options::Tab::kMultiplayer, "host row is on the multiplayer tab");
    }
  }
}

void TestUiTexts() {
  const std::vector<options::TextId> ids = {
      options::TextId::kTitle,
      options::TextId::kTabDisplay,
      options::TextId::kTabGraphics,
      options::TextId::kTabControls,
      options::TextId::kTabTouch,
      options::TextId::kTabMultiplayer,
      options::TextId::kTabSaves,
      options::TextId::kTabAudio,
      options::TextId::kTabSystem,
      options::TextId::kPageKeybinds,
      options::TextId::kPageHost,
      options::TextId::kOn,
      options::TextId::kOff,
      options::TextId::kApplyNow,
      options::TextId::kApplyNextLevel,
      options::TextId::kApplyRestart,
      options::TextId::kRestartBanner,
      options::TextId::kUnsavedTitle,
      options::TextId::kUnsavedSave,
      options::TextId::kUnsavedDiscard,
      options::TextId::kUnsavedCancel,
      options::TextId::kSaved,
      options::TextId::kSaveFailed,
      options::TextId::kPressAKey,
      options::TextId::kHintChange,
      options::TextId::kHintBack,
      options::TextId::kHintDefault,
      options::TextId::kHintSave,
      options::TextId::kHintTabs,
      options::TextId::kHintClose,
      options::TextId::kHintQuit,
      options::TextId::kEditTextTitle,
      options::TextId::kEditTextOk,
      options::TextId::kEditTextCancel,
  };
  Expect(ids.size() == 34, "all 34 TextIds are listed");
  for (options::TextId id : ids) {
    Expect(!options::Text(id, options::Lang::kEn).empty(), "Text EN is non-empty");
    Expect(!options::Text(id, options::Lang::kEs).empty(), "Text ES is non-empty");
  }
}

void TestPlatformMenuNames() {
#if defined(__ANDROID__)
  constexpr std::string_view title_en = "DEVICE OPTIONS";
  constexpr std::string_view title_es = "OPCIONES DEL DISPOSITIVO";
  constexpr std::string_view entry_en = "Options > Device Options";
  constexpr std::string_view entry_es = "Opciones > Opciones del dispositivo";
#else
  constexpr std::string_view title_en = "PC OPTIONS";
  constexpr std::string_view title_es = "OPCIONES DE PC";
  constexpr std::string_view entry_en = "Options > PC Options";
  constexpr std::string_view entry_es = "Opciones > Opciones de PC";
#endif
  Expect(options::Text(options::TextId::kTitle, options::Lang::kEn) == title_en,
         "English menu title matches the platform");
  Expect(options::Text(options::TextId::kTitle, options::Lang::kEs) == title_es,
         "Spanish menu title matches the platform");
  const options::Setting* shortcut = options::FindSetting("menu_shortcut");
  Expect(shortcut != nullptr, "menu shortcut exists");
  if (!shortcut) return;
  Expect(shortcut->help_en.find(entry_en) != std::string_view::npos,
         "English shortcut help names the platform menu");
  Expect(shortcut->help_es.find(entry_es) != std::string_view::npos,
         "Spanish shortcut help names the platform menu");
}

int CountTab(options::Tab tab) {
  int n = 0;
  for (const options::Setting& s : options::Catalog()) {
    if (s.tab == tab && s.page == options::Page::kMain) ++n;
  }
  return n;
}

int CountPage(options::Page page) {
  int n = 0;
  for (const options::Setting& s : options::Catalog()) {
    if (s.page == page) ++n;
  }
  return n;
}

void TestCounts() {
  Expect(CountTab(options::Tab::kDisplay) == 8, "display has 8 rows");
  Expect(CountTab(options::Tab::kGraphics) == 50, "graphics has 50 rows including the graphics API, renderer, draw source, anti-aliasing, quality heading, preset and 24 settings");
  Expect(CountTab(options::Tab::kControls) == 12, "controls has 12 rows (controller buttons page added)");
  Expect(CountTab(options::Tab::kTouch) == 6, "touch has 6 rows");
  Expect(CountTab(options::Tab::kMultiplayer) == 12, "multiplayer has 12 rows");
  Expect(CountTab(options::Tab::kSaves) == 5, "saves has 5 rows");
  Expect(CountTab(options::Tab::kAudio) == 3, "audio has 3 rows");
  Expect(CountTab(options::Tab::kSystem) == 15, "system has 15 rows");
  Expect(CountPage(options::Page::kKeybinds) == 24, "keybinds page has 24 rows");
  Expect(CountPage(options::Page::kPadButtons) == 14, "controller buttons page has 14 rows");
  Expect(CountPage(options::Page::kHost) == 7, "host page has 7 rows");
}

void TestGraphicsQualityRows() {
  namespace quality = rex::glue::graphics_quality;
  const auto* group = options::FindSetting("game_quality_group");
  Expect(group && group->kind == options::Kind::kInfo && group->label_en == "Game quality" &&
             group->label_es == "Calidad del juego", "quality group has a bilingual visible heading");
  const auto* preset = options::FindSetting("gow2_gfx_preset");
  Expect(preset && preset->choices.size() == 4 && preset->choices.back().value == "custom",
         "quality preset exposes Low Medium High and derived Custom");
  Expect(quality::Settings().size() == 24, "24 verified retail quality settings selected");
  for (const auto& setting : quality::Settings()) {
    const auto* row = options::FindSetting(setting.id);
    Expect(row != nullptr, std::string("quality setting in catalog: ") + std::string(setting.key));
    if (!row) continue;
    Expect(row->tab == options::Tab::kGraphics && row->kind == options::Kind::kChoice &&
               row->apply == options::Apply::kRestart && row->platforms == options::kAllPlatforms &&
               !row->advanced, "quality rows need restart and are available on both platforms");
    Expect(!row->help_en.empty() && !row->help_es.empty(), "quality row has help in both languages");
    Expect(row->choices.front().value == "auto", "quality row starts at platform default");
    for (std::string_view name : {"low", "medium", "high"}) {
      bool found = false;
      for (const auto& choice : row->choices) {
        if (choice.value == quality::PresetValue(setting, name)) found = true;
      }
      Expect(found, "every preset value is one of the row's fixed choices");
    }
  }
  const auto* raw = options::FindSetting("gow2_system_settings");
  Expect(raw && raw->advanced && raw->platforms == options::kAllPlatforms,
         "raw engine settings remain advanced on both platforms");
}

void TestCoopPlayersRow() {
  const auto* players = options::FindSetting("gow2_coop_players");
  Expect(players != nullptr, "co-op players row exists");
  if (players) {
    Expect(players->kind == options::Kind::kChoice && players->apply == options::Apply::kRestart,
           "co-op players is a restart choice");
    Expect(players->choices.size() == 5 && players->choices[0].value == "2" &&
               players->choices[1].value == "3" && players->choices[2].value == "4" &&
               players->choices[3].value == "8" && players->choices[4].value == "10",
           "co-op players offers 2, 3, 4, 8 and 10");
    Expect(players->help_en.find("5 to 10 players is experimental") != std::string_view::npos,
           "co-op players help says 5 to 10 is experimental (EN)");
    Expect(players->help_es.find("De 5 a 10 jugadores es experimental") != std::string_view::npos,
           "co-op players help says 5 to 10 is experimental (ES)");
  }
}

void TestSplitScreenRows() {
  const auto* players = options::FindSetting("local_users");
  Expect(players != nullptr, "split-screen players row exists");
  if (players) {
    Expect(players->kind == options::Kind::kChoice && players->apply == options::Apply::kRestart,
           "split-screen players is a restart choice");
    Expect(players->choices.size() == 2 && players->choices.front().value == "1" &&
               players->choices.back().value == "2", "split-screen players offers 1 and 2, the game's limit");
  }
  const auto* keyboard = options::FindSetting("input_pad_first_user");
  Expect(keyboard != nullptr, "keyboard is player 1 row exists");
  if (keyboard) {
    Expect(keyboard->kind == options::Kind::kToggle && keyboard->apply == options::Apply::kRestart,
           "keyboard is player 1 is a restart toggle");
  }
}

void TestWindowsUpdateRows() {
  for (const auto id : {"update_status", "gow2_update_check", "update_download",
                        "update_check_now", "update_dismiss"}) {
    const auto* setting = options::FindSetting(id);
    Expect(setting != nullptr, std::string("update row exists: ") + id);
    if (!setting) continue;
    Expect(setting->platforms == options::kWindows, "update rows only appear on Windows");
    Expect(setting->tab == options::Tab::kSystem && setting->page == options::Page::kMain,
           "update rows appear on the main System tab");
  }
}

void TestGpuBackendRow() {
  const auto* backend = options::FindSetting("gpu_backend");
  Expect(backend != nullptr, "graphics API row exists");
  if (backend) {
    Expect(backend->tab == options::Tab::kGraphics && backend->kind == options::Kind::kChoice &&
               backend->apply == options::Apply::kRestart && backend->platforms == options::kWindows,
           "graphics API is a Windows-only restart choice on the Graphics tab");
    Expect(backend->choices.size() == 3 && backend->choices[0].value == "any" &&
               backend->choices[1].value == "d3d12" && backend->choices[2].value == "vulkan",
           "graphics API offers any, d3d12 and vulkan");
  }
}

void TestLanguageRows() {
  const struct {
    const char* id;
    size_t choices;
    std::string_view first;
  } rows[] = {{"user_language", 9, "1"}, {"gow2_voice_language", 4, "auto"}};
  for (const auto& row : rows) {
    const auto* setting = options::FindSetting(row.id);
    Expect(setting != nullptr, std::string("language row exists: ") + row.id);
    if (!setting) continue;
    Expect(setting->tab == options::Tab::kSystem && setting->apply == options::Apply::kRestart &&
               setting->platforms == options::kAllPlatforms,
           std::string("language row is a System restart row on both platforms: ") + row.id);
    Expect(setting->choices.size() == row.choices, std::string("language row choice count: ") + row.id);
    Expect(!setting->choices.empty() && setting->choices.front().value == row.first,
           std::string("language row first choice: ") + row.id);
  }
}

int main() {
  try {
    TestIdsUnique();
    TestFindSetting();
    TestLabels();
    TestChoices();
    TestNumbers();
    TestTexts();
    TestVisibleIf();
    TestFpsCapHelp();
    TestPageTabs();
    TestUiTexts();
    TestPlatformMenuNames();
    TestCounts();
    TestWindowsUpdateRows();
    TestCoopPlayersRow();
    TestSplitScreenRows();
    TestGraphicsQualityRows();
    TestGpuBackendRow();
    TestLanguageRows();
  } catch (const std::exception& e) {
    ++g_checks;
    ++g_failures;
    std::printf("FAIL: exception: %s\n", e.what());
  } catch (...) {
    ++g_checks;
    ++g_failures;
    std::printf("FAIL: unknown exception\n");
  }
  std::printf("options_catalog_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
