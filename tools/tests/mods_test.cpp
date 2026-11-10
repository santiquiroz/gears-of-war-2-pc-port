// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_mods.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "third_party/TinySHA1.hpp"

namespace fs = std::filesystem;
namespace mods = rex::glue::mods;
namespace vl = rex::glue::voice_language;

// Local FileSha1 implementation for host tests (avoids SDK dependency).
namespace rex::glue {
bool FileSha1(const fs::path& path, std::array<uint8_t, 20>* out) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return false;
  sha1::SHA1 sha;
  std::vector<char> buffer(1 << 16);
  while (file) {
    file.read(buffer.data(), std::streamsize(buffer.size()));
    if (file.gcount() > 0) sha.processBytes(buffer.data(), size_t(file.gcount()));
  }
  const uint32_t* words = sha.getDigest();
  for (size_t i = 0; i < 5; ++i) {
    (*out)[i * 4 + 0] = uint8_t(words[i] >> 24);
    (*out)[i * 4 + 1] = uint8_t(words[i] >> 16);
    (*out)[i * 4 + 2] = uint8_t(words[i] >> 8);
    (*out)[i * 4 + 3] = uint8_t(words[i]);
  }
  return true;
}
}  // namespace rex::glue

int g_checks = 0;
int g_failures = 0;

void Expect(bool ok, std::string_view what) {
  ++g_checks;
  if (ok) return;
  ++g_failures;
  std::printf("FAIL: %.*s\n", int(what.size()), what.data());
}

void ExpectEq(std::string_view a, std::string_view b, std::string_view what) {
  ++g_checks;
  if (a == b) return;
  ++g_failures;
  std::printf("FAIL: %.*s: expected '%.*s', got '%.*s'\n",
              int(what.size()), what.data(),
              int(b.size()), b.data(), int(a.size()), a.data());
}

std::string Line(std::string_view size, std::string_view uncompressed, std::string_view third,
                 std::string_view path, std::string_view md5) {
  return std::string(size) + " " + std::string(uncompressed) + " " + std::string(third) + " " +
         std::string(path) + " " + std::string(md5);
}

std::string Join(const std::vector<std::string>& lines, std::string_view end) {
  std::string text;
  for (const auto& line : lines) text += line + std::string(end);
  return text;
}

struct TocFixture {
  std::string int_name = "Xbox360TOC.txt";
  std::string esm_name = "Xbox360TOC_ESM.txt";
  std::vector<std::string> int_lines;
  std::vector<std::string> esm_lines;

  TocFixture() {
    int_lines = {Line("100", "0", "5", "..\\GearGame\\CookedXenon\\SP_A_LOC_int.xxx", "aaa"),
                 Line("110", "0", "6", "..\\GearGame\\CookedXenon\\GUDS_X_SF_LOC_int.xxx", "bbb"),
                 Line("120", "0", "7", "..\\GearGame\\GearGame_LOC_int.xxx", "ggi")};
    esm_lines = {Line("200", "0", "5", "..\\GearGame\\CookedXenon\\SP_A_LOC_ESM.xxx", "ccc"),
                 Line("210", "0", "6", "..\\GearGame\\CookedXenon\\GUDS_X_SF_LOC_ESM.xxx", "ddd")};
  }

  std::vector<vl::TocFile> Tocs(std::string_view end = "\r\n") const {
    return {{int_name, Join(int_lines, end)},
            {esm_name, Join(esm_lines, end)}};
  }
};

const vl::TocFile* FindToc(const std::vector<vl::TocFile>& tocs, std::string_view name) {
  for (const auto& toc : tocs)
    if (toc.name == name) return &toc;
  return nullptr;
}

bool HasOverride(const std::vector<std::string>& lines, std::string_view prefix) {
  for (const auto& line : lines)
    if (line.starts_with(prefix)) return true;
  return false;
}

std::string GetOverrideSource(const std::vector<std::string>& lines, std::string_view prefix) {
  for (const auto& line : lines) {
    if (line.starts_with(prefix)) {
      const size_t eq = line.find('=');
      if (eq != std::string::npos) return std::string(line.substr(eq + 1));
    }
  }
  return {};
}

void TestModOrderAndDisable() {
  fs::path root = fs::temp_directory_path() / "gow2_mods_test_order";
  fs::remove_all(root);
  fs::create_directories(root / "alpha" / "GearGame");
  fs::create_directories(root / "_disabled" / "GearGame");
  fs::create_directories(root / ".hidden" / "GearGame");
  fs::create_directories(root / "beta" / "GearGame");
  std::ofstream(root / "alpha" / "mod.ini") << "name=Alpha Mod\nversion=1.0\nauthor=Tester\n";
  std::ofstream(root / "beta" / "mod.ini") << "name=Beta Mod\nversion=2.0\nauthor=Tester\n";

  auto mods = mods::FindMods(root);
  Expect(mods.size() == 2, "only alpha and beta, not _disabled or .hidden");
  Expect(mods[0].folder == "alpha", "alpha comes first (case-insensitive sort)");
  Expect(mods[1].folder == "beta", "beta comes second");
  Expect(mods[0].name == "Alpha Mod", "mod.ini name used");
  Expect(mods[1].name == "Beta Mod", "mod.ini name used");

  fs::remove_all(root);
}

void TestLaterModWins() {
  fs::path root = fs::temp_directory_path() / "gow2_mods_test_wins";
  fs::remove_all(root);
  fs::create_directories(root / "alpha" / "GearGame" / "CookedXenon");
  fs::create_directories(root / "beta" / "GearGame" / "CookedXenon");
  std::ofstream(root / "alpha" / "GearGame" / "CookedXenon" / "file.txt") << "alpha content";
  std::ofstream(root / "beta" / "GearGame" / "CookedXenon" / "file.txt") << "beta content";

  auto mods = mods::FindMods(root);
  auto files = mods::FindFiles(root, mods);
  TocFixture fx;
  auto tocs = fx.Tocs();
  auto plan = mods::BuildPlan(mods, files, tocs);

  Expect(plan.override_lines.size() == 2, "both files produce override lines");
  Expect(plan.winners.size() == 1, "only one winner for the same relative path");
  Expect(plan.winners[0].second->mod_folder == "beta", "beta wins (later mod)");
  Expect(plan.winners[0].second->source.find("beta") != std::string::npos, "winner source is beta's file");

  fs::remove_all(root);
}

void TestTocLineKeepsStartSector() {
  fs::path root = fs::temp_directory_path() / "gow2_mods_test_toc";
  fs::remove_all(root);
  fs::create_directories(root / "mymod" / "GearGame" / "CookedXenon");
  std::ofstream(root / "mymod" / "GearGame" / "CookedXenon" / "SP_A_LOC_int.xxx") << "new content";

  auto mods = mods::FindMods(root);
  auto files = mods::FindFiles(root, mods);
  TocFixture fx;
  fx.int_lines[0] = Line("100", "0", "42", "..\\GearGame\\CookedXenon\\SP_A_LOC_int.xxx", "aaa"); // third=42
  auto tocs = fx.Tocs();
  auto plan = mods::BuildPlan(mods, files, tocs);

  Expect(plan.edited_tocs.size() == 1, "one TOC edited");
  const vl::TocFile* edited = FindToc(plan.edited_tocs, "Xbox360TOC.txt");
  Expect(edited != nullptr, "edited TOC is Xbox360TOC.txt");
  Expect(edited->text.find(" 42 ") != std::string::npos, "StartSector (third field) kept as 42");
  Expect(edited->text.find("aaa") == std::string::npos, "old MD5 replaced");
  Expect(edited->text.find("new content") == std::string::npos, "source path not in TOC");

  fs::remove_all(root);
}

void TestUnknownFileAppendedToMainToc() {
  fs::path root = fs::temp_directory_path() / "gow2_mods_test_unknown";
  fs::remove_all(root);
  fs::create_directories(root / "mymod" / "GearGame" / "CustomFolder");
  std::ofstream(root / "mymod" / "GearGame" / "CustomFolder" / "newfile.txt") << "new file";

  auto mods = mods::FindMods(root);
  auto files = mods::FindFiles(root, mods);
  TocFixture fx;
  auto tocs = fx.Tocs();
  auto plan = mods::BuildPlan(mods, files, tocs);

  Expect(plan.edited_tocs.size() == 1, "main TOC edited");
  const vl::TocFile* edited = FindToc(plan.edited_tocs, "Xbox360TOC.txt");
  Expect(edited != nullptr, "edited TOC is Xbox360TOC.txt");
  Expect(edited->text.find("..\\GearGame\\CustomFolder\\newfile.txt") != std::string::npos,
         "unknown file appended to main TOC");
  Expect(edited->text.find("0 0 ") != std::string::npos, "appended line has size 0 and uncompressed 0");

  fs::remove_all(root);
}

void TestIgnoredExtensionsAndCoalescedWarning() {
  fs::path root = fs::temp_directory_path() / "gow2_mods_test_ignored";
  fs::remove_all(root);
  fs::create_directories(root / "mymod" / "GearGame");
  std::ofstream(root / "mymod" / "GearGame" / "test.xex") << "exe";
  std::ofstream(root / "mymod" / "GearGame" / "test.dll") << "dll";
  std::ofstream(root / "mymod" / "GearGame" / "Coalesced_int.bin") << "coalesced";
  std::ofstream(root / "mymod" / "GearGame" / "normal.txt") << "ok";

  auto mods = mods::FindMods(root);
  auto files = mods::FindFiles(root, mods);
  TocFixture fx;
  auto tocs = fx.Tocs();
  auto plan = mods::BuildPlan(mods, files, tocs);

  Expect(plan.warnings.size() == 3, "three warnings for skipped files");
  Expect(std::any_of(plan.warnings.begin(), plan.warnings.end(),
                     [](const std::string& w) { return w.find("test.xex") != std::string::npos && w.find("executable") != std::string::npos; }),
         "warning for .xex mentions executable");
  Expect(std::any_of(plan.warnings.begin(), plan.warnings.end(),
                     [](const std::string& w) { return w.find("test.dll") != std::string::npos && w.find("executable") != std::string::npos; }),
         "warning for .dll mentions executable");
  Expect(std::any_of(plan.warnings.begin(), plan.warnings.end(),
                     [](const std::string& w) { return w.find("Coalesced_int.bin") != std::string::npos && w.find("not supported yet") != std::string::npos; }),
         "warning for Coalesced mentions not supported yet");
  Expect(plan.winners.size() == 1, "only normal.txt is a winner");
  Expect(plan.winners[0].second->relative_path == "normal.txt", "winner is normal.txt");

  fs::remove_all(root);
}

void TestCompressedHeaderParse() {
  fs::path root = fs::temp_directory_path() / "gow2_mods_test_compressed";
  fs::remove_all(root);
  fs::create_directories(root / "mymod" / "GearGame" / "CookedXenon");

  std::vector<uint8_t> header(16);
  header[0] = 0x9e; header[1] = 0x2a; header[2] = 0x83; header[3] = 0xc1; // tag
  header[4] = 0x00; header[5] = 0x00; header[6] = 0x10; header[7] = 0x00; // block size (4096, big-endian)
  header[8] = 0x00; header[9] = 0x00; header[10] = 0x00; header[11] = 0x80; // compressed size (128)
  header[12] = 0x00; header[13] = 0x00; header[14] = 0x04; header[15] = 0x00; // uncompressed size (1024, big-endian)
  std::vector<uint8_t> content = header;
  content.insert(content.end(), 128, 0x00);

  std::ofstream out(root / "mymod" / "GearGame" / "CookedXenon" / "package.xxx", std::ios::binary);
  out.write(reinterpret_cast<const char*>(content.data()), content.size());
  out.close();

  auto mods = mods::FindMods(root);
  auto files = mods::FindFiles(root, mods);
  Expect(files.size() == 1, "one file found");
  Expect(files[0].uncompressed == 1024, "uncompressed size parsed from header (big-endian uint32 at offset 12)");

  fs::remove_all(root);
}

void TestTextureFolderOrder() {
  fs::path root = fs::temp_directory_path() / "gow2_mods_test_texorder";
  fs::remove_all(root);
  fs::create_directories(root / "alpha" / "4D53082D" / "replace");
  fs::create_directories(root / "beta" / "4D53082D" / "replace");
  fs::create_directories(root / "gamma" / "4D53082D" / "replace");

  auto mods = mods::FindMods(root);
  auto files = mods::FindFiles(root, mods);
  TocFixture fx;
  auto tocs = fx.Tocs();
  auto plan = mods::BuildPlan(mods, files, tocs);

  Expect(plan.texture_folders.size() == 3, "three texture folders");
  Expect(plan.texture_folders[0] == "gamma", "gamma first (later mod wins)");
  Expect(plan.texture_folders[1] == "beta", "beta second");
  Expect(plan.texture_folders[2] == "alpha", "alpha last (earliest mod)");

  fs::remove_all(root);
}

void TestEmptyModsFolder() {
  fs::path root = fs::temp_directory_path() / "gow2_mods_test_empty";
  fs::remove_all(root);
  fs::create_directories(root);

  auto mods = mods::FindMods(root);
  Expect(mods.empty(), "empty mods folder gives empty mod list");

  auto files = mods::FindFiles(root, mods);
  Expect(files.empty(), "no files from empty mods");

  TocFixture fx;
  auto tocs = fx.Tocs();
  auto plan = mods::BuildPlan(mods, files, tocs);
  Expect(plan.override_lines.empty(), "no override lines");
  Expect(plan.edited_tocs.empty(), "no edited TOCs");
  Expect(plan.texture_folders.empty(), "no texture folders");
  Expect(plan.warnings.empty(), "no warnings");

  fs::remove_all(root);
}

void TestModIniOptional() {
  fs::path root = fs::temp_directory_path() / "gow2_mods_test_ini";
  fs::remove_all(root);
  fs::create_directories(root / "nomini" / "GearGame");
  fs::create_directories(root / "withini" / "GearGame");
  std::ofstream(root / "withini" / "mod.ini") << "name=Custom Name\nauthor=Author\nversion=3.0\n";

  auto mods = mods::FindMods(root);
  Expect(mods.size() == 2, "both mods found");
  auto it_nomini = std::find_if(mods.begin(), mods.end(), [](const mods::Mod& m) { return m.folder == "nomini"; });
  auto it_withini = std::find_if(mods.begin(), mods.end(), [](const mods::Mod& m) { return m.folder == "withini"; });
  Expect(it_nomini != mods.end() && it_nomini->name == "nomini", "folder name used when no mod.ini name");
  Expect(it_withini != mods.end() && it_withini->name == "Custom Name", "mod.ini name used");
  Expect(it_withini->author == "Author", "author from mod.ini");
  Expect(it_withini->version == "3.0", "version from mod.ini");

  fs::remove_all(root);
}

void TestModIniIgnoresSectionsAndComments() {
  fs::path root = fs::temp_directory_path() / "gow2_mods_test_ini2";
  fs::remove_all(root);
  fs::create_directories(root / "mymod" / "GearGame");
  std::ofstream(root / "mymod" / "mod.ini") << "[section]\n; comment\n# comment\nname=Real Name\nkey=value\n";

  auto mods = mods::FindMods(root);
  Expect(mods.size() == 1, "mod found");
  Expect(mods[0].name == "Real Name", "section and comments ignored, only key=value parsed");

  fs::remove_all(root);
}

void TestOverrideLinesOrder() {
  fs::path root = fs::temp_directory_path() / "gow2_mods_test_override_order";
  fs::remove_all(root);
  fs::create_directories(root / "alpha" / "GearGame");
  fs::create_directories(root / "beta" / "GearGame");
  std::ofstream(root / "alpha" / "GearGame" / "file.txt") << "alpha";
  std::ofstream(root / "beta" / "GearGame" / "file.txt") << "beta";
  std::ofstream(root / "alpha" / "GearGame" / "other.txt") << "alpha other";

  auto mods = mods::FindMods(root);
  auto files = mods::FindFiles(root, mods);
  TocFixture fx;
  auto tocs = fx.Tocs();
  auto plan = mods::BuildPlan(mods, files, tocs);

  Expect(plan.override_lines.size() == 3, "three override lines");
  Expect(plan.override_lines[0].starts_with("GearGame\\file.txt="), "first is alpha's file.txt");
  Expect(plan.override_lines[1].starts_with("GearGame\\other.txt="), "second is alpha's other.txt");
  Expect(plan.override_lines[2].starts_with("GearGame\\file.txt="), "third is beta's file.txt (wins in VFS)");

  fs::remove_all(root);
}

int main() {
  TestModOrderAndDisable();
  TestLaterModWins();
  TestTocLineKeepsStartSector();
  TestUnknownFileAppendedToMainToc();
  TestIgnoredExtensionsAndCoalescedWarning();
  TestCompressedHeaderParse();
  TestTextureFolderOrder();
  TestEmptyModsFolder();
  TestModIniOptional();
  TestModIniIgnoresSectionsAndComments();
  TestOverrideLinesOrder();

  std::printf("mods_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}