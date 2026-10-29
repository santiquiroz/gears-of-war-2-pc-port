// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Live cvar values, menu capabilities, and selective TOML persistence.
#include "gow2_options_values.h"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <utility>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/runtime.h>

#include "gow2_mouse_aim.h"
#include "gow2_graphics_quality.h"
#include "gow2_options_actions.h"
#include "gow2_options_values_text.h"
#include "gow2_script_patch_groups.h"
#include "gow2_script_patch_stack.h"
#include "gow2_texture_packs.h"

#ifdef _WIN32
#include <windows.h>
#include "gow2_texture_pack_builder.h"
#include "gow2_texture_pack_plan.h"
#include "gow2_update_check.h"
#elif defined(__ANDROID__)
#include <sys/system_properties.h>
#endif

#ifndef GOW2_VERSION
#define GOW2_VERSION "dev"
#endif

namespace rex::glue::options {

REXCVAR_DEFINE_STRING(gow2_menu_language, "auto", "UI", "PC Options menu language: auto, en or es")
    .allowed({"auto", "en", "es"});
REXCVAR_DEFINE_BOOL(gow2_menu_advanced, false, "UI", "PC Options menu shows the advanced rows");
REXCVAR_DEFINE_INT32(gow2_host_lease_hours, 6, "Network", "Hours the in-game host keeps the router ports open")
    .range(1, 24);
REXCVAR_DEFINE_BOOL(gow2_host_close_on_exit, true, "Network", "The in-game host closes the router ports when the game exits");
REXCVAR_DEFINE_BOOL(gow2_launcher, true, "UI", "Open PC Options before the game loads, with a Play button");
REXCVAR_DEFINE_STRING(gow2_texture_pack, "", "GPU/Textures",
                      "Texture pack folder name under texture_packs, 'textures' for the old folder, 'off', or "
                      "empty to use texture_replace")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_INT32(gow2_texture_build_scale, 2, "GPU/Textures", "Build texture pack: 2x or 4x").range(2, 4);
REXCVAR_DEFINE_STRING(gow2_texture_build_quality, "disc", "GPU/Textures",
                      "Build texture pack: disc keeps each texture's format, bc7 is sharper and larger")
    .allowed({"disc", "bc7"});

namespace {

using Names = std::vector<std::string_view>;
using ReadValue = std::string (*)(std::string_view);

std::string CurrentValue(std::string_view name) {
  if (!rex::cvar::GetFlagInfo(name)) return {};
  return rex::cvar::GetFlagByName(name);
}

std::string DefaultValue(std::string_view name) {
  const auto* flag = rex::cvar::GetFlagInfo(name);
  return flag ? flag->default_value : std::string{};
}

std::string_view CvarName(std::string_view id) {
  if (id == "present_effect_android") return "present_effect";
  if (id == "gow2_save_dir_text") return "gow2_save_dir";
  return id;
}

bool IsReadOnly(std::string_view id) {
  const auto* row = FindSetting(id);
  if (!row) return false;
  return row->kind == Kind::kInfo || row->kind == Kind::kAction;
}

Names CompositeNames(std::string_view id) {
  if (id == "gow2_gfx_preset") {
    Names names;
    for (const auto& setting : graphics_quality::Settings()) names.push_back(setting.id);
    return names;
  }
  if (id == "window_mode") return {"fullscreen", "fullscreen_exclusive"};
  if (id == "window_size") return {"window_width", "window_height"};
  if (id == "upscaler" || id == "upscaler_dlss") return {"gow2_temporal", "gow2_upscaler"};
  if (id == "texture_memory")
    return {"texture_cache_memory_limit_soft", "texture_cache_memory_limit_hard"};
  return {};
}

std::string UpscalerValue(ReadValue read) {
  const auto upscaler = read("gow2_upscaler");
  if (read("gow2_temporal") != "true" || upscaler == "off") return "off";
  return upscaler;
}

std::string RowValue(std::string_view id, ReadValue read) {
  if (id == "gow2_gfx_preset") {
#ifdef __ANDROID__
    return graphics_quality::DetectPreset(read, true);
#else
    return graphics_quality::DetectPreset(read);
#endif
  }
  if (id == "window_mode")
    return values_text::WindowMode(read("fullscreen"), read("fullscreen_exclusive"));
  if (id == "window_size")
    return values_text::WindowSize(read("window_width"), read("window_height"));
  if (id == "upscaler" || id == "upscaler_dlss") return UpscalerValue(read);
  if (id == "texture_memory")
    return read("texture_cache_memory_limit_soft") + "/" + read("texture_cache_memory_limit_hard");
  if (id == "input_pad_first_user") return read("input_pad_first_user") == "1" ? "true" : "false";
  return read(CvarName(id));
}

bool SetCvar(std::string_view name, std::string_view value) {
  if (!rex::cvar::GetFlagInfo(name)) return false;
  return rex::cvar::SetFlagByName(name, value);
}

bool SetPair(std::string_view first, std::string_view first_value,
             std::string_view second, std::string_view second_value) {
  if (!rex::cvar::GetFlagInfo(first) || !rex::cvar::GetFlagInfo(second)) return false;
  if (!SetCvar(first, first_value)) return false;
  return SetCvar(second, second_value);
}

bool SetWindowMode(std::string_view value) {
  if (value == "windowed") return SetCvar("fullscreen", "false");
  if (value == "borderless") return SetPair("fullscreen_exclusive", "false", "fullscreen", "true");
  if (value == "exclusive") return SetPair("fullscreen_exclusive", "true", "fullscreen", "true");
  return false;
}

bool SetWindowSize(std::string_view value) {
  const auto size = values_text::ParseWindowSize(value);
  if (!size) return false;
  return SetPair("window_width", size->first, "window_height", size->second);
}

bool SetTextureMemory(std::string_view value) {
  const auto memory = values_text::ParseTextureMemory(value);
  if (!memory) return false;
  return SetPair("texture_cache_memory_limit_soft", memory->first,
                 "texture_cache_memory_limit_hard", memory->second);
}

bool SetUpscaler(std::string_view value) {
  if (value == "off") return SetPair("gow2_temporal", "false", "gow2_upscaler", "off");
  if (value == "fsr3" || value == "fsr4" || value == "dlss")
    return SetPair("gow2_upscaler", value, "gow2_temporal", "true");
  return false;
}

bool SetQualityPreset(std::string_view preset) {
  if (preset != "auto" && preset != "low" && preset != "medium" && preset != "high") return false;
  for (const auto& setting : graphics_quality::Settings()) {
    if (!rex::cvar::GetFlagInfo(setting.id)) return false;
  }
  for (const auto& setting : graphics_quality::Settings()) {
    const auto value = preset == "auto" ? "auto" : graphics_quality::PresetValue(setting, preset);
    if (!SetCvar(setting.id, value)) return false;
  }
  return true;
}

bool SetRow(std::string_view id, std::string_view value) {
  if (IsReadOnly(id)) return false;
  if (id == "gow2_gfx_preset") return SetQualityPreset(value);
  if (id == "window_mode") return SetWindowMode(value);
  if (id == "window_size") return SetWindowSize(value);
  if (id == "upscaler" || id == "upscaler_dlss") return SetUpscaler(value);
  if (id == "texture_memory") return SetTextureMemory(value);
  if (id == "input_pad_first_user") return SetCvar(id, value == "true" ? "1" : "0");
  return SetCvar(CvarName(id), value);
}

std::string Environment(const char* name) {
  const auto* value = std::getenv(name);
  return value ? value : "";
}

std::string Utf8Path(const std::filesystem::path& path) {
  const auto text = path.u8string();
  return {text.begin(), text.end()};
}

std::string Localized(std::string_view en, std::string_view es) {
  return std::string(MenuLang() == Lang::kEs ? es : en);
}

std::string CoopCheck() {
  const auto hash = rex::glue::script_patch::CoopStackHash();
  if (hash) return rex::glue::script_patch::FormatGroupHash(hash);
  return Localized("Not used with 2 players", "No se usa con 2 jugadores");
}

std::string HostResultText(const options_actions::HostResult& result) {
  if (!result.ok) return "Error: " + result.error;
  if (result.mode == "Start")
    return Localized("Open until ", "Abierto hasta ") + values_text::LeaseTime(result.lease_end) +
           Localized(" at ", " en ") + result.peer_name;
  if (result.mode == "Status" && result.open_ports > 0)
    return Localized("Open (", "Abierto (") + std::to_string(result.open_ports) +
           Localized(" ports)", " puertos)");
  return Localized("Closed", "Cerrado");
}

std::string HostStatus() {
  auto& host = options_actions::Host();
  if (host.Busy()) return Localized("Working…", "Trabajando…");
  const auto last = host.Last();
  if (!last) return Localized("Closed", "Cerrado");
  return HostResultText(*last);
}

#ifdef _WIN32
std::string UpdateStatus() {
  const auto snapshot = update::GetSnapshot();
  if (update::IsNewer(snapshot.release.tag, update::CurrentVersion()))
    return snapshot.release.tag + Localized(" available", " disponible");
  if (snapshot.failed) return Localized("Check failed (offline?)", "Error al buscar (¿sin conexión?)");
  if (!snapshot.checked) return Localized("Not checked yet", "Sin comprobar todavía");
  return Localized("Up to date (v", "Al día (v") + update::CurrentVersion() + ")";
}

std::string TextureBuildInfo() {
  const auto scale = texture_pack_plan::BuildScale(rex::cvar::Query<int32_t>("gow2_texture_build_scale"));
  const bool bc7 = rex::cvar::Query<std::string>("gow2_texture_build_quality") == "bc7";
  const auto multiplier = std::to_string(texture_pack_plan::MemoryMultiplier(scale, bc7));
  return std::to_string(texture_pack_builder::DumpCount()) +
         Localized(" · about " + multiplier + "x the video memory",
                   " · cerca de " + multiplier + "x la memoria de video");
}

std::string TextureBuildStatus() {
  using texture_pack_builder::State;
  const auto snapshot = texture_pack_builder::GetSnapshot();
  switch (snapshot.state) {
    case State::kIdle: return Localized("Not started", "Sin empezar");
    case State::kRunning:
      if (snapshot.total == 0) return Localized("Preparing…", "Preparando…");
      return Localized("Building ", "Creando ") + std::to_string(snapshot.done) + " / " +
             std::to_string(snapshot.total);
    case State::kDone:
      return Localized("Done: pick " + snapshot.message + " in Texture pack",
                       "Listo: elige " + snapshot.message + " en Pack de texturas");
    case State::kFailed: return Localized("Failed: ", "Falló: ") + snapshot.message;
    case State::kCancelled: return Localized("Cancelled", "Cancelado");
  }
  return {};
}
#endif

std::optional<toml_edit::ValueKind> PersistKind(rex::cvar::FlagType type) {
  using enum rex::cvar::FlagType;
  using enum toml_edit::ValueKind;
  switch (type) {
    case Boolean: return kBool;
    case Int32:
    case Int64:
    case Uint32:
    case Uint64: return kInt;
    case Double: return kFloat;
    case String: return kString;
    case Command: return std::nullopt;
  }
  return std::nullopt;
}

void AppendAssignment(std::string_view name, std::vector<toml_edit::Assignment>* out) {
  const auto* flag = rex::cvar::GetFlagInfo(name);
  if (!flag) return;
  const auto kind = PersistKind(flag->type);
  if (!kind) return;
  auto value = CurrentValue(name);
  if (*kind == toml_edit::ValueKind::kFloat) value = values_text::FloatText(value);
  out->push_back({std::string(name), *kind, std::move(value)});
}

bool ReadConfig(const std::filesystem::path& path, std::string* text, std::string* error) {
  if (toml_edit::ReadFile(path, text)) return true;
  std::error_code ec;
  const auto exists = std::filesystem::exists(path, ec);
  if (!ec && !exists) return true;
  *error = "Could not read " + Utf8Path(path);
  if (ec) *error += ": " + ec.message();
  return false;
}

bool HasFidelityFx() {
  const auto* flag = rex::cvar::GetFlagInfo("present_effect");
  if (!flag) return false;
  const auto& allowed = flag->constraints.allowed_values;
  return std::find(allowed.begin(), allowed.end(), "fsr") != allowed.end();
}

// Registered only by a runtime built with NVIDIA's SDK (SDK patch 0030), so self-built DLSS DLLs next to
// the release exe get the menu entry too (issue #18).
bool HasDlss() {
  return rex::cvar::GetFlagInfo("gow2_dlss_runtime") != nullptr;
}

std::string SpanishChoiceLabel(std::string_view label) {
  static constexpr std::pair<std::string_view, std::string_view> prefixes[] = {
      {"Main display", "Pantalla principal"},
      {"Display ", "Pantalla "},
      {"Automatic", "Automática"},
      {"Default (this device)", "Predeterminada (este equipo)"},
      {"Next to the game (portable)", "Junto al juego (portátil)"},
      {"Custom (", "Personalizada ("},
  };
  for (const auto& [en, es] : prefixes) {
    if (label.starts_with(en)) return std::string(es) + std::string(label.substr(en.size()));
  }
  return std::string(label);
}

void AddChoices(OwnedContext* out, std::string_view id,
                const std::vector<options_actions::NamedValue>& choices) {
  auto& owned = out->ctx.dynamic_choices[std::string(id)];
  for (const auto& choice : choices) {
    out->text.push_back(choice.value);
    const std::string_view value = out->text.back();
    out->text.push_back(choice.label);
    const std::string_view label = out->text.back();
    out->text.push_back(SpanishChoiceLabel(choice.label));
    const std::string_view label_es = out->text.back();
    owned.push_back({value, label, label_es});
  }
}

#ifdef _WIN32
std::vector<options_actions::NamedValue> TexturePackChoices() {
  const auto exe_dir = rex::filesystem::GetExecutablePath().parent_path();
  // "" is the default and what old gow2.toml files hold: without its own choice the row shows no value.
  std::vector<options_actions::NamedValue> choices{
      {"", Localized("Automatic (texture_replace)", "Automático (texture_replace)")},
      {"off", Localized("Off", "Desactivado")}};
  if (texture_packs::IsPackRoot(exe_dir / "textures"))
    choices.push_back({"textures", Localized("textures folder", "Carpeta textures")});
  for (const auto& name : texture_packs::PackNames(exe_dir / "texture_packs")) choices.push_back({name, name});
  return choices;
}
#endif

void AddUnavailable(Context* ctx) {
  for (const auto& row : Catalog()) {
    if (IsReadOnly(row.id)) continue;
    if (!CompositeNames(row.id).empty()) continue;
    if (CvarName(row.id) != row.id) continue;
    if (!rex::cvar::GetFlagInfo(row.id)) ctx->unavailable.emplace(row.id);
  }
}

Lang SystemLang() {
#ifdef _WIN32
  return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_SPANISH ? Lang::kEs : Lang::kEn;
#elif defined(__ANDROID__)
  char locale[PROP_VALUE_MAX]{};
  __system_property_get("persist.sys.locale",locale);
  if (!locale[0]) __system_property_get("ro.product.locale",locale);
  return std::string_view(locale).starts_with("es") ? Lang::kEs : Lang::kEn;
#else
  if (Environment("LC_ALL").starts_with("es")) return Lang::kEs;
  if (Environment("LANG").starts_with("es")) return Lang::kEs;
  return Lang::kEn;
#endif
}

}  // namespace

CvarValues::CvarValues(GamePaths paths) : paths_(std::move(paths)) {}

void CvarValues::set_paths(GamePaths paths) { paths_ = std::move(paths); }

std::string CvarValues::Get(std::string_view id) const {
  if (id == "upscale_ratio")
    return values_text::UpscaleRatio(CurrentValue("resolution_scale"),
                                     Environment("GOW2_TARGET_W"), Environment("GOW2_TARGET_H"));
  if (id == "coop_check") return CoopCheck();
  if (id == "host_status") return HostStatus();
#ifdef _WIN32
  if (id == "update_status") return UpdateStatus();
  if (id == "texture_build_info") return TextureBuildInfo();
  if (id == "texture_build_status") return TextureBuildStatus();
#endif
  if (id == "save_dir_effective") return Utf8Path(paths_.user_data_root);
  if (id == "config_path") return Utf8Path(paths_.config_path);
  if (id == "version") return GOW2_VERSION;
#if defined(__ANDROID__)
  if (id == "menu_shortcut") return Localized("Gear · Back + Start", "Engranaje · Back + Start");
#else
  if (id == "menu_shortcut") return "F10 · Back + Start";
#endif
  if (IsReadOnly(id)) return {};
  return RowValue(id, CurrentValue);
}

bool CvarValues::Set(std::string_view id, std::string_view value) {
  if (!SetRow(id, value)) return false;
  REXLOG_INFO("PCOPTIONS set {} = {}", id, value);
  return true;
}

std::string CvarValues::Default(std::string_view id) const {
  if (IsReadOnly(id)) return {};
  if (id == "gow2_gfx_preset") return "auto";
  return RowValue(id, DefaultValue);
}

std::vector<toml_edit::Assignment> Persist(std::string_view id) {
  std::vector<toml_edit::Assignment> out;
  if (IsReadOnly(id)) return out;
  auto names = CompositeNames(id);
  if (names.empty()) names.push_back(CvarName(id));
  for (const auto name : names) AppendAssignment(name, &out);
  return out;
}

bool SaveRows(const std::filesystem::path& config_path,
              const std::set<std::string, std::less<>>& ids, std::string* error) {
  std::string failure;
  std::string text;
  std::vector<toml_edit::Assignment> assignments;
  for (const auto& id : ids) {
    const auto row = Persist(id);
    assignments.insert(assignments.end(), row.begin(), row.end());
  }
  bool ok = ReadConfig(config_path, &text, &failure);
  if (ok) ok = toml_edit::WriteFileAtomic(config_path, toml_edit::Upsert(text, assignments), &failure);
  if (error) *error = failure;
  if (!ok) {
    REXLOG_WARN("PCOPTIONS save failed: {}", failure);
    return false;
  }
  REXLOG_INFO("PCOPTIONS saved {} settings to {}", assignments.size(), Utf8Path(config_path));
  return true;
}

void BuildContext(bool touch_controls, OwnedContext* out) {
  if (!out) return;
  out->ctx = {};
  out->text.clear();
#ifdef _WIN32
  out->ctx.platform = kWindows;
#else
  out->ctx.platform = kAndroid;
#endif
  out->ctx.touch = touch_controls;
  out->ctx.fidelityfx = HasFidelityFx();
  out->ctx.dlss = HasDlss();
  out->ctx.native_mouse = rex::glue::NativeMouseAimInstalled();
  out->ctx.advanced = CurrentValue("gow2_menu_advanced") == "true";
  out->ctx.game_running = rex::Runtime::instance() != nullptr;
  out->ctx.lang = MenuLang();
  AddChoices(out, "monitor", options_actions::ListMonitors());
  AddChoices(out, "gow2_gpu", options_actions::ListGpus());
  AddChoices(out, "gow2_save_dir", options_actions::ListSaveFolders(CurrentValue("gow2_save_dir")));
#ifdef _WIN32
  AddChoices(out, "gow2_texture_pack", TexturePackChoices());
  if (!texture_pack_builder::ToolsPresent()) out->ctx.disabled.emplace("gow2_build_texture_pack");
#endif
  AddUnavailable(&out->ctx);
}

Lang MenuLang() {
  const auto language = CurrentValue("gow2_menu_language");
  if (language == "en") return Lang::kEn;
  if (language == "es") return Lang::kEs;
  return SystemLang();
}

}  // namespace rex::glue::options
