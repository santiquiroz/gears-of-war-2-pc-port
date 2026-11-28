// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_update_install_core.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;
namespace update = rex::glue::update_install;

namespace {

int g_failures = 0;

void Expect(bool ok, const char* what) {
  if (ok) return;
  ++g_failures;
  std::fprintf(stderr, "FAIL: %s\n", what);
}

void WriteFile(const fs::path& path, const std::string& content) {
  std::error_code error;
  fs::create_directories(path.parent_path(), error);
  std::ofstream file(path, std::ios::binary);
  file << content;
}

std::string ReadFile(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void TestParseChecksum() {
  const std::string uppercase(64, 'A');
  const std::string lowercase(64, 'a');
  Expect(update::ParseChecksum(uppercase + "  archive.zip", "archive.zip") == lowercase,
         "two-space filename is parsed and lowercased");
  Expect(update::ParseChecksum(uppercase + " *archive.zip\r\n", "archive.zip") == lowercase,
         "star filename and CRLF are parsed");
  Expect(update::ParseChecksum(" \t" + uppercase + " \r\n", "archive.zip") == lowercase,
         "bare checksum with surrounding blanks is parsed");
  Expect(!update::ParseChecksum(uppercase + "  other.zip", "archive.zip"), "wrong filename rejected");
  Expect(!update::ParseChecksum(std::string(63, 'a'), "archive.zip"), "short checksum rejected");
  Expect(!update::ParseChecksum(std::string(63, 'a') + "g", "archive.zip"), "non-hex checksum rejected");
  Expect(!update::ParseChecksum(uppercase + "  archive.zip\nextra", "archive.zip"), "multiple lines rejected");
  Expect(!update::ParseChecksum(uppercase + "*archive.zip", "archive.zip"), "missing separator rejected");
}

void TestSafePathsAndStageNames() {
  Expect(update::SafeRelativePath("a/b.dll"), "forward-slash relative path accepted");
  Expect(!update::SafeRelativePath("../x"), "leading parent component rejected");
  Expect(!update::SafeRelativePath("a/../b"), "interior parent component rejected");
  Expect(!update::SafeRelativePath("/etc/x"), "leading slash rejected");
  Expect(!update::SafeRelativePath("\\x"), "leading backslash rejected");
  Expect(!update::SafeRelativePath("C:x"), "drive prefix rejected");
  Expect(!update::SafeRelativePath("a\\..\\b"), "backslash parent component rejected");
  Expect(!update::SafeRelativePath("a:b"), "colon rejected");
  Expect(!update::SafeRelativePath(""), "empty path rejected");
  Expect(update::StageFolderName("v0.4.11") == "v0.4.11", "safe stage tag preserved");
  Expect(update::StageFolderName("v1/../x") == "v1_.._x", "unsafe stage tag sanitized");
  Expect(update::StageFolderName("") == "unknown", "empty stage tag uses fallback");
  Expect(update::StageFolderName("..") == "unknown", "dot-dot stage name uses fallback");
  Expect(update::StageFolderName(".") == "unknown", "dot stage name uses fallback");
}

void TestFindPayloadRoot(const fs::path& root) {
  const fs::path extracted = root / "extracted";
  std::error_code error;
  fs::create_directories(extracted / "gow2-recomp-v1-win", error);
  Expect(update::FindPayloadRoot(extracted) == extracted / "gow2-recomp-v1-win",
         "single directory becomes payload root");
  WriteFile(extracted / "extra.txt", "x");
  Expect(update::FindPayloadRoot(extracted) == extracted, "multiple entries retain extraction root");
}

void TestPlanApply(const fs::path& root) {
  const fs::path payload = root / "plan-payload";
  const fs::path install = root / "plan-install";
  WriteFile(payload / "z" / "second.dll", "second");
  WriteFile(payload / "a.dll", "first");
  WriteFile(payload / "nested" / "gow2.toml", "player config");
  WriteFile(payload / "gow2_game_path.txt", "player path");
  const auto plan = update::PlanApply(payload, install);
  Expect(plan.size() == 2, "plan skips player files");
  if (plan.size() == 2) {
    Expect(plan[0].staged.generic_string() == (payload / "a.dll").generic_string(), "plan sorted first file");
    Expect(plan[1].staged.generic_string() == (payload / "z" / "second.dll").generic_string(),
           "plan sorted nested file");
    Expect(plan[0].target == install / "a.dll", "plan target uses relative path");
    Expect(plan[0].backup == fs::path((install / "a.dll").string() + ".gow2old"),
           "backup suffix appended to filename");
  }

  const auto failed_plan = update::PlanApply(payload / "missing", install);
  const auto failed_result = update::ApplyPlan(failed_plan);
  Expect(!failed_result.ok && !failed_result.error.empty(),
         "payload scan failure is not reported as an empty successful apply");

  const fs::path empty_payload = root / "empty-payload";
  std::error_code error;
  fs::create_directories(empty_payload, error);
  Expect(update::ApplyPlan(update::PlanApply(empty_payload, install)).ok,
         "an empty payload remains a successful no-op");
}

void TestApplySuccess(const fs::path& root) {
  const fs::path payload = root / "apply-payload";
  const fs::path install = root / "apply-install";
  WriteFile(payload / "existing.dll", "new bytes");
  WriteFile(payload / "new.dll", "brand new");
  WriteFile(install / "existing.dll", "old bytes");
  WriteFile(install / "existing.dll.gow2old", "stale backup");
  const auto result = update::ApplyPlan(update::PlanApply(payload, install));
  Expect(result.ok, "apply succeeds");
  Expect(result.error.empty(), "successful apply has no error");
  Expect(ReadFile(install / "existing.dll") == "new bytes", "existing target replaced");
  Expect(ReadFile(install / "existing.dll.gow2old") == "old bytes", "old bytes moved to backup");
  Expect(ReadFile(install / "new.dll") == "brand new", "new target installed");
  Expect(!fs::exists(install / "new.dll.gow2old"), "new target has no backup");
  Expect(result.backups.size() == 1 && result.backups[0] == install / "existing.dll.gow2old",
         "result lists only created backup");
}

void TestApplyRollback(const fs::path& root) {
  const fs::path payload = root / "rollback-payload";
  const fs::path install = root / "rollback-install";
  WriteFile(payload / "a.dll", "new a");
  WriteFile(payload / "zdir" / "later.dll", "new later");
  WriteFile(install / "a.dll", "old a");
  WriteFile(install / "zdir", "blocks directory creation");
  const auto result = update::ApplyPlan(update::PlanApply(payload, install));
  Expect(!result.ok, "later parent-file conflict fails apply");
  Expect(!result.error.empty() && result.error.find("zdir") != std::string::npos,
         "apply failure reports failing path and error");
  Expect(result.backups.empty(), "failed apply reports no backups");
  Expect(ReadFile(install / "a.dll") == "old a", "rollback restores earlier target bytes");
  Expect(!fs::exists(install / "a.dll.gow2old"), "rollback removes backup from earlier target");
  Expect(ReadFile(payload / "a.dll") == "new a", "rollback moves new target back to staging");
  Expect(ReadFile(payload / "zdir" / "later.dll") == "new later", "unapplied staged file remains");
}

void TestCleanupList(const fs::path& root) {
  const fs::path first = root / "cleanup-one.gow2old";
  const fs::path second = root / "cleanup-two.gow2old";
  const fs::path list = root / "cleanup-list.txt";
  WriteFile(first, "one");
  WriteFile(second, "two");
  const std::vector<fs::path> backups = {first, second};
  Expect(update::WriteCleanupList(list, backups), "cleanup list written");
  Expect(update::ReadCleanupList(list) == backups, "cleanup list round trips paths");
  update::RunCleanup(list);
  Expect(!fs::exists(first) && !fs::exists(second), "cleanup removes listed backups");
  Expect(!fs::exists(list), "cleanup deletes empty cleanup list");
  update::RunCleanup(root / "missing-cleanup-list.txt");
}

void TestCleanupSafety(const fs::path& root) {
  const fs::path backup = root / "cleanup-safe.gow2old";
  const fs::path unrelated = root / "unrelated.txt";
  const fs::path directory = root / "directory.gow2old";
  const fs::path list = root / "cleanup-safety-list.txt";
  const fs::path invalid_list = root / "cleanup-list-directory";
  const fs::path marker = invalid_list / "marker.txt";
  WriteFile(backup, "backup");
  WriteFile(unrelated, "keep");
  std::error_code error;
  fs::create_directories(directory, error);
  WriteFile(marker, "preserve list destination");
  Expect(!update::WriteCleanupList(invalid_list, {}), "cleanup writer rejects a directory destination");
  Expect(ReadFile(marker) == "preserve list destination", "failed cleanup rewrite preserves destination");
  const std::vector<fs::path> paths = {backup, unrelated, directory};
  Expect(update::WriteCleanupList(list, paths), "cleanup safety list written");

  update::RunCleanup(list);

  Expect(!fs::exists(backup), "cleanup removes valid backup files");
  Expect(fs::exists(unrelated), "cleanup retains paths without the backup suffix");
  Expect(fs::is_directory(directory), "cleanup retains directories even with the backup suffix");
  Expect(update::ReadCleanupList(list) == std::vector<fs::path>({unrelated, directory}),
         "cleanup list retains paths that were not eligible for removal");
}

}  // namespace

int main() {
  const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto root = fs::temp_directory_path() / ("gow2-update-install-core-test-" + std::to_string(nonce));
  std::error_code error;
  fs::create_directories(root, error);
  if (error) {
    std::fprintf(stderr, "FAIL: create temporary directory: %s\n", error.message().c_str());
    return 1;
  }
  TestParseChecksum();
  TestSafePathsAndStageNames();
  TestFindPayloadRoot(root);
  TestPlanApply(root);
  TestApplySuccess(root);
  TestApplyRollback(root);
  TestCleanupList(root);
  TestCleanupSafety(root);
  fs::remove_all(root, error);
  if (error) {
    std::fprintf(stderr, "FAIL: remove temporary directory: %s\n", error.message().c_str());
    return 1;
  }
  std::printf("update_install_core_test: %d failures\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}