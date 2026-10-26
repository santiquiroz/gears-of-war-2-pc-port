// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Voice language at boot: reads the disc's TOCs, writes the edited copies and the VFS overrides file.
#include "gow2_voice_language_setup.h"

#include <algorithm>
#include <exception>
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

#include <rex/cvar.h>

#include "gow2_voice_language.h"

namespace rex::glue {

REXCVAR_DEFINE_STRING(gow2_voice_language, "auto", "UI", "Voice language: auto (same as subtitles), en, es or fr");

namespace {

namespace fs = std::filesystem;
namespace vl = voice_language;

std::string Lower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c); });
  return text;
}

std::string Utf8(const fs::path& path) {
  const std::u8string text = path.u8string();
  return std::string(text.begin(), text.end());
}

fs::path FromUtf8(std::string_view text) {
  return fs::path(std::u8string(text.begin(), text.end()));
}

bool RegularFiles(const fs::path& dir, std::vector<fs::path>& files) {
  std::error_code ec;
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
    std::error_code type_ec;
    if (it->is_regular_file(type_ec)) files.push_back(it->path());
  }
  return !ec;
}

bool HasCoalesced(const std::vector<fs::path>& cooked, const std::string& text_code) {
  const std::string wanted = "coalesced_" + Lower(text_code) + ".bin";
  for (const fs::path& file : cooked) {
    if (Lower(Utf8(file.filename())) == wanted) return true;
  }
  return false;
}

bool IsToc(const std::string& lower_name) {
  return lower_name.starts_with("xbox360toc") && lower_name.ends_with(".txt");
}

bool ReadWhole(const fs::path& path, std::string& text) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return !in.bad();
}

bool WriteWhole(const fs::path& path, const std::string& text) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) return false;
  out.write(text.data(), static_cast<std::streamsize>(text.size()));
  out.close();
  return !out.fail();
}

bool ReadTocs(const fs::path& game_root, std::vector<vl::TocFile>& tocs, std::string& failed) {
  std::vector<fs::path> files;
  if (!RegularFiles(game_root / "GearGame", files)) {
    failed = "could not list GearGame";
    return false;
  }
  for (const fs::path& file : files) {
    const std::string name = Utf8(file.filename());
    if (!IsToc(Lower(name))) continue;
    vl::TocFile toc{name, {}};
    if (!ReadWhole(file, toc.text)) {
      failed = "could not read GearGame\\" + name;
      return false;
    }
    tocs.push_back(std::move(toc));
  }
  return true;
}

fs::path Absolute(const fs::path& path) {
  std::error_code ec;
  fs::path absolute = fs::absolute(path, ec);
  return ec ? path : absolute.lexically_normal();
}

fs::path SourcePath(const fs::path& game_root, const vl::FileSwap& swap) {
  fs::path path = game_root;
  std::string_view guest = swap.guest;
  const size_t last = guest.find_last_of("\\/");
  if (last == std::string_view::npos) return Absolute(path / FromUtf8(swap.source));
  guest = guest.substr(0, last);
  while (!guest.empty()) {
    const size_t sep = guest.find_first_of("\\/");
    if (sep != 0) path /= FromUtf8(guest.substr(0, sep));
    if (sep == std::string_view::npos) break;
    guest.remove_prefix(sep + 1);
  }
  return Absolute(path / FromUtf8(swap.source));
}

void RemoveOverrides(const fs::path& overrides) {
  std::error_code ec;
  if (fs::exists(overrides, ec)) fs::remove(overrides, ec);
}

VoiceLanguageSetup Failed(VoiceLanguageSetup setup, const std::string& why) {
  setup.overrides_file.clear();
  setup.failed = true;
  setup.summary = "VOICES " + why + "; the voices stay as the game picks them";
  return setup;
}

VoiceLanguageSetup WriteOverrides(VoiceLanguageSetup setup, const fs::path& game_root, const fs::path& voice_dir,
                                  const vl::VoicePlan& plan) {
  std::error_code ec;
  fs::create_directories(voice_dir, ec);
  if (ec) return Failed(std::move(setup), "could not create " + Utf8(voice_dir));
  std::string lines;
  for (const vl::TocFile& toc : plan.edited_tocs) {
    const fs::path copy = Absolute(voice_dir / FromUtf8(toc.name));
    if (!WriteWhole(copy, toc.text)) return Failed(std::move(setup), "could not write " + Utf8(copy));
    lines += "GearGame\\" + toc.name + "=" + Utf8(copy) + "\n";
  }
  for (const vl::FileSwap& swap : plan.swaps) {
    lines += swap.guest + "=" + Utf8(SourcePath(game_root, swap)) + "\n";
  }
  const fs::path overrides = Absolute(voice_dir / "overrides.txt");
  if (!WriteWhole(overrides, lines)) {
    RemoveOverrides(overrides);
    return Failed(std::move(setup), "could not write " + Utf8(overrides));
  }
  setup.overrides_file = overrides;
  return setup;
}

bool VoiceSetMissing(const vl::VoicePlan& plan) {
  return plan.swaps.empty() && plan.summary.find(" is not on this disc") != std::string::npos;
}

VoiceLanguageSetup Prepare(const fs::path& game_root, const fs::path& cache_dir, uint32_t user_language,
                           std::string_view voice_choice) {
  VoiceLanguageSetup setup;
  const fs::path voice_dir = cache_dir / "voice";
  std::vector<fs::path> cooked;
  if (!RegularFiles(game_root / "GearGame" / "CookedXenon", cooked)) {
    RemoveOverrides(voice_dir / "overrides.txt");
    return Failed(std::move(setup), "could not list GearGame\\CookedXenon");
  }
  std::string text_code = vl::TextLanguageCode(user_language);
  setup.text_missing = !HasCoalesced(cooked, text_code);
  if (setup.text_missing) text_code = "INT";
  std::vector<vl::TocFile> tocs;
  std::string failed;
  if (!ReadTocs(game_root, tocs, failed)) {
    RemoveOverrides(voice_dir / "overrides.txt");
    return Failed(std::move(setup), failed);
  }
  const vl::VoicePlan plan = vl::PlanVoiceSwap(tocs, text_code, vl::VoiceChoiceCode(voice_choice));
  setup.summary = plan.summary;
  setup.failed = VoiceSetMissing(plan);
  if (plan.swaps.empty()) {
    RemoveOverrides(voice_dir / "overrides.txt");
    return setup;
  }
  return WriteOverrides(std::move(setup), game_root, voice_dir, plan);
}

}  // namespace

VoiceLanguageSetup PrepareVoiceLanguage(const fs::path& game_root, const fs::path& cache_dir, uint32_t user_language,
                                        std::string_view voice_choice) {
  try {
    return Prepare(game_root, cache_dir, user_language, voice_choice);
  } catch (const std::exception& e) {
    VoiceLanguageSetup setup;
    setup.failed = true;
    setup.summary = std::string("VOICES setup failed (") + e.what() + "); the voices stay as the game picks them";
    return setup;
  }
}

}  // namespace rex::glue
