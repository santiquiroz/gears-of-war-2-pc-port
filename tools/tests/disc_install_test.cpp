// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_disc_install.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

int g_checks = 0;
int g_failures = 0;
void Expect(bool ok, std::string_view what) {
  ++g_checks;
  if (ok) return;
  ++g_failures;
  std::printf("FAIL: %.*s\n", int(what.size()), what.data());
}

using namespace rex::glue::disc_install;

void CheckIsDiscFolder() {
  std::error_code ec;
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "gow2_disc_install_test";
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  std::ofstream(dir / "default.xex", std::ios::binary) << 'x';
  Expect(!IsDiscFolder(dir), "a folder with only default.xex is not a disc folder");
  std::filesystem::create_directories(dir / "GearGame", ec);
  Expect(IsDiscFolder(dir), "default.xex and a GearGame folder make a disc folder");
  std::filesystem::remove(dir / "default.xex", ec);
  Expect(!IsDiscFolder(dir), "a folder with only GearGame is not a disc folder");
  std::filesystem::remove_all(dir, ec);
}

int main() {
  Expect(ClassifyPick("C:/x/Gears.iso") == PickKind::kIso, "an .iso is a disc image");
  Expect(ClassifyPick("GEARS.ISO") == PickKind::kIso, "an upper-case .ISO is a disc image");
  Expect(ClassifyPick("C:/g/default.xex.iso") == PickKind::kIso, "default.xex.iso is a disc image");
  Expect(ClassifyPick("C:/g/default.xex") == PickKind::kGameExecutable, "default.xex is the game executable");
  Expect(ClassifyPick("DEFAULT.XEX") == PickKind::kGameExecutable, "DEFAULT.XEX is the game executable");
  Expect(ClassifyPick("C:/g/gow2.exe") == PickKind::kOther, "gow2.exe is neither");
  Expect(ClassifyPick("C:/g/iso") == PickKind::kOther, "a file named iso is neither");
  Expect(ClassifyPick("") == PickKind::kOther, "an empty path is neither");

  Expect(IsSafeEntryName("default.xex"), "default.xex is a safe name");
  Expect(IsSafeEntryName("$SystemUpdate"), "$SystemUpdate is a safe name");
  Expect(IsSafeEntryName("GearGame"), "GearGame is a safe name");
  Expect(IsSafeEntryName("Coalesced_int.bin"), "Coalesced_int.bin is a safe name");
  Expect(!IsSafeEntryName(""), "an empty name is unsafe");
  Expect(!IsSafeEntryName("."), ". is unsafe");
  Expect(!IsSafeEntryName(".."), ".. is unsafe");
  Expect(!IsSafeEntryName("a/b"), "a slash is unsafe");
  Expect(!IsSafeEntryName("a\\b"), "a backslash is unsafe");
  Expect(!IsSafeEntryName("C:"), "a drive is unsafe");
  Expect(!IsSafeEntryName("a?b"), "a question mark is unsafe");
  Expect(!IsSafeEntryName(std::string("x\x01", 2)), "a control byte is unsafe");
  Expect(!IsSafeEntryName("a."), "a trailing dot is unsafe");
  Expect(!IsSafeEntryName("a "), "a trailing space is unsafe");
  Expect(!IsSafeEntryName("CON"), "CON is unsafe");
  Expect(!IsSafeEntryName("con.txt"), "con.txt is unsafe");
  Expect(!IsSafeEntryName("Nul.bin"), "Nul.bin is unsafe");
  Expect(!IsSafeEntryName("prn"), "prn is unsafe");
  Expect(!IsSafeEntryName("AUX.dat"), "AUX.dat is unsafe");
  Expect(!IsSafeEntryName("COM1"), "COM1 is unsafe");
  Expect(!IsSafeEntryName("lpt9.x"), "lpt9.x is unsafe");
  Expect(IsSafeEntryName("CONSOLE"), "CONSOLE is a safe name");
  Expect(IsSafeEntryName("COM10"), "COM10 is a safe name");
  Expect(IsSafeEntryName("COM0"), "COM0 is a safe name");
  Expect(IsSafeEntryName("AUXILIARY.txt"), "AUXILIARY.txt is a safe name");

  Expect(RequiredFreeBytes(0) == (256ull << 20), "an empty disc still needs the 256 MiB margin");
  Expect(RequiredFreeBytes(7ull << 30) == (7ull << 30) + (256ull << 20), "7 GiB of files need 7 GiB plus 256 MiB");

  CheckIsDiscFolder();

  std::printf("disc_install_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
