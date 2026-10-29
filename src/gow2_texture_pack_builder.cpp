// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Texture pack builder: dump DDS -> texconv PNG -> Real-ESRGAN x4 -> texconv DDS with a full mip chain.
#include "gow2_texture_pack_builder.h"

#ifdef _WIN32
#include "gow2_gpu_pick.h"
#include "gow2_texture_pack_plan.h"
#include "gow2_texture_packs.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <optional>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <rex/filesystem.h>
#include <rex/logging.h>

#include <windows.h>

namespace rex::glue::texture_pack_builder {
namespace {

namespace fs = std::filesystem;
namespace plan = texture_pack_plan;
using texture_packs::Utf8;

constexpr size_t kBatchSize = 64;
constexpr DWORD kTexconvTimeoutMs = 5 * 60 * 1000;
constexpr DWORD kEsrganTimeoutMs = 60 * 60 * 1000;
// Measured on the hospital dump against realesrgan-x4plus: tones off by 3.3 instead of 5.0 (median), dark
// pixels added at most 1 % instead of 28 % (x4plus crushes dark paint and concrete), and 15x faster (49 s
// for 431 textures).
constexpr const char* kEsrganModel = "realesr-animevideov3-x4";

struct Failure {
  std::string message;
};
struct Cancelled {};

struct Job {
  std::string file_name;
  plan::DumpName dump;
  plan::Plan plan;
};

// Mirrors vulkan_core.h: the SDK ships no Vulkan headers and only these few are needed.
namespace vk {
using Instance = struct Instance_T*;
using PhysicalDevice = struct PhysicalDevice_T*;
using Result = int32_t;
using VoidFunction = void (*)();
constexpr int32_t kStructureTypeApplicationInfo = 0;
constexpr int32_t kStructureTypeInstanceCreateInfo = 1;
constexpr uint32_t kApiVersion10 = 1u << 22;
constexpr size_t kDeviceNameOffset = 20;
constexpr size_t kDeviceNameSize = 256;
struct ApplicationInfo {
  int32_t sType;
  const void* pNext;
  const char* pApplicationName;
  uint32_t applicationVersion;
  const char* pEngineName;
  uint32_t engineVersion;
  uint32_t apiVersion;
};
struct InstanceCreateInfo {
  int32_t sType;
  const void* pNext;
  uint32_t flags;
  const ApplicationInfo* pApplicationInfo;
  uint32_t enabledLayerCount;
  const char* const* ppEnabledLayerNames;
  uint32_t enabledExtensionCount;
  const char* const* ppEnabledExtensionNames;
};
using GetInstanceProcAddr = VoidFunction (*)(Instance, const char*);
using CreateInstance = Result (*)(const InstanceCreateInfo*, const void*, Instance*);
using DestroyInstance = void (*)(Instance, const void*);
using EnumeratePhysicalDevices = Result (*)(Instance, uint32_t*, PhysicalDevice*);
using GetPhysicalDeviceProperties = void (*)(PhysicalDevice, void*);
}  // namespace vk

fs::path ExeDir() { return rex::filesystem::GetExecutablePath().parent_path(); }
fs::path TexconvExe() { return ExeDir() / "tools" / "texconv" / "texconv.exe"; }
fs::path EsrganDir() { return ExeDir() / "tools" / "realesrgan"; }
fs::path EsrganExe() { return EsrganDir() / "realesrgan-ncnn-vulkan.exe"; }
fs::path EsrganModels() { return EsrganDir() / "models"; }
fs::path DumpDir() { return ExeDir() / "textures" / "4D53082D" / "dump"; }
fs::path PacksDir() { return ExeDir() / "texture_packs"; }

bool IsFile(const fs::path& path) {
  std::error_code ec;
  return fs::is_regular_file(path, ec);
}

std::wstring Widen(const std::string& utf8) {
  return fs::path(std::u8string(utf8.begin(), utf8.end())).wstring();
}

double SecondsSince(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

std::vector<std::string> DumpNames() {
  std::vector<std::string> names;
  std::error_code ec;
  for (fs::directory_iterator it(DumpDir(), fs::directory_options::skip_permission_denied, ec);
       !ec && it != fs::directory_iterator(); it.increment(ec)) {
    std::string name = Utf8(it->path().filename());
    if (plan::ParseDumpName(name)) names.push_back(std::move(name));
  }
  return names;
}

std::vector<uint8_t> ReadBytes(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

plan::Content Classify(const fs::path& file, const plan::DumpName& dump) {
  const auto bytes = ReadBytes(file);
  return plan::ClassifyTexture(dump.format, plan::DdsMip0(bytes, dump));
}

template <typename Fn>
Fn VulkanProc(vk::GetInstanceProcAddr get, vk::Instance instance, const char* name) {
  return reinterpret_cast<Fn>(get(instance, name));
}

std::string VulkanDeviceName(vk::GetPhysicalDeviceProperties get_properties, vk::PhysicalDevice device) {
  // The real VkPhysicalDeviceProperties is 824 bytes; the spare room covers a driver writing a newer layout.
  alignas(8) uint8_t properties[1024] = {};
  get_properties(device, properties);
  const char* name = reinterpret_cast<const char*>(properties + vk::kDeviceNameOffset);
  return std::string(name, strnlen(name, vk::kDeviceNameSize));
}

std::vector<std::string> VulkanDeviceNames(vk::GetInstanceProcAddr get, vk::Instance instance) {
  const auto enumerate = VulkanProc<vk::EnumeratePhysicalDevices>(get, instance, "vkEnumeratePhysicalDevices");
  const auto properties = VulkanProc<vk::GetPhysicalDeviceProperties>(get, instance, "vkGetPhysicalDeviceProperties");
  if (!enumerate || !properties) return {};
  uint32_t count = 0;
  if (enumerate(instance, &count, nullptr) < 0 || count == 0) return {};
  std::vector<vk::PhysicalDevice> devices(count);
  if (enumerate(instance, &count, devices.data()) < 0) return {};
  devices.resize(count);
  std::vector<std::string> names;
  for (const auto device : devices) names.push_back(VulkanDeviceName(properties, device));
  return names;
}

std::vector<std::string> VulkanDeviceNames(vk::GetInstanceProcAddr get) {
  const auto create = VulkanProc<vk::CreateInstance>(get, nullptr, "vkCreateInstance");
  if (!create) return {};
  const vk::ApplicationInfo app{vk::kStructureTypeApplicationInfo, nullptr, "gow2", 0, nullptr, 0, vk::kApiVersion10};
  const vk::InstanceCreateInfo info{vk::kStructureTypeInstanceCreateInfo, nullptr, 0, &app, 0, nullptr, 0, nullptr};
  vk::Instance instance = nullptr;
  if (create(&info, nullptr, &instance) < 0 || !instance) return {};
  auto names = VulkanDeviceNames(get, instance);
  if (const auto destroy = VulkanProc<vk::DestroyInstance>(get, instance, "vkDestroyInstance")) destroy(instance, nullptr);
  return names;
}

// ncnn numbers its -g devices in this same enumeration order.
std::vector<std::string> VulkanDeviceNames() {
  const HMODULE loader = LoadLibraryW(L"vulkan-1.dll");
  if (!loader) return {};
  std::vector<std::string> names;
  if (const auto get = reinterpret_cast<vk::GetInstanceProcAddr>(GetProcAddress(loader, "vkGetInstanceProcAddr"))) {
    names = VulkanDeviceNames(get);
  }
  FreeLibrary(loader);
  return names;
}

// Vulkan's device 0 is not DXGI's: it can be the integrated GPU, about 10x slower.
std::optional<uint32_t> EsrganDevice() {
  const std::string gpu = ChooseRenderGpu().name;
  const auto names = VulkanDeviceNames();
  const auto index = plan::VulkanIndexForName(names, gpu);
  if (index) REXLOG_INFO("TEXBUILD Real-ESRGAN on Vulkan device {} '{}' (game GPU '{}')", *index, names[*index], gpu);
  else REXLOG_INFO("TEXBUILD no Vulkan match for '{}', using the default device", gpu);
  return index;
}

void ClearDir(const fs::path& dir) {
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  if (ec) throw Failure{"could not create " + Utf8(dir) + ": " + ec.message()};
}

class Builder {
 public:
  ~Builder() { Shutdown(); }

  bool Start(uint32_t scale, bool high_quality) {
    std::lock_guard lock(mutex_);
    if (snapshot_.state == State::kRunning) return false;
    if (thread_.joinable()) thread_.join();
    cancel_ = false;
    snapshot_ = {State::kRunning, 0, 0, {}, snapshot_.finished};
    thread_ = std::thread([this, scale, high_quality] { Run(scale, high_quality); });
    return true;
  }

  void Cancel() {
    std::lock_guard lock(mutex_);
    if (snapshot_.state != State::kRunning) return;
    cancel_ = true;
    if (child_) TerminateProcess(child_, 1);
  }

  Snapshot Get() {
    std::lock_guard lock(mutex_);
    return snapshot_;
  }

  void Shutdown() {
    Cancel();
    if (thread_.joinable()) thread_.join();
  }

 private:
  void Run(uint32_t scale, bool high_quality) {
    const std::string pack = plan::PackName(scale, high_quality);
    work_ = PacksDir() / ".build" / fs::path(std::u8string(pack.begin(), pack.end()));
    replace_ = PacksDir() / fs::path(std::u8string(pack.begin(), pack.end())) / "4D53082D" / "replace";
    auto [state, message] = BuildOutcome(scale, high_quality, pack);
    std::error_code ec;
    fs::remove_all(work_, ec);
    if (ec) REXLOG_WARN("TEXBUILD could not delete {}: {}", Utf8(work_), ec.message());
    fs::remove(PacksDir() / ".build", ec);  // only when no other pack's work folder is left in it
    Finish(state, std::move(message));
  }

  std::pair<State, std::string> BuildOutcome(uint32_t scale, bool high_quality, const std::string& pack) {
    const auto start = std::chrono::steady_clock::now();
    try {
      Build(scale, high_quality);
      REXLOG_INFO("TEXBUILD {} done: {} textures in {:.0f} s", pack, Get().done, SecondsSince(start));
      return {State::kDone, pack};
    } catch (const Cancelled&) {
      REXLOG_INFO("TEXBUILD {} cancelled after {:.0f} s", pack, SecondsSince(start));
      return {State::kCancelled, {}};
    } catch (const Failure& failure) {
      REXLOG_WARN("TEXBUILD {} failed: {}", pack, failure.message);
      return {State::kFailed, failure.message};
    } catch (const std::exception& e) {
      REXLOG_WARN("TEXBUILD {} failed: {}", pack, e.what());
      return {State::kFailed, e.what()};
    }
  }

  void Finish(State state, std::string message) {
    std::lock_guard lock(mutex_);
    snapshot_.state = state;
    snapshot_.message = std::move(message);
    ++snapshot_.finished;
  }

  void Build(uint32_t scale, bool high_quality) {
    const auto jobs = PlanJobs(scale, high_quality);
    { std::lock_guard lock(mutex_); snapshot_.total = static_cast<uint32_t>(jobs.size()); }
    // Created even with nothing to upscale, so the Done message always names a pack that exists.
    std::error_code ec;
    fs::create_directories(replace_, ec);
    if (ec) throw Failure{"could not create " + Utf8(replace_) + ": " + ec.message()};
    if (jobs.empty()) return;
    esrgan_device_ = EsrganDevice();
    CreateJob();
    for (size_t first = 0; first < jobs.size(); first += kBatchSize) {
      const size_t last = (std::min)(jobs.size(), first + kBatchSize);
      RunBatch(std::vector<Job>(jobs.begin() + static_cast<std::ptrdiff_t>(first),
                                jobs.begin() + static_cast<std::ptrdiff_t>(last)));
    }
  }

  // Outputs that already exist are skipped, so a build resumes and a later one adds only new dumps.
  std::vector<Job> PlanJobs(uint32_t scale, bool high_quality) {
    const auto names = DumpNames();
    if (names.empty()) throw Failure{"no dumped textures in " + Utf8(DumpDir())};
    std::vector<Job> jobs;
    size_t existing = 0, skipped = 0, normal_maps = 0, grayscale = 0;
    for (const auto& name : names) {
      if (cancel_) throw Cancelled{};
      const auto dump = *plan::ParseDumpName(name);
      if (IsFile(replace_ / name)) { ++existing; continue; }
      const auto content =
          plan::IsColorFormat(dump.format) ? Classify(DumpDir() / name, dump) : plan::Content::kColor;
      const auto planned = plan::PlanTexture(dump, content, scale, high_quality);
      if (planned.step == plan::Step::kUpscale) jobs.push_back({name, dump, planned});
      else if (content == plan::Content::kNormalMap) ++normal_maps;
      else if (content == plan::Content::kGrayscale) ++grayscale;
      else ++skipped;
    }
    REXLOG_INFO("TEXBUILD {} dumps: {} to upscale, {} already built, {} normal maps, {} grayscale and {} small, "
                "noise or mask textures keep the disc version",
                names.size(), jobs.size(), existing, normal_maps, grayscale, skipped);
    return jobs;
  }

  void RunBatch(const std::vector<Job>& jobs) {
    const auto start = std::chrono::steady_clock::now();
    for (const char* dir : {"in", "png", "png4", "out"}) ClearDir(work_ / dir);
    for (const auto& job : jobs) {
      fs::copy_file(DumpDir() / job.file_name, work_ / "in" / job.file_name, fs::copy_options::overwrite_existing);
    }
    // texconv expands the wildcard itself, so the command line stays short.
    // --ignore-srgb on every texconv call: read as sRGB, the PNGs come back linear and the pack renders dark.
    RunTool(TexconvExe(), {"-nologo", "-y", "--ignore-srgb", "-ft", "png", "-o", "png", "in\\*.dds"}, kTexconvTimeoutMs,
            "texconv");
    RunEsrgan(jobs);
    for (const auto& job : jobs) EncodeOne(job);
    REXLOG_INFO("TEXBUILD batch of {} in {:.0f} s", jobs.size(), SecondsSince(start));
  }

  void RunEsrgan(const std::vector<Job>& jobs) {
    std::vector<std::string> args = {"-i", "png", "-o", "png4", "-n", kEsrganModel, "-s", "4", "-f", "png",
                                     "-m", Utf8(EsrganModels())};
    if (esrgan_device_) args.insert(args.end(), {"-g", std::to_string(*esrgan_device_)});
    RunTool(EsrganExe(), std::move(args), kEsrganTimeoutMs, "realesrgan-ncnn-vulkan");
    CheckEsrganLog();
    for (const auto& job : jobs) {
      const std::string png = StemOf(job) + ".png";
      if (!IsFile(work_ / "png4" / png)) throw Failure{"realesrgan-ncnn-vulkan did not write png4\\" + png};
    }
  }

  // ncnn can report a failed Vulkan call and still exit 0.
  void CheckEsrganLog() {
    const auto bytes = ReadBytes(LogPath());
    const std::string_view log(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    if (const auto line = plan::VulkanFailureLine(log)) {
      throw Failure{"realesrgan-ncnn-vulkan reported '" + *line + "': the tile may not fit in video memory"};
    }
  }

  static std::string StemOf(const Job& job) { return job.file_name.substr(0, job.file_name.size() - 4); }

  fs::path LogPath() const { return work_ / "tools.log"; }

  // Written to out\ first and moved in whole, so a cancelled build never leaves a half-written DDS behind.
  void EncodeOne(const Job& job) {
    // -sepalpha: the game uses alpha as a mask (a door's paint wear), and alpha-weighted resizing and mips turn
    // the color under low alpha black (measured mean RGB 60,64,79 -> 36,41,54).
    std::vector<std::string> args = {"-nologo", "-y", "--ignore-srgb", "-sepalpha", "-w", std::to_string(job.plan.width), "-h",
                                     std::to_string(job.plan.height), "-f", job.plan.dxgi, "-m", "0", "-o", "out"};
    // texconv has no Lanczos; cubic keeps the downsized 4x output sharper than the default filter.
    if (job.plan.width != job.dump.width * 4 || job.plan.height != job.dump.height * 4) {
      args.insert(args.end(), {"-if", "CUBIC"});
    }
    args.push_back("png4\\" + StemOf(job) + ".png");
    RunTool(TexconvExe(), std::move(args), kTexconvTimeoutMs, "texconv");
    std::error_code ec;
    fs::rename(work_ / "out" / job.file_name, replace_ / job.file_name, ec);
    if (ec) throw Failure{"could not move " + job.file_name + " into the pack: " + ec.message()};
    std::lock_guard lock(mutex_);
    ++snapshot_.done;
  }

  // A job object takes the children down with the game if it crashes.
  void CreateJob() {
    if (job_) return;
    job_ = CreateJobObjectW(nullptr, nullptr);
    if (!job_) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
  }

  PROCESS_INFORMATION Launch(const fs::path& exe, std::vector<std::string> args, std::string_view tool) {
    args.insert(args.begin(), Utf8(exe));
    std::wstring command = Widen(plan::CommandLine(args));
    const std::wstring dir = work_.wstring();
    const HANDLE log = OpenLog(tool);
    PROCESS_INFORMATION process{};
    const bool started = CreateWithLog(exe, command, dir, log, process);
    const DWORD error = GetLastError();
    CloseHandle(log);
    if (!started) throw Failure{"could not start " + std::string(tool) + " (error " + std::to_string(error) + ")"};
    if (job_) AssignProcessToJobObject(job_, process.hProcess);
    ResumeThread(process.hThread);
    CloseHandle(process.hThread);
    return process;
  }

  // Truncated per child, so the scan after realesrgan sees only its own output.
  HANDLE OpenLog(std::string_view tool) {
    SECURITY_ATTRIBUTES inherit{sizeof(inherit), nullptr, TRUE};
    const HANDLE log = CreateFileW(LogPath().c_str(), GENERIC_WRITE, FILE_SHARE_READ, &inherit, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log == INVALID_HANDLE_VALUE) {
      throw Failure{"could not create the log for " + std::string(tool) + " (error " +
                    std::to_string(GetLastError()) + ")"};
    }
    return log;
  }

  // The handle list keeps every other inheritable handle of the game out of the child.
  static bool CreateWithLog(const fs::path& exe, std::wstring& command, const std::wstring& dir, HANDLE log,
                            PROCESS_INFORMATION& process) {
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    std::vector<std::byte> buffer(size);
    auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(buffer.data());
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &size)) return false;
    HANDLE inherited = log;
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdOutput = log;
    startup.StartupInfo.hStdError = log;
    startup.lpAttributeList = attributes;
    const bool started =
        UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, &inherited, sizeof(inherited),
                                  nullptr, nullptr) &&
        CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
                       CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr, dir.c_str(),
                       &startup.StartupInfo, &process);
    const DWORD error = GetLastError();
    DeleteProcThreadAttributeList(attributes);
    SetLastError(error);
    return started;
  }

  void RunTool(const fs::path& exe, std::vector<std::string> args, DWORD timeout_ms, std::string_view tool) {
    const PROCESS_INFORMATION process = Launch(exe, std::move(args), tool);
    {
      std::lock_guard lock(mutex_);
      child_ = process.hProcess;
      if (cancel_) TerminateProcess(child_, 1);
    }
    const DWORD wait = WaitForSingleObject(process.hProcess, timeout_ms);
    if (wait == WAIT_TIMEOUT) {
      TerminateProcess(process.hProcess, 1);
      WaitForSingleObject(process.hProcess, 5000);
    }
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    { std::lock_guard lock(mutex_); child_ = nullptr; }
    CloseHandle(process.hProcess);
    if (cancel_) throw Cancelled{};
    if (wait == WAIT_TIMEOUT) throw Failure{std::string(tool) + " timed out"};
    if (code != 0) throw Failure{std::string(tool) + " exited with code " + std::to_string(code)};
  }

  std::mutex mutex_;
  Snapshot snapshot_;
  std::thread thread_;
  std::atomic<bool> cancel_{false};
  HANDLE child_ = nullptr;
  HANDLE job_ = nullptr;
  fs::path work_;
  fs::path replace_;
  std::optional<uint32_t> esrgan_device_;
};

Builder& TheBuilder() {
  static Builder builder;
  return builder;
}

}  // namespace

bool ToolsPresent() {
  return IsFile(TexconvExe()) && IsFile(EsrganExe()) && IsFile(EsrganDir() / "vcomp140.dll") &&
         IsFile(EsrganModels() / (std::string(kEsrganModel) + ".bin")) &&
         IsFile(EsrganModels() / (std::string(kEsrganModel) + ".param"));
}

size_t DumpCount() {
  static std::mutex mutex;
  static size_t count = 0;
  static std::chrono::steady_clock::time_point scanned{};
  std::lock_guard lock(mutex);
  const auto now = std::chrono::steady_clock::now();
  if (scanned == std::chrono::steady_clock::time_point{} || now - scanned > std::chrono::seconds(3)) {
    count = DumpNames().size();
    scanned = now;
  }
  return count;
}

bool Start(uint32_t scale, bool high_quality) {
  if (!ToolsPresent()) return false;
  REXLOG_INFO("TEXBUILD start {}", plan::PackName(scale, high_quality));
  return TheBuilder().Start(scale, high_quality);
}

void Cancel() { TheBuilder().Cancel(); }

Snapshot GetSnapshot() { return TheBuilder().Get(); }

void Shutdown() { TheBuilder().Shutdown(); }

}  // namespace rex::glue::texture_pack_builder

#else

// The bundled tools are Windows programs and only Direct3D 12 loads packs: elsewhere there is nothing to build.
namespace rex::glue::texture_pack_builder {

bool ToolsPresent() { return false; }
size_t DumpCount() { return 0; }
bool Start(uint32_t, bool) { return false; }
void Cancel() {}
Snapshot GetSnapshot() { return {}; }
void Shutdown() {}

}  // namespace rex::glue::texture_pack_builder

#endif
