// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_update_check.h"

#ifdef _WIN32

#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>

#include <array>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(gow2_update_check, true, "UI", "Check GitHub for a new PC release at most twice a day");

namespace rex::glue::update {
namespace {

constexpr size_t kMaxResponse = 2 * 1024 * 1024;

bool Enabled() { return rex::cvar::Query<bool>("gow2_update_check"); }

int64_t NowSeconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
}

struct CloseInternet {
  void operator()(void* handle) const { WinHttpCloseHandle(handle); }
};
using Internet = std::unique_ptr<void, CloseInternet>;

void Require(bool success) {
  if (!success) throw std::runtime_error("WinHTTP request failed");
}

std::wstring ToWide(const std::string& utf8) {
  const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                                      static_cast<int>(utf8.size()), nullptr, 0);
  if (size <= 0) return {};
  std::wstring result(size, L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                      static_cast<int>(utf8.size()), result.data(), size);
  return result;
}

std::string ReadResponse(HINTERNET request) {
  std::string result;
  std::array<char, 8192> buffer;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
  for (;;) {
    if (std::chrono::steady_clock::now() >= deadline)
      throw std::runtime_error("response timed out");
    DWORD read = 0;
    Require(WinHttpReadData(request, buffer.data(), static_cast<DWORD>(buffer.size()), &read));
    if (!read) return result;
    if (result.size() + read > kMaxResponse) throw std::runtime_error("response too large");
    result.append(buffer.data(), read);
  }
}

Release FetchLatest() {
  const auto agent = ToWide("gow2-recomp/" + CurrentVersion());
  Internet session(WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
  Require(session != nullptr);
  Require(WinHttpSetTimeouts(session.get(), 8000, 8000, 8000, 8000));
  Internet connection(WinHttpConnect(session.get(), L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0));
  Require(connection != nullptr);
  Internet request(WinHttpOpenRequest(connection.get(), L"GET",
      L"/repos/santiquiroz/gears-of-war-2-pc-port/releases/latest", nullptr,
      WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
  Require(request != nullptr);
  DWORD disabled = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
  Require(WinHttpSetOption(request.get(), WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)));
  Require(WinHttpSendRequest(request.get(), L"Accept: application/vnd.github+json\r\n", -1L,
      WINHTTP_NO_REQUEST_DATA, 0, 0, 0));
  Require(WinHttpReceiveResponse(request.get(), nullptr));
  DWORD status = 0;
  DWORD size = sizeof(status);
  Require(WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
      WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX));
  if (status != 200) throw std::runtime_error("GitHub returned HTTP " + std::to_string(status));
  const auto release = ParseRelease(ReadResponse(request.get()));
  if (!release) throw std::runtime_error("invalid release JSON");
  return *release;
}

Cache LoadCache(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return {};
  std::string content(kMaxResponse + 1, '\0');
  file.read(content.data(), static_cast<std::streamsize>(content.size()));
  content.resize(static_cast<size_t>(file.gcount()));
  if (content.size() > kMaxResponse) throw std::runtime_error("cache too large");
  const auto cache = ParseCache(content);
  if (!cache) throw std::runtime_error("invalid cache JSON");
  return *cache;
}

void SaveCache(const std::filesystem::path& path, const Cache& cache) {
  std::filesystem::create_directories(path.parent_path());
  auto temporary = path;
  temporary += L".tmp";
  {
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    file << SerializeCache(cache);
    file.close();
    if (!file) throw std::runtime_error("cannot write update cache");
  }
  if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    throw std::runtime_error("cannot replace update cache");
}

class Checker {
 public:
  ~Checker() {
    {
      std::lock_guard lock(mutex_);
      worker_.request_stop();
    }
    wake_.notify_all();
  }

  void StartAt(const std::filesystem::path& root) {
    if (root.empty()) return;
    std::lock_guard lock(mutex_);
    if (worker_.joinable()) return;
    path_ = root / L"update_check.json";
    worker_ = std::jthread([this](std::stop_token stop) { Run(stop); });
  }

  Snapshot SnapshotNow() {
    std::lock_guard lock(mutex_);
    return {cache_.release, cache_.dismissed, !cache_.release.tag.empty(), cache_.failed, busy_};
  }

  void QueueCheck() {
    if (!Enabled()) return;
    std::lock_guard lock(mutex_);
    if (busy_ || requested_) return;
    requested_ = true;
    wake_.notify_one();
  }

  void SkipVersion() {
    std::lock_guard lock(mutex_);
    if (cache_.release.tag.empty()) return;
    cache_.dismissed = cache_.release.tag;
    save_requested_ = true;
    wake_.notify_one();
  }

  bool Notice() {
    if (!Enabled()) return false;
    std::lock_guard lock(mutex_);
    if (notice_shown_ || !ShouldNotify(cache_.release, CurrentVersion(), cache_.dismissed)) return false;
    notice_shown_ = true;
    return true;
  }

 private:
  void ReadCache() {
    try {
      auto cache = LoadCache(path_);
      std::lock_guard lock(mutex_);
      cache_ = std::move(cache);
    } catch (const std::exception& error) {
      REXLOG_INFO("UPDATECHECK cache unavailable: {}", error.what());
    }
  }

  void Persist() {
    Cache copy;
    {
      std::lock_guard lock(mutex_);
      copy = cache_;
      save_requested_ = false;
    }
    try {
      SaveCache(path_, copy);
    } catch (const std::exception& error) {
      REXLOG_INFO("UPDATECHECK cache not saved: {}", error.what());
    }
  }

  bool BeginCheck(bool force) {
    if (!Enabled()) return false;
    std::lock_guard lock(mutex_);
    if (!ShouldCheck(NowSeconds(), cache_.checked_at, force)) return false;
    cache_.checked_at = NowSeconds();
    busy_ = true;
    return true;
  }

  void PerformCheck(bool force) {
    if (!BeginCheck(force)) return;
    Persist();
    try {
      auto release = FetchLatest();
      std::lock_guard lock(mutex_);
      if (!release.draft && !release.prerelease) cache_.release = std::move(release);
      cache_.failed = false;
    } catch (const std::exception& error) {
      { std::lock_guard lock(mutex_); cache_.failed = true; }
      REXLOG_INFO("UPDATECHECK failed; keeping last result: {}", error.what());
    }
    Persist();
    std::lock_guard lock(mutex_);
    busy_ = false;
  }

  void Run(std::stop_token stop) {
    ReadCache();
    PerformCheck(false);
    while (!stop.stop_requested()) {
      std::unique_lock lock(mutex_);
      wake_.wait(lock, [this, stop] { return stop.stop_requested() || requested_ || save_requested_; });
      if (stop.stop_requested()) return;
      const bool check = std::exchange(requested_, false);
      lock.unlock();
      if (check) PerformCheck(true);
      Persist();
    }
  }

  std::mutex mutex_;
  std::condition_variable wake_;
  Cache cache_;
  std::filesystem::path path_;
  bool busy_ = false;
  bool requested_ = false;
  bool save_requested_ = false;
  bool notice_shown_ = false;
  std::jthread worker_;
};

Checker& Service() {
  static Checker checker;
  return checker;
}

bool SafeReleaseUrl(const std::string& url) {
  constexpr std::string_view prefix = "https://github.com/santiquiroz/gears-of-war-2-pc-port/releases/";
  if (!url.starts_with(prefix)) return false;
  return url.find_first_of("\r\n\t\\\"") == std::string::npos && url.find('\0') == std::string::npos;
}

}

void Start(const std::filesystem::path& root) { Service().StartAt(root); }
void CheckNow() { Service().QueueCheck(); }
void Dismiss() { Service().SkipVersion(); }
Snapshot GetSnapshot() { return Service().SnapshotNow(); }
std::string CurrentVersion() { return GOW2_VERSION; }
bool TakeNotice() { return Service().Notice(); }

bool OpenDownload() {
  const auto snapshot = GetSnapshot();
  if (!SafeReleaseUrl(snapshot.release.html_url)) return false;
  const auto url = ToWide(snapshot.release.html_url);
  if (url.empty()) return false;
  return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
}

}

#else
// Android checks for updates in the Java launcher (update/UpdateChecker.java).
namespace rex::glue::update {
void Start(const std::filesystem::path&) {}
void CheckNow() {}
void Dismiss() {}
bool OpenDownload() { return false; }
Snapshot GetSnapshot() { return {}; }
std::string CurrentVersion() { return GOW2_VERSION; }
bool TakeNotice() { return false; }
}  // namespace rex::glue::update
#endif
