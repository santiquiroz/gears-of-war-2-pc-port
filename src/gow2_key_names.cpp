// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_key_names.h"

#include <cstring>
#include <map>

namespace rex::glue::keynames {
namespace {

constexpr const char* kDigits[] = {"Zero", "One", "Two", "Three", "Four",
                                   "Five", "Six", "Seven", "Eight", "Nine"};
constexpr const char* kNumpad[] = {"NumPadZero", "NumPadOne", "NumPadTwo", "NumPadThree",
                                   "NumPadFour", "NumPadFive", "NumPadSix", "NumPadSeven",
                                   "NumPadEight", "NumPadNine"};

struct Named {
  uint16_t vk;
  const char* name;
};

// Windows virtual-key codes to the engine's names (Engine.Keys).
constexpr Named kFixed[] = {
    {0x08, "BackSpace"}, {0x09, "Tab"}, {0x0D, "Enter"}, {0x13, "Pause"}, {0x14, "CapsLock"},
    {0x1B, "Escape"}, {0x20, "SpaceBar"}, {0x21, "PageUp"}, {0x22, "PageDown"}, {0x23, "End"},
    {0x24, "Home"}, {0x25, "Left"}, {0x26, "Up"}, {0x27, "Right"}, {0x28, "Down"},
    {0x2D, "Insert"}, {0x2E, "Delete"}, {0x6A, "Multiply"}, {0x6B, "Add"}, {0x6D, "Subtract"},
    {0x6E, "Decimal"}, {0x6F, "Divide"}, {0x90, "NumLock"}, {0x91, "ScrollLock"},
    {0xA0, "LeftShift"}, {0xA1, "RightShift"}, {0xA2, "LeftControl"}, {0xA3, "RightControl"},
    {0xA4, "LeftAlt"}, {0xA5, "RightAlt"}, {0xBA, "Semicolon"}, {0xBB, "Equals"},
    {0xBC, "Comma"}, {0xBD, "Underscore"}, {0xBE, "Period"}, {0xBF, "Slash"}, {0xC0, "Tilde"},
    {0xDB, "LeftBracket"}, {0xDC, "Backslash"}, {0xDD, "RightBracket"}, {0xDE, "SingleQuote"},
    {0x01, "LeftMouseButton"}, {0x02, "RightMouseButton"}, {0x04, "MiddleMouseButton"},
    {0x05, "ThumbMouseButton"}, {0x06, "ThumbMouseButton2"},
};

// The keybind_* names (SDK src/ui/keybinds.cpp) to engine names.
const std::map<std::string, std::vector<std::string>, std::less<>>& BindTable() {
  static const auto* table = [] {
    auto* t = new std::map<std::string, std::vector<std::string>, std::less<>>;
    for (char c = 'A'; c <= 'Z'; ++c) (*t)[std::string(1, c)] = {std::string(1, c)};
    for (int d = 0; d <= 9; ++d) {
      (*t)[std::to_string(d)] = {kDigits[d]};
      (*t)["Numpad" + std::to_string(d)] = {kNumpad[d]};
    }
    for (int f = 1; f <= 12; ++f) (*t)["F" + std::to_string(f)] = {"F" + std::to_string(f)};
    (*t)["Backtick"] = {"Tilde"};
    (*t)["Minus"] = {"Underscore"};
    (*t)["Plus"] = {"Equals"};
    (*t)["Comma"] = {"Comma"};
    (*t)["Period"] = {"Period"};
    (*t)["Semicolon"] = {"Semicolon"};
    (*t)["Slash"] = {"Slash"};
    (*t)["Backslash"] = {"Backslash"};
    (*t)["LBracket"] = {"LeftBracket"};
    (*t)["RBracket"] = {"RightBracket"};
    (*t)["Quote"] = {"SingleQuote"};
    (*t)["Escape"] = {"Escape"};
    (*t)["Return"] = {"Enter"};
    (*t)["NumpadEnter"] = {"Enter"};
    (*t)["Space"] = {"SpaceBar"};
    (*t)["Tab"] = {"Tab"};
    (*t)["Backspace"] = {"BackSpace"};
    (*t)["Delete"] = {"Delete"};
    (*t)["Insert"] = {"Insert"};
    (*t)["Home"] = {"Home"};
    (*t)["End"] = {"End"};
    (*t)["PageUp"] = {"PageUp"};
    (*t)["PageDown"] = {"PageDown"};
    for (const char* d : {"Left", "Right", "Up", "Down"}) (*t)[d] = {d};
    (*t)["Shift"] = {"LeftShift", "RightShift"};
    (*t)["Control"] = {"LeftControl", "RightControl"};
    (*t)["Ctrl"] = {"LeftControl", "RightControl"};
    (*t)["Alt"] = {"LeftAlt", "RightAlt"};
    (*t)["NumpadPlus"] = {"Add"};
    (*t)["NumpadMinus"] = {"Subtract"};
    (*t)["NumpadStar"] = {"Multiply"};
    (*t)["NumpadSlash"] = {"Divide"};
    (*t)["Pause"] = {"Pause"};
    (*t)["CapsLock"] = {"CapsLock"};
    (*t)["NumLock"] = {"NumLock"};
    (*t)["ScrollLock"] = {"ScrollLock"};
    (*t)["LMB"] = {"LeftMouseButton"};
    (*t)["RMB"] = {"RightMouseButton"};
    (*t)["MMB"] = {"MiddleMouseButton"};
    return t;
  }();
  return *table;
}

std::string_view Trim(std::string_view s) {
  while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
  while (!s.empty() && s.back() == ' ') s.remove_suffix(1);
  return s;
}

}  // namespace

const char* EngineNameForVk(uint16_t vk) {
  static char letter[26][2];
  if (vk >= 0x41 && vk <= 0x5A) {
    char* s = letter[vk - 0x41];
    s[0] = char(vk);
    s[1] = 0;
    return s;
  }
  if (vk >= 0x30 && vk <= 0x39) return kDigits[vk - 0x30];
  if (vk >= 0x60 && vk <= 0x69) return kNumpad[vk - 0x60];
  if (vk >= 0x70 && vk <= 0x7B) {
    static const char* const f[] = {"F1", "F2", "F3", "F4", "F5", "F6",
                                    "F7", "F8", "F9", "F10", "F11", "F12"};
    return f[vk - 0x70];
  }
  for (const Named& n : kFixed)
    if (n.vk == vk) return n.name;
  return nullptr;
}

std::vector<std::string> EngineNamesForBind(std::string_view sdk_name) {
  const auto it = BindTable().find(sdk_name);
  return it == BindTable().end() ? std::vector<std::string>{} : it->second;
}

std::vector<Chord> ParseChords(std::string_view value) {
  std::vector<Chord> out;
  while (!value.empty()) {
    const size_t comma = value.find(',');
    std::string_view token = Trim(value.substr(0, comma));
    value = comma == std::string_view::npos ? std::string_view{} : value.substr(comma + 1);
    Chord chord;
    for (;;) {
      const size_t plus = token.find('+');
      if (plus == std::string_view::npos || plus == 0 || plus + 1 >= token.size()) break;
      const std::string_view head = Trim(token.substr(0, plus));
      if (head == "Shift") chord.shift = true;
      else if (head == "Ctrl" || head == "Control") chord.control = true;
      else if (head == "Alt") chord.alt = true;
      else break;
      token = Trim(token.substr(plus + 1));
    }
    chord.engine_names = EngineNamesForBind(token);
    if (!chord.engine_names.empty()) out.push_back(std::move(chord));
  }
  return out;
}

}  // namespace rex::glue::keynames
