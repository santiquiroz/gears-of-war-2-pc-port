// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rex::glue::graphics_quality {

struct Setting {
  std::string_view key, id, retail, low, medium;
  bool boolean;
  std::vector<std::string_view> choices;
};

inline const std::vector<Setting>& Settings() {
  static const std::vector<Setting> settings = {
      {"LightEnvironmentShadows", "gow2_gfx_light_environment_shadows", "True", "off", "off", true, {"auto", "on", "off"}},
      {"DynamicShadows", "gow2_gfx_dynamic_shadows", "True", "off", "on", true, {"auto", "on", "off"}},
      {"DynamicLights", "gow2_gfx_dynamic_lights", "True", "on", "on", true, {"auto", "on", "off"}},
      {"CompositeDynamicLights", "gow2_gfx_composite_dynamic_lights", "True", "on", "on", true, {"auto", "on", "off"}},
      {"Bloom", "gow2_gfx_bloom", "True", "off", "on", true, {"auto", "on", "off"}},
      {"UseHighQualityBloom", "gow2_gfx_use_high_quality_bloom", "True", "off", "off", true, {"auto", "on", "off"}},
      {"MotionBlur", "gow2_gfx_motion_blur", "True", "off", "off", true, {"auto", "on", "off"}},
      {"DepthOfField", "gow2_gfx_depth_of_field", "True", "off", "on", true, {"auto", "on", "off"}},
      {"AmbientOcclusion", "gow2_gfx_ambient_occlusion", "True", "off", "off", true, {"auto", "on", "off"}},
      {"LensFlares", "gow2_gfx_lens_flares", "True", "off", "on", true, {"auto", "on", "off"}},
      {"Distortion", "gow2_gfx_distortion", "True", "off", "on", true, {"auto", "on", "off"}},
      {"StaticDecals", "gow2_gfx_static_decals", "True", "on", "on", true, {"auto", "on", "off"}},
      {"DynamicDecals", "gow2_gfx_dynamic_decals", "True", "off", "on", true, {"auto", "on", "off"}},
      {"FogVolumes", "gow2_gfx_fog_volumes", "True", "off", "on", true, {"auto", "on", "off"}},
      {"SpeedTreeLeaves", "gow2_gfx_speed_tree_leaves", "True", "off", "on", true, {"auto", "on", "off"}},
      {"SpeedTreeFronds", "gow2_gfx_speed_tree_fronds", "True", "off", "on", true, {"auto", "on", "off"}},
      {"ShadowFilterQualityBias", "gow2_gfx_shadow_filter_quality_bias", "0", "-1", "0", false, {"auto", "-1", "0", "1"}},
      {"DetailMode", "gow2_gfx_detail_mode", "2", "0", "1", false, {"auto", "0", "1", "2"}},
      {"SkeletalMeshLODBias", "gow2_gfx_skeletal_mesh_lod_bias", "0", "2", "1", false, {"auto", "0", "1", "2"}},
      {"ParticleLODBias", "gow2_gfx_particle_lod_bias", "0", "2", "1", false, {"auto", "0", "1", "2"}},
      {"FoliageDrawRadiusMultiplier", "gow2_gfx_foliage_draw_radius_multiplier", "1.000000", "0.25", "0.5", false, {"auto", "0.25", "0.5", "1.000000"}},
      {"MaxShadowResolution", "gow2_gfx_max_shadow_resolution", "800", "256", "512", false, {"auto", "256", "512", "800"}},
      {"ShadowTexelsPerPixel", "gow2_gfx_shadow_texels_per_pixel", "1.27324", "0.5", "1", false, {"auto", "0.5", "1", "1.27324"}},
      {"DecalCullDistanceScale", "gow2_gfx_decal_cull_distance_scale", "1.0", "0.4", "0.7", false, {"auto", "0.4", "0.7", "1.0"}},
  };
  return settings;
}

inline std::string PresetValue(const Setting& setting, std::string_view preset) {
  if (preset == "low") return std::string(setting.low);
  if (preset == "medium") return std::string(setting.medium);
  if (preset != "high") return {};
  if (setting.boolean) return setting.retail == "True" ? "on" : "off";
  return std::string(setting.retail);
}

template <typename Read>
inline std::string DetectPreset(const Read& read, bool android = false) {
  const bool all_auto = std::all_of(Settings().begin(), Settings().end(), [&](const Setting& s) {
    return read(s.id) == "auto";
  });
  if (all_auto && !android) return "high";
  for (std::string_view preset : {"low", "medium", "high"}) {
    const bool matches = std::all_of(Settings().begin(), Settings().end(), [&](const Setting& s) {
      return read(s.id) == PresetValue(s, preset);
    });
    if (matches) return std::string(preset);
  }
  return "custom";
}

inline std::string Lower(std::string_view value) {
  std::string result(value);
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return char(std::tolower(c)); });
  return result;
}

inline bool IsBooleanKey(std::string_view key) {
  const std::string lower = Lower(key);
  for (const auto& setting : Settings())
    if (setting.boolean && Lower(setting.key) == lower) return true;
  for (std::string_view boolean : {"UnbatchedDecals", "DirectionalLightmaps", "DropParticleDistortion",
       "OnlyStreamInTextures", "FloatingPointRenderTargets", "OneFrameThreadLag", "UseVsync",
       "UpscaleScreenPercentage", "Fullscreen", "AllowD3D10", "bEnableVSMShadows",
       "bEnableBranchingPCFShadows", "bAllowBetterModulatedShadows",
       "bEnableForegroundShadowsOnWorld", "bEnableForegroundSelfShadowing", "bAllowFracturedDamage"})
    if (Lower(boolean) == lower) return true;
  return false;
}

inline std::string EngineValue(std::string_view key, std::string_view value) {
  if (!IsBooleanKey(key)) return std::string(value);
  const std::string lower = Lower(value);
  if (lower == "true" || lower == "on" || lower == "1") return "1";
  if (lower == "false" || lower == "off" || lower == "0") return "0";
  return std::string(value);
}

inline std::string Trim(std::string_view value) {
  const size_t first = value.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) return {};
  return std::string(value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1));
}

using Pairs = std::vector<std::pair<std::string, std::string>>;

inline void SetPair(Pairs* pairs, std::string_view key, std::string_view value) {
  const std::string lower = Lower(key);
  const auto existing = std::find_if(pairs->begin(), pairs->end(), [&](const auto& pair) {
    return Lower(pair.first) == lower;
  });
  const std::string engine_value = EngineValue(key, value);
  if (existing != pairs->end()) existing->second = engine_value;
  else pairs->emplace_back(key, engine_value);
}

inline void ApplyRaw(Pairs* pairs, std::string_view raw) {
  size_t start = 0;
  while (start < raw.size()) {
    const size_t end = std::min(raw.find(';', start), raw.size());
    const std::string_view item = raw.substr(start, end - start);
    const size_t equals = item.find('=');
    if (equals != std::string_view::npos) {
      const std::string key = Trim(item.substr(0, equals));
      if (!key.empty()) SetPair(pairs, key, Trim(item.substr(equals + 1)));
    }
    start = end + 1;
  }
}

template <typename Read>
inline Pairs Compose(bool android, const Read& read, std::string_view raw) {
  Pairs pairs;
  if (android) SetPair(&pairs, "LightEnvironmentShadows", "off");
  for (const auto& setting : Settings()) {
    const std::string value = read(setting.id);
    if (value == "auto") continue;
    if (std::find(setting.choices.begin(), setting.choices.end(), value) == setting.choices.end()) continue;
    SetPair(&pairs, setting.key, value);
  }
  const std::string render_scale = read("gow2_render_scale");
  if (render_scale == "85" || render_scale == "75" || render_scale == "50")
    SetPair(&pairs, "ScreenPercentage", render_scale);
  ApplyRaw(&pairs, raw);
  return pairs;
}

inline bool PadBundleToSize(std::vector<uint8_t>* bytes, uint64_t size) {
  if (bytes->size() > size) return false;
  bytes->resize(size_t(size), 0);
  return true;
}

template <typename Rebuild>
inline bool PadOrKeepRetail(std::vector<uint8_t>* bytes, uint64_t size, const Rebuild& rebuild) {
  if (PadBundleToSize(bytes, size)) return true;
  std::vector<uint8_t> retail = rebuild();
  if (!PadBundleToSize(&retail, size)) return false;
  *bytes = std::move(retail);
  return true;
}

}  // namespace rex::glue::graphics_quality
