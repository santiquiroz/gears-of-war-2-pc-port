// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_disc_install.h"

#ifdef _WIN32

#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <algorithm>
#include <chrono>
#include <cwchar>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

#include <rex/filesystem/devices/disc_image_device.h>
#include <rex/filesystem/entry.h>
#include <rex/logging.h>

namespace rex::glue::disc_install {
namespace {

using rex::filesystem::Entry;

constexpr size_t kChunkBytes = size_t(4) << 20;

std::wstring Widen(std::string_view text, UINT code_page) {
  if (text.empty()) return {};
  const int size = MultiByteToWideChar(code_page, 0, text.data(), int(text.size()), nullptr, 0);
  if (size <= 0) return {};
  std::wstring out(size_t(size), L'\0');
  MultiByteToWideChar(code_page, 0, text.data(), int(text.size()), out.data(), size);
  return out;
}

std::string Narrow(const std::filesystem::path& path) {
  const std::wstring wide = path.wstring();
  if (wide.empty()) return {};
  const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), nullptr, 0,
                                       nullptr, nullptr);
  if (size <= 0) return {};
  std::string out(size_t(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), out.data(), size, nullptr,
                      nullptr);
  return out;
}

// std::error_code messages come from FormatMessageA, so they are in the ANSI code page.
std::wstring Reason(const std::error_code& ec) { return Widen(ec.message(), CP_ACP); }

std::wstring Gigabytes(uint64_t bytes) {
  wchar_t buf[32];
  std::swprintf(buf, 32, L"%.1f", double(bytes) / double(1ull << 30));
  return buf;
}

bool IsDirectory(const Entry& entry) {
  return (entry.attributes() & rex::filesystem::kFileAttributeDirectory) != 0;
}

InstallResult Failed(std::wstring error) {
  InstallResult result;
  result.error = std::move(error);
  return result;
}

bool HasChild(const Entry& dir, std::string_view name, bool directory) {
  for (const auto& child : dir.children()) {
    if (EqualsIgnoreCase(child->name(), name) && IsDirectory(*child) == directory) return true;
  }
  return false;
}

bool HasGameAtRoot(const Entry* root) {
  return root && HasChild(*root, "default.xex", false) && HasChild(*root, "GearGame", true);
}

std::wstring CheckFreeSpace(const std::filesystem::path& app_home, uint64_t total_file_bytes) {
  std::error_code ec;
  const auto space = std::filesystem::space(app_home, ec);
  const uint64_t needed = RequiredFreeBytes(total_file_bytes);
  if (ec || space.available >= needed) return {};
  return L"Extracting the disc image needs " + Gigabytes(needed) + L" GB free in " +
         app_home.wstring() + L", but only " + Gigabytes(space.available) +
         L" GB is available. Free some space there, or put the game's folder on another drive.";
}

std::wstring PrepareStaging(const std::filesystem::path& staging) {
  std::error_code ec;
  std::filesystem::remove_all(staging, ec);
  if (ec) {
    return L"Could not clear " + staging.wstring() + L" from an earlier extraction: " +
           Reason(ec) + L". Delete that folder, then choose the .iso again.";
  }
  // Only a folder created right now is known to be empty; never extract over leftovers.
  if (std::filesystem::create_directories(staging, ec)) return {};
  if (!ec) ec = std::make_error_code(std::errc::file_exists);
  return L"Could not create " + staging.wstring() + L": " + Reason(ec) +
         L". Put the game's folder somewhere you can write (for example Documents), or extract "
         L"the .iso yourself.";
}

class Progress {
 public:
  explicit Progress(uint64_t total) : total_(total) {
    if (FAILED(CoCreateInstance(CLSID_ProgressDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog_)))) {
      dialog_ = nullptr;
      return;
    }
    dialog_->SetTitle(L"Gears of War 2");
    dialog_->SetLine(1, L"Extracting your disc image (only the first time)...", FALSE, nullptr);
    dialog_->StartProgressDialog(nullptr, nullptr,
                                 PROGDLG_NORMAL | PROGDLG_AUTOTIME | PROGDLG_NOMINIMIZE, nullptr);
  }
  ~Progress() {
    if (!dialog_) return;
    dialog_->StopProgressDialog();
    dialog_->Release();
  }
  Progress(const Progress&) = delete;
  Progress& operator=(const Progress&) = delete;

  void ShowFile(const std::filesystem::path& relative) {
    if (dialog_) dialog_->SetLine(2, relative.wstring().c_str(), TRUE, nullptr);
  }
  void Advance(uint64_t bytes) {
    done_ += bytes;
    if (dialog_) dialog_->SetProgress64(done_, total_);
  }
  bool Cancelled() const { return dialog_ && dialog_->HasUserCancelled(); }
  uint64_t done() const { return done_; }

 private:
  IProgressDialog* dialog_ = nullptr;
  uint64_t total_ = 0;
  uint64_t done_ = 0;
};

struct Extraction {
  explicit Extraction(Progress& p) : progress(p) {}
  Progress& progress;
  uint64_t files = 0;
  std::wstring error;
  bool cancelled = false;
};

bool WriteFailed(Extraction& x, const std::filesystem::path& relative,
                 const std::filesystem::path& out) {
  x.error = L"Could not extract " + relative.wstring() + L" to " + out.wstring() +
            L". The disk may be full.";
  return false;
}

bool ExtractFile(Entry& entry, const std::filesystem::path& out,
                 const std::filesystem::path& relative, Extraction& x) {
  x.progress.ShowFile(relative);
  std::ofstream file(out, std::ios::binary | std::ios::trunc);
  if (!file) return WriteFailed(x, relative, out);
  const uint64_t size = entry.size();
  if (size > 0) {
    const auto mapped = entry.OpenMapped(rex::memory::MappedMemory::Mode::kRead);
    if (!mapped || mapped->size() < size) return WriteFailed(x, relative, out);
    for (uint64_t offset = 0; offset < size;) {
      if (x.progress.Cancelled()) {
        x.cancelled = true;
        return false;
      }
      const size_t chunk = size_t(std::min<uint64_t>(kChunkBytes, size - offset));
      file.write(reinterpret_cast<const char*>(mapped->data() + offset), std::streamsize(chunk));
      if (!file) return WriteFailed(x, relative, out);
      offset += chunk;
      x.progress.Advance(chunk);
    }
  }
  file.close();
  if (!file) return WriteFailed(x, relative, out);
  ++x.files;
  return true;
}

bool ExtractTree(Entry& dir, const std::filesystem::path& out,
                 const std::filesystem::path& relative, Extraction& x) {
  for (const auto& child : dir.children()) {
    if (!IsSafeEntryName(child->name())) {
      x.error = L"The disc image has an invalid file name; extraction stopped.";
      return false;
    }
    const std::wstring name = Widen(child->name(), CP_UTF8);
    const std::filesystem::path child_out = out / name;
    const std::filesystem::path child_relative = relative / name;
    std::error_code ec;
    // Staging starts empty, so a hit means two disc names Windows treats as one (like a and A).
    if (std::filesystem::exists(child_out, ec)) {
      x.error = L"The disc image has two entries with the same name (" +
                child_relative.wstring() + L"); extraction stopped.";
      return false;
    }
    if (IsDirectory(*child)) {
      if (!std::filesystem::create_directory(child_out, ec) || ec) {
        return WriteFailed(x, child_relative, child_out);
      }
      if (!ExtractTree(*child, child_out, child_relative, x)) return false;
    } else if (!ExtractFile(*child, child_out, child_relative, x)) {
      return false;
    }
  }
  return true;
}

struct Outcome {
  bool ok = false;
  uint64_t files = 0;
  uint64_t bytes = 0;
  std::wstring error;
  bool cancelled = false;
};

InstallResult Abandon(const std::filesystem::path& staging, const Outcome& outcome) {
  std::error_code ec;
  std::filesystem::remove_all(staging, ec);
  InstallResult result;
  result.error = outcome.error;
  result.cancelled = outcome.error.empty() && outcome.cancelled;
  if (result.cancelled) {
    REXLOG_INFO("gow2 disc image extraction cancelled by the player");
  } else {
    REXLOG_WARN("gow2 disc image extraction stopped after {} files", outcome.files);
  }
  return result;
}

}  // namespace

InstallResult InstallFromIso(const std::filesystem::path& iso, const std::filesystem::path& app_home) {
  const std::filesystem::path target = app_home / kInstallFolderName;
  const std::filesystem::path staging = app_home / kStagingFolderName;
  if (IsDiscFolder(target)) {
    InstallResult result;
    result.game_folder = target;
    return result;
  }
  std::error_code ec;
  if (std::filesystem::exists(target, ec)) {
    return Failed(L"The folder " + target.wstring() +
                  L" already exists but is not a complete game folder. Move or delete it, then "
                  L"choose the .iso again.");
  }

  rex::filesystem::DiscImageDevice device("\\GOW2ISO", iso);
  if (!device.Initialize()) {
    return Failed(iso.wstring() + L" is not an Xbox 360 disc image, or it could not be read.");
  }
  if (!HasGameAtRoot(device.root())) {
    return Failed(L"default.xex and GearGame were not found in this disc image. Choose the .iso "
                  L"of your own Gears of War 2 disc.");
  }
  if (auto error = CheckFreeSpace(app_home, device.total_file_size()); !error.empty()) {
    return Failed(std::move(error));
  }
  if (auto error = PrepareStaging(staging); !error.empty()) return Failed(std::move(error));

  const auto start = std::chrono::steady_clock::now();
  Outcome outcome;
  {
    Progress progress(device.total_file_size());
    Extraction x(progress);
    outcome.ok = ExtractTree(*const_cast<Entry*>(device.root()), staging, {}, x);
    outcome.files = x.files;
    outcome.bytes = progress.done();
    outcome.error = std::move(x.error);
    outcome.cancelled = x.cancelled;
  }
  if (!outcome.ok) return Abandon(staging, outcome);

  std::filesystem::rename(staging, target, ec);
  if (ec) {
    std::wstring error = L"Could not rename " + staging.wstring() + L" to " + target.wstring() +
                         L": " + Reason(ec) + L".";
    std::filesystem::remove_all(staging, ec);
    return Failed(std::move(error));
  }
  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  REXLOG_INFO("gow2 extracted {} files ({} bytes) from {} into {} in {:.1f} s", outcome.files,
              outcome.bytes, Narrow(iso), Narrow(target), seconds);
  InstallResult result;
  result.game_folder = target;
  return result;
}

}  // namespace rex::glue::disc_install

#endif
