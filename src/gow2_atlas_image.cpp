// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_atlas_image.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

namespace rex::glue::atlas {
namespace {

constexpr size_t kHeaderBytes = 148;  // "DDS " + 124-byte header + 20-byte DX10 header
constexpr uint32_t kDxgiR8G8B8A8Unorm = 28;

uint32_t Le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24); }

Image Halve(const Image& in) {
  Image out;
  out.width = std::max(in.width / 2, 1);
  out.height = std::max(in.height / 2, 1);
  out.rgba.resize(size_t(out.width) * out.height * 4);
  for (int y = 0; y < out.height; ++y) {
    for (int x = 0; x < out.width; ++x) {
      float sum[4] = {0, 0, 0, 0};
      int n = 0;
      for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx) {
          const int sx = std::min(x * 2 + dx, in.width - 1), sy = std::min(y * 2 + dy, in.height - 1);
          const uint8_t* p = &in.rgba[(size_t(sy) * in.width + sx) * 4];
          const float a = p[3] / 255.0f;
          sum[0] += p[0] * a;
          sum[1] += p[1] * a;
          sum[2] += p[2] * a;
          sum[3] += a;
          ++n;
        }
      uint8_t* q = &out.rgba[(size_t(y) * out.width + x) * 4];
      const float a = sum[3] / n;
      for (int c = 0; c < 3; ++c) q[c] = a > 1e-4f ? uint8_t(std::lround(std::min(255.0f, sum[c] / sum[3]))) : 0;
      q[3] = uint8_t(std::lround(a * 255.0f));
    }
  }
  return out;
}

}  // namespace

int MipCount(int width, int height) {
  int n = 1;
  while (width > 1 || height > 1) {
    width = std::max(width / 2, 1);
    height = std::max(height / 2, 1);
    ++n;
  }
  return n;
}

bool ReadDds(const std::filesystem::path& path, Image* mip0, std::vector<uint8_t>* header) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  std::vector<uint8_t> head(kHeaderBytes);
  if (!f.read(reinterpret_cast<char*>(head.data()), std::streamsize(head.size()))) return false;
  if (std::memcmp(head.data(), "DDS ", 4) != 0) return false;
  const int height = int(Le32(&head[12])), width = int(Le32(&head[16]));
  if (Le32(&head[84]) != 0x30315844 /* DX10 */ || Le32(&head[128]) != kDxgiR8G8B8A8Unorm) return false;
  if (width <= 0 || height <= 0 || width > 16384 || height > 16384) return false;
  Image image;
  image.width = width;
  image.height = height;
  image.rgba.resize(size_t(width) * height * 4);
  if (!f.read(reinterpret_cast<char*>(image.rgba.data()), std::streamsize(image.rgba.size()))) return false;
  *mip0 = std::move(image);
  if (header) *header = std::move(head);
  return true;
}

bool WriteDds(const std::filesystem::path& path, const std::vector<uint8_t>& header, const Image& image) {
  if (header.size() != kHeaderBytes || image.rgba.size() != size_t(image.width) * image.height * 4) return false;
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  const std::filesystem::path tmp = path.string() + ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(header.data()), std::streamsize(header.size()));
    Image level = image;
    for (;;) {
      f.write(reinterpret_cast<const char*>(level.rgba.data()), std::streamsize(level.rgba.size()));
      if (level.width == 1 && level.height == 1) break;
      level = Halve(level);
    }
    if (!f) return false;
  }
  std::filesystem::rename(tmp, path, ec);  // replaces an existing file on Windows too
  return !ec;
}

void Blit(Image& dst, const Image& src, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
  if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
  if (sx < 0 || sy < 0 || sx + sw > src.width || sy + sh > src.height) return;
  for (int y = 0; y < dh; ++y) {
    const int ty = dy + y;
    if (ty < 0 || ty >= dst.height) continue;
    for (int x = 0; x < dw; ++x) {
      const int tx = dx + x;
      if (tx < 0 || tx >= dst.width) continue;
      // The source area this destination pixel covers.
      const float fx0 = sx + float(x) * sw / dw, fx1 = sx + float(x + 1) * sw / dw;
      const float fy0 = sy + float(y) * sh / dh, fy1 = sy + float(y + 1) * sh / dh;
      float acc[4] = {0, 0, 0, 0}, weight = 0;
      for (int py = int(std::floor(fy0)); py < int(std::ceil(fy1)) && py < src.height; ++py) {
        for (int px = int(std::floor(fx0)); px < int(std::ceil(fx1)) && px < src.width; ++px) {
          const float w = (std::min(fx1, px + 1.0f) - std::max(fx0, float(px))) *
                          (std::min(fy1, py + 1.0f) - std::max(fy0, float(py)));
          if (w <= 0) continue;
          const uint8_t* p = &src.rgba[(size_t(py) * src.width + px) * 4];
          const float a = p[3] / 255.0f;
          acc[0] += w * p[0] * a;
          acc[1] += w * p[1] * a;
          acc[2] += w * p[2] * a;
          acc[3] += w * a;
          weight += w;
        }
      }
      if (weight <= 0) continue;
      const float sa = acc[3] / weight;  // source alpha
      if (sa <= 1e-4f) continue;
      float src_rgb[3] = {acc[0] / acc[3], acc[1] / acc[3], acc[2] / acc[3]};
      uint8_t* q = &dst.rgba[(size_t(ty) * dst.width + tx) * 4];
      const float da = q[3] / 255.0f;
      const float oa = sa + da * (1 - sa);
      for (int c = 0; c < 3; ++c) {
        const float v = (src_rgb[c] * sa + q[c] * da * (1 - sa)) / oa;
        q[c] = uint8_t(std::lround(std::clamp(v, 0.0f, 255.0f)));
      }
      q[3] = uint8_t(std::lround(oa * 255.0f));
    }
  }
}

}  // namespace rex::glue::atlas
