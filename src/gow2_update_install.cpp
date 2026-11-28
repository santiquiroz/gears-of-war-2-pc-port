// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_update_install.h"

#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(gow2_update_auto_install, true, "UI",
                    "Download new PC releases in the background and install them on the next start");

#if defined(_WIN32) || (defined(__linux__) && !defined(__ANDROID__))

#include "gow2_process.h"
#include "gow2_sha256.h"
#include "gow2_update_check.h"
#include "gow2_update_install_core.h"

#include <rex/filesystem.h>
#include <rex/logging.h>

#include <chrono>
#include <exception>
#include <fstream>
#include <future>
#include <mutex>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace rex::glue::update_install {
namespace {

struct InstallService {
  std::mutex mutex;
  InstallSnapshot snapshot;
  bool ready_stage_scanned = false;
};

InstallService& Service() {
  // Detached workers can still publish their final state during process teardown.
  static InstallService* service = new InstallService;
  return *service;
}

std::string PathUtf8(const std::filesystem::path& path) {
  const std::u8string text = path.u8string();
  return {text.begin(), text.end()};
}

std::filesystem::path StageRoot() { return InstallDir() / ".gow2-update"; }

std::filesystem::path StagePath(std::string_view tag) {
  return StageRoot() / StageFolderName(tag);
}

std::filesystem::path CleanupPath() { return StageRoot() / "cleanup.txt"; }

bool IsBusy(InstallState state) {
  return state == InstallState::kDownloading || state == InstallState::kVerifying ||
      state == InstallState::kExtracting;
}

void SetSnapshot(InstallState state, const std::string& tag, int percent,
                 const std::string& error) {
  InstallService& service = Service();
  std::lock_guard lock(service.mutex);
  service.snapshot = {state, tag, percent, error};
}

void SetDownloadPercent(const std::string& tag, const std::filesystem::path& part,
                        uint64_t total) {
  std::error_code error;
  const uintmax_t size = std::filesystem::file_size(part, error);
  const int percent = DownloadPercent(error ? 0 : static_cast<uint64_t>(size), total);
  InstallService& service = Service();
  std::lock_guard lock(service.mutex);
  if (service.snapshot.state == InstallState::kDownloading && service.snapshot.tag == tag)
    service.snapshot.percent = percent;
}

void LogStep(std::string_view step, const std::string& tag) {
  REXLOG_INFO("UPDATEINSTALL {} {}", step, tag);
}

void FailInstall(const std::string& tag, const std::string& error) {
  REXLOG_WARN("UPDATEINSTALL failed {}: {}", tag, error);
  SetSnapshot(InstallState::kFailed, tag, -1, error);
}

#ifdef _WIN32
std::string SystemToolPath(const wchar_t* name) {
  std::vector<wchar_t> directory(32768);
  const UINT size = GetSystemDirectoryW(directory.data(), static_cast<UINT>(directory.size()));
  if (size == 0 || size >= directory.size()) return {};
  return PathUtf8(std::filesystem::path(std::wstring(directory.data(), size)) / name);
}
#endif

std::string CurlPath() {
#ifdef _WIN32
  return SystemToolPath(L"curl.exe");
#else
  return "curl";
#endif
}

std::string TarPath() {
#ifdef _WIN32
  return SystemToolPath(L"tar.exe");
#else
  return "tar";
#endif
}

std::filesystem::path PartPath(const std::filesystem::path& destination) {
  std::filesystem::path part = destination;
  part += ".part";
  return part;
}

process::Result RunMonitored(const std::vector<std::string>& arguments,
                             const std::filesystem::path& part, uint64_t total,
                             const std::string& tag) {
  std::future<process::Result> running = std::async(std::launch::async, [&arguments] {
    return process::Run(arguments, 0);
  });
  while (running.wait_for(std::chrono::milliseconds(250)) != std::future_status::ready)
    SetDownloadPercent(tag, part, total);
  SetDownloadPercent(tag, part, total);
  return running.get();
}

std::vector<std::string> CurlArguments(const std::string& url,
                                       const std::filesystem::path& part) {
  return {CurlPath(), "-fsSL", "--proto", "=https", "--proto-redir", "=https",
          "--max-filesize", "1073741824", "-A", "gow2-recomp/" + update::CurrentVersion(),
          "-o", PathUtf8(part), url};
}

std::string CurlError(const process::Result& result) {
  if (!result.started) return "curl not available";
  return "download failed (curl exit " + std::to_string(result.exit_code) + ")";
}

bool DownloadFile(const std::string& url, const std::filesystem::path& destination,
                  uint64_t total, const std::string& tag, std::string* error) {
  const std::filesystem::path part = PartPath(destination);
  std::error_code filesystem_error;
  std::filesystem::remove(part, filesystem_error);
  if (filesystem_error) {
    *error = "cannot prepare download file: " + filesystem_error.message();
    return false;
  }
  const process::Result result = RunMonitored(CurlArguments(url, part), part, total, tag);
  if (!result.started || result.exit_code != 0 || !result.error.empty()) {
    *error = CurlError(result);
    filesystem_error.clear();
    std::filesystem::remove(part, filesystem_error);
    return false;
  }
  std::filesystem::rename(part, destination, filesystem_error);
  if (!filesystem_error) return true;
  *error = "cannot finish download: " + filesystem_error.message();
  return false;
}

bool RecreateStage(const std::filesystem::path& stage, std::string* error) {
  std::error_code filesystem_error;
  std::filesystem::remove_all(stage, filesystem_error);
  if (filesystem_error) {
    *error = "cannot remove previous stage: " + filesystem_error.message();
    return false;
  }
  std::filesystem::create_directories(stage, filesystem_error);
  if (!filesystem_error) return true;
  *error = "cannot create stage: " + filesystem_error.message();
  return false;
}

std::optional<std::string> ReadFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return std::nullopt;
  constexpr size_t kMaxStageText = 4096;
  std::string content(kMaxStageText + 1, '\0');
  file.read(content.data(), static_cast<std::streamsize>(content.size()));
  const std::streamsize count = file.gcount();
  if (file.bad() || count < 0 || static_cast<size_t>(count) > kMaxStageText) return std::nullopt;
  content.resize(static_cast<size_t>(count));
  return content;
}

bool VerifyArchive(const std::filesystem::path& archive, const std::filesystem::path& sidecar,
                   const std::string& archive_name) {
  const auto content = ReadFile(sidecar);
  if (!content) return false;
  const auto expected = ParseChecksum(*content, archive_name);
  if (!expected) return false;
  const auto actual = sha256::HashFileHex(archive);
  return actual && *actual == *expected;
}

void RemoveFile(const std::filesystem::path& path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

std::vector<std::string> TarArguments(const std::filesystem::path& archive,
                                      const std::filesystem::path& extract) {
#ifdef _WIN32
  return {TarPath(), "-xf", PathUtf8(archive), "-C", PathUtf8(extract)};
#else
  return {TarPath(), "-xzf", PathUtf8(archive), "-C", PathUtf8(extract)};
#endif
}

bool ExtractArchive(const std::filesystem::path& archive, const std::filesystem::path& extract,
                    std::string* error) {
  std::error_code filesystem_error;
  std::filesystem::create_directories(extract, filesystem_error);
  if (filesystem_error) {
    *error = "cannot create extraction directory: " + filesystem_error.message();
    return false;
  }
  const process::Result result = process::Run(TarArguments(archive, extract), 0);
  if (!result.started) {
    *error = "tar not available";
    return false;
  }
  if (result.exit_code == 0 && result.error.empty()) return true;
  *error = "tar failed (exit " + std::to_string(result.exit_code) + ")";
  return false;
}

bool HasGameExecutable(const std::filesystem::path& payload) {
#ifdef _WIN32
  const std::filesystem::path executable = payload / L"gow2.exe";
#else
  const std::filesystem::path executable = payload / "gow2";
#endif
  std::error_code error;
  return std::filesystem::is_regular_file(executable, error) && !error;
}

bool WriteReady(const std::filesystem::path& stage, const std::string& tag) {
  std::ofstream file(stage / "READY", std::ios::binary | std::ios::trunc);
  if (!file) return false;
  file.write(tag.data(), static_cast<std::streamsize>(tag.size()));
  file.flush();
  if (!file) return false;
  file.close();
  return !file.fail();
}

void InstallRelease(const update::Release& release) {
  const std::string& tag = release.tag;
  LogStep("start", tag);
  if (tag.empty() || !IsHttpsUrl(release.download_url) || !IsHttpsUrl(release.checksum_url)) {
    FailInstall(tag, "release has no download for this platform");
    return;
  }
  const auto archive_name = ArchiveNameFromUrl(release.download_url);
  if (!archive_name) {
    FailInstall(tag, "release has no download for this platform");
    return;
  }
  const std::filesystem::path stage = StagePath(tag);
  std::string error;
  if (!RecreateStage(stage, &error)) {
    FailInstall(tag, error);
    return;
  }

  const std::filesystem::path archive = stage / *archive_name;
  const std::filesystem::path sidecar = stage / (*archive_name + ".sha256");
  LogStep("downloading", tag);
  if (!DownloadFile(release.download_url, archive, release.download_size, tag, &error) ||
      !DownloadFile(release.checksum_url, sidecar, 0, tag, &error)) {
    FailInstall(tag, error);
    return;
  }

  LogStep("verifying", tag);
  SetSnapshot(InstallState::kVerifying, tag, -1, {});
  if (!VerifyArchive(archive, sidecar, *archive_name)) {
    RemoveFile(archive);
    FailInstall(tag, "checksum mismatch");
    return;
  }

  LogStep("extracting", tag);
  SetSnapshot(InstallState::kExtracting, tag, -1, {});
  const std::filesystem::path extract = stage / "extract";
  if (!ExtractArchive(archive, extract, &error)) {
    RemoveFile(archive);
    FailInstall(tag, error);
    return;
  }
  const std::filesystem::path payload = FindPayloadRoot(extract);
  if (!HasGameExecutable(payload)) {
    RemoveFile(archive);
    FailInstall(tag, "unexpected archive layout");
    return;
  }
  if (!WriteReady(stage, tag)) {
    FailInstall(tag, "cannot write READY marker");
    return;
  }
  LogStep("ready", tag);
  SetSnapshot(InstallState::kReady, tag, 100, {});
}

void RunInstall(const update::Release& release) {
  try {
    InstallRelease(release);
  } catch (const std::exception& error) {
    FailInstall(release.tag, error.what());
  } catch (...) {
    FailInstall(release.tag, "update installation failed");
  }
}

std::optional<std::string> ReadReadyTag(const std::filesystem::path& stage) {
  const auto tag = ReadFile(stage / "READY");
  if (!tag || tag->empty() || StageFolderName(*tag) != stage.filename().string())
    return std::nullopt;
  return tag;
}

bool IsReadyStage(const std::filesystem::path& stage, std::string_view tag) {
  const auto ready_tag = ReadReadyTag(stage);
  return ready_tag && *ready_tag == tag;
}

bool IsNewerThanRunning(std::string_view tag) {
  return update::IsNewer(tag, update::CurrentVersion());
}

void RemoveStage(const std::filesystem::path& stage) {
  std::error_code error;
  std::filesystem::remove_all(stage, error);
}

bool FailApply(const std::filesystem::path& stage, const std::string& tag,
               const std::string& message, std::string* error) {
  if (!stage.empty()) RemoveStage(stage);
  if (error) *error = message;
  REXLOG_WARN("UPDATEINSTALL apply failed {}: {}", tag, message);
  SetSnapshot(InstallState::kFailed, tag, -1, message);
  return false;
}

bool IsRealDirectory(const std::filesystem::path& path) {
  std::error_code error;
  return std::filesystem::symlink_status(path, error).type() == std::filesystem::file_type::directory &&
      !error;
}

}  // namespace

std::filesystem::path InstallDir() {
  return rex::filesystem::GetExecutablePath().parent_path();
}

void StartInstall(const update::Release& release) {
  InstallService& service = Service();
  std::string launch_error;
  {
    std::lock_guard lock(service.mutex);
    if (IsBusy(service.snapshot.state) ||
        (service.snapshot.state == InstallState::kReady && service.snapshot.tag == release.tag)) {
      return;
    }
    service.snapshot = {InstallState::kDownloading, release.tag, -1, {}};
    try {
      std::thread([release] { RunInstall(release); }).detach();
    } catch (const std::exception& error) {
      launch_error = error.what();
      service.snapshot = {InstallState::kFailed, release.tag, -1, launch_error};
    }
  }
  if (!launch_error.empty()) REXLOG_WARN("UPDATEINSTALL failed {}: {}", release.tag, launch_error);
}

InstallSnapshot GetInstallSnapshot() {
  InstallService& service = Service();
  {
    std::lock_guard lock(service.mutex);
    if (service.snapshot.state == InstallState::kReady &&
        !IsNewerThanRunning(service.snapshot.tag)) {
      service.snapshot = {};
    }
    if (service.snapshot.state != InstallState::kIdle || service.ready_stage_scanned)
      return service.snapshot;
    service.ready_stage_scanned = true;
  }
  if (const auto tag = ReadyStagedTag(); tag && IsNewerThanRunning(*tag)) {
    std::lock_guard lock(service.mutex);
    if (service.snapshot.state == InstallState::kIdle)
      service.snapshot = {InstallState::kReady, *tag, 100, {}};
    return service.snapshot;
  }
  std::lock_guard lock(service.mutex);
  return service.snapshot;
}

std::optional<std::string> ReadyStagedTag() {
  const std::filesystem::path root = StageRoot();
  std::error_code error;
  std::filesystem::directory_iterator it(root, error);
  if (error) return std::nullopt;
  std::optional<std::string> newest;
  const std::filesystem::directory_iterator end;
  while (it != end) {
    if (IsRealDirectory(it->path())) {
      const auto tag = ReadReadyTag(it->path());
      if (tag && (!newest || update::IsNewer(*tag, *newest))) newest = *tag;
    }
    it.increment(error);
    if (error) return newest;
  }
  return newest;
}

bool ApplyStagedUpdate(std::string* error) {
  if (error) error->clear();
  const InstallSnapshot snapshot = GetInstallSnapshot();
  std::string tag;
  if (snapshot.state == InstallState::kReady && IsReadyStage(StagePath(snapshot.tag), snapshot.tag)) {
    tag = snapshot.tag;
  } else if (const auto ready = ReadyStagedTag()) {
    tag = *ready;
  }
  if (tag.empty()) return FailApply({}, snapshot.tag, "no ready update stage", error);

  LogStep("applying", tag);
  const std::filesystem::path stage = StagePath(tag);
  if (!IsNewerThanRunning(tag))
    return FailApply(stage, tag, "staged update is not newer than the running version", error);
  const std::filesystem::path payload = FindPayloadRoot(stage / "extract");
  const Plan plan = PlanApply(payload, InstallDir());
  if (!plan.error.empty()) return FailApply(stage, tag, plan.error, error);
  const ApplyResult result = ApplyPlan(plan);
  if (!result.ok) return FailApply(stage, tag, result.error, error);
  if (!WriteCleanupList(CleanupPath(), result.backups))
    return FailApply(stage, tag, "cannot write cleanup list", error);

  RemoveStage(stage);
  SetSnapshot(InstallState::kIdle, {}, -1, {});
  LogStep("applied", tag);
  return true;
}

void CleanupAfterUpdate() {
  LogStep("cleanup", "startup");
  const std::filesystem::path root = StageRoot();
  RunCleanup(CleanupPath());
  std::error_code error;
  std::filesystem::directory_iterator it(root, error);
  if (error) return;
  std::vector<std::filesystem::path> stages;
  const std::filesystem::directory_iterator end;
  while (it != end) {
    if (IsRealDirectory(it->path())) stages.push_back(it->path());
    it.increment(error);
    if (error) break;
  }
  for (const auto& stage : stages) {
    if (ReadReadyTag(stage)) continue;
    RemoveStage(stage);
    LogStep("removed incomplete stage", stage.filename().string());
  }
}

}  // namespace rex::glue::update_install

#else

namespace rex::glue::update_install {

std::filesystem::path InstallDir() { return {}; }

void StartInstall(const update::Release&) {}

InstallSnapshot GetInstallSnapshot() { return {}; }

bool ApplyStagedUpdate(std::string* error) {
  if (error) *error = "updates are not supported on Android";
  return false;
}

std::optional<std::string> ReadyStagedTag() { return std::nullopt; }

void CleanupAfterUpdate() {}

}  // namespace rex::glue::update_install

#endif