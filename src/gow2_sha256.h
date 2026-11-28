// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace rex::glue::sha256 {

class Hasher {
 public:
  Hasher();
  void Update(const void* data, std::size_t size);
  std::array<std::uint8_t, 32> Finish();

 private:
  void ProcessBlock(const std::uint8_t* block);

  std::array<std::uint32_t, 8> state_{};
  std::array<std::uint8_t, 64> buffer_{};
  std::array<std::uint8_t, 32> digest_{};
  std::uint64_t total_bytes_ = 0;
  std::size_t buffered_ = 0;
  bool finished_ = false;
};

std::string ToHex(const std::array<std::uint8_t, 32>& digest);
std::optional<std::string> HashFileHex(const std::filesystem::path& path);

}  // namespace rex::glue::sha256