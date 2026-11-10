// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// The PC Options menu's lists, folder picker, restart and internet hosting.
#include "gow2_options_actions.h"

#include <rex/cvar.h>
#include <rex/logging.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "gow2_gpu_pick.h"
#include "gow2_host_line.h"

#ifdef _WIN32
#include <windows.h>

#include <shellapi.h>
#include <shobjidl.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#endif

REXCVAR_DEFINE_INT32(gow2_wait_pid, 0, "UI",
                     "Internal: process id the game waits for at start-up after an in-game restart");

namespace rex::glue::options_actions {

namespace {

bool HasValue(const std::vector<NamedValue>& rows, std::string_view value) {
  for (const NamedValue& row : rows) {
    if (row.value == value) return true;
  }
  return false;
}

#ifdef _WIN32

std::string WideToUtf8(std::wstring_view wide) {
  if (wide.empty()) return {};
  const int size =
      WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), nullptr, 0, nullptr, nullptr);
  if (size <= 0) return {};
  std::string out(size_t(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), out.data(), size, nullptr, nullptr);
  return out;
}

std::wstring Utf8ToWide(std::string_view text) {
  if (text.empty()) return {};
  const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
  if (size <= 0) return {};
  std::wstring out(size_t(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), out.data(), size);
  return out;
}

bool RunShellOpen(const std::wstring& target, DWORD* error) {
  SHELLEXECUTEINFOW info{};
  info.cbSize = sizeof(info);
  info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
  info.lpVerb = L"open";
  info.lpFile = target.c_str();
  info.nShow = SW_SHOWNORMAL;
  if (ShellExecuteExW(&info)) return true;
  *error = GetLastError();
  return false;
}

void LogShellOpenResult(const std::wstring& target, bool opened, DWORD error) {
  if (opened) {
    REXLOG_INFO("PCOPTIONS opened {}", WideToUtf8(target));
    return;
  }
  REXLOG_WARN("PCOPTIONS could not open {} (error {})", WideToUtf8(target), error);
}

void OpenInShellOnWorker(std::wstring target, void* window_to_minimize) {
  const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  DWORD error = ERROR_SUCCESS;
  const bool opened = RunShellOpen(target, &error);
  LogShellOpenResult(target, opened, error);
  if (opened && window_to_minimize != nullptr) {
    // The browser or Explorer opens behind a fullscreen game.
    ShowWindowAsync(static_cast<HWND>(window_to_minimize), SW_MINIMIZE);
  }
  if (SUCCEEDED(initialized)) CoUninitialize();
}

std::string Dimensions(int width, int height) {
  return std::to_string(width) + "x" + std::to_string(height);
}

struct MonitorRow {
  bool primary = false;
  std::wstring name;
  int width = 0;
  int height = 0;
};

std::wstring MonitorName(const DISPLAY_DEVICEW& adapter) {
  DISPLAY_DEVICEW monitor{};
  monitor.cb = sizeof(monitor);
  if (EnumDisplayDevicesW(adapter.DeviceName, 0, &monitor, 0) && monitor.DeviceString[0] != L'\0') {
    return monitor.DeviceString;
  }
  return adapter.DeviceString;
}

void ReadMonitorSize(const wchar_t* device, int* width, int* height) {
  DEVMODEW settings{};
  settings.dmSize = sizeof(settings);
  if (!EnumDisplaySettingsW(device, ENUM_CURRENT_SETTINGS, &settings)) return;
  *width = int(settings.dmPelsWidth);
  *height = int(settings.dmPelsHeight);
}

std::vector<MonitorRow> AttachedMonitors() {
  std::vector<MonitorRow> rows;
  for (DWORD device = 0;; ++device) {
    DISPLAY_DEVICEW adapter{};
    adapter.cb = sizeof(adapter);
    if (!EnumDisplayDevicesW(nullptr, device, &adapter, 0)) break;
    if (!(adapter.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) continue;
    MonitorRow row;
    row.primary = (adapter.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0;
    row.name = MonitorName(adapter);
    ReadMonitorSize(adapter.DeviceName, &row.width, &row.height);
    rows.push_back(std::move(row));
  }
  std::stable_partition(rows.begin(), rows.end(),
                        [](const MonitorRow& row) { return row.primary; });
  return rows;
}

std::string FormatGigabytes(uint64_t bytes) {
  const double gb = double(bytes) / (1024.0 * 1024.0 * 1024.0);
  char text[32]{};
  if (gb < 10) {
    std::snprintf(text, sizeof(text), "%.1f", gb);
  } else {
    std::snprintf(text, sizeof(text), "%.0f", gb);
  }
  return text;
}

std::wstring EnvWide(const wchar_t* name) {
  const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
  if (size <= 1) return {};
  std::wstring value(size - 1, L'\0');
  GetEnvironmentVariableW(name, value.data(), size);
  return value;
}

bool DirExists(std::wstring_view path) {
  if (path.empty()) return false;
  std::error_code ec;
  return std::filesystem::is_directory(std::filesystem::path(path), ec);
}

std::string SavesUnder(std::wstring_view root) {
  return WideToUtf8(root) + "\\GearsOfWar2\\saves";
}

void AppendOneDrive(std::vector<NamedValue>* out) {
  constexpr const wchar_t* kWide[] = {L"OneDrive", L"OneDriveConsumer"};
  constexpr const char* kNarrow[] = {"OneDrive", "OneDriveConsumer"};
  for (int i = 0; i < 2; ++i) {
    const std::wstring root = EnvWide(kWide[i]);
    if (!DirExists(root)) continue;
    const std::string value = std::string("%") + kNarrow[i] + "%\\GearsOfWar2\\saves";
    out->push_back({value, "OneDrive (" + SavesUnder(root) + ")"});
    return;
  }
}

void AppendGoogleDrive(std::vector<NamedValue>* out) {
  std::vector<std::wstring> roots = {L"G:\\My Drive", L"G:\\M\u00ED unidad"};
  const std::wstring profile = EnvWide(L"USERPROFILE");
  if (!profile.empty()) {
    roots.push_back(profile + L"\\Google Drive");
    roots.push_back(profile + L"\\My Drive");
  }
  for (const std::wstring& root : roots) {
    if (!DirExists(root)) continue;
    const std::string value = SavesUnder(root);
    out->push_back({value, "Google Drive (" + value + ")"});
    return;
  }
}

void AppendDropbox(std::vector<NamedValue>* out) {
  const std::wstring profile = EnvWide(L"USERPROFILE");
  if (profile.empty() || !DirExists(profile + L"\\Dropbox")) return;
  const std::string value = SavesUnder(profile + L"\\Dropbox");
  out->push_back({value, "Dropbox (" + value + ")"});
}

std::wstring ModulePath() {
  std::wstring exe(32768, L'\0');
  exe.resize(GetModuleFileNameW(nullptr, exe.data(), DWORD(exe.size())));
  return exe;
}

std::wstring CurrentDirectory() {
  const DWORD size = GetCurrentDirectoryW(0, nullptr);
  if (size <= 1) return {};
  std::wstring dir(size - 1, L'\0');
  GetCurrentDirectoryW(size, dir.data());
  return dir;
}

std::wstring_view SkipFirstToken(std::wstring_view command) {
  size_t i = 0;
  while (i < command.size() && (command[i] == L' ' || command[i] == L'\t')) ++i;
  if (i < command.size() && command[i] == L'"') {
    ++i;
    while (i < command.size() && command[i] != L'"') ++i;
    if (i < command.size()) ++i;
  } else {
    while (i < command.size() && command[i] != L' ' && command[i] != L'\t') ++i;
  }
  return command.substr(i);
}

std::wstring DropWaitPidArg(std::wstring_view args) {
  constexpr std::wstring_view kPrefix = L"--gow2_wait_pid=";
  std::wstring kept;
  size_t i = 0;
  while (i < args.size()) {
    while (i < args.size() && (args[i] == L' ' || args[i] == L'\t')) ++i;
    if (i >= args.size()) break;
    const size_t begin = i;
    if (args[i] == L'"') {
      ++i;
      while (i < args.size() && args[i] != L'"') ++i;
      if (i < args.size()) ++i;
    } else {
      while (i < args.size() && args[i] != L' ' && args[i] != L'\t') ++i;
    }
    std::wstring_view token = args.substr(begin, i - begin);
    std::wstring_view bare = token;
    if (bare.size() >= 2 && bare.front() == L'"' && bare.back() == L'"') {
      bare = bare.substr(1, bare.size() - 2);
    }
    if (bare.size() < kPrefix.size() || bare.substr(0, kPrefix.size()) != kPrefix) {
      if (!kept.empty()) kept.push_back(L' ');
      kept.append(token);
    }
  }
  return kept;
}

struct HostState {
  std::mutex mutex;
  std::optional<HostResult> last;
  std::atomic<bool> busy{false};
  std::atomic<bool> opened{false};
};

std::shared_ptr<HostState> HostStateInstance() {
  static std::shared_ptr<HostState> state = std::make_shared<HostState>();
  return state;
}

std::wstring ExeDir() {
  const std::wstring exe = ModulePath();
  const size_t slash = exe.find_last_of(L"\\/");
  return slash == std::wstring::npos ? std::wstring() : exe.substr(0, slash);
}

bool IsInsideFolder(const std::filesystem::path& path, const std::filesystem::path& folder) {
  auto part = path.begin();
  for (const auto& root_part : folder) {
    if (part == path.end()) return false;
    const std::wstring& name = part->native();
    const std::wstring& root_name = root_part.native();
    if (CompareStringOrdinal(name.data(), int(name.size()), root_name.data(), int(root_name.size()),
                             TRUE) != CSTR_EQUAL) return false;
    ++part;
  }
  return part != path.end();
}

std::wstring FindHostScript() {
  const std::wstring exe_dir = ExeDir();
  if (exe_dir.empty()) return {};
  std::error_code ec;
  const auto dir = std::filesystem::weakly_canonical(exe_dir, ec);
  if (ec) return {};
  const auto script = std::filesystem::weakly_canonical(dir / L"scripts" / L"host-online.ps1", ec);
  if (ec || !IsInsideFolder(script, dir)) return {};
  if (!std::filesystem::is_regular_file(script, ec)) return {};
  return script.wstring();
}

struct ChildPipes {
  HANDLE read = nullptr;
  HANDLE write = nullptr;
};

void CloseIf(HANDLE& handle) {
  if (handle == nullptr) return;
  CloseHandle(handle);
  handle = nullptr;
}

bool MakeOutputPipe(ChildPipes* pipes) {
  SECURITY_ATTRIBUTES inherit{};
  inherit.nLength = sizeof(inherit);
  inherit.bInheritHandle = TRUE;
  if (!CreatePipe(&pipes->read, &pipes->write, &inherit, 0)) return false;
  if (!SetHandleInformation(pipes->read, HANDLE_FLAG_INHERIT, 0)) {
    CloseIf(pipes->read);
    CloseIf(pipes->write);
    return false;
  }
  return true;
}

HANDLE OpenNullInput() {
  SECURITY_ATTRIBUTES inherit{};
  inherit.nLength = sizeof(inherit);
  inherit.bInheritHandle = TRUE;
  HANDLE input =
      CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING,
                  0, nullptr);
  return input == INVALID_HANDLE_VALUE ? nullptr : input;
}

bool IsPsModulePathEntry(std::wstring_view entry) {
  constexpr std::wstring_view kName = L"PSMODULEPATH";
  if (entry.size() <= kName.size() || entry[kName.size()] != L'=') return false;
  for (size_t i = 0; i < kName.size(); ++i) {
    wchar_t c = entry[i];
    if (c >= L'a' && c <= L'z') c = wchar_t(c - (L'a' - L'A'));
    if (c != kName[i]) return false;
  }
  return true;
}

std::vector<wchar_t> ChildEnvironment() {
  std::vector<wchar_t> block;
  wchar_t* env = GetEnvironmentStringsW();
  if (env == nullptr) return block;
  for (wchar_t* entry = env; *entry != L'\0';) {
    const std::wstring_view text(entry);
    if (!IsPsModulePathEntry(text)) {
      block.insert(block.end(), text.begin(), text.end());
      block.push_back(L'\0');
    }
    entry += text.size() + 1;
  }
  FreeEnvironmentStringsW(env);
  block.push_back(L'\0');
  return block;
}

bool StartHiddenChild(std::wstring* command, const std::vector<wchar_t>& env, HANDLE out_write,
                      HANDLE in_null, PROCESS_INFORMATION* child) {
  STARTUPINFOW start{};
  start.cb = sizeof(start);
  start.dwFlags = STARTF_USESTDHANDLES;
  start.hStdInput = in_null;
  start.hStdOutput = out_write;
  start.hStdError = out_write;
  void* env_block = env.empty() ? nullptr : const_cast<wchar_t*>(env.data());
  return CreateProcessW(nullptr, command->data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, env_block, nullptr, &start,
                        child) != FALSE;
}

void DrainPipe(HANDLE read_end, std::string* out) {
  char chunk[4096];
  while (true) {
    DWORD ready = 0;
    if (!PeekNamedPipe(read_end, nullptr, 0, nullptr, &ready, nullptr) || ready == 0) return;
    const DWORD want = ready < 4096 ? ready : 4096;
    DWORD got = 0;
    if (!ReadFile(read_end, chunk, want, &got, nullptr) || got == 0) return;
    out->append(chunk, got);
  }
}

std::string WaitChildOutput(HANDLE read_end, HANDLE process) {
  std::string out;
  const ULONGLONG deadline = GetTickCount64() + 180000;
  while (true) {
    DrainPipe(read_end, &out);
    if (WaitForSingleObject(process, 50) == WAIT_OBJECT_0) {
      DrainPipe(read_end, &out);
      return out;
    }
    if (GetTickCount64() >= deadline) {
      TerminateProcess(process, 1);
      WaitForSingleObject(process, 5000);
      DrainPipe(read_end, &out);
      return out;
    }
  }
}

std::vector<std::string> SplitOutputLines(const std::string& output) {
  std::vector<std::string> lines;
  size_t begin = 0;
  while (begin <= output.size()) {
    size_t end = output.find('\n', begin);
    if (end == std::string::npos) end = output.size();
    std::string line = output.substr(begin, end - begin);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    lines.push_back(std::move(line));
    begin = end + 1;
  }
  return lines;
}

std::string LastHostLine(const std::vector<std::string>& lines) {
  constexpr std::string_view kPrefix = "GOW2HOST ";
  for (size_t i = lines.size(); i > 0; --i) {
    const std::string& line = lines[i - 1];
    if (line.size() >= kPrefix.size() && line.compare(0, kPrefix.size(), kPrefix) == 0) return line;
  }
  return {};
}

std::string LastNonEmptyLine(const std::vector<std::string>& lines) {
  for (size_t i = lines.size(); i > 0; --i) {
    if (!lines[i - 1].empty()) return lines[i - 1];
  }
  return {};
}

HostResult ResultFromOutput(const std::string& mode, const std::string& output) {
  const std::vector<std::string> lines = SplitOutputLines(output);
  const std::string host = LastHostLine(lines);
  if (!host.empty()) {
    if (std::optional<HostResult> parsed = host_line::ParseHostLine(host)) return *parsed;
  }
  HostResult fallback;
  fallback.mode = mode;
  fallback.error = LastNonEmptyLine(lines);
  return fallback;
}

HostResult RunHostCommandInner(const std::string& mode, std::wstring command) {
  HostResult result;
  result.mode = mode;
  ChildPipes pipes;
  HANDLE in_null = nullptr;
  PROCESS_INFORMATION child{};
  std::string output;
  bool ran = false;
  if (MakeOutputPipe(&pipes)) {
    in_null = OpenNullInput();
    if (in_null != nullptr) {
      ran = StartHiddenChild(&command, ChildEnvironment(), pipes.write, in_null, &child);
      CloseIf(in_null);
      CloseIf(pipes.write);
      if (ran) output = WaitChildOutput(pipes.read, child.hProcess);
    }
  }
  CloseIf(pipes.read);
  CloseIf(pipes.write);
  CloseIf(in_null);
  if (child.hProcess != nullptr) CloseHandle(child.hProcess);
  if (child.hThread != nullptr) CloseHandle(child.hThread);
  if (!ran) {
    result.error = "could not start powershell.exe";
    return result;
  }
  return ResultFromOutput(mode, output);
}

void RunHostCommand(std::shared_ptr<HostState> state, std::string mode, std::wstring command) {
  const HostResult result = RunHostCommandInner(mode, std::move(command));
  {
    std::lock_guard<std::mutex> lock(state->mutex);
    state->last = result;
  }
  if (mode == "Start" && result.ok) state->opened.store(true, std::memory_order_release);
  if (mode == "Stop" && result.ok) state->opened.store(false, std::memory_order_release);
  REXLOG_INFO("PCOPTIONS host {}: ok={} {} {}", mode, result.ok ? "true" : "false",
              result.public_address, result.error);
  state->busy.store(false, std::memory_order_release);
}

bool LaunchHostRun(const std::string& mode, const std::vector<std::wstring>& args) {
  const std::shared_ptr<HostState> state = HostStateInstance();
  if (state->busy.exchange(true, std::memory_order_acq_rel)) return false;
  const std::wstring script = FindHostScript();
  if (script.empty()) {
    state->busy.store(false, std::memory_order_release);
    return false;
  }
  std::wstring command = host_line::QuoteWindowsArg(L"powershell.exe");
  const std::vector<std::wstring> prefix = {
      L"-NoProfile", L"-NonInteractive", L"-ExecutionPolicy", L"Bypass", L"-File", script};
  for (const std::wstring& arg : prefix) command += L" " + host_line::QuoteWindowsArg(arg);
  for (const std::wstring& arg : args) command += L" " + host_line::QuoteWindowsArg(arg);
  try {
    std::thread(RunHostCommand, state, mode, command).detach();
  } catch (...) {
    state->busy.store(false, std::memory_order_release);
    return false;
  }
  return true;
}

#endif

}  // namespace

std::vector<NamedValue> ListMonitors() {
#ifdef _WIN32
  std::vector<NamedValue> out;
  const std::vector<MonitorRow> rows = AttachedMonitors();
  if (rows.empty()) return out;
  const MonitorRow& main = rows.front();
  out.push_back({"0", "Main display (" + Dimensions(main.width, main.height) + ")"});
  int number = 1;
  for (const MonitorRow& row : rows) {
    const std::string numeral = std::to_string(number);
    out.push_back({numeral, "Display " + numeral + ": " + WideToUtf8(row.name) + " (" +
                                Dimensions(row.width, row.height) + ")"});
    ++number;
  }
  return out;
#else
  return {};
#endif
}

std::vector<NamedValue> ListGpus() {
#ifdef _WIN32
  std::vector<NamedValue> out{{"auto", "Automatic"}};
  for (const GpuAdapterInfo& gpu : ListRenderGpus()) {
    out.push_back(
        {std::to_string(gpu.index), gpu.name + " (" + FormatGigabytes(gpu.dedicated_bytes) + " GB)"});
  }
  return out;
#else
  return {};
#endif
}

std::vector<NamedValue> ListSaveFolders(std::string_view current) {
  std::vector<NamedValue> out;
  out.push_back({"", "Default (this device)"});
  out.push_back({"saves", "Next to the game (portable)"});
#ifdef _WIN32
  AppendOneDrive(&out);
  AppendGoogleDrive(&out);
  AppendDropbox(&out);
#endif
  if (!current.empty() && !HasValue(out, current)) {
    const std::string custom(current);
    out.push_back({custom, "Custom (" + custom + ")"});
  }
  return out;
}

std::optional<std::string> PickFolder(std::string_view title) {
#ifdef _WIN32
  const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  std::optional<std::string> result;
  IFileOpenDialog* dialog = nullptr;
  if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                 IID_PPV_ARGS(&dialog)))) {
    DWORD opts = 0;
    dialog->GetOptions(&opts);
    dialog->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    const std::wstring wide_title = Utf8ToWide(title);
    dialog->SetTitle(wide_title.c_str());
    IShellItem* item = nullptr;
    if (SUCCEEDED(dialog->Show(nullptr)) && SUCCEEDED(dialog->GetResult(&item))) {
      PWSTR path = nullptr;
      if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
        result = WideToUtf8(path);
        CoTaskMemFree(path);
      }
      item->Release();
    }
    dialog->Release();
  }
  if (SUCCEEDED(co)) CoUninitialize();
  return result;
#else
  (void)title;
  return std::nullopt;
#endif
}

void OpenInShell(std::wstring target, void* window_to_minimize) {
#ifdef _WIN32
  std::thread([target = std::move(target), window_to_minimize] {
    OpenInShellOnWorker(target, window_to_minimize);
  }).detach();
#else
  (void)target;
  (void)window_to_minimize;
#endif
}

void OpenFolder(const std::filesystem::path& dir, void* window_to_minimize) {
  std::error_code ec;
  if (!std::filesystem::exists(dir, ec)) std::filesystem::create_directories(dir, ec);
  OpenInShell(dir.wstring(), window_to_minimize);
}

bool RestartGame() {
#ifdef _WIN32
  const std::wstring exe = ModulePath();
  if (exe.empty()) return false;
  std::wstring args = DropWaitPidArg(SkipFirstToken(GetCommandLineW()));
  args += args.empty() ? L"--gow2_wait_pid=" : L" --gow2_wait_pid=";
  args += std::to_wstring(GetCurrentProcessId());
  REXLOG_INFO("PCOPTIONS restarting: {} {}", WideToUtf8(exe), WideToUtf8(args));
  const std::wstring dir = CurrentDirectory();
  std::wstring command = L'"' + exe + L'"' + L' ' + args;
  STARTUPINFOW start{};
  start.cb = sizeof(start);
  PROCESS_INFORMATION child{};
  const BOOL started =
      CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                     dir.empty() ? nullptr : dir.c_str(), &start, &child);
  if (!started) return false;
  CloseHandle(child.hProcess);
  CloseHandle(child.hThread);
  return true;
#else
  return false;
#endif
}

void WaitForPreviousInstance() {
#ifdef _WIN32
  const int32_t pid = REXCVAR_GET(gow2_wait_pid);
  if (pid <= 0 || DWORD(pid) == GetCurrentProcessId()) return;
  HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, DWORD(pid));
  if (process == nullptr) return;
  const ULONGLONG start = GetTickCount64();
  WaitForSingleObject(process, 20000);
  const ULONGLONG waited = GetTickCount64() - start;
  CloseHandle(process);
  REXLOG_INFO("PCOPTIONS waited {} ms for the previous instance {}", waited, pid);
#endif
}

std::optional<HostResult> ParseHostLine(std::string_view line) {
  return host_line::ParseHostLine(line);
}

bool HostRunner::Start(const HostRequest& request) {
#ifdef _WIN32
  const std::string* invalid = nullptr;
  if (!host_line::IsValidHostname(request.hostname)) invalid = &request.hostname;
  if (!host_line::IsValidAllowPeers(request.allow_peers)) invalid = &request.allow_peers;
  if (invalid != nullptr) {
    HostResult result;
    result.mode = "Start";
    result.error = "Invalid address: " + invalid->substr(0, 80);
    const std::shared_ptr<HostState> state = HostStateInstance();
    std::lock_guard<std::mutex> lock(state->mutex);
    state->last = std::move(result);
    return false;
  }
  std::vector<std::wstring> args = {L"-Start", L"-LeaseHours",
                                    std::to_wstring(request.lease_hours)};
  if (!request.hostname.empty()) {
    args.push_back(L"-Hostname");
    args.push_back(Utf8ToWide(request.hostname));
  }
  if (!request.allow_peers.empty()) {
    args.push_back(L"-AllowPeers");
    args.push_back(Utf8ToWide(request.allow_peers));
  }
  args.insert(args.end(), {L"-Players", std::to_wstring(request.players), L"-PortOffset",
                           std::to_wstring(request.port_offset), L"-Json"});
  return LaunchHostRun("Start", args);
#else
  (void)request;
  return false;
#endif
}

bool HostRunner::Stop() {
#ifdef _WIN32
  return LaunchHostRun("Stop", {L"-Stop", L"-Json"});
#else
  return false;
#endif
}

bool HostRunner::Status() {
#ifdef _WIN32
  return LaunchHostRun("Status", {L"-Status", L"-Json"});
#else
  return false;
#endif
}

bool HostRunner::Busy() const {
#ifdef _WIN32
  return HostStateInstance()->busy.load(std::memory_order_acquire);
#else
  return false;
#endif
}

std::optional<HostResult> HostRunner::Last() const {
#ifdef _WIN32
  const std::shared_ptr<HostState> state = HostStateInstance();
  std::lock_guard<std::mutex> lock(state->mutex);
  return state->last;
#else
  return std::nullopt;
#endif
}

std::string HostRunner::ScriptPath() const {
#ifdef _WIN32
  return WideToUtf8(FindHostScript());
#else
  return {};
#endif
}

bool HostRunner::OpenedThisSession() const {
#ifdef _WIN32
  return HostStateInstance()->opened.load(std::memory_order_acquire);
#else
  return false;
#endif
}

void HostRunner::StopBlocking(int timeout_ms) {
#ifdef _WIN32
  const ULONGLONG deadline = GetTickCount64() + ULONGLONG(timeout_ms < 0 ? 0 : timeout_ms);
  const auto wait = [this, deadline]() {
    while (Busy()) {
      const ULONGLONG now = GetTickCount64();
      if (now >= deadline) return false;
      Sleep(DWORD(std::min<ULONGLONG>(20, deadline - now)));
    }
    return true;
  };
  if (!wait() || !OpenedThisSession() || GetTickCount64() >= deadline) return;
  if (Stop()) wait();
#else
  (void)timeout_ms;
#endif
}

HostRunner& Host() {
  static HostRunner host;
  return host;
}

}  // namespace rex::glue::options_actions
