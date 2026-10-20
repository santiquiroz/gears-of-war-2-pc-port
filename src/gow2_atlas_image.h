// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Small RGBA8 image helpers for the keycap atlas: read and write the R8G8B8A8 DDS files the pack
// tools make (a DX10 header and a full mip chain), and blit one image onto another. Pure, tested
// on the host.
#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace rex::glue::atlas {

struct Image {
  int width = 0, height = 0;
  std::vector<uint8_t> rgba;  // straight (not premultiplied) alpha, tightly packed rows
};

// Mip 0 of an R8G8B8A8 DDS with a DX10 header (148 bytes before the data). `header` receives those
// bytes so a file of the same size and mip count can be written back.
bool ReadDds(const std::filesystem::path& path, Image* mip0, std::vector<uint8_t>* header);

// `header` as ReadDds gave it, then `image` and the mip chain down to 1x1 (box filter on
// premultiplied colour).
bool WriteDds(const std::filesystem::path& path, const std::vector<uint8_t>& header, const Image& image);

// `src`'s region (sx, sy, sw, sh) scaled to (dw x dh) at (dx, dy) of `dst`, area-averaged on
// premultiplied colour and blended over the destination.
void Blit(Image& dst, const Image& src, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);

// Number of mip levels of a width x height image down to 1x1.
int MipCount(int width, int height);

}  // namespace rex::glue::atlas
