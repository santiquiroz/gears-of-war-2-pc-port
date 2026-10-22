// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// The arithmetic of the widescreen support (src/gow2_widescreen.cpp), header only so a host test can
// check it: which aspect a setting names, how wide the game's back buffer becomes, how much shadow
// map still fits the EDRAM, and how the field of view widens.
#pragma once

#include <cmath>
#include <cstdlib>
#include <string>

namespace rex::glue::widescreen {

constexpr int kOriginalWidth = 1280;  // what the game builds at 16:9
constexpr int kTileWidth = 80;        // an EDRAM tile at 32 bits per pixel is 80 pixels wide
constexpr double kAutoMinAspect = 1.85;  // above 16:9 (1.78) with room for 16:9.5 panels
constexpr int kMinWidth = 960;
constexpr int kMaxWidth = 1680;       // 21:9; scene colour and depth then use 9.5 of the 10 MB of EDRAM
constexpr long long kEdramBytes = 10LL * 1024 * 1024;

inline bool GpuNameHas(const std::string& gpu_name, const char* vendor) {
  std::string upper;
  for (char c : gpu_name) upper += char((c >= 'a' && c <= 'z') ? c - 32 : c);
  return upper.find(vendor) != std::string::npos;
}

// Patch 0026 widens the EDRAM only on the pixel shader interlock (ROV) render target path. With "auto",
// widescreen therefore turns on only where that path is in use: render_target_path_d3d12 = "rov", or the
// automatic choice on NVIDIA and Intel; on AMD the automatic choice is host render targets ("rtv"), which
// still wrap the half-resolution targets onto the scene depth above 1280 wide. The native renderer keeps host
// render targets without that wrap, so it widens on AMD too.
inline bool AutoWidescreenSafe(bool native_renderer, const std::string& render_target_path, const std::string& gpu_name) {
  if (native_renderer) return true;
  if (render_target_path == "rov") return true;
  if (render_target_path == "rtv") return false;
  return GpuNameHas(gpu_name, "NVIDIA") || GpuNameHas(gpu_name, "INTEL");
}

// On AMD, "auto" asks for the ROV path when the player left the render target path unset (issue #11). Measured on
// an RX 7800 XT (2026-10-21): ROV draws the chapter 2 hospital right at 16:9 and at 1680 wide and holds the game's
// 30 fps; with the frame rate unlocked at resolution_scale 2 it is about 19 % slower than host render targets.
// The native renderer keeps the host render targets, so nothing switches.
inline bool AutoWidescreenSwitchesToRov(bool native_renderer, const std::string& render_target_path,
                                        const std::string& gpu_name) {
  if (native_renderer) return false;
  return render_target_path.empty() && (GpuNameHas(gpu_name, "AMD") || GpuNameHas(gpu_name, "RADEON"));
}

// "off", "", "0", "false" -> 0; "auto" -> display_aspect when it is wider than 16:9, else 0; "21:9" or "2.39" -> that
// aspect. Anything outside 1.2 .. 4 names no widescreen (0).
inline double ParseAspect(const std::string& text, double display_aspect) {
  if (text.empty() || text == "off" || text == "0" || text == "false") return 0.0;
  double a = 0.0;
  if (text == "auto") {
    // A 16:9 (or narrower) display is what the game already fills; only a wider one switches it on.
    a = display_aspect >= kAutoMinAspect ? display_aspect : 0.0;
  } else if (const size_t colon = text.find(':'); colon != std::string::npos) {
    const double w = std::atof(text.substr(0, colon).c_str());
    const double h = std::atof(text.substr(colon + 1).c_str());
    if (w > 0 && h > 0) a = w / h;
  } else {
    a = std::atof(text.c_str());
  }
  return (a >= 1.2 && a <= 4.0) ? a : 0.0;
}

// The back buffer width for an aspect at the given height: a multiple of 80 between 960 and 1680, or 0
// when it is what the game builds anyway (1280 at 720 lines) or the aspect is 0.
inline int BackBufferWidth(double aspect, int height) {
  if (aspect <= 0.0) return 0;
  int w = int(std::lround(aspect * height / double(kTileWidth))) * kTileWidth;
  if (w < kMinWidth) w = kMinWidth;
  if (w > kMaxWidth) w = kMaxWidth;
  return (w == kOriginalWidth && height == 720) ? 0 : w;
}

// The largest square 32-bit shadow depth map (a multiple of 32 pixels) that fits what is left of the
// EDRAM after the scene colour and depth, or 0 when the game's own 800 fits.
inline int MaxShadowResolution(int width, int height) {
  if (width <= 0) return 0;
  const long long left = kEdramBytes - 8LL * width * height;
  const int size = int(std::sqrt(double(left > 0 ? left : 0) / 4.0)) / 32 * 32;
  if (size >= 800) return 0;
  return size < 128 ? 128 : size;
}

// EDRAM tiles (80x16 samples at 32 bits) the game's per-frame layout reaches at a back buffer of width x height:
// scene colour and depth, then the half-resolution passes behind them (a depth buffer and two colour targets, each
// half the size with the pitch rounded up to whole tiles). Measured with the GPU trace: 1680x704 sets render target
// bases up to tile 2332 and ends at 2574; 1360x704 ends at 2090. The console has 2048 tiles; the pixel shader
// interlock path of sdk/patches/0021 emulates 4096.
constexpr int kConsoleEdramTiles = 2048;
constexpr int kVirtualEdramTiles = 4096;
inline int EdramTilesNeeded(int width, int height) {
  const auto tiles = [](int w, int h) { return ((w + kTileWidth - 1) / kTileWidth) * ((h + 15) / 16); };
  return 2 * tiles(width, height) + 3 * tiles(width / 2, height / 2);
}

// The widest multiple of 80 whose layout fits the given number of EDRAM tiles.
inline int MaxWidthForEdram(int edram_tiles, int height) {
  int w = kTileWidth;
  while (EdramTilesNeeded(w + kTileWidth, height) <= edram_tiles) w += kTileWidth;
  return w;
}

// The camera's half field of view (radians) widened for a back buffer of the given width: the tangent
// grows by width / 1280 so the vertical view stays the one of the 16:9 game.
inline double WidenHalfFov(double half_fov, int width) {
  if (width <= 0) return half_fov;
  return std::atan(std::tan(half_fov) * (double(width) / double(kOriginalWidth)));
}

}  // namespace rex::glue::widescreen
