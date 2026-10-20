// gow2 - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <random>
#include <string>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/rex_app.h>
#include <rex/system.h>

#include "gow2_coalesced.h"
#include "gow2_audio_volume.h"
#include "gow2_native_keys.h"
#include "gow2_prompt_swap.h"
#include "gow2_coop_network.h"
#include "gow2_crash_watch.h"
#include "gow2_frame_stats.h"
#include "gow2_gpu_pick.h"
#include "gow2_machine_xuid.h"
#include "gow2_mouse_aim.h"
#include "gow2_options_actions.h"
#include "gow2_options_overlay.h"
#include "gow2_save_dir.h"
#include "gow2_sha_table.h"
#include "gow2_utf8_guard.h"
#include "gow2_xam_input_guard.h"

#if defined(__ANDROID__)
#include "gow2_android_devices.h"
#include "gow2_android_power.h"
#include "gow2_touch_controls.h"
#endif

#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#endif

class Gow2App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Gow2App>(new Gow2App(ctx, "gow2",
        PPCImageConfig));
  }

  // A folder with only default.xex (like the repo's codegen input) boots to a black screen.
  static bool IsDiscFolder(const std::filesystem::path& dir) {
    std::error_code ec;
    return std::filesystem::exists(dir / "default.xex", ec) &&
           std::filesystem::is_directory(dir / "GearGame", ec);
  }

  static std::filesystem::path WithXex(const std::filesystem::path& dir) {
    std::error_code ec;
    if (dir.empty()) return {};
    if (IsDiscFolder(dir)) return dir;
    for (std::filesystem::directory_iterator it(
             dir, std::filesystem::directory_options::skip_permission_denied, ec);
         !ec && it != std::filesystem::directory_iterator(); ++it) {
      std::error_code e2;
      if (!it->is_directory(e2)) continue;
      if (IsDiscFolder(it->path())) return it->path();
    }
    return {};
  }

  static std::filesystem::path AppHome() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH * 2];
    const DWORD n = GetEnvironmentVariableW(L"GOW2_HOME", buf, MAX_PATH * 2);
    if (n && n < MAX_PATH * 2) return std::filesystem::path(buf);
#endif
    return rex::filesystem::GetExecutablePath().parent_path();
  }

  static std::filesystem::path RememberedGamePathFile() {
    return AppHome() / "gow2_game_path.txt";
  }

  // One UTF-8 line with a folder path, or empty when missing/unreadable.
  static std::filesystem::path ReadRememberedGamePath() {
    std::ifstream in(RememberedGamePathFile(), std::ios::binary);
    if (!in) return {};
    std::string line;
    if (!std::getline(in, line)) return {};
    while (!line.empty() &&
           (line.back() == '\n' || line.back() == '\r' || line.back() == ' ' ||
            line.back() == '\t')) {
      line.pop_back();
    }
    const auto first = line.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    line.erase(0, first);
    if (line.empty()) return {};
#ifdef _WIN32
    int wide = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, nullptr, 0);
    if (wide <= 0) return {};
    std::wstring out(static_cast<size_t>(wide - 1), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, out.data(), wide)) {
      return {};
    }
    return std::filesystem::path(out);
#else
    return std::filesystem::path(line);
#endif
  }

  static void WriteRememberedGamePath(const std::filesystem::path& dir) {
#ifdef _WIN32
    const std::wstring wide = dir.wstring();
    int utf8 = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0,
                                   nullptr, nullptr);
    if (utf8 <= 0) return;
    std::string out(static_cast<size_t>(utf8 - 1), '\0');
    if (!WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, out.data(), utf8,
                             nullptr, nullptr)) {
      return;
    }
    std::ofstream(RememberedGamePathFile(),
                  std::ios::binary | std::ios::trunc)
        << out << '\n';
#else
    std::ofstream(RememberedGamePathFile(),
                  std::ios::binary | std::ios::trunc)
        << dir.string() << '\n';
#endif
  }

#ifdef _WIN32
  // Standard Windows folder picker. Returns empty when cancelled.
  static std::filesystem::path PickGameFolder() {
    const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    std::filesystem::path result;
    IFileOpenDialog* dlg = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&dlg)))) {
      DWORD opts = 0;
      dlg->GetOptions(&opts);
      dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
      dlg->SetTitle(L"Select your Gears of War 2 folder (the one with default.xex)");
      IShellItem* item = nullptr;
      if (SUCCEEDED(dlg->Show(nullptr)) && SUCCEEDED(dlg->GetResult(&item))) {
        PWSTR path = nullptr;
        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
          result = path;
          CoTaskMemFree(path);
        }
        item->Release();
      }
      dlg->Release();
    }
    if (SUCCEEDED(co)) CoUninitialize();
    return result;
  }

  // Asks once. On a valid pick the choice is remembered and used; otherwise
  // explains the requirement and returns the fallback for a clean runtime failure.
  static std::filesystem::path PromptAndRememberGameFolder(
      const std::filesystem::path& fallback) {
    const std::filesystem::path picked = PickGameFolder();
    if (auto hit = WithXex(picked); !hit.empty()) {
      WriteRememberedGamePath(hit);
      REXLOG_INFO("gow2 content root chosen by the player: {}", hit.string());
      return hit;
    }
    MessageBoxW(nullptr,
                L"Gears of War 2 needs your own game files to run.\n\n"
                L"The selected folder must contain default.xex and GearGame "
                L"from your own retail disc.",
                L"Gears of War 2", MB_OK | MB_ICONWARNING);
    return fallback;
  }
#endif

  static std::filesystem::path ResolveContentRoot(const std::filesystem::path& given) {
    std::error_code ec;
    if (!given.empty() && std::filesystem::exists(given, ec)) {
      if (auto hit = WithXex(given); !hit.empty()) return hit;
    }
    if (const auto remembered = ReadRememberedGamePath(); !remembered.empty()) {
      ec.clear();
      if (std::filesystem::exists(remembered, ec)) {
        if (auto hit = WithXex(remembered); !hit.empty()) {
          if (hit != given) {
            REXLOG_INFO("gow2 content root from gow2_game_path.txt: {}",
                        hit.string());
          }
          return hit;
        }
      }
    }
    std::filesystem::path dir = AppHome();
    for (int depth = 0; !dir.empty(); ++depth) {
      if (auto hit = WithXex(dir); !hit.empty()) {
        if (hit != given) REXLOG_INFO("gow2 found content root {}", hit.string());
        return hit;
      }
      if (depth >= 4) break;
      auto parent = dir.parent_path();
      if (parent == dir) break;
      dir = parent;
    }
#ifdef _WIN32
    return PromptAndRememberGameFolder(given);
#else
    return given;
#endif
  }

  static bool IsSetByPlayer(const char* name) {
    return rex::cvar::GetFlagSource(name) != rex::cvar::Source::kDefault;
  }

  static bool SetIfUnset(const char* name, const std::string& value) {
    if (IsSetByPlayer(name)) return false;
    return rex::cvar::SetFlagByName(name, value);
  }

  // An explicit d3d12_adapter in gow2.toml or on the command line wins over gow2_gpu.
  // Off Windows ChooseRenderGpu picks nothing (index -1), so this stays a no-op there.
  static void PickRenderGpu() {
    const rex::glue::GpuChoice gpu = rex::glue::ChooseRenderGpu();
    if (gpu.index >= 0 && SetIfUnset("d3d12_adapter", std::to_string(gpu.index))) {
      REXLOG_INFO("gow2 d3d12_adapter -> {} ({}; {})", gpu.index, gpu.name, gpu.reason);
    }
    // Internal resolution follows the GPU unless the player set resolution_scale:
    // 720p on an integrated GPU, 1440p on a dedicated one.
    if (gpu.index >= 0 && SetIfUnset("resolution_scale", gpu.integrated ? "1" : "2")) {
      REXLOG_INFO("gow2 resolution_scale -> {} ({}, set resolution_scale in gow2.toml to change)",
                  gpu.integrated ? 1 : 2, gpu.integrated ? "integrated GPU" : "dedicated GPU");
    }
    // Anisotropic filtering follows the GPU unless the player set anisotropic_override:
    // 2x on an integrated GPU, 8x on a dedicated one.
    if (gpu.index >= 0 && SetIfUnset("anisotropic_override", gpu.integrated ? "2" : "4")) {
      REXLOG_INFO("gow2 anisotropic_override -> {} ({}, set anisotropic_override in gow2.toml to change)",
                  gpu.integrated ? 2 : 4, gpu.integrated ? "integrated GPU" : "dedicated GPU");
    }
    // Re-uploading every CPU-written page each frame costs ~180 MB/s of guest uploads; on an
    // integrated GPU that bandwidth is shared with the CPU (measured: -98 % uploads, +4 % fps).
    if (gpu.index >= 0 && gpu.integrated && SetIfUnset("clear_memory_page_state", "false")) {
      REXLOG_INFO("gow2 clear_memory_page_state -> false (integrated GPU, set it in gow2.toml to change)");
    }
  }

  // Coalesced tweaks (mouse look, key prompts) work on a cached copy of the player's own bundles.
  // Runs from OnPreSetup: OnConfigurePaths comes before gow2.toml is loaded and before logging,
  // and the overrides must be live before the runtime mounts the game folder.
  void PrepareCoalesced() {
    rex::glue::CoalescedOptions options = rex::glue::CoalescedOptionsFromCvars();
    rex::glue::RememberBakedBindings(options.native_bindings);
    const rex::glue::CoalescedResult coalesced =
        rex::glue::PrepareCoalescedOverrides(game_data_root_, cache_root_, options);
#ifdef _WIN32
    _putenv_s("GOW2_VFS_OVERRIDES", coalesced.overrides.c_str());
#else
    setenv("GOW2_VFS_OVERRIDES", coalesced.overrides.c_str(), 1);
#endif
    if (!coalesced.summary.empty()) REXLOG_INFO("{}", coalesced.summary);
    coalesced_hashes_ = coalesced.hashes;
  }

  // The temporal upscalers scale the finished frame to the screen size and skip every frame while
  // it is unknown. GOW2_TARGET_W/H set by the player win.
  static void SetUpscaleTarget() {
#ifdef _WIN32
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    const bool have = EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm) && dm.dmPelsHeight;
    if (!std::getenv("GOW2_TARGET_W"))
      _putenv_s("GOW2_TARGET_W", std::to_string(have ? dm.dmPelsWidth : 1920).c_str());
    if (!std::getenv("GOW2_TARGET_H"))
      _putenv_s("GOW2_TARGET_H", std::to_string(have ? dm.dmPelsHeight : 1080).c_str());
#endif
  }

  // The game hash-checks the Coalesced bundles; the edited copies' hashes go into its table once the
  // XEX is in memory and before any game code runs.
  void OnPostLoadXexImage() override { rex::glue::PatchHashSec(coalesced_hashes_); }

#if defined(__ANDROID__)
  // A device found at launch becomes this launch's default value, so the settings overlay's
  // "Save to config" does not write it to gow2.toml for a later launch without that device.
  static void SetLaunchDefault(const char* name, const std::string& value) {
    if (rex::cvar::GetFlagSource(name) != rex::cvar::Source::kDefault) return;
    if (!rex::cvar::SetFlagByName(name, value)) return;
    for (rex::cvar::FlagEntry& entry : rex::cvar::GetRegistry()) {
      if (entry.name != name) continue;
      entry.default_value = entry.getter();
      entry.source = rex::cvar::Source::kDefault;
      return;
    }
  }

  // On a phone the keyboard/mouse driver runs only when a keyboard or mouse is attached at launch;
  // a mouse also turns the camera (through the native mouse path when gow2_mouse_aim allows it).
  static void UseAttachedKeyboardAndMouse() {
    const bool keyboard = rex::glue::PhysicalKeyboardConnected();
    const bool mouse = rex::glue::ExternalMouseConnected();
    if (keyboard || mouse) SetLaunchDefault("mnk_mode", "true");
    if (mouse) SetLaunchDefault("mnk_mouse", "true");
    REXLOG_INFO("INPUTDEV keyboard {}, mouse {} -> mnk_mode {}, mnk_mouse {}",
                keyboard ? "connected" : "not connected", mouse ? "connected" : "not connected",
                rex::cvar::Query<bool>("mnk_mode") ? "on" : "off",
                rex::cvar::Query<bool>("mnk_mouse") ? "on" : "off");
  }

  void AddTouchControls(rex::RuntimeConfig& config) {
    touch_ = rex::glue::CreateTouchControls(rex::glue::TouchscreenPresent());
    rex::glue::AddTouchDriver(config, touch_);
  }
#endif

  void OnPreSetup(rex::RuntimeConfig& config) override {
    rex::glue::options_actions::WaitForPreviousInstance();
#if defined(__ANDROID__)
    UseAttachedKeyboardAndMouse();
    AddTouchControls(config);
    const bool unlock_fps = rex::cvar::Query<bool>("gow2_unlock_fps");
    if (unlock_fps) SetLaunchDefault("present_android_frame_rate", "0");
    // Each occlusion query ends the render pass twice; off cut GPU work 6-12 % with no visible change.
    SetIfUnset("occlusion_query_enable", "false");
    REXLOG_INFO("DISPLAY frame rate hint {} Hz (gow2_unlock_fps {})",
                rex::cvar::Query<int32_t>("present_android_frame_rate"), unlock_fps);
#else
    SetIfUnset("mnk_mode", "true");
#endif
    rex::glue::PrepareNativeMouseAim();
    PrepareCoalesced();
#ifdef _WIN32
    rex::glue::PrepareKeycapPack();
#endif
    PickRenderGpu();
    SetUpscaleTarget();
    rex::glue::EnableSystemLinkForCoop();
    config.gpu_plugin = "xenos";
    // Guest content names are raw bytes; the runtime's UTF-8 decoder throws on a bad lead byte.
    rex::glue::InstallUtf8Guard();
    rex::glue::InstallXamInputGuard();
    rex::glue::InstallDiscErrorSilencer();
    // Last: it installs the unhandled-exception filter; the others only observe first-chance events.
    rex::glue::InstallCrashWatch();
    ConfigurePcOptionsEntry();
  }

  static void ConfigurePcOptionsEntry() {
    const bool enabled = rex::glue::HostUiHooksAvailable() &&
                         rex::cvar::Query<bool>("gow2_pc_options_entry");
    if (enabled) SetIfUnset("xam_device_selector_ui_command", "gow2_pc_options_modal");
    const bool active = enabled && rex::cvar::GetFlagByName("xam_device_selector_ui_command") ==
                                      "gow2_pc_options_modal";
    REXLOG_INFO("PCOPTIONS Options-menu entry {}", active ? "on" : "off");
  }

  void OnConfigurePaths(rex::PathConfig& paths) override {
    // Find the disc folder next to the exe so a double-click boots without --game_data_root.
    paths.game_data_root = ResolveContentRoot(paths.game_data_root);
    paths.update_data_root = paths.game_data_root / "$SystemUpdate";
#ifdef _WIN32
    // An explicit --user_data_root (cache then defaults to its cache folder) or --cache_root wins:
    // one PC can run several instances, each with its own saves, profile and shader cache.
    if (wchar_t local[MAX_PATH]{}; GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH)) {
      const std::filesystem::path base = std::filesystem::path(local) / L"GearsOfWar2PC";
      const bool own_user_root = IsSetByPlayer("user_data_root");
      if (!own_user_root) paths.user_data_root = base / "user";
      if (!own_user_root && !IsSetByPlayer("cache_root")) paths.cache_root = base / "cache";
      std::error_code ec;
      std::filesystem::create_directories(paths.cache_root, ec);
      std::filesystem::create_directories(paths.user_data_root, ec);
    }
#endif
    game_data_root_ = paths.game_data_root;
    cache_root_ = paths.cache_root;
    options_paths_.config_path = paths.config_path;
  }

  static void ReportSaveDir(const rex::glue::SaveDirChoice& saves) {
    if (saves.summary.empty()) return;
    if (saves.is_warning) {
      REXLOG_WARN("{}", saves.summary);
      rex::ShowSimpleMessageBox(rex::SimpleMessageBoxType::Warning, saves.summary);
      return;
    }
    REXLOG_INFO("{}", saves.summary);
  }

  // The command line outranks gow2.toml: a test instance's --user_data_root keeps its own saves
  // even when gow2.toml points gow2_save_dir at the player's cloud folder.
  static bool UserDataRootOutranksSaveDir() {
    return rex::cvar::GetFlagSource("user_data_root") > rex::cvar::GetFlagSource("gow2_save_dir");
  }

  static void ReportSaveDirOutranked(const std::filesystem::path& user_data_root) {
    if (rex::glue::SaveDirSetting().empty()) return;
    const std::u8string root = user_data_root.u8string();
    REXLOG_INFO("gow2_save_dir ignored: --user_data_root keeps saves, profile settings and "
                "achievements in {}",
                std::string(root.begin(), root.end()));
  }

  // gow2.toml is loaded by now (OnConfigurePaths runs before it), and the runtime is not built yet.
  std::optional<rex::PathConfig> OnFinalizePaths(const rex::PathConfig& defaults,
                                                 std::function<void(rex::PathConfig)> resume) override {
    (void)resume;
    rex::PathConfig paths = defaults;
    if (UserDataRootOutranksSaveDir()) {
      ReportSaveDirOutranked(paths.user_data_root);
      options_paths_ = {paths.config_path, paths.user_data_root};
      rex::glue::options::UpdatePcOptionsPaths(options_paths_);
      return paths;
    }
    const rex::glue::SaveDirChoice saves = rex::glue::ChooseUserDataRoot(
        rex::glue::SaveDirSetting(), paths.user_data_root, paths.cache_root,
        paths.config_path.parent_path());
    ReportSaveDir(saves);
    PinMachineXuid(paths.user_data_root, saves.user_data_root != paths.user_data_root);
    paths.user_data_root = saves.user_data_root;
    options_paths_ = {paths.config_path, paths.user_data_root};
    rex::glue::options::UpdatePcOptionsPaths(options_paths_);
    return paths;
  }

  static uint64_t RandomXuidBits() {
    std::random_device device;
    return (uint64_t{device()} << 32) | device();
  }

  static std::string PathUtf8(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
  }

  // Patch 0010 keeps the System Link XUID in the user data folder, which gow2_save_dir may point at
  // a folder several PCs sync: the XUID stays in this PC's default folder and reaches 0010 as
  // user_xuid, so two PCs never join one session under the same XUID.
  static void PinMachineXuid(const std::filesystem::path& machine_root, bool user_root_moved) {
    const bool link_active = rex::cvar::GetFlagByName("net_link_active") == "true";
    const bool xuid_set = !rex::cvar::GetFlagByName("user_xuid").empty();
    if (!rex::glue::ShouldPinMachineXuid(user_root_moved, link_active, xuid_set)) return;
    const rex::glue::MachineXuid machine =
        rex::glue::LoadOrCreateMachineXuid(machine_root, RandomXuidBits);
    const std::string file = PathUtf8(machine_root / rex::glue::kMachineXuidFileName);
    if (!rex::cvar::SetFlagByName("user_xuid", machine.xuid)) {
      REXLOG_WARN("Could not set user_xuid to this PC's System Link XUID {} ({})", machine.xuid, file);
      return;
    }
    REXLOG_INFO("System Link XUID {} {} {}: it stays on this PC, gow2_save_dir moves only the saves",
                machine.xuid, machine.created ? "made and kept in" : "read from", file);
    if (!machine.saved) {
      REXLOG_WARN("Could not write {}: the System Link XUID {} lasts this run only", file, machine.xuid);
    }
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  // void OnLoadXexImage(std::string& xex_image) override {}
  void OnPostSetup() override {
    rex::glue::InstallNativeMouseAim(window());
    rex::glue::InstallNativeKeys(window());
    rex::glue::InstallAudioVolume();
    rex::glue::InstallFrameStats();
#ifdef _WIN32
    rex::glue::InstallPromptSwap(game_data_root_);
#endif
#if defined(__ANDROID__)
    rex::glue::ApplySustainedPerformanceMode();
#endif
  }
  void OnConfigureFonts(ImFontAtlas* atlas) override {
    rex::glue::options::AddOptionsFonts(atlas);
  }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
#if defined(__ANDROID__)
    touch_overlay_ = rex::glue::CreateTouchOverlay(drawer, touch_);
    const bool touch_controls = touch_ != nullptr;
#else
    const bool touch_controls = false;
#endif
    options_overlay_ = rex::glue::options::CreateOptionsOverlay(drawer, options_paths_, touch_controls);
  }

  static void CloseHostPortsBeforeExit() {
    auto& host = rex::glue::options_actions::Host();
    if (!rex::cvar::Query<bool>("gow2_host_close_on_exit")) return;
    host.StopBlocking(15000);
  }

  bool OnWindowCloseRequested() override {
    CloseHostPortsBeforeExit();
    return true;
  }

  void OnShutdown() override {
    rex::glue::options::ClosePcOptionsForShutdown();
    CloseHostPortsBeforeExit();
    options_overlay_.reset();
#if defined(__ANDROID__)
    touch_overlay_.reset();
#endif
  }
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}

 private:
  rex::glue::options::GamePaths options_paths_;
  std::unique_ptr<rex::ui::ImGuiDialog> options_overlay_;
  std::filesystem::path game_data_root_;
  std::filesystem::path cache_root_;
  std::vector<rex::glue::HashSecEntry> coalesced_hashes_;
#if defined(__ANDROID__)
  std::shared_ptr<rex::glue::TouchControls> touch_;
  std::unique_ptr<rex::ui::ImGuiDialog> touch_overlay_;
#endif
};
