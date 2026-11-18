// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Host-side test of the Coalesced UTF-8 conversion and locale text (no game data, no SDK).
#include "gow2_coalesced_text.h"
#include "gow2_graphics_quality.h"
#include <unordered_map>

#include <cstdio>
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

void TestUtf8() {
  using rex::glue::Utf8ToUtf16;
  Expect(Utf8ToUtf16("").empty(), "empty stays empty");
  Expect(Utf8ToUtf16("PC Options") == u"PC Options", "ascii passes through");
  Expect(Utf8ToUtf16("caf\xC3\xA9") == u"caf\u00E9", "2-byte sequence decodes");
  Expect(Utf8ToUtf16("\xE2\x82\xAC") == u"\u20AC", "3-byte sequence decodes");
  const std::u16string smile = Utf8ToUtf16("\xF0\x9F\x98\x80");
  Expect(smile == u"\U0001F600", "4-byte sequence decodes to U+1F600");
  // Surrogates have no \u escape, so the expected halves are numeric.
  const std::u16string pair{char16_t(0xD83D), char16_t(0xDE00)};
  Expect(smile == pair, "astral plane uses a surrogate pair");
  Expect(smile.size() == 2 && smile[0] >= char16_t(0xD800) && smile[0] <= char16_t(0xDBFF) &&
             smile[1] >= char16_t(0xDC00) && smile[1] <= char16_t(0xDFFF),
         "surrogate halves are in range");
  Expect(Utf8ToUtf16("\xF4\x8F\xBF\xBF") ==
             std::u16string{char16_t(0xDBFF), char16_t(0xDFFF)},
         "U+10FFFF still encodes");
  Expect(Utf8ToUtf16("\xFF") == u"\uFFFD", "0xFF becomes U+FFFD");
  Expect(Utf8ToUtf16("\x80") == u"\uFFFD", "lone continuation becomes U+FFFD");
  Expect(Utf8ToUtf16("\xE2\x82") == u"\uFFFD", "truncated sequence becomes U+FFFD");
  Expect(Utf8ToUtf16("\xC0\xAF") == u"\uFFFD", "overlong encoding becomes U+FFFD");
  Expect(Utf8ToUtf16("\xED\xA0\x80") == u"\uFFFD", "encoded surrogate becomes U+FFFD");
  Expect(Utf8ToUtf16("\xF4\x90\x80\x80") == u"\uFFFD", "past U+10FFFF becomes U+FFFD");
  Expect(Utf8ToUtf16("a\xFF"
                     "b") == u"a\uFFFDb",
         "bad byte inside text becomes one U+FFFD");
}

struct Case {
  const char* locale;
  const char* label;
  const char* description;
};

void TestLocales(bool android) {
  using rex::glue::PcOptionsEntryText;
  static const Case kWindowsCases[] = {
      {"int", "PC Options", "Graphics, controls, network and saves."},
      {"esn", "Opciones de PC", "Pantalla, gráficos, controles, red y partidas en este PC."},
      {"esm", "Opciones de PC", "Pantalla, gráficos, controles, red y partidas en este PC."},
      {"deu", "PC-Optionen", "Anzeige, Grafik, Steuerung, Netzwerk und Spielstände auf diesem PC."},
      {"fra", "Options PC", "Affichage, graphismes, commandes, réseau et sauvegardes sur ce PC."},
      {"ita", "Opzioni PC", "Schermo, grafica, controlli, rete e salvataggi su questo PC."},
      {"pol", "Opcje PC", "Ekran, grafika, sterowanie, sieć i zapisy na tym komputerze."},
      {"cze", "Možnosti PC", "Obraz, grafika, ovládání, síť a uložené pozice na tomto PC."},
      {"hun", "PC-beállítások", "Kép, grafika, irányítás, hálózat és mentések ezen a PC-n."},
      {"rus", "Настройки ПК", "Экран, графика, управление, сеть и сохранения на этом ПК."},
      {"kor", "PC 옵션", "이 PC의 화면, 그래픽, 조작, 네트워크 및 저장 설정."},
      {"cht", "PC 選項", "此電腦的顯示、圖形、控制、網路與存檔設定。"},
  };
  static const Case kAndroidCases[] = {
      {"int", "Device Options", "Graphics, controls, network and saves."},
      {"esn", "Opciones del dispositivo", "Pantalla, gráficos, controles, red y partidas en este dispositivo."},
      {"esm", "Opciones del dispositivo", "Pantalla, gráficos, controles, red y partidas en este dispositivo."},
      {"deu", "Geräteoptionen", "Anzeige, Grafik, Steuerung, Netzwerk und Spielstände auf diesem Gerät."},
      {"fra", "Options appareil", "Affichage, graphismes, commandes, réseau et sauvegardes sur cet appareil."},
      {"ita", "Opzioni dispositivo", "Schermo, grafica, controlli, rete e salvataggi su questo dispositivo."},
      {"pol", "Opcje urządzenia", "Ekran, grafika, sterowanie, sieć i zapisy na tym urządzeniu."},
      {"cze", "Možnosti zařízení", "Obraz, grafika, ovládání, síť a uložené pozice na tomto zařízení."},
      {"hun", "Eszközbeállítások", "Kép, grafika, irányítás, hálózat és mentések ezen az eszközön."},
      {"rus", "Настройки устройства", "Экран, графика, управление, сеть и сохранения на этом устройстве."},
      {"kor", "기기 옵션", "이 기기의 화면, 그래픽, 조작, 네트워크 및 저장 설정."},
      {"cht", "裝置選項", "此裝置的顯示、圖形、控制、網路與存檔設定。"},
  };
  const auto& kCases = android ? kAndroidCases : kWindowsCases;
  for (const Case& c : kCases) {
    const rex::glue::LocalizedPair got = PcOptionsEntryText(c.locale, android);
    char what[64];
    std::snprintf(what, sizeof(what), "locale %s has its own text", c.locale);
    Expect(got.label == std::string_view(c.label) &&
               got.description == std::string_view(c.description),
           what);
  }
  static const char* const kFolded[] = {"INT", "ESN", "ESM", "DEU", "FRA", "ITA",
                                        "POL", "CZE", "HUN", "RUS", "KOR", "CHT"};
  for (size_t i = 0; i < 12; ++i) {
    const rex::glue::LocalizedPair upper = PcOptionsEntryText(kFolded[i], android);
    char what[64];
    std::snprintf(what, sizeof(what), "locale %s matches case-insensitively", kFolded[i]);
    Expect(upper.label == std::string_view(kCases[i].label) &&
               upper.description == std::string_view(kCases[i].description),
           what);
  }
  Expect(PcOptionsEntryText("EsN", android).label == PcOptionsEntryText("esn", android).label,
         "mixed case matches too");
  for (const char* unknown : {"jpn", "xxx", "", "e", "esnn", "in"}) {
    const rex::glue::LocalizedPair got = PcOptionsEntryText(unknown, android);
    char what[64];
    std::snprintf(what, sizeof(what), "unknown locale '%s' falls back to int", unknown);
    Expect(got.label == std::string_view(kCases[0].label) &&
               got.description ==
                   std::string_view("Graphics, controls, network and saves."),
           what);
  }
  Expect(PcOptionsEntryText("esm", android).label == PcOptionsEntryText("esn", android).label &&
             PcOptionsEntryText("esm", android).description == PcOptionsEntryText("esn", android).description,
         "esm shares the esn text");
}

void TestLocalizedConversion(bool android) {
  using rex::glue::PcOptionsEntryText;
  using rex::glue::Utf8ToUtf16;
  Expect(Utf8ToUtf16(PcOptionsEntryText("kor", android).label) ==
             (android ? u"기기 옵션" : u"PC \uC635\uC158"),
         "korean label converts to the expected code units");
  Expect(Utf8ToUtf16(PcOptionsEntryText("rus", android).label) ==
             (android ? u"Настройки устройства" :
                        u"\u041D\u0430\u0441\u0442\u0440\u043E\u0439\u043A\u0438 \u041F\u041A"),
         "russian label converts to the expected code units");
  static const char* const kLocales[] = {"int", "esn", "esm", "deu", "fra", "ita",
                                         "pol", "cze", "hun", "rus", "kor", "cht"};
  for (const char* loc : kLocales) {
    const rex::glue::LocalizedPair t = PcOptionsEntryText(loc, android);
    const std::u16string label = Utf8ToUtf16(t.label);
    const std::u16string desc = Utf8ToUtf16(t.description);
    char what[64];
    std::snprintf(what, sizeof(what), "locale %s converts with no U+FFFD", loc);
    Expect(!t.label.empty() && !t.description.empty() &&
               label.find(u'\uFFFD') == std::u16string::npos &&
               desc.find(u'\uFFFD') == std::u16string::npos,
           what);
  }
}

void TestGraphicsQuality() {
  namespace quality = rex::glue::graphics_quality;
  std::unordered_map<std::string, std::string> values;
  const auto read = [&](std::string_view id) {
    const auto found = values.find(std::string(id));
    return found == values.end() ? std::string("auto") : found->second;
  };
  Expect(quality::Settings().size() == 24, "quality metadata includes 24 retail keys");
  Expect(quality::Compose(false, read, "").empty(), "PC auto leaves every retail value unchanged");
  Expect(quality::Compose(true, read, "") == quality::Pairs{{"LightEnvironmentShadows", "0"}},
         "Android auto disables only object shadows");
  Expect(quality::DetectPreset(read) == "high", "PC default is retail high");
  Expect(quality::DetectPreset(read, true) == "custom", "Android default is distinct from high");
  values["gow2_gfx_light_environment_shadows"] = "on";
  values["gow2_gfx_bloom"] = "off";
  Expect(quality::Compose(true, read, "") == quality::Pairs{{"LightEnvironmentShadows", "1"}, {"Bloom", "0"}},
         "explicit rows override platform defaults with shortest booleans");
  const auto raw = quality::Compose(true, read,
      " bloom = True ; LIGHTENVIRONMENTSHADOWS=False; Bloom=off;Invalid; =3;UseVsync=TRUE");
  Expect(raw == quality::Pairs{{"LightEnvironmentShadows", "0"}, {"Bloom", "0"}, {"UseVsync", "1"}},
         "raw overrides explicit rows case insensitively and last pair wins");
  Expect(quality::EngineValue("bEnableVSMShadows", "False") == "0",
         "raw boolean keys outside menu use shortest false value");
  Expect(quality::EngineValue("OtherKey", "True") == "True", "unknown raw keys stay verbatim");
  values["gow2_gfx_bloom"] = "invalid";
  Expect(quality::Compose(false, read, "") == quality::Pairs{{"LightEnvironmentShadows", "1"}},
         "invalid fixed-choice rows are ignored");
  for (std::string_view preset : {"low", "medium", "high"}) {
    for (const auto& setting : quality::Settings()) {
      const std::string value = quality::PresetValue(setting, preset);
      Expect(std::find(setting.choices.begin(), setting.choices.end(), value) != setting.choices.end(),
             "preset uses a supported explicit row value");
      values[std::string(setting.id)] = value;
    }
    Expect(quality::DetectPreset(read) == preset, "preset detection round trips explicit rows");
    const auto composed = quality::Compose(true, read, "");
    Expect(composed.size() == quality::Settings().size(), "preset explicitly sets all quality keys");
    if (preset == "high")
      Expect(composed.front().second == "1", "high overrides Android object-shadow workaround");
    values["gow2_gfx_motion_blur"] = "auto";
    Expect(quality::DetectPreset(read) == "custom", "individual edit switches preset to Custom");
  }
  std::unordered_map<std::string, std::string> scale;
  const auto read_scale = [&](std::string_view id) {
    const auto found = scale.find(std::string(id));
    return found == scale.end() ? std::string("auto") : found->second;
  };
  scale["gow2_render_scale"] = "75";
  Expect(quality::Compose(false, read_scale, "") == quality::Pairs{{"ScreenPercentage", "75"}},
         "render scale 75 writes ScreenPercentage");
  scale["gow2_render_scale"] = "100";
  Expect(quality::Compose(false, read_scale, "").empty(),
         "render scale 100 keeps the retail percentage");
  scale["gow2_render_scale"] = "75";
  Expect(quality::Compose(false, read_scale, "ScreenPercentage=60") ==
             quality::Pairs{{"ScreenPercentage", "60"}},
         "raw ScreenPercentage overrides the render scale row");
}

void TestBundleSizeFallback() {
  namespace quality = rex::glue::graphics_quality;
  std::vector<uint8_t> bytes{1, 2};
  Expect(quality::PadBundleToSize(&bytes, 4) && bytes == std::vector<uint8_t>({1, 2, 0, 0}),
         "short bundle pads to exact TOC size with zero bytes");
  Expect(quality::PadBundleToSize(&bytes, 4), "exact-size bundle is accepted");
  Expect(!quality::PadBundleToSize(&bytes, 3) && bytes.size() == 4,
         "oversized bundle is rejected without truncation");
  int rebuilt = 0;
  Expect(quality::PadOrKeepRetail(&bytes, 4, [&] {
    ++rebuilt;
    return std::vector<uint8_t>{9};
  }) && rebuilt == 0, "fitting edit does not rebuild retail settings");
  Expect(quality::PadOrKeepRetail(&bytes, 3, [&] {
    ++rebuilt;
    return std::vector<uint8_t>{9, 8};
  }) && rebuilt == 1 && bytes == std::vector<uint8_t>({9, 8, 0}),
         "production growth fallback serves retail settings padded to TOC size");
  const auto before = bytes;
  Expect(!quality::PadOrKeepRetail(&bytes, 1, [] {
    return std::vector<uint8_t>{7, 6};
  }) && bytes == before, "even oversized retail rebuild is rejected without corrupting bytes");
}

int main() {
  TestGraphicsQuality();
  TestBundleSizeFallback();
  TestUtf8();
  TestLocales(false);
  TestLocales(true);
  TestLocalizedConversion(false);
  TestLocalizedConversion(true);
  std::printf("coalesced_text_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
