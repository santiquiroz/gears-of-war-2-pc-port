// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_xex_identity.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string_view>

int g_checks = 0;
int g_failures = 0;
void Expect(bool ok, std::string_view what) {
  ++g_checks;
  if (ok) return;
  ++g_failures;
  std::printf("FAIL: %.*s\n", int(what.size()), what.data());
}

std::span<const uint8_t> Bytes(std::string_view s) {
  return {reinterpret_cast<const uint8_t*>(s.data()), s.size()};
}

int main() {
  using namespace rex::glue::xex_identity;
  Expect(Fnv1a64Update(kFnv1a64Basis, {}) == 0xCBF29CE484222325ull, "empty input is the offset basis");
  Expect(Fnv1a64Update(kFnv1a64Basis, Bytes("a")) == 0xAF63DC4C8601EC8Cull, "fnv1a64 of a");
  Expect(Fnv1a64Update(kFnv1a64Basis, Bytes("foobar")) == 0x85944171F73967E8ull, "fnv1a64 of foobar");

  const std::filesystem::path temp = std::filesystem::temp_directory_path() / "gow2_xex_identity_test.bin";
  {
    std::ofstream out(temp, std::ios::binary);
    out << "foobar";
  }
  Expect(HashFile(temp) == 0x85944171F73967E8ull, "HashFile reads the whole file");
  std::error_code ec;
  std::filesystem::remove(temp, ec);
  Expect(!HashFile(temp).has_value(), "HashFile on a missing file is nullopt");

  const XexIdentity us = DescribeXex(0xFAC26BECC11DA6A6ull);
  Expect(us.is_us_retail, "the US retail hash is recognised");
  Expect(us.text.starts_with("XEX default.xex fnv1a64 FAC26BECC11DA6A6: the US retail"), "US retail text");
  const XexIdentity other = DescribeXex(0x85944171F73967E8ull);
  Expect(!other.is_us_retail, "another hash is not US retail");
  Expect(other.text.find("85944171F73967E8") != std::string::npos, "other text has the hash");
  Expect(other.text.find("not the US retail release") != std::string::npos, "other text says not US retail");
  const XexIdentity none = DescribeXex(std::nullopt);
  Expect(!none.is_us_retail, "unreadable is not US retail");
  Expect(none.text == "XEX default.xex could not be read", "unreadable text");
  std::printf("xex_identity_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
