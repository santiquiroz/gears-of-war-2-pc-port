// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_keycaps.h"

#include <cstring>

#include "gow2_keycap_layout.inc"

namespace rex::glue::keycap {
namespace {

std::string Trim(const std::string& s) {
  const size_t a = s.find_first_not_of(' ');
  if (a == std::string::npos) return {};
  return s.substr(a, s.find_last_not_of(' ') - a + 1);
}

bool IsModifier(const std::string& s) {
  return s == "Shift" || s == "Ctrl" || s == "Control" || s == "Alt";
}

std::string Canonical(const std::string& name) { return name == "Ctrl" ? "Control" : name; }

// A setting's value, or the default the port ships when it is empty.
std::string Setting(const std::function<std::string(const char*)>& get, const char* name,
                    const char* fallback) {
  std::string v = get(name);
  return v.empty() ? fallback : v;
}

Prompt Single(const std::string& list) {
  const Binding b = ParseBinding(list);
  const char16_t key = CharFor(b.key);
  if (!key) return {};
  Prompt p;
  p.brief = Markup(std::u16string(1, key));
  std::u16string all;
  for (const std::string& m : b.modifiers) {
    const char16_t c = CharFor(m);
    if (!c) return {p.brief, p.brief};  // a modifier with no cap: show the key alone
    all.push_back(c);
  }
  all.push_back(key);
  p.full = Markup(all);
  return p;
}

Prompt OneGlyph(const std::string& id) {
  const char16_t c = CharFor(id);
  if (!c) return {};
  std::u16string m = Markup(std::u16string(1, c));
  return {m, m};
}

// Plain key names of a list of bindings (no modifiers allowed), or empty.
std::vector<std::string> PlainKeys(const std::vector<std::string>& lists) {
  std::vector<std::string> names;
  for (const std::string& l : lists) {
    const Binding b = ParseBinding(l);
    if (!b.modifiers.empty() || b.key.empty()) return {};
    names.push_back(b.key);
  }
  return names;
}

bool SameKeys(const std::vector<std::string>& names, std::initializer_list<const char*> want) {
  if (names.size() != want.size()) return false;
  size_t i = 0;
  for (const char* w : want)
    if (names[i++] != w) return false;
  return true;
}

// The four keys of a stick as the game's cluster glyph, else the generic "move" glyph.
Prompt Stick(const std::vector<std::string>& up_left_down_right) {
  const auto k = PlainKeys(up_left_down_right);
  if (SameKeys(k, {"W", "A", "S", "D"})) return OneGlyph("CL_WASD");
  if (SameKeys(k, {"Up", "Left", "Down", "Right"})) return OneGlyph("CL_ARROWS");
  return OneGlyph("IC_MOVE");
}

}  // namespace

Binding ParseBinding(const std::string& list) {
  Binding out;
  std::string first = Trim(list.substr(0, list.find(',')));
  for (;;) {
    const size_t plus = first.find('+');
    if (plus == std::string::npos || plus == 0 || plus + 1 >= first.size()) break;
    const std::string head = Trim(first.substr(0, plus));
    if (!IsModifier(head)) break;
    out.modifiers.push_back(Canonical(head));
    first = Trim(first.substr(plus + 1));
  }
  out.key = Canonical(first);
  return out;
}

char16_t CharFor(const std::string& id) {
  for (const KeyChar& k : kKeyChars)
    if (id == k.id) return k.ch;
  return 0;
}

std::u16string Markup(const std::u16string& chars) {
  return u"<Fonts:Warfare_HUD.Xbox360_18pt>" + chars + u"<Fonts:/>";
}

const std::vector<std::string>& ControllerKeys() {
  static const std::vector<std::string> keys = {
      "Controller_A", "Controller_B", "Controller_X", "Controller_Y", "Controller_LeftStick",
      "Controller_LeftStickDown", "Controller_RightStick", "Controller_LeftStickClick",
      "Controller_RightStickClick", "Controller_LeftTrigger", "Controller_RightTrigger",
      "Controller_DPad", "Controller_DPadUp", "Controller_DPadRight", "Controller_DPadDown",
      "Controller_DPadLeft", "Controller_ArrowUp", "Controller_ArrowDown", "Controller_ArrowLeft",
      "Controller_ArrowRight", "Controller_LeftBumper", "Controller_RightBumper",
      "Controller_Start", "Controller_Back", "Controller_UpDown", "Controller_UpDownLeftRight"};
  return keys;
}

Prompt PromptFor(const std::string& key, const std::function<std::string(const char*)>& get) {
  const auto S = [&](const char* name, const char* fallback) { return Setting(get, name, fallback); };
  const auto lstick = [&] {
    return std::vector<std::string>{S("keybind_lstick_up", "W"), S("keybind_lstick_left", "A"),
                                    S("keybind_lstick_down", "S"), S("keybind_lstick_right", "D")};
  };
  if (key == "Controller_A") return Single(S("keybind_a", "Space"));
  if (key == "Controller_B") return Single(S("keybind_b", "F,V"));
  if (key == "Controller_X") return Single(S("keybind_x", "E"));
  if (key == "Controller_Y") return Single(S("keybind_y", "Q"));
  if (key == "Controller_LeftTrigger") return Single(S("keybind_left_trigger", "RMB"));
  if (key == "Controller_RightTrigger") return Single(S("keybind_right_trigger", "LMB"));
  if (key == "Controller_LeftBumper") return Single(S("keybind_left_shoulder", "G"));
  if (key == "Controller_RightBumper") return Single(S("keybind_right_shoulder", "R"));
  if (key == "Controller_LeftStickClick") return Single(S("keybind_lstick_press", "C"));
  if (key == "Controller_RightStickClick") return Single(S("keybind_rstick_press", "MMB,Z"));
  if (key == "Controller_Start") return Single(S("keybind_start", "Return,Escape"));
  if (key == "Controller_Back") return Single(S("keybind_back", "Tab"));
  if (key == "Controller_DPadUp") return Single(S("keybind_dpad_up", "1"));
  if (key == "Controller_DPadRight") return Single(S("keybind_dpad_right", "2"));
  if (key == "Controller_DPadDown") return Single(S("keybind_dpad_down", "3"));
  if (key == "Controller_DPadLeft") return Single(S("keybind_dpad_left", "4"));
  if (key == "Controller_LeftStickDown" || key == "Controller_ArrowDown")
    return Single(S("keybind_lstick_down", "S"));
  if (key == "Controller_ArrowUp") return Single(S("keybind_lstick_up", "W"));
  if (key == "Controller_ArrowLeft") return Single(S("keybind_lstick_left", "A"));
  if (key == "Controller_ArrowRight") return Single(S("keybind_lstick_right", "D"));
  if (key == "Controller_LeftStick" || key == "Controller_UpDownLeftRight") return Stick(lstick());
  if (key == "Controller_UpDown") {
    const auto k = PlainKeys({S("keybind_lstick_up", "W"), S("keybind_lstick_down", "S")});
    if (SameKeys(k, {"W", "S"})) return OneGlyph("PR_WS");
    if (SameKeys(k, {"Up", "Down"})) return OneGlyph("PR_UPDOWN");
    return OneGlyph("IC_MOVE");
  }
  if (key == "Controller_RightStick") {
    if (get("mnk_mouse") != "false") return OneGlyph("IC_MOUSE");  // the mouse looks
    return Stick({S("keybind_rstick_up", "Up"), S("keybind_rstick_left", "Left"),
                  S("keybind_rstick_down", "Down"), S("keybind_rstick_right", "Right")});
  }
  if (key == "Controller_DPad") {
    const auto k = PlainKeys({S("keybind_dpad_up", "1"), S("keybind_dpad_right", "2"),
                              S("keybind_dpad_down", "3"), S("keybind_dpad_left", "4")});
    if (SameKeys(k, {"1", "2", "3", "4"})) return OneGlyph("CL_1234");
    return OneGlyph("IC_MOVE");
  }
  return {};
}

}  // namespace rex::glue::keycap
