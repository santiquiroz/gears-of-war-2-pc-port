// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_sha256.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void Expect(bool ok, const char* what) {
  if (ok) return;
  ++g_failures;
  std::fprintf(stderr, "FAIL: %s\n", what);
}

std::string Hash(const void* data, size_t size) {
  rex::glue::sha256::Hasher hasher;
  hasher.Update(data, size);
  return rex::glue::sha256::ToHex(hasher.Finish());
}

void TestVectors() {
  Expect(Hash("", 0) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
         "empty message vector");
  Expect(Hash("abc", 3) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
         "abc vector");
  const std::string long_message = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  Expect(Hash(long_message.data(), long_message.size()) ==
             "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
         "long message vector");
}

void TestMillionBytesAndUnevenChunks() {
  const std::string input(1000000, 'a');
  const std::string expected = "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0";
  Expect(Hash(input.data(), input.size()) == expected, "million a vector");

  rex::glue::sha256::Hasher hasher;
  constexpr size_t chunks[] = {1, 63, 64, 65};
  size_t offset = 0;
  size_t chunk_index = 0;
  while (offset < input.size()) {
    const size_t count = std::min(chunks[chunk_index % 4], input.size() - offset);
    hasher.Update(input.data() + offset, count);
    offset += count;
    ++chunk_index;
  }
  Expect(rex::glue::sha256::ToHex(hasher.Finish()) == expected, "million a uneven chunks");
}

void TestHashFile(const fs::path& root) {
  const fs::path file_path = root / "content.bin";
  {
    std::ofstream file(file_path, std::ios::binary);
    file << "abc";
  }
  Expect(rex::glue::sha256::HashFileHex(file_path) ==
             std::optional<std::string>("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),
         "file hash matches in-memory digest");
  Expect(!rex::glue::sha256::HashFileHex(root / "missing.bin"), "missing file is unreadable");
}

}  // namespace

int main() {
  const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto root = fs::temp_directory_path() / ("gow2-sha256-test-" + std::to_string(nonce));
  std::error_code error;
  fs::create_directories(root, error);
  if (error) {
    std::fprintf(stderr, "FAIL: create temporary directory: %s\n", error.message().c_str());
    return 1;
  }
  TestVectors();
  TestMillionBytesAndUnevenChunks();
  TestHashFile(root);
  fs::remove_all(root, error);
  if (error) {
    std::fprintf(stderr, "FAIL: remove temporary directory: %s\n", error.message().c_str());
    return 1;
  }
  std::printf("sha256_test: %d failures\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}