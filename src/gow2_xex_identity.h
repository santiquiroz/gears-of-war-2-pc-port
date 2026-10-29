// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Which default.xex the player has: a 64-bit FNV-1a of the whole file, compared with the US retail
// file this port was recompiled from. Header only so a host test can check it.
#pragma once

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace rex::glue::xex_identity {

inline constexpr uint64_t kUsRetailFnv1a64 = 0xFAC26BECC11DA6A6ull;
inline constexpr uint64_t kFnv1a64Basis = 14695981039346656037ull;

constexpr uint64_t Fnv1a64Update(uint64_t hash, std::span<const uint8_t> bytes) {
  for (uint8_t b : bytes) {
    hash ^= b;
    hash *= 1099511628211ull;
  }
  return hash;
}

inline std::optional<uint64_t> HashFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return std::nullopt;
  std::vector<char> chunk(1024 * 1024);
  uint64_t hash = kFnv1a64Basis;
  while (file) {
    file.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
    const std::streamsize got = file.gcount();
    if (file.bad() || (!file && !file.eof())) return std::nullopt;
    hash = Fnv1a64Update(hash, {reinterpret_cast<const uint8_t*>(chunk.data()), static_cast<size_t>(got)});
  }
  return hash;
}

struct XexIdentity {
  bool is_us_retail;
  std::string text;
};

inline std::string Hex16(uint64_t value) {
  char buf[17];
  std::snprintf(buf, sizeof(buf), "%016llX", static_cast<unsigned long long>(value));
  return buf;
}

inline XexIdentity DescribeXex(std::optional<uint64_t> hash) {
  if (!hash) return {false, "XEX default.xex could not be read"};
  if (*hash == kUsRetailFnv1a64)
    return {true, "XEX default.xex fnv1a64 FAC26BECC11DA6A6: the US retail release this port was recompiled from"};
  return {false, "XEX default.xex fnv1a64 " + Hex16(*hash) +
                     ": not the US retail release (FAC26BECC11DA6A6) this port was recompiled from; other releases "
                     "(PAL, Japan, demos) are untested and can crash at random points"};
}

}  // namespace rex::glue::xex_identity
