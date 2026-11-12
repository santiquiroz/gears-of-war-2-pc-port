// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Host-side test of the PC Options catalog data (no game data, no SDK).
#include "gow2_options_catalog.h"
#include "gow2_graphics_quality.h"

#include <cstdio>
#include <iterator>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
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
  bool has_vrr = false;
  for (const auto& choice : cap->choices)
    has_vrr = has_vrr || (choice.value == "-1" && choice.label_en == "Monitor refresh - 3 (VRR)" &&
                          choice.label_es == "Frecuencia del monitor - 3 (VRR)");
  Expect(has_vrr, "frame rate limit offers the VRR refresh choice");
}

void TestMenuScaleChoices() {
  const auto* scale = options::FindSetting("gow2_menu_scale");
  Expect(scale != nullptr, "menu size setting exists");
  if (!scale) return;
  constexpr std::string_view expected[]{"75", "100", "125"};
  Expect(scale->choices.size() == std::size(expected), "menu size has three choices");
  for (size_t i = 0; i < scale->choices.size() && i < std::size(expected); ++i)
    Expect(scale->choices[i].value == expected[i], "menu size choice is in percent range");
}

void TestWidescreenChoices() {
  const auto* aspect = options::FindSetting("gow2_widescreen");
  Expect(aspect != nullptr, "aspect ratio setting exists");
  if (!aspect) return;
  bool has_16_10 = false;
  bool has_4_3 = false;
  for (const auto& choice : aspect->choices) {
    has_16_10 = has_16_10 || choice.value == "16:10";
    has_4_3 = has_4_3 || choice.value == "4:3";
  }
  Expect(has_16_10, "aspect ratio offers 16:10");
  Expect(has_4_3, "aspect ratio offers 4:3");
}

void TestPageTabs() {
  for (const options::Setting& s : options::Catalog()) {
    if (s.page == options::Page::kKeybinds) {
      Expect(s.tab == options::Tab::kControls, "keybinds row is on the controls tab");
    }
    if (s.page == options::Page::kHost) {
      Expect(s.tab == options::Tab::kMultiplayer, "host row is on the multiplayer tab");
    }
    if (s.page == options::Page::kTexturePackBuilder) {
      Expect(s.tab == options::Tab::kGraphics && s.platforms == options::kWindows,
             "texture pack builder row is on the Windows graphics tab");
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
      options::TextId::kPageTexturePackBuilder,
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
      options::TextId::kHintPlay,
      options::TextId::kEditTextTitle,
      options::TextId::kEditTextOk,
      options::TextId::kEditTextCancel,
      options::TextId::kCineSkip,
      options::TextId::kCineHostOnly,
      options::TextId::kOpeningDownload,
  };
  Expect(ids.size() == 39, "all 39 TextIds are listed");
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
  Expect(CountTab(options::Tab::kDisplay) == 15, "display has 15 rows including the aspect ratio, HDR and its brightness settings, the menu size, keep 16:9 shape and the frame counter");
  Expect(CountTab(options::Tab::kGraphics) == 54, "graphics has 54 rows including frame generation, keep original menus, the graphics API, renderer, draw source, anti-aliasing, quality heading, preset, 24 settings, texture pack, its folder and the pack builder");
  Expect(CountTab(options::Tab::kControls) == 12, "controls has 12 rows (controller buttons page added)");
  Expect(CountTab(options::Tab::kTouch) == 6, "touch has 6 rows");
  Expect(CountTab(options::Tab::kMultiplayer) == 14, "multiplayer has 14 rows including friendly fire and Go to player 1");
  Expect(CountTab(options::Tab::kSaves) == 5, "saves has 5 rows");
  Expect(CountTab(options::Tab::kAudio) == 3, "audio has 3 rows");
  Expect(CountTab(options::Tab::kSystem) == 19, "system has 19 rows including skip intro videos, the skip hint, show options at start and F12 screenshots");
  Expect(CountPage(options::Page::kKeybinds) == 24, "keybinds page has 24 rows");
  Expect(CountPage(options::Page::kPadButtons) == 14, "controller buttons page has 14 rows");
  Expect(CountPage(options::Page::kHost) == 7, "host page has 7 rows");
  Expect(CountPage(options::Page::kTexturePackBuilder) == 6, "texture pack builder page has 6 rows");
}

void TestHdrRows() {
  const auto* hdr = options::FindSetting("present_hdr");
  const auto* paper_white = options::FindSetting("present_hdr_paper_white_nits");
  const auto* peak = options::FindSetting("present_hdr_peak_nits");
  Expect(hdr != nullptr, "HDR setting exists");
  Expect(paper_white != nullptr, "HDR brightness setting exists");
  Expect(peak != nullptr, "HDR peak brightness setting exists");
  if (!hdr || !paper_white || !peak) return;
  Expect(hdr->tab == options::Tab::kDisplay && hdr->kind == options::Kind::kToggle &&
             hdr->apply == options::Apply::kRestart && hdr->platforms == options::kWindows,
         "HDR is a Windows-only Display restart toggle");
  for (const auto* row : {paper_white, peak}) {
    Expect(row->tab == options::Tab::kDisplay && row->kind == options::Kind::kNumber &&
               row->apply == options::Apply::kNow && row->platforms == options::kWindows,
           "HDR brightness is a Windows-only apply-now Display number");
    Expect(row->visible_if_id == "present_hdr" && row->visible_if_value == "true",
           "HDR brightness is visible only when HDR is on");
    Expect(row->decimals == 0 && row->unit == " nits", "HDR brightness uses whole nits");
  }
  Expect(paper_white->min == 80 && paper_white->max == 500 && paper_white->step == 10,
         "HDR paper white ranges from 80 to 500 nits in steps of 10");
  Expect(peak->min == 400 && peak->max == 4000 && peak->step == 100,
         "HDR peak brightness ranges from 400 to 4000 nits in steps of 100");
}

void TestScreenshotRow() {
  const auto* screenshot = options::FindSetting("present_screenshot_key_enabled");
  Expect(screenshot != nullptr, "F12 screenshots setting exists");
  if (!screenshot) return;
  Expect(screenshot->tab == options::Tab::kSystem && screenshot->kind == options::Kind::kToggle &&
             screenshot->apply == options::Apply::kNow && screenshot->platforms == options::kWindows,
         "F12 screenshots is a Windows-only apply-now System toggle");
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

void TestFrameGenerationRow() {
  const auto* frame_generation = options::FindSetting("gow2_frame_generation");
  Expect(frame_generation != nullptr, "frame generation row exists");
  if (!frame_generation) return;
  Expect(frame_generation->tab == options::Tab::kGraphics &&
             frame_generation->kind == options::Kind::kChoice &&
             frame_generation->apply == options::Apply::kRestart &&
             frame_generation->platforms == options::kWindows,
         "frame generation is a Windows-only Graphics restart choice");
  Expect(frame_generation->choices.size() == 2 && frame_generation->choices[0].value == "off" &&
             frame_generation->choices[1].value == "fsr3",
         "frame generation offers off and fsr3");
}

void TestSkipIntroRow() {
  const auto* skip = options::FindSetting("gow2_skip_intro_movies");
  Expect(skip != nullptr, "skip intro videos row exists");
  if (skip) {
    Expect(skip->tab == options::Tab::kSystem && skip->kind == options::Kind::kToggle &&
               skip->apply == options::Apply::kRestart && skip->platforms == options::kAllPlatforms,
           "skip intro videos is a System restart toggle on both platforms");
  }
}

void TestLauncherRow() {
  const auto* launcher = options::FindSetting("gow2_launcher");
  Expect(launcher != nullptr, "show options at start row exists");
  if (launcher) {
    Expect(launcher->tab == options::Tab::kSystem && launcher->kind == options::Kind::kToggle &&
               launcher->apply == options::Apply::kNow && launcher->platforms == options::kWindows,
           "show options at start is an apply-now System toggle on Windows");
  }
}

void TestTexturePackRows() {
  Expect(options::FindSetting("texture_replace") == nullptr, "the texture pack chooser replaces the toggle");
  const auto* pack = options::FindSetting("gow2_texture_pack");
  Expect(pack != nullptr, "texture pack row exists");
  if (pack) {
    Expect(pack->tab == options::Tab::kGraphics && pack->kind == options::Kind::kChoice &&
               pack->apply == options::Apply::kRestart && pack->platforms == options::kWindows &&
               pack->dynamic_choices,
           "texture pack is a dynamic restart choice on the Windows Graphics tab");
  }
  const auto* folder = options::FindSetting("gow2_open_texture_packs");
  Expect(folder && folder->kind == options::Kind::kAction && folder->platforms == options::kWindows &&
             folder->need == options::Need::kNothing,
         "open texture packs folder is a Windows action that also works in the launcher");
  const auto* runtime = options::FindSetting("runtime_settings");
  Expect(runtime && runtime->need == options::Need::kGameRunning, "runtime settings need the running game");
  for (const char* id : {"host_online", "host_status", "host_open", "host_close"}) {
    const auto* host = options::FindSetting(id);
    Expect(host && host->need == options::Need::kGameRunning, "host rows need the running game");
  }
}

// The launcher opens before the runtime: only these rows call into it. Key bindings and controller
// buttons only write keybind_* / pad_map_* cvars (registered by rexruntime.dll at load) and read keys
// through ImGui; the input driver picks them up when the game starts.
void TestTexturePackBuilderRows() {
  const auto& catalog = options::Catalog();
  const auto at = [&](std::string_view id) {
    for (size_t i = 0; i < catalog.size(); ++i) {
      if (catalog[i].id == id) return static_cast<int>(i);
    }
    return -1;
  };
  const auto* build = options::FindSetting("gow2_build_texture_pack");
  Expect(build && build->kind == options::Kind::kAction && build->tab == options::Tab::kGraphics &&
             build->page == options::Page::kMain && build->platforms == options::kWindows &&
             build->need == options::Need::kNothing,
         "build texture pack is a Windows Graphics action that works in the launcher");
  Expect(at("gow2_build_texture_pack") == at("gow2_open_texture_packs") + 1,
         "build texture pack comes right after open texture packs folder");
  Expect(build && !build->disabled_help_en.empty() && !build->disabled_help_es.empty(),
         "build texture pack says why when the tools are missing");
  const auto* dump = options::FindSetting("texture_dump");
  Expect(dump && dump->help_en.find("Build texture pack") != std::string_view::npos &&
             dump->help_es.find("Crear pack de texturas") != std::string_view::npos,
         "dump textures help points at Build texture pack");
  const std::vector<std::pair<std::string_view, options::Kind>> page{
      {"gow2_texture_build_scale", options::Kind::kChoice}, {"gow2_texture_build_quality", options::Kind::kChoice},
      {"texture_build_info", options::Kind::kInfo},         {"texture_build_start", options::Kind::kAction},
      {"texture_build_cancel", options::Kind::kAction},     {"texture_build_status", options::Kind::kInfo}};
  for (const auto& [id, kind] : page) {
    const auto* row = options::FindSetting(id);
    Expect(row && row->page == options::Page::kTexturePackBuilder && row->kind == kind &&
               row->apply == options::Apply::kNow,
           std::string("builder page row: ") + std::string(id));
  }
  const auto* scale = options::FindSetting("gow2_texture_build_scale");
  Expect(scale && scale->choices.size() == 2 && scale->choices[0].value == "2" && scale->choices[1].value == "4",
         "scale offers 2x and 4x");
  const auto* quality = options::FindSetting("gow2_texture_build_quality");
  Expect(quality && quality->choices.size() == 2 && quality->choices[0].value == "disc" &&
             quality->choices[1].value == "bc7",
         "quality offers the disc formats and BC7");
}

void TestLauncherAudit() {
  const std::unordered_set<std::string_view> expected{"coop_check", "coop_go_to_player1",
                                                      "host_online", "host_status", "host_open",
                                                      "host_close", "runtime_settings"};
  std::unordered_set<std::string_view> found;
  for (const auto& setting : options::Catalog()) {
    if (setting.need == options::Need::kGameRunning) found.insert(setting.id);
  }
  Expect(found == expected,
         "only the co-op check, Go to player 1, host page entries and runtime settings need the game");
  for (const char* id : {"keybindings", "pad_buttons", "user_name", "local_users", "save_dir_browse",
                         "save_open", "restart_game", "quit_game"}) {
    const auto* row = options::FindSetting(id);
    Expect(row && row->need == options::Need::kNothing, std::string("works in the launcher: ") + id);
  }
}

void TestCoopRequestRows() {
  const auto* ff = options::FindSetting("gow2_friendly_fire");
  Expect(ff && ff->tab == options::Tab::kMultiplayer && ff->kind == options::Kind::kToggle &&
             ff->apply == options::Apply::kNow && ff->platforms == options::kAllPlatforms,
         "friendly fire is an apply-now Multiplayer toggle on every platform");
  const auto* tp = options::FindSetting("coop_go_to_player1");
  Expect(tp && tp->tab == options::Tab::kMultiplayer && tp->kind == options::Kind::kAction &&
             tp->platforms == options::kAllPlatforms,
         "Go to player 1 is a Multiplayer action on every platform");
}

void TestLetterboxRow() {
  const auto* letterbox = options::FindSetting("present_letterbox");
  Expect(letterbox != nullptr, "keep 16:9 shape row exists");
  if (letterbox) {
    Expect(letterbox->tab == options::Tab::kDisplay && letterbox->kind == options::Kind::kToggle &&
               letterbox->apply == options::Apply::kNow && letterbox->platforms == options::kAndroid,
           "keep 16:9 shape is an apply-now Display toggle on Android");
  }
}

void TestFrameCounterRow() {
  const auto* counter = options::FindSetting("gow2_show_fps");
  Expect(counter != nullptr, "frame counter row exists");
  if (!counter) return;
  Expect(counter->tab == options::Tab::kDisplay && counter->kind == options::Kind::kToggle &&
             counter->apply == options::Apply::kNow && counter->platforms == options::kAllPlatforms,
         "frame counter is an apply-now Display toggle on both platforms");
  Expect(counter->label_en == "Frame counter" && counter->label_es == "Contador de FPS",
         "frame counter labels");
  Expect(counter->need == options::Need::kNothing, "frame counter works in the launcher");
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
    TestMenuScaleChoices();
    TestWidescreenChoices();
    TestPageTabs();
    TestUiTexts();
    TestPlatformMenuNames();
    TestCounts();
    TestHdrRows();
    TestScreenshotRow();
    TestWindowsUpdateRows();
    TestCoopPlayersRow();
    TestSplitScreenRows();
    TestGraphicsQualityRows();
    TestGpuBackendRow();
    TestFrameGenerationRow();
    TestSkipIntroRow();
    TestCoopRequestRows();
    TestLetterboxRow();
    TestFrameCounterRow();
    TestLauncherRow();
    TestTexturePackRows();
    TestTexturePackBuilderRows();
    TestLauncherAudit();
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
