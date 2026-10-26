// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_voice_language.h"

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

int g_checks = 0;
int g_failures = 0;
void Expect(bool ok, std::string_view what) {
  ++g_checks;
  if (ok) return;
  ++g_failures;
  std::printf("FAIL: %.*s\n", int(what.size()), what.data());
}

using namespace rex::glue::voice_language;

std::string Line(std::string_view size, std::string_view name, std::string_view md5) {
  return std::string(size) + " " + std::string(size) + "1 " + std::string(size) + "2 ..\\GearGame\\CookedXenon\\" +
         std::string(name) + " " + std::string(md5);
}

std::string Join(const std::vector<std::string>& lines, std::string_view end) {
  std::string text;
  for (const auto& line : lines) text += line + std::string(end);
  return text;
}

struct Fixture {
  std::string int_name = "Xbox360TOC.txt";
  std::string esm_name = "Xbox360TOC_ESM.txt";
  std::vector<std::string> int_lines;
  std::vector<std::string> esm_lines;
  std::vector<std::string> fra_lines;

  Fixture(std::string_view sp_a_int = "SP_A_LOC_int.xxx") {
    int_lines = {Line("100", sp_a_int, "aaa"),
                 Line("110", "GUDS_X_SF_LOC_int.xxx", "bbb"),
                 Line("120", "GearGame_LOC_int.xxx", "ggi"),
                 Line("130", "GearGame_LOC_KOR.xxx", "ggk"),
                 Line("140", "Startup_int.xxx", "sti"),
                 Line("150", "SP_OnlyInt_LOC_int.xxx", "ooo"),
                 "garbage"};
    esm_lines = {Line("200", "SP_A_LOC_ESM.xxx", "ccc"),
                 Line("210", "GUDS_X_SF_LOC_ESM.xxx", "ddd"),
                 Line("220", "GearGame_LOC_ESM.xxx", "gge"),
                 Line("230", "Startup_ESM.xxx", "ste")};
    fra_lines = {Line("300", "SP_A_LOC_FRA.xxx", "eee")};
  }

  std::vector<TocFile> Tocs(std::string_view end = "\r\n") const {
    return {{int_name, Join(int_lines, end)},
            {esm_name, Join(esm_lines, end)},
            {"Xbox360TOC_FRA.txt", Join(fra_lines, end)}};
  }
};

bool HasSwap(const VoicePlan& plan, std::string_view guest, std::string_view source) {
  for (const auto& swap : plan.swaps)
    if (swap.guest == guest && swap.source == source) return true;
  return false;
}

const TocFile* Edited(const VoicePlan& plan, std::string_view name) {
  for (const auto& toc : plan.edited_tocs)
    if (toc.name == name) return &toc;
  return nullptr;
}

bool Contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

void ExpectNothingSwapped(const std::vector<TocFile>& tocs, std::string_view text, std::string_view voice,
                          std::string_view what) {
  VoicePlan plan = PlanVoiceSwap(tocs, text, voice);
  std::string expected = "VOICES text " + std::string(text) + ", voices same as text: nothing swapped";
  Expect(plan.swaps.empty() && plan.edited_tocs.empty() && plan.summary == expected, what);
}

int main() {
  Expect(TextLanguageCode(1) == "INT", "user_language 1 is INT");
  Expect(TextLanguageCode(5) == "ESM", "user_language 5 is ESM");
  Expect(TextLanguageCode(4) == "FRA", "user_language 4 is FRA");
  Expect(TextLanguageCode(3) == "DEU", "user_language 3 is DEU");
  Expect(TextLanguageCode(6) == "ITA", "user_language 6 is ITA");
  Expect(TextLanguageCode(11) == "POL", "user_language 11 is POL");
  Expect(TextLanguageCode(12) == "RUS", "user_language 12 is RUS");
  Expect(TextLanguageCode(7) == "KOR", "user_language 7 is KOR");
  Expect(TextLanguageCode(8) == "CHT", "user_language 8 is CHT");
  Expect(TextLanguageCode(99) == "INT", "an unknown user_language is INT");
  Expect(VoiceChoiceCode("auto").empty(), "auto voices have no code");
  Expect(VoiceChoiceCode("en") == "INT", "en voices are INT");
  Expect(VoiceChoiceCode("es") == "ESM", "es voices are ESM");
  Expect(VoiceChoiceCode("fr") == "FRA", "fr voices are FRA");
  Expect(VoiceChoiceCode("xx").empty(), "an unknown voice choice has no code");

  const std::string sample = "100 0 5 ..\\GearGame\\CookedXenon\\A.xxx aaa";
  auto parsed = ParseTocLine(sample);
  Expect(parsed && parsed->size == "100" && parsed->uncompressed == "0" && parsed->third == "5" &&
             parsed->path == "..\\GearGame\\CookedXenon\\A.xxx" && parsed->md5 == "aaa",
         "a TOC line parses into its five fields");
  Expect(!ParseTocLine("garbage"), "a one-field line does not parse");
  Expect(!ParseTocLine(sample + " extra"), "a six-field line does not parse");
  Expect(parsed && FormatTocLine(*parsed) == sample, "a parsed line formats back to itself");

  Fixture fx;
  auto tocs = fx.Tocs();

  {
    VoicePlan plan = PlanVoiceSwap(tocs, "ESM", "INT");
    Expect(plan.swaps.size() == 2, "text ESM voices INT swaps two packages");
    Expect(HasSwap(plan, "GearGame\\CookedXenon\\SP_A_LOC_ESM.xxx", "SP_A_LOC_int.xxx"),
           "text ESM voices INT serves SP_A int as ESM");
    Expect(HasSwap(plan, "GearGame\\CookedXenon\\GUDS_X_SF_LOC_ESM.xxx", "GUDS_X_SF_LOC_int.xxx"),
           "text ESM voices INT serves GUDS int as ESM");
    Expect(plan.edited_tocs.size() == 1, "text ESM voices INT edits one TOC");
    const TocFile* esm = Edited(plan, "Xbox360TOC_ESM.txt");
    std::vector<std::string> expected = fx.esm_lines;
    expected[0] = "100 1001 1002 ..\\GearGame\\CookedXenon\\SP_A_LOC_ESM.xxx aaa";
    expected[1] = "110 1101 1102 ..\\GearGame\\CookedXenon\\GUDS_X_SF_LOC_ESM.xxx bbb";
    Expect(esm && esm->text == Join(expected, "\r\n"),
           "the ESM TOC takes the int lines' sizes and md5, keeps the rest and its CRLF");
    Expect(plan.summary == "VOICES text ESM, voices INT: 2 packages swapped", "text ESM voices INT summary");
  }

  {
    VoicePlan plan = PlanVoiceSwap(tocs, "INT", "ESM");
    Expect(plan.swaps.size() == 2, "text INT voices ESM swaps two packages");
    Expect(HasSwap(plan, "GearGame\\CookedXenon\\SP_A_LOC_int.xxx", "SP_A_LOC_ESM.xxx"),
           "text INT voices ESM serves SP_A ESM as int");
    Expect(HasSwap(plan, "GearGame\\CookedXenon\\GUDS_X_SF_LOC_int.xxx", "GUDS_X_SF_LOC_ESM.xxx"),
           "text INT voices ESM serves GUDS ESM as int");
    const TocFile* intl = Edited(plan, "Xbox360TOC.txt");
    std::vector<std::string> expected = fx.int_lines;
    expected[0] = "200 2001 2002 ..\\GearGame\\CookedXenon\\SP_A_LOC_int.xxx ccc";
    expected[1] = "210 2101 2102 ..\\GearGame\\CookedXenon\\GUDS_X_SF_LOC_int.xxx ddd";
    Expect(plan.edited_tocs.size() == 1 && intl && intl->text == Join(expected, "\r\n"),
           "text INT voices ESM edits only the int TOC, keeping SP_OnlyInt and garbage");
    Expect(plan.summary == "VOICES text INT, voices ESM: 2 packages swapped", "text INT voices ESM summary");

    VoicePlan deu = PlanVoiceSwap(tocs, "DEU", "ESM");
    Expect(deu.swaps.size() == 2 && HasSwap(deu, "GearGame\\CookedXenon\\SP_A_LOC_int.xxx", "SP_A_LOC_ESM.xxx") &&
               HasSwap(deu, "GearGame\\CookedXenon\\GUDS_X_SF_LOC_int.xxx", "GUDS_X_SF_LOC_ESM.xxx"),
           "text DEU voices ESM swaps like text INT");
    Expect(deu.edited_tocs.size() == 1 && Edited(deu, "Xbox360TOC.txt") &&
               Edited(deu, "Xbox360TOC.txt")->text == intl->text,
           "text DEU voices ESM edits the int TOC like text INT");
  }

  {
    VoicePlan plan = PlanVoiceSwap(tocs, "FRA", "ESM");
    Expect(plan.swaps.size() == 2, "text FRA voices ESM swaps two packages");
    Expect(HasSwap(plan, "GearGame\\CookedXenon\\SP_A_LOC_FRA.xxx", "SP_A_LOC_ESM.xxx"),
           "text FRA voices ESM serves SP_A ESM as FRA");
    Expect(HasSwap(plan, "GearGame\\CookedXenon\\GUDS_X_SF_LOC_int.xxx", "GUDS_X_SF_LOC_ESM.xxx"),
           "text FRA voices ESM serves GUDS ESM as int, since there is no FRA GUDS");
    const TocFile* fra = Edited(plan, "Xbox360TOC_FRA.txt");
    const TocFile* intl = Edited(plan, "Xbox360TOC.txt");
    Expect(plan.edited_tocs.size() == 2 && fra && intl, "text FRA voices ESM edits the FRA and int TOCs");
    Expect(fra && fra->text == "200 2001 2002 ..\\GearGame\\CookedXenon\\SP_A_LOC_FRA.xxx ccc\r\n",
           "the FRA TOC takes the ESM SP_A line");
    std::vector<std::string> expected = fx.int_lines;
    expected[1] = "210 2101 2102 ..\\GearGame\\CookedXenon\\GUDS_X_SF_LOC_int.xxx ddd";
    Expect(intl && intl->text == Join(expected, "\r\n"), "the int TOC takes only the ESM GUDS line");
    Expect(plan.summary == "VOICES text FRA, voices ESM: 2 packages swapped", "text FRA voices ESM summary");
  }

  ExpectNothingSwapped(tocs, "DEU", "INT", "text DEU voices INT swaps nothing");
  ExpectNothingSwapped(tocs, "KOR", "", "text KOR auto voices swaps nothing");
  ExpectNothingSwapped(tocs, "ESM", "ESM", "text ESM voices ESM swaps nothing");
  ExpectNothingSwapped(tocs, "ESM", "", "text ESM auto voices swaps nothing");
  ExpectNothingSwapped(tocs, "INT", "INT", "text INT voices INT swaps nothing");

  {
    std::vector<TocFile> only_int = {tocs[0]};
    VoicePlan plan = PlanVoiceSwap(only_int, "INT", "ESM");
    Expect(plan.swaps.empty() && plan.edited_tocs.empty(), "a missing voice set swaps nothing");
    Expect(plan.summary == "VOICES voice set ESM is not on this disc: nothing swapped", "missing voice set summary");
  }

  {
    Fixture mixed("sp_a_loc_INT.xxx");
    mixed.esm_name = "Xbox360TOC_esm.txt";
    VoicePlan plan = PlanVoiceSwap(mixed.Tocs(), "esm", "int");
    Expect(plan.swaps.size() == 2, "mixed case names still swap two packages");
    Expect(HasSwap(plan, "GearGame\\CookedXenon\\SP_A_LOC_ESM.xxx", "sp_a_loc_INT.xxx"),
           "mixed case keeps the source's own spelling");
    Expect(HasSwap(plan, "GearGame\\CookedXenon\\GUDS_X_SF_LOC_ESM.xxx", "GUDS_X_SF_LOC_int.xxx"),
           "mixed case still finds GUDS");
    const TocFile* esm = Edited(plan, "Xbox360TOC_esm.txt");
    Expect(plan.edited_tocs.size() == 1 && esm &&
               Contains(esm->text, "100 1001 1002 ..\\GearGame\\CookedXenon\\SP_A_LOC_ESM.xxx aaa\r\n"),
           "mixed case edits the TOC under its own name and keeps the target's path");
  }

  {
    VoicePlan plan = PlanVoiceSwap(fx.Tocs("\n"), "INT", "ESM");
    const TocFile* intl = Edited(plan, "Xbox360TOC.txt");
    std::vector<std::string> expected = fx.int_lines;
    expected[0] = "200 2001 2002 ..\\GearGame\\CookedXenon\\SP_A_LOC_int.xxx ccc";
    expected[1] = "210 2101 2102 ..\\GearGame\\CookedXenon\\GUDS_X_SF_LOC_int.xxx ddd";
    Expect(intl && intl->text == Join(expected, "\n"), "an LF TOC stays LF after editing");

    std::vector<TocFile> no_final = fx.Tocs("\r\n");
    no_final[0].text.resize(no_final[0].text.size() - 2);
    VoicePlan cut = PlanVoiceSwap(no_final, "INT", "ESM");
    const TocFile* cut_int = Edited(cut, "Xbox360TOC.txt");
    std::string cut_expected = Join(expected, "\r\n");
    cut_expected.resize(cut_expected.size() - 2);
    Expect(cut_int && cut_int->text == cut_expected, "a TOC without a final newline stays without one");
  }

  std::printf("voice_language_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
