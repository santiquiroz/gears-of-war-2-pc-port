// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// A game-style PC Options menu sharing the runtime's ImGui context.
#include "gow2_options_overlay.h"
#include "gow2_host_ui.h"
#include "gow2_options_actions.h"
#include "gow2_options_input.h"
#ifdef _WIN32
#include "gow2_update_check.h"
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <imgui.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/virtual_key.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context.h>

namespace rex::glue::options {
namespace {

constexpr ImU32 kRed = IM_COL32(179, 25, 27, 255);
constexpr ImU32 kWhite = IM_COL32(242, 242, 242, 255);
constexpr ImU32 kGray = IM_COL32(200, 200, 200, 255);
constexpr ImU32 kAmber = IM_COL32(224, 160, 48, 255);
constexpr ImU32 kGreen = IM_COL32(108, 192, 112, 255);
constexpr ImU32 kBlue = IM_COL32(106, 168, 232, 255);

TextId PageTitle(Page page) {
  if (page == Page::kHost) return TextId::kPageHost;
  if (page == Page::kPadButtons) return TextId::kPagePadButtons;
  return TextId::kPageKeybinds;
}
ImFont* g_regular = nullptr;
ImFont* g_bold = nullptr;
std::mutex g_modal_mutex;
std::condition_variable g_modal_cv;
bool g_exists = false;
bool g_shutdown = false;
uint64_t g_closed = 0;
uint64_t g_generation = 0;

bool DrawButton(const char* label, ImVec2 size, bool primary = false) {
  ImGui::PushStyleColor(ImGuiCol_Button,primary ? ImGui::ColorConvertU32ToFloat4(kRed) : ImVec4(1.f,1.f,1.f,.08f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,primary ? ImGui::ColorConvertU32ToFloat4(IM_COL32(205,40,42,255)) : ImVec4(1.f,1.f,1.f,.14f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,primary ? ImGui::ColorConvertU32ToFloat4(IM_COL32(155,20,22,255)) : ImVec4(1.f,1.f,1.f,.20f));
  ImGui::PushStyleColor(ImGuiCol_Text,IM_COL32_WHITE);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,4.f);
  const bool clicked = ImGui::Button(label,size);
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(4);
  return clicked;
}

int64_t NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

PadSample CurrentPad() {
  const auto pad = ReadHostPad();
  if (!pad.connected) return {};
  return {pad.buttons, pad.lx, pad.ly};
}

bool HostInputsReleased(const ImGuiIO& io) {
  const auto pad = ReadHostPad();
  if (pad.buttons || std::abs(pad.lx) > .25f || std::abs(pad.ly) > .25f) return false;
  if (std::abs(pad.rx) > .25f || std::abs(pad.ry) > .25f) return false;
  if (pad.lt >= 30 || pad.rt >= 30) return false;
  for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key) {
    if (ImGui::IsKeyDown(static_cast<ImGuiKey>(key))) return false;
  }
  for (bool down : io.MouseDown) {
    if (down) return false;
  }
  return true;
}

void NotifyUi(bool shown) {
  auto* runtime = rex::Runtime::instance();
  if (runtime && runtime->kernel_state()) runtime->kernel_state()->BroadcastNotification(0x9, shown ? 1 : 0);
}

void NotifyClosed() {
  { std::lock_guard lock(g_modal_mutex); ++g_closed; }
  g_modal_cv.notify_all();
}

void ReleaseNotificationLater(uint64_t generation) {
  std::thread([generation] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::lock_guard lock(g_modal_mutex);
    if (!g_shutdown && generation == g_generation && !HostUiOpen()) NotifyUi(false);
  }).detach();
}

const char* SourceName(OpenSource source) {
  switch (source) {
    case OpenSource::kShortcut: return "shortcut";
    case OpenSource::kChord: return "chord";
    case OpenSource::kGear: return "gear";
    case OpenSource::kGameMenu: return "game-menu";
  }
  return "shortcut";
}

void PushFont(float size, bool bold = false) {
  auto* font = bold ? g_bold : g_regular;
  ImGui::PushFont(font ? font : ImGui::GetFont(), size);
}

void DrawText(ImVec2 pos, std::string_view text, float size, ImU32 color, bool bold = false) {
  PushFont(size, bold);
  ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), size, pos, color,
                                    text.data(), text.data() + text.size());
  ImGui::PopFont();
}

float TextWidth(std::string_view text, float size, bool bold = false) {
  PushFont(size, bold);
  const float width = ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
  ImGui::PopFont();
  return width;
}

std::string ElideMiddle(std::string_view text, float width, float font) {
  if (TextWidth(text,font) <= width) return std::string(text);
  if (TextWidth("...",font) > width) return {};
  std::vector<size_t> characters;
  for (size_t i=0;i<text.size();++i) {
    if ((static_cast<unsigned char>(text[i]) & 0xc0) != 0x80) characters.push_back(i);
  }
  characters.push_back(text.size());
  for (size_t count=characters.size()-1;count>0;--count) {
    const size_t keep = count-1;
    const size_t left = characters[(keep+1)/2];
    const size_t right = characters[characters.size()-1-keep/2];
    std::string result(text.substr(0,left));
    result += "...";
    result += text.substr(right);
    if (TextWidth(result,font) <= width) return result;
  }
  return "...";
}

std::string Uppercase(std::string_view text) {
  std::string result(text);
  for (char& c : result) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
  for (size_t i=0;i+1<result.size();++i) {
    if (static_cast<unsigned char>(result[i]) != 0xc3) continue;
    const auto tail = static_cast<unsigned char>(result[i+1]);
    constexpr std::array<unsigned char,7> lowercase{0xa1,0xa9,0xad,0xb3,0xba,0xbc,0xb1};
    if (std::find(lowercase.begin(),lowercase.end(),tail) != lowercase.end()) result[i+1] -= 0x20;
  }
  return result;
}

void DeleteLastCharacter(std::vector<char>& buffer) {
  size_t size = std::strlen(buffer.data());
  if (!size) return;
  do { --size; } while (size && (static_cast<unsigned char>(buffer[size]) & 0xc0) == 0x80);
  buffer[size] = 0;
}

void CloseHostPortsBeforeExit() {
  if (!rex::cvar::Query<bool>("gow2_host_close_on_exit")) return;
  auto& host = options_actions::Host();
  host.StopBlocking(15000);
}

void RequestQuit() {
  auto* runtime = rex::Runtime::instance();
  if (!runtime || !runtime->app_context()) return;
  runtime->app_context()->CallInUIThread([] {
    auto* current = rex::Runtime::instance();
    if (current && current->display_window()) current->display_window()->RequestClose();
  });
}

// Key mapping adapted from ReXGlue settings_overlay.cpp (BSD-3-Clause).
rex::ui::VirtualKey KeyToVirtualKey(ImGuiKey key) {
  using VK = rex::ui::VirtualKey;
  if (key >= ImGuiKey_A && key <= ImGuiKey_Z)
    return static_cast<VK>(static_cast<uint16_t>(VK::kA) + key - ImGuiKey_A);
  if (key >= ImGuiKey_0 && key <= ImGuiKey_9)
    return static_cast<VK>(static_cast<uint16_t>(VK::k0) + key - ImGuiKey_0);
  if (key >= ImGuiKey_F1 && key <= ImGuiKey_F24)
    return static_cast<VK>(static_cast<uint16_t>(VK::kF1) + key - ImGuiKey_F1);
  if (key >= ImGuiKey_Keypad0 && key <= ImGuiKey_Keypad9)
    return static_cast<VK>(static_cast<uint16_t>(VK::kNumpad0) + key - ImGuiKey_Keypad0);
  switch (key) {
    case ImGuiKey_Space: return VK::kSpace;
    case ImGuiKey_Enter: case ImGuiKey_KeypadEnter: return VK::kReturn;
    case ImGuiKey_Tab: return VK::kTab;
    case ImGuiKey_Backspace: return VK::kBack;
    case ImGuiKey_Delete: return VK::kDelete;
    case ImGuiKey_Insert: return VK::kInsert;
    case ImGuiKey_Home: return VK::kHome;
    case ImGuiKey_End: return VK::kEnd;
    case ImGuiKey_PageUp: return VK::kPrior;
    case ImGuiKey_PageDown: return VK::kNext;
    case ImGuiKey_LeftArrow: return VK::kLeft;
    case ImGuiKey_RightArrow: return VK::kRight;
    case ImGuiKey_UpArrow: return VK::kUp;
    case ImGuiKey_DownArrow: return VK::kDown;
    case ImGuiKey_LeftShift: case ImGuiKey_RightShift: return VK::kShift;
    case ImGuiKey_LeftCtrl: case ImGuiKey_RightCtrl: return VK::kControl;
    case ImGuiKey_LeftAlt: case ImGuiKey_RightAlt: return VK::kMenu;
    case ImGuiKey_LeftSuper: return VK::kLWin;
    case ImGuiKey_RightSuper: return VK::kRWin;
    case ImGuiKey_Menu: return VK::kApps;
    case ImGuiKey_CapsLock: return VK::kCapital;
    case ImGuiKey_NumLock: return VK::kNumLock;
    case ImGuiKey_ScrollLock: return VK::kScroll;
    case ImGuiKey_PrintScreen: return VK::kSnapshot;
    case ImGuiKey_Pause: return VK::kPause;
    case ImGuiKey_GraveAccent: return VK::kOem3;
    case ImGuiKey_Minus: return VK::kOemMinus;
    case ImGuiKey_Equal: return VK::kOemPlus;
    case ImGuiKey_LeftBracket: return VK::kOem4;
    case ImGuiKey_RightBracket: return VK::kOem6;
    case ImGuiKey_Backslash: return VK::kOem5;
    case ImGuiKey_Semicolon: return VK::kOem1;
    case ImGuiKey_Apostrophe: return VK::kOem7;
    case ImGuiKey_Comma: return VK::kOemComma;
    case ImGuiKey_Period: return VK::kOemPeriod;
    case ImGuiKey_Slash: return VK::kOem2;
    case ImGuiKey_KeypadDecimal: return VK::kDecimal;
    case ImGuiKey_KeypadDivide: return VK::kDivide;
    case ImGuiKey_KeypadMultiply: return VK::kMultiply;
    case ImGuiKey_KeypadSubtract: return VK::kSubtract;
    case ImGuiKey_KeypadAdd: return VK::kAdd;
    default: return VK::kNone;
  }
}

enum class Device { kKeyboard, kMouse, kPad, kTouch };

Device StartingDevice(OpenSource source, bool touch_controls) {
  if (source == OpenSource::kChord) return Device::kPad;
  if (source == OpenSource::kGear) return Device::kTouch;
#if defined(__ANDROID__)
  if (touch_controls) return Device::kTouch;
#endif
  return Device::kKeyboard;
}

class OptionsOverlay;
OptionsOverlay* g_overlay = nullptr;

class OptionsOverlay final : public rex::ui::ImGuiDialog {
 public:
  OptionsOverlay(rex::ui::ImGuiDrawer* drawer, GamePaths paths, bool touch)
      : ImGuiDialog(drawer), values_(std::move(paths)), touch_(touch) {
#ifdef _WIN32
    update::Start(values_.paths().user_data_root);
#endif
    BuildContext(touch_, &context_);
    CaptureLaunch(state_, Catalog(), values_);
    reader_.Reset(CurrentPad());
    rex::ui::RegisterBind("bind_pc_options", "F10", "Toggle the PC Options menu",
                          [] { TogglePcOptions(OpenSource::kShortcut); });
  }

  ~OptionsOverlay() override {
    rex::ui::UnregisterBind("bind_pc_options");
    CloseForShutdown();
    {
      std::lock_guard lock(g_modal_mutex);
      g_exists = false;
      g_shutdown = true;
      if (g_overlay == this) g_overlay = nullptr;
    }
    g_modal_cv.notify_all();
  }

  void OpenMenu(OpenSource source) {
    if (open_) return;
    source_ = source;
    BuildContext(touch_, &context_);
    options::Open(state_, Catalog(), values_, context_.ctx);
    RestoreHostChange();
    releasing_ = false;
    open_ = true;
    focus_ = scroll_selection_ = true;
    device_ = StartingDevice(source, touch_);
    { std::lock_guard lock(g_modal_mutex); ++g_generation; }
    SetHostUiOpen(true);
    rex::cvar::SetFlagByName("xam_input_block", "true");
    if (source != OpenSource::kGameMenu) NotifyUi(true);
    reader_.Reset(CurrentPad());
    REXLOG_INFO("PCOPTIONS open ({})", SourceName(source));
  }

  void Toggle(OpenSource source) {
    if (!open_) { OpenMenu(source); return; }
    Dispatch(Command::kBack);
  }

  void SetPaths(const GamePaths& paths) { values_.set_paths(paths); }

  void CloseMenu() {
    if (!open_) return;
    open_ = false;
    retire_popups_ = true;
    popup_id_.clear();
    releasing_ = true;
    release_deadline_ = NowMs() + 5000;
    reader_.Reset(CurrentPad());
    NotifyClosed();
    REXLOG_INFO("PCOPTIONS close");
  }

  void CloseForShutdown() {
    CloseMenu();
    ReleaseInput(false);
  }

 protected:
#if defined(__ANDROID__)
  bool WantsContinuousPaint() const override { return open_ || releasing_ || toast_until_ > NowMs(); }
#endif
  void OnDraw(ImGuiIO& io) override {
    pad_ = CurrentPad();
    const auto now = NowMs();
    if (reader_.ToggleChord(pad_, now)) Toggle(OpenSource::kChord);
    ObserveHostResult();
    CheckInputRelease(io, now);
    if (!open_) {
      RetirePopups();
      DrawToast(io);
#ifdef _WIN32
      DrawUpdateNotice(io, now);
#endif
      return;
    }
    TrackDevice(io);
    if (popup_id_.empty()) ReadMenuInput(now);
    if (!open_) { RetirePopups(); return; }
    DrawMenu(io);
    DrawToast(io);
#ifdef _WIN32
    DrawUpdateNotice(io, now);
#endif
  }

 private:
  void ReleaseInput(bool notify = true) {
    if (!releasing_) return;
    releasing_ = false;
    rex::cvar::SetFlagByName("xam_input_block", "false");
    SetHostUiOpen(false);
    if (!notify || source_ == OpenSource::kGameMenu) return;
    uint64_t generation;
    { std::lock_guard lock(g_modal_mutex); generation = g_generation; }
    ReleaseNotificationLater(generation);
  }

  void CheckInputRelease(const ImGuiIO& io, int64_t now) {
    if (!releasing_) return;
    if (HostInputsReleased(io)) { ReleaseInput(); return; }
    if (now < release_deadline_) return;
    REXLOG_INFO("PCOPTIONS input release timed out after 5 s");
    ReleaseInput();
  }

  std::string_view T(TextId id) const { return Text(id, context_.ctx.lang); }

  void Refresh() { BuildContext(touch_, &context_); }

  void Dispatch(Command command) {
    if (!open_ || !popup_id_.empty()) return;
    scroll_selection_ = true;
    ApplyOutcome(Handle(state_, command, Catalog(), values_, context_.ctx));
  }

  void ApplyOutcome(const Outcome& outcome) {
    switch (outcome.kind) {
      case Outcome::Kind::kChanged: Refresh(); break;
      case Outcome::Kind::kClose: CloseMenu(); break;
      case Outcome::Kind::kSave: Save(); break;
      case Outcome::Kind::kSaveAndClose: if (Save()) CloseMenu(); break;
      case Outcome::Kind::kEditText: BeginEditor(outcome.id, false); break;
      case Outcome::Kind::kCaptureKey: BeginEditor(outcome.id, true); break;
      case Outcome::Kind::kRunAction: RunAction(outcome.id); break;
      default: break;
    }
  }

  bool Save() {
    if (values_.paths().config_path.empty()) {
      toast_ = std::string(T(TextId::kSaveFailed));
      toast_until_ = NowMs() + 3000;
      return false;
    }
    std::string error;
    const bool ok = SaveRows(values_.paths().config_path, state_.touched, &error);
    if (ok) {
      MarkSaved(state_, Catalog(), values_);
      closed_host_original_.reset();
    }
    const auto filename = values_.paths().config_path.filename().u8string();
    toast_ = std::string(T(ok ? TextId::kSaved : TextId::kSaveFailed)) + (ok ? " " : ": ") +
        (ok ? std::string(filename.begin(),filename.end()) : error);
    toast_until_ = NowMs() + 3000;
    return ok;
  }

  bool SaveBeforeExit() { return state_.touched.empty() || Save(); }

  void RunAction(const std::string& id) {
    REXLOG_INFO("PCOPTIONS action {}", id);
#ifdef _WIN32
    if (id == "update_download") { update::OpenDownload(); return; }
    if (id == "update_check_now") { update::CheckNow(); return; }
    if (id == "update_dismiss") {
      update::Dismiss();
      update_notice_until_ = 0;
      return;
    }
#endif
    if (id == "runtime_settings") {
      CloseMenu();
      auto* runtime = rex::Runtime::instance();
      if (runtime) ToggleRuntimeSettings(runtime->display_window());
      return;
    }
    if (id == "quit_game") {
      if (!SaveBeforeExit()) return;
      CloseHostPortsBeforeExit();
      RequestQuit();
      return;
    }
    if (id == "restart_game") {
      if (!SaveBeforeExit()) return;
      CloseHostPortsBeforeExit();
      if (options_actions::RestartGame()) RequestQuit();
      return;
    }
    if (id == "save_open") { options_actions::OpenFolder(values_.paths().user_data_root); return; }
    if (id == "save_dir_browse") { BrowseFolder(); return; }
    if (id == "host_open") { StartHost(); return; }
    if (id == "host_close") options_actions::Host().Stop();
  }

  void BrowseFolder() {
    const auto* setting = FindSetting("save_dir_browse");
    const auto title = setting ? Label(*setting, context_.ctx.lang) : T(TextId::kTitle);
    const auto folder = options_actions::PickFolder(title);
    if (folder) CommitText(state_, "gow2_save_dir_text", *folder, Catalog(), values_);
    Refresh();
  }

  void StartHost() {
    options_actions::HostRequest request;
    request.lease_hours = rex::cvar::Query<int32_t>("gow2_host_lease_hours");
    request.hostname = rex::cvar::Query<std::string>("net_public_address");
    request.allow_peers = rex::cvar::Query<std::string>("net_allow_peers");
    request.players = rex::cvar::Query<int32_t>("gow2_coop_players");
    request.port_offset = rex::cvar::Query<int32_t>("net_port_offset");
    if (options_actions::Host().Start(request)) host_pending_ = true;
  }

  void ObserveHostResult() {
    if (!host_pending_ || options_actions::Host().Busy()) return;
    host_pending_ = false;
    const auto result = options_actions::Host().Last();
    if (!result || result->mode != "Start" || !result->ok) return;
    if (!rex::cvar::Query<std::string>("net_public_address").empty()) return;
    if (!open_ && !closed_host_original_)
      closed_host_original_ = values_.Get("net_public_address");
    CommitText(state_, "net_public_address", result->peer_name, Catalog(), values_);
    Refresh();
  }

  void RestoreHostChange() {
    if (!closed_host_original_) return;
    state_.opened["net_public_address"] = *closed_host_original_;
    if (values_.Get("net_public_address") != *closed_host_original_)
      state_.touched.insert("net_public_address");
    closed_host_original_.reset();
  }

  void TrackDevice(const ImGuiIO& io) {
    if (pad_.buttons != previous_pad_.buttons || std::abs(pad_.lx - previous_pad_.lx) > .15f ||
        std::abs(pad_.ly - previous_pad_.ly) > .15f) device_ = Device::kPad;
    previous_pad_ = pad_;
    if (io.MouseSource != ImGuiMouseSource_TouchScreen &&
        (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)) device_ = Device::kMouse;
    for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_GamepadStart; ++key)
      if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key), false)) device_ = Device::kKeyboard;
    if (ImGui::IsMouseClicked(0))
      device_ = io.MouseSource == ImGuiMouseSource_TouchScreen ? Device::kTouch : Device::kMouse;
  }

  void ReadMenuInput(int64_t now) {
    for (const auto command : reader_.Update(pad_, now)) Dispatch(command);
    constexpr std::array<std::pair<ImGuiKey, Command>, 13> keys{{
        {ImGuiKey_UpArrow, Command::kUp}, {ImGuiKey_DownArrow, Command::kDown},
        {ImGuiKey_LeftArrow, Command::kLeft}, {ImGuiKey_RightArrow, Command::kRight},
        {ImGuiKey_Enter, Command::kAccept}, {ImGuiKey_Space, Command::kAccept},
        {ImGuiKey_Escape, Command::kBack}, {ImGuiKey_Backspace, Command::kBack},
        {ImGuiKey_Q, Command::kTabPrev}, {ImGuiKey_PageUp, Command::kTabPrev},
        {ImGuiKey_E, Command::kTabNext}, {ImGuiKey_PageDown, Command::kTabNext},
        {ImGuiKey_Delete, Command::kResetRow}}};
    for (const auto [key, command] : keys)
      if (key != ImGuiKey_None && ImGui::IsKeyPressed(key)) Dispatch(command);
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) Dispatch(Command::kSave);
  }

  bool Hit(const char* id, ImVec2 pos, ImVec2 size) {
    ImGui::SetCursorScreenPos(pos);
    return ImGui::InvisibleButton(id, size);
  }

  void RetirePopups() {
    if (!retire_popups_) return;
    retire_popups_ = false;
    ImGui::SetNextWindowPos({-100,-100});
    ImGui::SetNextWindowSize({1,1});
    ImGui::Begin("##pc-options",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|
        ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings);
    constexpr std::array<const char*,2> popups{"##unsaved-options","##options-editor"};
    for (const auto* popup : popups) {
      if (!ImGui::IsPopupOpen(popup)) continue;
      if (!ImGui::BeginPopupModal(popup,nullptr,ImGuiWindowFlags_NoDecoration)) continue;
      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
    ImGui::End();
  }

  void DrawMenu(ImGuiIO& io) {
    h_ = io.DisplaySize.y;
    width_ = std::min(.90f * io.DisplaySize.x, 1.75f * h_);
    left_ = (io.DisplaySize.x - width_) * .5f;
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    if (focus_) { ImGui::SetNextWindowFocus(); focus_ = false; }
    constexpr auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground;
    ImGui::Begin("##pc-options", nullptr, flags);
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled({0, 0}, io.DisplaySize, IM_COL32(0, 0, 0, 230));
    const ImVec2 panel_min{left_-.02f*h_,.03f*h_};
    const ImVec2 panel_max{left_+width_+.02f*h_,.97f*h_};
    draw->AddRectFilled(panel_min,panel_max,IM_COL32(12,12,12,240));
    draw->AddRect(panel_min,panel_max,IM_COL32(255,255,255,15),0.f,0,1.f);
    DrawHeader();
    DrawTabs();
    DrawRows();
    DrawHelp();
    DrawRestart();
    DrawFooter();
    if (state_.prompt == Prompt::kUnsaved || ImGui::IsPopupOpen("##unsaved-options")) DrawUnsaved();
    DrawEditor();
    ImGui::End();
  }

  void DrawHeader() {
    const float y = .06f * h_;
    const auto title = T(TextId::kTitle);
    DrawText({left_, y}, title, .06f*h_, kWhite, true);
    const float title_width = TextWidth(title, .06f*h_, true);
    ImGui::GetWindowDrawList()->AddRectFilled({left_, y + .07f*h_},
        {left_ + title_width, y + .076f*h_}, kRed);
    if (state_.page == Page::kMain) return;
    const auto page = T(PageTitle(state_.page));
    DrawTriangle({left_+title_width+.03f*h_,y+.0355f*h_},true,kGray);
    DrawText({left_+title_width+.05f*h_,y+.018f*h_},page,.035f*h_,kGray,true);
  }

  void DrawCap(ImVec2 pos, std::string_view key, float size) {
    const float width = TextWidth(key, size) + .016f*h_;
    ImGui::GetWindowDrawList()->AddRectFilled(pos, {pos.x+width,pos.y+size+.012f*h_},
        IM_COL32(255,255,255,24), .004f*h_);
    DrawText({pos.x+.008f*h_,pos.y+.004f*h_}, key, size, kGray);
  }

  void DrawTabs() {
    const auto tabs = VisibleTabs(Catalog(), values_, context_.ctx);
    const float y = .165f*h_;
    const float hint = std::max(44.f, .05f*h_);
    // Pointers tap the tabs directly; touches reach ImGui as mouse clicks, so both hide the hints.
    const bool pointer = device_ == Device::kTouch || device_ == Device::kMouse;
    const float inset = pointer ? 0.f : hint;
    if (!pointer) {
      DrawCap({left_, y}, device_ == Device::kPad ? "LB" : "Q", .02f*h_);
      if (Hit("##previous-tab", {left_,y}, {hint,hint})) Dispatch(Command::kTabPrev);
      DrawCap({left_+width_-hint,y}, device_ == Device::kPad ? "RB" : "E", .02f*h_);
      if (Hit("##next-tab", {left_+width_-hint,y}, {hint,hint})) Dispatch(Command::kTabNext);
    }
    ImGui::SetCursorScreenPos({left_+inset,y});
    ImGui::BeginChild("##options-tabs",{width_-inset*2,hint+8},ImGuiChildFlags_None,
        ImGuiWindowFlags_HorizontalScrollbar|ImGuiWindowFlags_NoBackground);
    for (size_t i=0;i<tabs.size();++i) {
      if (i) ImGui::SameLine(0,0);
      const auto pos = ImGui::GetCursorScreenPos();
      const auto text = Uppercase(TabName(tabs[i],context_.ctx.lang));
      const float size = .03f*h_;
      const float tab_width = std::max(44.f,TextWidth(text,size,true)+.022f*h_);
      ImGui::PushID(static_cast<int>(i));
      if (Hit("##tab", pos, {tab_width,hint})) {
        if (state_.prompt == Prompt::kNone && popup_id_.empty()) {
          SelectTab(state_,static_cast<int>(i),Catalog(),values_,context_.ctx);
          scroll_selection_ = true;
        }
      }
      const bool selected = state_.tab == static_cast<int>(i);
      const auto color = selected ? IM_COL32_WHITE : ImGui::IsItemHovered() ? IM_COL32(208,208,208,255) : IM_COL32(154,154,154,255);
      DrawText({pos.x+.011f*h_,pos.y},text,size,color,true);
      // Under the text, not at the bottom of the 44 px touch target (far below the text on a phone).
      const float line_y = pos.y+size*1.25f;
      if (selected) ImGui::GetWindowDrawList()->AddRectFilled({pos.x,line_y},{pos.x+tab_width,line_y+.004f*h_},kRed);
      if (selected && scroll_selection_) ImGui::SetScrollHereX(.5f);
      ImGui::PopID();
    }
    ImGui::EndChild();
  }

  void DrawRows() {
    const float top = std::max(.235f*h_,.165f*h_+std::max(44.f,.05f*h_)+12);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,std::max(6.f,.006f*h_));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding,3.f);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,IM_COL32(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,IM_COL32(150,150,150,115));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered,IM_COL32(190,190,190,153));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,kRed);
    ImGui::SetCursorScreenPos({left_, top});
    ImGui::BeginChild("##option-rows", {width_, .67f*h_-top}, ImGuiChildFlags_None,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNavInputs);
    const auto rows = Rows(state_,Catalog(),values_,context_.ctx);
    for (size_t index=0;index<rows.size();++index) DrawRow(rows[index],static_cast<int>(index));
    ImGui::EndChild();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    scroll_selection_ = false;
  }

  void DrawTriangle(ImVec2 center, bool right, ImU32 color) {
    const float r = std::max(4.f,.006f*h_);
    const float sign = right ? 1.f : -1.f;
    ImGui::GetWindowDrawList()->AddTriangleFilled({center.x+sign*r,center.y},
        {center.x-sign*r,center.y-r},{center.x-sign*r,center.y+r},color);
  }

  void DrawRestartIcon(ImVec2 center) {
    auto* draw = ImGui::GetWindowDrawList();
    const float radius = std::max(5.f,.008f*h_);
    constexpr float end = 5.5f;
    draw->PathArcTo(center,radius,.5f,end,20);
    draw->PathStroke(kAmber,0,std::max(1.f,.0015f*h_));
    const ImVec2 tip{center.x+std::cos(end)*radius,center.y+std::sin(end)*radius};
    const float r = radius*.55f;
    draw->AddTriangleFilled({tip.x-r,tip.y},{tip.x+r,tip.y},{tip.x,tip.y+r},kAmber);
  }

  void DrawRow(const RowView& row, int index) {
    if (row.setting->id == "game_quality_group") {
      DrawQualityHeading(*row.setting);
      return;
    }
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float row_height = std::max(44.f,.066f*h_);
    const float row_width = ImGui::GetContentRegionAvail().x;
    ImGui::PushID(index);
    const float arrow_width = std::max(44.f,.04f*h_);
    if (row.selected) DrawSelection(pos,{row_width,row_height});
    const auto color = row.selected ? IM_COL32_WHITE : kGray;
    const float font = .029f*h_;
    const auto label = Label(*row.setting,context_.ctx.lang);
    const float label_x = pos.x+.022f*h_;
    const float label_y = pos.y+(row.selected ? .004f*h_ : (row_height-font)*.5f);
    const float gap = .02f*h_;
    const float right = pos.x+row_width-arrow_width;
    const float value_start = label_x+row_width*.4f+gap+arrow_width;
    const float value_available = std::max(0.f,right-value_start);
    const auto value = ElideMiddle(row.value_text,value_available,font);
    const float value_width = TextWidth(value,font);
    const float value_x = right-value_width;
    const float arrow_left = value_x-arrow_width;
    const float hit_end = row.can_increase ? right-pos.x : row_width;
    const float row_hit_width = row.can_decrease ? std::max(1.f,arrow_left-pos.x) : hit_end;
    const bool clicked = Hit("##row",pos,{row_hit_width,row_height});
    const bool value_clicked = row.can_decrease && Hit("##value",{value_x,pos.y},
        {std::max(1.f,hit_end-(value_x-pos.x)),row_height});
    const float label_max = std::max(row_width*.4f,arrow_left-label_x-gap);
    auto* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect({label_x,pos.y},{label_x+label_max,pos.y+row_height},true);
    DrawText({label_x,label_y},label,font,row.selectable ? color : IM_COL32(138,138,138,255));
    draw->PopClipRect();
    DrawText({value_x,pos.y+(row_height-font)*.5f},value,font,
             row.selectable ? color : IM_COL32(176,176,176,255));
    if (row.changed) draw->AddCircleFilled({label_x+std::min(label_max,TextWidth(label,font))+.009f*h_,label_y+.014f*h_},.0035f*h_,kAmber);
    if (row.restart_pending) DrawRestartIcon({label_x+std::min(label_max,TextWidth(label,font))+.028f*h_,label_y+.014f*h_});
    if (row.setting->kind == Kind::kAction || row.setting->kind == Kind::kText)
      DrawTriangle({pos.x+row_width-arrow_width*.5f,pos.y+row_height*.5f},true,color);
    if (row.selected) DrawApplyBadge(*row.setting,{label_x,pos.y+.038f*h_});
    if (row.setting->kind == Kind::kNumber) DrawProgress(row,{value_x,pos.y+row_height-.008f*h_},value_width);
    const bool blocked = state_.prompt != Prompt::kNone || !popup_id_.empty();
    if (row.can_decrease) DrawStep(row,index,-1,{arrow_left,pos.y},row_height,blocked);
    if (row.can_increase) DrawStep(row,index,1,{right,pos.y},row_height,blocked);
    if ((clicked || value_clicked) && row.selectable && !blocked)
      ApplyOutcome(Click(state_,index,Catalog(),values_,context_.ctx));
    ImGui::SetCursorScreenPos({pos.x,pos.y+row_height});
    ImGui::Dummy({row_width,0});
    if (row.selected && scroll_selection_) ImGui::SetScrollHereY(.5f);
    ImGui::PopID();
  }

  void DrawQualityHeading(const Setting& setting) {
    const auto pos = ImGui::GetCursorScreenPos();
    const auto width = ImGui::GetContentRegionAvail().x;
    const float height = std::max(38.f, .05f * h_);
    const float inset = .022f * h_;
    DrawText({pos.x + inset, pos.y + .012f * h_}, Label(setting, context_.ctx.lang),
             .026f * h_, IM_COL32_WHITE);
    ImGui::GetWindowDrawList()->AddLine({pos.x + inset, pos.y + height - 2},
        {pos.x + width - inset, pos.y + height - 2}, kGray);
    ImGui::Dummy({width, height});
  }

  void DrawSelection(ImVec2 pos, ImVec2 size) {
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(pos,{pos.x+size.x,pos.y+size.y},IM_COL32(179,25,27,77));
    draw->AddRectFilled(pos,{pos.x+.006f*h_,pos.y+size.y},kRed);
  }

  void DrawApplyBadge(const Setting& row, ImVec2 pos) {
    if (row.kind == Kind::kAction || row.kind == Kind::kInfo) return;
    TextId text = TextId::kApplyNow;
    ImU32 color = kGreen;
    if (row.apply == Apply::kNextLevel) { text = TextId::kApplyNextLevel; color = kBlue; }
    if (row.apply == Apply::kRestart) { text = TextId::kApplyRestart; color = kAmber; }
    DrawText(pos,T(text),.018f*h_,color);
  }

  void DrawProgress(const RowView& row, ImVec2 pos, float width) {
    const auto& setting = *row.setting;
    const auto current = values_.Get(setting.id);
    char* end = nullptr;
    const double number = std::strtod(current.c_str(),&end);
    if (end == current.c_str() || setting.max <= setting.min) return;
    width = std::max(width,.08f*h_);
    const float fraction = static_cast<float>(std::clamp((number-setting.min)/(setting.max-setting.min),0.,1.));
    ImGui::GetWindowDrawList()->AddRectFilled(pos,{pos.x+width,pos.y+.002f*h_},IM_COL32(255,255,255,35));
    ImGui::GetWindowDrawList()->AddRectFilled(pos,{pos.x+width*fraction,pos.y+.002f*h_},kRed);
  }

  void DrawStep(const RowView&, int index, int direction, ImVec2 pos, float height, bool blocked) {
    const float width = std::max(44.f,.04f*h_);
    DrawTriangle({pos.x+width*.5f,pos.y+height*.5f},direction>0,kGray);
    if (Hit(direction<0 ? "##decrease" : "##increase",pos,{width,height}) && !blocked)
      ApplyOutcome(Step(state_,index,direction,Catalog(),values_,context_.ctx));
  }

  void DrawHelp() {
    const ImVec2 pos{left_,.69f*h_};
    ImGui::GetWindowDrawList()->AddRectFilled(pos,{left_+width_,.79f*h_},IM_COL32(255,255,255,13));
    PushFont(.024f*h_);
    ImGui::SetCursorScreenPos({left_+.018f*h_,pos.y+.013f*h_});
    ImGui::PushStyleColor(ImGuiCol_Text,IM_COL32(189,189,189,255));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+width_-.036f*h_);
    const auto help = CurrentHelp(state_,Catalog(),values_,context_.ctx);
    ImGui::TextUnformatted(help.data(),help.data()+help.size());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::PopFont();
  }

  void DrawRestart() {
    const auto pending = RestartPending(state_,Catalog(),values_,context_.ctx);
    if (pending.empty()) return;
    std::string text(T(TextId::kRestartBanner));
    for (size_t i=0;i<pending.size();++i) {
      text += i ? ", " : " ";
      text += pending[i];
    }
    PushFont(.019f*h_);
    ImGui::SetCursorScreenPos({left_,.806f*h_});
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+width_*.78f);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kAmber),"%s",text.c_str());
    ImGui::PopTextWrapPos();
#ifdef _WIN32
    const auto label = context_.ctx.lang == Lang::kEs ? "Reiniciar ahora" : "Restart now";
    ImGui::SetCursorScreenPos({left_+width_*.80f,.80f*h_});
    const bool clicked = DrawButton(label,{width_*.20f,std::max(44.f,.066f*h_)},true);
    if (clicked && !ModalActive()) RunAction("restart_game");
#endif
    ImGui::PopFont();
  }

  bool ModalActive() const { return state_.prompt != Prompt::kNone || !popup_id_.empty(); }

  void DrawFooter() {
    if (device_ == Device::kTouch || device_ == Device::kMouse) {
      DrawPointerFooter();
      return;
    }
    constexpr std::array<Command,4> commands{Command::kAccept,Command::kBack,Command::kResetRow,Command::kSave};
    const std::array<TextId,4> text{TextId::kHintChange,
        state_.page == Page::kMain && state_.touched.empty() ? TextId::kHintClose : TextId::kHintBack,
        TextId::kHintDefault,TextId::kHintSave};
    constexpr std::array<const char*,4> keys{"Enter","Esc","Del","Ctrl+S"};
    constexpr std::array<const char*,4> pad_keys{"A","B","X","Y"};
    constexpr std::array<ImU32,4> colors{kGreen,IM_COL32(224,80,80,255),IM_COL32(74,144,226,255),IM_COL32(232,196,64,255)};
    const float item_width = width_/4;
    for (size_t i=0;i<commands.size();++i) {
      const ImVec2 pos{left_+item_width*static_cast<float>(i),.875f*h_};
      ImGui::PushID(static_cast<int>(i));
      if (device_ == Device::kPad) DrawPadHint(pos,pad_keys[i],T(text[i]),colors[i]);
      else {
        DrawCap(pos,keys[i],.020f*h_);
        DrawText({pos.x+TextWidth(keys[i],.020f*h_)+.024f*h_,pos.y+.004f*h_},T(text[i]),.022f*h_,kGray);
      }
      if (Hit("##footer",pos,{item_width,std::max(44.f,.055f*h_)}) && !ModalActive()) Dispatch(commands[i]);
      ImGui::PopID();
    }
  }

  void DrawPointerFooter() {
    constexpr std::array<Command,3> commands{Command::kSave,Command::kResetRow,Command::kBack};
    const std::array<TextId,3> labels{TextId::kHintSave,TextId::kHintDefault,
        state_.page == Page::kMain && state_.touched.empty() ? TextId::kHintClose : TextId::kHintBack};
    const float item_width = width_/3;
    PushFont(.024f*h_,true);
    for (size_t i=0;i<commands.size();++i) {
      ImGui::SetCursorScreenPos({left_+item_width*static_cast<float>(i),.875f*h_});
      ImGui::PushID(static_cast<int>(i));
      const bool clicked = DrawButton(std::string(T(labels[i])).c_str(),
          {item_width-.008f*h_,std::max(44.f,.055f*h_)},commands[i] == Command::kSave);
      ImGui::PopID();
      if (clicked && !ModalActive()) Dispatch(commands[i]);
    }
    ImGui::PopFont();
  }

  void DrawPadHint(ImVec2 pos, const char* key, std::string_view text, ImU32 color) {
    const float radius = .016f*h_;
    const ImVec2 center{pos.x+radius,pos.y+radius};
    ImGui::GetWindowDrawList()->AddCircleFilled(center,radius,color);
    DrawText({center.x-TextWidth(key,.022f*h_,true)*.5f,center.y-.011f*h_},key,.022f*h_,IM_COL32(15,15,15,255),true);
    DrawText({pos.x+radius*2+.01f*h_,pos.y+.004f*h_},text,.023f*h_,kGray);
  }

  bool BeginPanel(const char* id, float width, float height) {
    const auto display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({display.x*.5f,display.y*.5f},ImGuiCond_Always,{.5f,.5f});
    ImGui::SetNextWindowSize({width,std::min(height,display.y*.94f)});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,IM_COL32(20,20,20,247));
    ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(1.f,1.f,1.f,.08f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{.022f*h_,.022f*h_});
    ImGui::OpenPopup(id);
    const bool visible = ImGui::BeginPopupModal(id,nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
    if (visible) {
      const auto pos = ImGui::GetWindowPos();
      ImGui::GetWindowDrawList()->AddRectFilled(pos,{pos.x+width,pos.y+.005f*h_},kRed);
      PushFont(.026f*h_);
    }
    return visible;
  }

  void EndPanel(bool visible) {
    if (visible) { ImGui::PopFont(); ImGui::EndPopup(); }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
  }

  float PanelTextHeight(std::string_view text, float width) const {
    PushFont(.026f*h_);
    const float height = ImGui::CalcTextSize(text.data(),text.data()+text.size(),false,
        std::max(1.f,width-.044f*h_)).y;
    ImGui::PopFont();
    return height;
  }

  float UnsavedHeight(float width) const {
    return .044f*h_+PanelTextHeight(T(TextId::kUnsavedTitle),width)+
        ImGui::GetStyle().ItemSpacing.y*2+std::max(44.f,.06f*h_);
  }

  float KeyboardHeight(float available) const {
    const auto& style = ImGui::GetStyle();
    const float minimum_width = 44.f*10+style.ItemSpacing.x*9;
    const float scrollbar = minimum_width > available ? style.ScrollbarSize : 0.f;
    return std::max(44.f,.062f*h_)*5+style.ItemSpacing.y*4+scrollbar;
  }

  float EditorHeight(float width, bool osk) const {
    const auto* setting = FindSetting(popup_id_);
    const auto title = capture_ ? T(TextId::kPressAKey) :
        setting ? Label(*setting,context_.ctx.lang) : T(TextId::kTitle);
    const auto& style = ImGui::GetStyle();
    float height = .044f*h_+PanelTextHeight(title,width)+style.ItemSpacing.y+
        std::max(44.f,.055f*h_);
    if (capture_) return height;
    height += std::max(44.f,.026f*h_+style.FramePadding.y*2)+style.ItemSpacing.y;
    if (osk) height += KeyboardHeight(width-.044f*h_)+style.ItemSpacing.y;
    return height;
  }

  void DrawUnsaved() {
    const float width = std::min(width_,.95f*h_);
    const bool visible = BeginPanel("##unsaved-options",width,UnsavedHeight(width));
    if (visible) {
      if (state_.prompt != Prompt::kUnsaved) {
        ImGui::CloseCurrentPopup();
        EndPanel(visible);
        return;
      }
      const auto title = T(TextId::kUnsavedTitle);
      ImGui::TextWrapped("%.*s",static_cast<int>(title.size()),title.data());
      ImGui::Spacing();
      constexpr std::array<TextId,3> labels{TextId::kUnsavedSave,TextId::kUnsavedDiscard,TextId::kUnsavedCancel};
      for (int i=0;i<3;++i) {
        if (i) ImGui::SameLine();
        ImGui::PushID(i);
        const bool clicked = DrawButton(std::string(T(labels[i])).c_str(),
            {ImGui::GetWindowWidth()/3-.025f*h_,std::max(44.f,.06f*h_)},state_.prompt_choice == i);
        ImGui::PopID();
        if (clicked) { state_.prompt_choice=i; Dispatch(Command::kAccept); }
      }
      if (state_.prompt != Prompt::kUnsaved || !open_) ImGui::CloseCurrentPopup();
    }
    EndPanel(visible);
  }

  void BeginEditor(const std::string& id, bool capture) {
    const auto* setting = FindSetting(id);
    if (!setting) return;
    popup_id_ = id;
    capture_ = capture;
    popup_focus_ = true;
    popup_frame_ = ImGui::GetFrameCount();
    const auto value = values_.Get(id);
    buffer_.assign(std::max(size_t{1},setting->max_length)+1,0);
    if (!capture) std::memcpy(buffer_.data(),value.data(),std::min(value.size(),buffer_.size()-1));
    popup_pad_.Reset(pad_);
    keyboard_gates_ = {};
    keyboard_row_ = keyboard_col_ = 0;
    shift_ = false;
    symbols_ = false;
    pending_edits_.clear();
    show_keyboard_ = device_ == Device::kPad;
  }

  void FinishEditor(bool accept) {
    FlushEdits();
    if (accept && !capture_) CommitText(state_,popup_id_,buffer_.data(),Catalog(),values_);
    popup_id_.clear();
    Refresh();
    reader_.Reset(pad_);
    ImGui::CloseCurrentPopup();
  }

  void DrawEditor() {
    if (popup_id_.empty()) return;
    if (device_ == Device::kPad) show_keyboard_ = true;
    const bool osk = !capture_ && show_keyboard_;
    const float width = std::min(width_,1.12f*h_);
    const bool visible = BeginPanel("##options-editor",width,EditorHeight(width,osk));
    if (visible) {
      if (capture_) DrawCapture();
      else DrawTextEditor(osk);
    }
    EndPanel(visible);
  }

  void DrawCapture() {
    const auto text = T(TextId::kPressAKey);
    ImGui::TextWrapped("%.*s",static_cast<int>(text.size()),text.data());
    if (DrawButton(std::string(T(TextId::kEditTextCancel)).c_str(),
        {ImGui::GetContentRegionAvail().x,std::max(44.f,.055f*h_)})) {
      FinishEditor(false);
      return;
    }
    const auto commands = popup_pad_.Update(pad_,NowMs());
    if (std::find(commands.begin(),commands.end(),Command::kBack) != commands.end() ||
        ImGui::IsKeyPressed(ImGuiKey_Escape,false)) { FinishEditor(false); return; }
    if (ImGui::GetFrameCount() == popup_frame_) return;
    const auto key = CapturedKey();
    if (key.empty()) return;
    CommitKey(state_,popup_id_,key,Catalog(),values_);
    FinishEditor(false);
  }

  std::string CapturedKey() {
    for (int key=ImGuiKey_NamedKey_BEGIN;key<ImGuiKey_GamepadStart;++key) {
      const auto imgui_key = static_cast<ImGuiKey>(key);
      if (!ImGui::IsKeyPressed(imgui_key,false)) continue;
      const auto vk = KeyToVirtualKey(imgui_key);
      if (vk != rex::ui::VirtualKey::kNone) return rex::ui::VirtualKeyToString(vk);
    }
    constexpr std::array<const char*,3> mouse{"LMB","RMB","MMB"};
    for (int button=0;button<3;++button) if (ImGui::IsMouseClicked(button)) return mouse[button];
    return {};
  }

  void DrawTextEditor(bool osk) {
    const auto* setting = FindSetting(popup_id_);
    if (!setting) { FinishEditor(false); return; }
    const auto label = Label(*setting,context_.ctx.lang);
    ImGui::TextWrapped("%.*s",static_cast<int>(label.size()),label.data());
    ImGui::SetNextItemWidth(-1);
    if (popup_focus_) { ImGui::SetKeyboardFocusHere(); popup_focus_ = false; }
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg,ImVec4(179.f/255.f,25.f/255.f,27.f/255.f,.55f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(1.f,1.f,1.f,.10f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,ImVec4(1.f,1.f,1.f,.14f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,ImVec4(1.f,1.f,1.f,.18f));
    const auto padding = ImGui::GetStyle().FramePadding;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
        ImVec2(padding.x,std::max(padding.y,(44.f-ImGui::GetFontSize())*.5f)));
    const bool accepted = ImGui::InputText("##option-text",buffer_.data(),buffer_.size(),
        ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_CallbackAlways,EditCallback,this);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    if (!ImGui::IsItemActive()) FlushEdits();
    if (accepted) {
      FinishEditor(true); return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape,false)) { FinishEditor(false); return; }
    if (osk) { ReadKeyboardPad(); if (popup_id_.empty()) return; DrawKeyboard(); }
    else ReadEditorPad();
    if (popup_id_.empty()) return;
    DrawEditorButtons();
  }

  void ReadEditorPad() {
    for (const auto command : popup_pad_.Update(pad_,NowMs())) {
      if (command == Command::kBack) { FinishEditor(false); return; }
      if (command == Command::kSave) { FinishEditor(true); return; }
    }
  }

  void DrawEditorButtons() {
    const auto size = ImVec2{ImGui::GetContentRegionAvail().x*.48f,std::max(44.f,.055f*h_)};
    if (DrawButton(std::string(T(TextId::kEditTextOk)).c_str(),size)) { FinishEditor(true); return; }
    ImGui::SameLine();
    if (DrawButton(std::string(T(TextId::kEditTextCancel)).c_str(),size)) FinishEditor(false);
  }

  int KeyboardColumns() const { return keyboard_row_ == 4 ? 4 : static_cast<int>(KeyboardRows()[keyboard_row_].size()); }

  const std::array<std::string_view,4>& KeyboardRows() const {
    static constexpr std::array<std::string_view,4> rows{"1234567890","qwertyuiop","asdfghjkl.","zxcvbnm-_:"};
    static constexpr std::array<std::string_view,4> symbols{
        "!@#$%^&*()",",./\\;:=+-_","'\"?[]{}<>|","~`"};
    return symbols_ ? symbols : rows;
  }

  void ReadKeyboardPad() {
    const auto now = NowMs();
    const std::array<bool,4> held{(pad_.buttons&1)!=0 || pad_.ly>.5f,
        (pad_.buttons&2)!=0 || pad_.ly<-.5f,(pad_.buttons&4)!=0 || pad_.lx<-.5f,
        (pad_.buttons&8)!=0 || pad_.lx>.5f};
    if (keyboard_gates_[0].Update(held[0],now)) keyboard_row_=(keyboard_row_+4)%5;
    if (keyboard_gates_[1].Update(held[1],now)) keyboard_row_=(keyboard_row_+1)%5;
    keyboard_col_=std::min(keyboard_col_,KeyboardColumns()-1);
    if (keyboard_gates_[2].Update(held[2],now)) keyboard_col_=(keyboard_col_+KeyboardColumns()-1)%KeyboardColumns();
    if (keyboard_gates_[3].Update(held[3],now)) keyboard_col_=(keyboard_col_+1)%KeyboardColumns();
    for (const auto command : popup_pad_.Update(pad_,now)) {
      if (command == Command::kBack) { FinishEditor(false); return; }
      if (command == Command::kSave) { FinishEditor(true); return; }
      if (command == Command::kResetRow) pending_edits_.push_back({true,{}});
      if (command == Command::kAccept) TypeKeyboardKey(keyboard_row_,keyboard_col_);
      if (popup_id_.empty()) return;
    }
  }

  std::string KeyboardLabel(int row, int col) const {
    if (row < 4) {
      char c = KeyboardRows()[row][col];
      if (shift_ && c >= 'a' && c <= 'z') c -= 'a'-'A';
      return std::string(1,c);
    }
    switch (col) {
      case 0: return "Shift";
      case 1: return context_.ctx.lang == Lang::kEs ? "Espacio" : "Space";
      case 2: return context_.ctx.lang == Lang::kEs ? "Borrar" : "Delete";
      default: return symbols_ ? "[ABC]" : "[Symbols]";
    }
  }

  void TypeKeyboardKey(int row, int col) {
    if (row < 4) { AppendText(KeyboardLabel(row,col)); return; }
    if (col == 0) { shift_ = !shift_; return; }
    if (col == 1) { AppendText(" "); return; }
    if (col == 2) pending_edits_.push_back({true,{}});
    if (col == 3) symbols_ = !symbols_;
  }

  void AppendText(std::string_view text) {
    pending_edits_.push_back({false,std::string(text)});
  }

  void AppendBuffer(std::string_view text) {
    const auto length = std::strlen(buffer_.data());
    if (length+text.size() >= buffer_.size()) return;
    std::memcpy(buffer_.data()+length,text.data(),text.size());
    buffer_[length+text.size()] = 0;
  }

  void FlushEdits() {
    for (const auto& edit : pending_edits_) {
      if (edit.erase) DeleteLastCharacter(buffer_);
      else AppendBuffer(edit.text);
    }
    pending_edits_.clear();
  }

  static int EditCallback(ImGuiInputTextCallbackData* data) {
    auto& self = *static_cast<OptionsOverlay*>(data->UserData);
    for (const auto& edit : self.pending_edits_) {
      if (edit.erase) {
        int end = data->BufTextLen;
        if (!end) continue;
        int start = end-1;
        while (start && (static_cast<unsigned char>(data->Buf[start]) & 0xc0) == 0x80) --start;
        data->DeleteChars(start,end-start);
      } else data->InsertChars(data->BufTextLen,edit.text.c_str());
    }
    self.pending_edits_.clear();
    return 0;
  }

  void DrawKeyboard() {
    const float available = ImGui::GetContentRegionAvail().x;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float key_width = std::max(44.f,(available-spacing*9)/10);
    const float keyboard_width = key_width*10+spacing*9;
    const float height = std::max(44.f,.062f*h_);
    ImGui::BeginChild("##options-keyboard",{available,KeyboardHeight(available)},ImGuiChildFlags_None,
        ImGuiWindowFlags_HorizontalScrollbar);
    for (int row=0;row<5;++row) {
      const int columns = row==4 ? 4 : static_cast<int>(KeyboardRows()[row].size());
      const float width = (keyboard_width-spacing*(columns-1))/columns;
      for (int col=0;col<columns;++col) {
        if (col) ImGui::SameLine();
        ImGui::PushID(row*16+col);
        const bool selected = keyboard_row_ == row && keyboard_col_ == col;
        const bool clicked = DrawButton(KeyboardLabel(row,col).c_str(),{width,height},selected);
        if (selected && device_ == Device::kPad) {
          ImGui::SetScrollHereX(.5f);
          ImGui::SetScrollHereY(.5f);
        }
        ImGui::PopID();
        if (clicked) TypeKeyboardKey(row,col);
        if (popup_id_.empty()) { ImGui::EndChild(); return; }
      }
    }
    ImGui::EndChild();
  }

#ifdef _WIN32
  void DrawUpdateNotice(const ImGuiIO& io, int64_t now) {
    if (!rex::cvar::Query<bool>("gow2_update_check")) {
      update_notice_until_ = 0;
      return;
    }
    if (update::TakeNotice()) {
      const auto snapshot = update::GetSnapshot();
      const bool spanish = MenuLang() == Lang::kEs;
      update_notice_ = (spanish ? "Actualización disponible: " : "Update available: ") +
          snapshot.release.tag + (spanish ? " — Opciones > Opciones de PC > Sistema" :
                                            " — Options > PC Options > System");
      update_notice_until_ = now + 8000;
    }
    const auto remaining = update_notice_until_ - now;
    if (remaining <= 0 || update_notice_.empty()) return;
    const float alpha = std::min(1.f,static_cast<float>(remaining)/500.f);
    ImGui::SetNextWindowPos({io.DisplaySize.x*.96f,io.DisplaySize.y*.04f},ImGuiCond_Always,{1,0});
    ImGui::SetNextWindowBgAlpha(.95f*alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,alpha);
    ImGui::Begin("##update-notice",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|
        ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing);
    PushFont(.024f*io.DisplaySize.y);
    ImGui::TextUnformatted(update_notice_.c_str());
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar();
  }
#endif

  void DrawToast(const ImGuiIO& io) {
    const auto remaining = toast_until_ - NowMs();
    if (remaining <= 0 || toast_.empty()) return;
    const float alpha = std::min(1.f,static_cast<float>(remaining)/500.f);
    ImGui::SetNextWindowPos({io.DisplaySize.x*.96f,io.DisplaySize.y*.96f},ImGuiCond_Always,{1,1});
    ImGui::SetNextWindowBgAlpha(.95f*alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,alpha);
    ImGui::Begin("##options-toast",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|
        ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing);
    PushFont(.024f*io.DisplaySize.y);
    ImGui::TextUnformatted(toast_.c_str());
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar();
  }

  CvarValues values_;
  OwnedContext context_;
  State state_;
  PadReader reader_;
  PadReader popup_pad_;
  PadSample pad_{};
  PadSample previous_pad_{};
  std::array<RepeatGate,4> keyboard_gates_{};
  bool touch_ = false;
  bool open_ = false;
  bool releasing_ = false;
  int64_t release_deadline_ = 0;
  bool focus_ = false;
  bool scroll_selection_ = false;
  bool capture_ = false;
  bool popup_focus_ = false;
  bool shift_ = false;
  bool symbols_ = false;
  bool host_pending_ = false;
  std::optional<std::string> closed_host_original_;
  bool show_keyboard_ = false;
  bool retire_popups_ = false;
  OpenSource source_ = OpenSource::kShortcut;
  Device device_ = Device::kKeyboard;
  std::string popup_id_;
  std::vector<char> buffer_;
  struct TextEdit { bool erase; std::string text; };
  std::vector<TextEdit> pending_edits_;
  int popup_frame_ = 0;
  int keyboard_row_ = 0;
  int keyboard_col_ = 0;
  float h_ = 1080;
  float width_ = 0;
  float left_ = 0;
  std::string toast_;
  int64_t toast_until_ = 0;
#ifdef _WIN32
  std::string update_notice_;
  int64_t update_notice_until_ = 0;
#endif
};

void RunModal() {
  auto* runtime = rex::Runtime::instance();
  if (!runtime || !runtime->app_context()) return;
  auto* context = runtime->app_context();
  if (context->IsInUIThread()) return;
  uint64_t closed;
  {
    std::lock_guard lock(g_modal_mutex);
    if (!g_exists || g_shutdown) return;
    closed = g_closed;
  }
  if (!context->CallInUIThread([closed] {
    bool available;
    { std::lock_guard lock(g_modal_mutex); available = g_exists && !g_shutdown && g_closed == closed; }
    if (available && g_overlay) g_overlay->OpenMenu(OpenSource::kGameMenu);
    else NotifyClosed();
  })) return;
  std::unique_lock lock(g_modal_mutex);
  while (g_exists && !g_shutdown && g_closed == closed)
    g_modal_cv.wait_for(lock,std::chrono::milliseconds(500));
}

ImFont* LoadFont(ImFontAtlas* atlas, const char* path) {
  std::error_code error;
  if (!std::filesystem::exists(path,error)) return nullptr;
  static constexpr ImWchar ranges[]{0x0020,0x00FF,0x0100,0x017F,0x2000,0x206F,
      0x2190,0x21FF,0x2200,0x22FF,0x25A0,0x25FF,0};
  return atlas->AddFontFromFileTTF(path,24.f,nullptr,ranges);
}

}

void AddOptionsFonts(ImFontAtlas* atlas) {
  if (!atlas) return;
#ifdef _WIN32
  g_regular = LoadFont(atlas,"C:\\Windows\\Fonts\\segoeui.ttf");
  g_bold = LoadFont(atlas,"C:\\Windows\\Fonts\\segoeuib.ttf");
#elif defined(__ANDROID__)
  g_regular = LoadFont(atlas,"/system/fonts/Roboto-Regular.ttf");
  if (!g_regular) g_regular = LoadFont(atlas,"/system/fonts/RobotoStatic-Regular.ttf");
  if (!g_regular) g_regular = LoadFont(atlas,"/system/fonts/NotoSans-Regular.ttf");
  g_bold = LoadFont(atlas,"/system/fonts/Roboto-Bold.ttf");
  if (!g_bold) g_bold = LoadFont(atlas,"/system/fonts/RobotoStatic-Bold.ttf");
  if (!g_bold) g_bold = LoadFont(atlas,"/system/fonts/NotoSans-Bold.ttf");
#endif
  if (!g_bold) g_bold = g_regular;
}

std::unique_ptr<rex::ui::ImGuiDialog> CreateOptionsOverlay(
    rex::ui::ImGuiDrawer* drawer, const GamePaths& paths, bool touch_controls) {
  { std::lock_guard lock(g_modal_mutex); if (g_exists) return nullptr; }
  auto overlay = std::make_unique<OptionsOverlay>(drawer,paths,touch_controls);
  g_overlay = overlay.get();
  { std::lock_guard lock(g_modal_mutex); g_exists=true; g_shutdown=false; }
  return overlay;
}

void TogglePcOptions(OpenSource source) {
  if (g_overlay) g_overlay->Toggle(source);
}

void UpdatePcOptionsPaths(const GamePaths& paths) {
  if (g_overlay) g_overlay->SetPaths(paths);
}

void ClosePcOptionsForShutdown() {
  { std::lock_guard lock(g_modal_mutex); g_shutdown=true; g_exists=false; ++g_generation; }
  if (g_overlay) g_overlay->CloseForShutdown();
  g_overlay = nullptr;
  g_modal_cv.notify_all();
}

}

REXCVAR_DEFINE_COMMAND_ARGS(gow2_pc_options_modal, [](std::string_view) {
  rex::glue::options::RunModal();
}, "Menu", "Open PC Options and wait for it to close");
