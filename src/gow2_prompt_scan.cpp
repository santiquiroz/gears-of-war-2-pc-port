// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_prompt_scan.h"

#include <algorithm>
#include <cstring>

namespace rex::glue::promptscan {
namespace {

uint32_t Be32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

std::vector<uint8_t> Utf16BeWithNul(const std::u16string& s) {
  std::vector<uint8_t> b;
  b.reserve(s.size() * 2 + 2);
  for (char16_t c : s) {
    b.push_back(uint8_t(c >> 8));
    b.push_back(uint8_t(c));
  }
  b.push_back(0);
  b.push_back(0);
  return b;
}

}  // namespace

std::vector<StringHit> FindStrings(const uint8_t* mem, size_t size, uint32_t guest_addr,
                                   const std::vector<std::u16string>& wanted) {
  std::vector<StringHit> hits;
  if (wanted.empty() || (guest_addr & 1)) return hits;
  std::vector<std::vector<uint8_t>> bytes;
  size_t shortest = SIZE_MAX;
  for (const auto& w : wanted) {
    bytes.push_back(Utf16BeWithNul(w));
    shortest = std::min(shortest, bytes.back().size());
  }
  for (size_t i = 0; i + shortest <= size; i += 2) {
    // Every wanted text starts with '<' (the font tag): a cheap test before the comparisons.
    if (mem[i] != 0 || mem[i + 1] != '<') continue;
    for (size_t k = 0; k < bytes.size(); ++k) {
      const auto& b = bytes[k];
      if (i + b.size() <= size && std::memcmp(mem + i, b.data(), b.size()) == 0)
        hits.push_back({guest_addr + uint32_t(i), k});
    }
  }
  return hits;
}

std::vector<HeaderHit> FindHeaders(const uint8_t* mem, size_t size, uint32_t guest_addr,
                                   const std::unordered_map<uint32_t, uint32_t>& strings) {
  std::vector<HeaderHit> hits;
  if (strings.empty() || (guest_addr & 3)) return hits;
  uint32_t lo = UINT32_MAX, hi = 0;
  for (const auto& kv : strings) {
    lo = std::min(lo, kv.first);
    hi = std::max(hi, kv.first);
  }
  for (size_t i = 0; i + 12 <= size; i += 4) {
    const uint32_t chars = Be32(mem + i);
    if (chars < lo || chars > hi) continue;
    const auto it = strings.find(chars);
    if (it == strings.end()) continue;
    const uint32_t num = Be32(mem + i + 4), max = Be32(mem + i + 8);
    if (num == it->second && max >= num) hits.push_back({guest_addr + uint32_t(i), chars, num, max});
  }
  return hits;
}

std::vector<CharTableHit> FindCharTable(const uint8_t* mem, size_t size, uint32_t guest_addr) {
  // StartU, StartV, USize, VSize of 'A'..'E' in the retail font.
  static const int32_t kCells[5][4] = {
      {201, 55, 24, 26}, {227, 55, 26, 26}, {0, 82, 18, 26}, {21, 83, 18, 26}, {41, 83, 19, 26}};
  std::vector<CharTableHit> hits;
  if (guest_addr & 3) return hits;
  for (uint32_t stride : {24u, 20u, 21u, 28u, 32u}) {
    const size_t span = size_t(stride) * 4 + 16;
    for (size_t i = 0; i + span <= size; i += 4) {
      if (Be32(mem + i) != uint32_t(kCells[0][0])) continue;  // cheap first test
      bool ok = true;
      for (size_t c = 0; c < 5 && ok; ++c)
        for (size_t f = 0; f < 4 && ok; ++f)
          ok = Be32(mem + i + c * stride + f * 4) == uint32_t(kCells[c][f]);
      if (ok) hits.push_back({guest_addr + uint32_t(i) - 65 * stride, stride});
    }
  }
  return hits;
}

}  // namespace rex::glue::promptscan
