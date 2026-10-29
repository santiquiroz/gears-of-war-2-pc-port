// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
//
// The in-game PC Options menu: what every row is (label, help, kind, values, when it applies, platforms).
// Pure data, no SDK. gow2_options_model.h drives navigation over it; gow2_options_overlay.cpp draws it.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace rex::glue::options {

enum class Lang : uint8_t { kEn, kEs };

enum class Tab : uint8_t { kDisplay, kGraphics, kControls, kTouch, kMultiplayer, kSaves, kAudio, kSystem };

// kMain rows sit in their tab; the other pages open from an action row and close with Back.
enum class Page : uint8_t { kMain, kKeybinds, kPadButtons, kHost, kTexturePackBuilder };

enum class Kind : uint8_t {
  kToggle,   // "true" / "false"
  kChoice,   // one of choices (or the dynamic choices the context supplies)
  kNumber,   // min..max in step increments
  kText,     // free text up to max_length
  kKeybind,  // a key list like "F,V"; Accept captures a new key
  kAction,   // Accept runs it (or opens a page); no value
  kInfo,     // read-only text; not selectable
};

enum class Apply : uint8_t { kNow, kNextLevel, kRestart };

enum PlatformMask : uint8_t { kWindows = 1, kAndroid = 2, kAllPlatforms = 3 };

// What the running game offers; rows that need something missing are hidden.
enum class Need : uint8_t {
  kNothing,
  kTouch,           // touch controls were created at launch
  kFidelityFx,      // the runtime was built with AMD FidelityFX (FSR 1 / CAS present effects)
  kDlss,            // a DLSS-capable runtime (never in the release)
  kNoDlss,
  kNativeMouse,     // the native mouse-look hooks are installed
  kNoNativeMouse,   // the mouse drives the right stick
  kGameRunning,     // the runtime exists (hidden in the launcher, which opens before it)
};

struct Choice {
  std::string_view value;
  std::string_view label_en;
  std::string_view label_es;
};

struct Setting {
  std::string_view id;  // unique; the Values adapter maps it to one or more cvars
  Tab tab = Tab::kDisplay;
  Page page = Page::kMain;
  Kind kind = Kind::kInfo;
  Apply apply = Apply::kNow;
  uint8_t platforms = kAllPlatforms;
  Need need = Need::kNothing;
  bool advanced = false;  // shown only with "Show advanced options" on
  bool dynamic_choices = false;  // choices come from Context::dynamic_choices[id]
  std::string_view label_en;
  std::string_view label_es;
  std::string_view help_en;
  std::string_view help_es;
  // Help shown instead while the context disables the row (Context::disabled).
  std::string_view disabled_help_en;
  std::string_view disabled_help_es;
  std::vector<Choice> choices;
  double min = 0;
  double max = 0;
  double step = 1;
  int decimals = 0;
  double display_scale = 1;  // shown value = value * display_scale (opacity 0..1 shown as 0..100)
  std::string_view unit;     // appended to the shown number: "%", "x", " s"
  size_t max_length = 0;     // kText
  // Shown only when the row with this id currently has this value ("!" prefix: has any other value).
  std::string_view visible_if_id;
  std::string_view visible_if_value;
};

enum class TextId : uint8_t {
  kTitle,
  kTabDisplay, kTabGraphics, kTabControls, kTabTouch, kTabMultiplayer, kTabSaves, kTabAudio, kTabSystem,
  kPageKeybinds, kPagePadButtons, kPageHost, kPageTexturePackBuilder,
  kOn, kOff,
  kApplyNow, kApplyNextLevel, kApplyRestart,
  kRestartBanner,      // followed by the labels of the pending rows
  kUnsavedTitle, kUnsavedSave, kUnsavedDiscard, kUnsavedCancel,
  kSaved, kSaveFailed,
  kPressAKey,          // key capture prompt
  kHintChange, kHintBack, kHintDefault, kHintSave, kHintTabs, kHintClose,
  kHintQuit,
  kHintPlay,           // the launcher's Back: leave the menu and load the game
  kEditTextTitle, kEditTextOk, kEditTextCancel,
};

const std::vector<Setting>& Catalog();
const Setting* FindSetting(std::string_view id);
std::string_view Text(TextId id, Lang lang);
std::string_view Label(const Setting& s, Lang lang);
std::string_view Help(const Setting& s, Lang lang);
std::string_view DisabledHelp(const Setting& s, Lang lang);
std::string_view ChoiceLabel(const Choice& c, Lang lang);
std::string_view TabName(Tab tab, Lang lang);

}  // namespace rex::glue::options
