// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Host-side test of the keycap prompt mapping (no game data, no SDK): which pad glyphs of the
// button font the live swap points at keycaps for the HUD prompts that skip [ControllerFont].
#include "gow2_keycaps.h"
#include "gow2_keycap_layout.inc"

#include <cstdio>
#include <map>
#include <string>
#include <string_view>

int g_checks = 0;
int g_failures = 0;
void Expect(bool ok, std::string_view what) {
  ++g_checks;
  if (ok) return;
  ++g_failures;
  std::printf("FAIL: %.*s\n", int(what.size()), what.data());
}

using namespace rex::glue;

// The [ControllerFont] values of the English bundle (Coalesced_int.bin, GearGameUI.int).
const std::vector<std::pair<std::string, std::u16string>>& RetailControllerFont() {
  static const std::vector<std::pair<std::string, std::u16string>> v = {
      {"Controller_A", keycap::Markup(u"A")},
      {"Controller_B", keycap::Markup(u"B")},
      {"Controller_X", keycap::Markup(u"X")},
      {"Controller_Y", keycap::Markup(u"Y")},
      {"Controller_LeftTrigger", keycap::Markup(u"]")},
      {"Controller_RightTrigger", keycap::Markup(u"^")},
      {"Controller_LeftBumper", keycap::Markup(u"_")},
      {"Controller_RightBumper", keycap::Markup(u"`")},
      {"Controller_Start", keycap::Markup(u"\\>")},
      {"Controller_Back", keycap::Markup(u"\\<")},
  };
  return v;
}

std::function<std::string(const char*)> Settings(std::map<std::string, std::string> values) {
  return [values](const char* name) {
    const auto it = values.find(name);
    return it != values.end() ? it->second : std::string();
  };
}

char16_t CapFor(const std::vector<keycap::GlyphSwap>& swaps, char16_t pad) {
  for (const auto& s : swaps)
    if (s.pad == pad) return s.cap;
  return 0;
}

void TestSingleGlyph() {
  Expect(keycap::SingleGlyph(keycap::Markup(u"X")) == u'X', "one glyph is read back");
  Expect(keycap::SingleGlyph(keycap::Markup(u"\\>")) == 0, "two glyphs are not one");
  Expect(keycap::SingleGlyph(keycap::Markup(u"")) == 0, "no glyph");
  Expect(keycap::SingleGlyph(u"[Space]") == 0, "a text label has no glyph");
  Expect(keycap::SingleGlyph(u"<Fonts:Other_Font>X<Fonts:/>") == 0, "another font is not the button font");
}

void TestDefaultBindings() {
  const auto get = Settings({{"mnk_mouse", "true"}});
  const auto swaps = keycap::PadGlyphSwaps(RetailControllerFont(),
                                           [&](const std::string& key) { return keycap::PromptFor(key, get); });
  // Pick up (X), reload (RB), aim (LT), fire (RT) and cover (A) show the port's default keys.
  Expect(CapFor(swaps, u'X') == keycap::CharFor("E"), "X shows E");
  Expect(CapFor(swaps, u'A') == keycap::CharFor("Space"), "A shows Space");
  Expect(CapFor(swaps, u'`') == keycap::CharFor("R"), "RB shows R");
  Expect(CapFor(swaps, u']') == keycap::CharFor("RMB"), "LT shows the right mouse button");
  Expect(CapFor(swaps, u'^') == keycap::CharFor("LMB"), "RT shows the left mouse button");
  Expect(CapFor(swaps, u'_') == keycap::CharFor("G"), "LB shows G");
  // Start and Back are two characters, and '\\' is shared by both: never moved.
  Expect(CapFor(swaps, u'\\') == 0 && CapFor(swaps, u'>') == 0 && CapFor(swaps, u'<') == 0,
         "Start and Back keep their glyphs");
  for (const auto& s : swaps) {
    bool pad_is_keycap = false;
    for (const keycap::Cell& c : keycap::kCells) pad_is_keycap = pad_is_keycap || c.ch == s.pad;
    Expect(!pad_is_keycap, "a keycap character's own cell is never moved");
  }
}

void TestRebindAndModifiers() {
  const auto get = Settings({{"keybind_x", "Shift+F, E"}, {"keybind_right_shoulder", "Semicolon"}});
  const auto swaps = keycap::PadGlyphSwaps(RetailControllerFont(),
                                           [&](const std::string& key) { return keycap::PromptFor(key, get); });
  Expect(CapFor(swaps, u'X') == keycap::CharFor("F"), "a chord shows its main key in one glyph");
  Expect(CapFor(swaps, u'`') == keycap::CharFor("Semicolon"), "a rebind is followed");
}

void TestNoCapLeavesGlyph() {
  const auto swaps = keycap::PadGlyphSwaps(RetailControllerFont(), [](const std::string& key) {
    return key == "Controller_A" ? keycap::Prompt{keycap::Markup(u"\x21"), keycap::Markup(u"\x21")}
                                 : keycap::Prompt{};
  });
  Expect(swaps.size() == 1 && CapFor(swaps, u'A') == u'\x21', "only entries with a cap are moved");
}

void TestConflictingEntries() {
  // Two entries drawing the same pad glyph but bound to different keys: the glyph cannot show both.
  const std::vector<std::pair<std::string, std::u16string>> originals = {
      {"Controller_ArrowDown", keycap::Markup(u"3")},
      {"Controller_LeftStickDown", keycap::Markup(u"3")},
      {"Controller_DPadDown", keycap::Markup(u"3")},
      {"Controller_A", keycap::Markup(u"A")},
  };
  const auto swaps = keycap::PadGlyphSwaps(originals, [](const std::string& key) {
    if (key == "Controller_DPadDown") return keycap::Prompt{keycap::Markup(u"\x22"), keycap::Markup(u"\x22")};
    if (key == "Controller_A") return keycap::Prompt{keycap::Markup(u"\x24"), keycap::Markup(u"\x24")};
    return keycap::Prompt{keycap::Markup(u"\x21"), keycap::Markup(u"\x21")};
  });
  Expect(CapFor(swaps, u'3') == 0, "a glyph claimed by two different caps stays the pad's");
  Expect(CapFor(swaps, u'A') == u'\x24', "the other glyphs still move");
  // Duplicate slots of one entry (the same string found twice in memory) are not a conflict.
  const auto twice = keycap::PadGlyphSwaps({{"Controller_A", keycap::Markup(u"A")}, {"Controller_A", keycap::Markup(u"A")}},
                                           [](const std::string&) {
                                             return keycap::Prompt{keycap::Markup(u"\x21"), keycap::Markup(u"\x21")};
                                           });
  Expect(twice.size() == 1 && CapFor(twice, u'A') == u'\x21', "the same entry twice moves once");
}

int main() {
  TestSingleGlyph();
  TestDefaultBindings();
  TestRebindAndModifiers();
  TestNoCapLeavesGlyph();
  TestConflictingEntries();
  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
