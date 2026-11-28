// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_sha256.h"

#include <algorithm>
#include <array>
#include <fstream>

namespace rex::glue::sha256 {

namespace {

constexpr std::array<std::uint32_t, 64> kRoundConstants = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
    0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
    0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
    0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
    0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
    0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
    0xc67178f2};

constexpr std::array<std::uint32_t, 8> kInitialState = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

std::uint32_t RotateRight(std::uint32_t value, unsigned int amount) {
  return (value >> amount) | (value << (32 - amount));
}

std::uint32_t ReadWord(const std::uint8_t* bytes) {
  return (static_cast<std::uint32_t>(bytes[0]) << 24) |
      (static_cast<std::uint32_t>(bytes[1]) << 16) |
      (static_cast<std::uint32_t>(bytes[2]) << 8) | static_cast<std::uint32_t>(bytes[3]);
}

}  // namespace

Hasher::Hasher() : state_(kInitialState) {}

void Hasher::ProcessBlock(const std::uint8_t* block) {
  std::array<std::uint32_t, 64> words{};
  for (std::size_t i = 0; i < 16; ++i) words[i] = ReadWord(block + i * 4);
  for (std::size_t i = 16; i < words.size(); ++i) {
    const std::uint32_t s0 = RotateRight(words[i - 15], 7) ^ RotateRight(words[i - 15], 18) ^
        (words[i - 15] >> 3);
    const std::uint32_t s1 = RotateRight(words[i - 2], 17) ^ RotateRight(words[i - 2], 19) ^
        (words[i - 2] >> 10);
    words[i] = words[i - 16] + s0 + words[i - 7] + s1;
  }

  auto working = state_;
  for (std::size_t i = 0; i < words.size(); ++i) {
    const std::uint32_t sum1 = RotateRight(working[4], 6) ^ RotateRight(working[4], 11) ^
        RotateRight(working[4], 25);
    const std::uint32_t choose = (working[4] & working[5]) ^ (~working[4] & working[6]);
    const std::uint32_t temp1 = working[7] + sum1 + choose + kRoundConstants[i] + words[i];
    const std::uint32_t sum0 = RotateRight(working[0], 2) ^ RotateRight(working[0], 13) ^
        RotateRight(working[0], 22);
    const std::uint32_t majority = (working[0] & working[1]) ^ (working[0] & working[2]) ^
        (working[1] & working[2]);
    const std::uint32_t temp2 = sum0 + majority;
    working[7] = working[6];
    working[6] = working[5];
    working[5] = working[4];
    working[4] = working[3] + temp1;
    working[3] = working[2];
    working[2] = working[1];
    working[1] = working[0];
    working[0] = temp1 + temp2;
  }
  for (std::size_t i = 0; i < state_.size(); ++i) state_[i] += working[i];
}

void Hasher::Update(const void* data, std::size_t size) {
  if (finished_ || size == 0 || data == nullptr) return;
  const auto* bytes = static_cast<const std::uint8_t*>(data);
  total_bytes_ += static_cast<std::uint64_t>(size);

  if (buffered_ != 0) {
    const std::size_t count = std::min(size, buffer_.size() - buffered_);
    std::copy_n(bytes, count, buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_));
    buffered_ += count;
    bytes += count;
    size -= count;
    if (buffered_ == buffer_.size()) {
      ProcessBlock(buffer_.data());
      buffered_ = 0;
    }
  }
  while (size >= buffer_.size()) {
    ProcessBlock(bytes);
    bytes += buffer_.size();
    size -= buffer_.size();
  }
  if (size != 0) {
    std::copy_n(bytes, size, buffer_.begin());
    buffered_ = size;
  }
}

std::array<std::uint8_t, 32> Hasher::Finish() {
  if (finished_) return digest_;
  const std::uint64_t bit_length = total_bytes_ * 8;
  buffer_[buffered_++] = 0x80;
  if (buffered_ > 56) {
    std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_), buffer_.end(), 0);
    ProcessBlock(buffer_.data());
    buffered_ = 0;
  }
  std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_), buffer_.begin() + 56, 0);
  for (std::size_t i = 0; i < 8; ++i) {
    buffer_[63 - i] = static_cast<std::uint8_t>(bit_length >> (i * 8));
  }
  ProcessBlock(buffer_.data());

  for (std::size_t i = 0; i < state_.size(); ++i) {
    digest_[i * 4] = static_cast<std::uint8_t>(state_[i] >> 24);
    digest_[i * 4 + 1] = static_cast<std::uint8_t>(state_[i] >> 16);
    digest_[i * 4 + 2] = static_cast<std::uint8_t>(state_[i] >> 8);
    digest_[i * 4 + 3] = static_cast<std::uint8_t>(state_[i]);
  }
  finished_ = true;
  return digest_;
}

std::string ToHex(const std::array<std::uint8_t, 32>& digest) {
  constexpr char kHex[] = "0123456789abcdef";
  try {
    std::string result;
    result.reserve(digest.size() * 2);
    for (std::uint8_t byte : digest) {
      result.push_back(kHex[byte >> 4]);
      result.push_back(kHex[byte & 0x0f]);
    }
    return result;
  } catch (...) {
    return {};
  }
}

std::optional<std::string> HashFileHex(const std::filesystem::path& path) {
  try {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;

    Hasher hasher;
    std::array<char, 64 * 1024> buffer{};
    while (file) {
      file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
      const std::streamsize count = file.gcount();
      if (count > 0) hasher.Update(buffer.data(), static_cast<std::size_t>(count));
    }
    if (file.bad() || !file.eof()) return std::nullopt;
    return ToHex(hasher.Finish());
  } catch (...) {
    return std::nullopt;
  }
}

}  // namespace rex::glue::sha256