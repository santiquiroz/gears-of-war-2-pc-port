#include "gow2_precache_plan.h"
#include <iostream>
#include <limits>
using namespace rex::graphics::d3d12::precache;
int main() {
  int failed = 0, passed = 0;
  const auto check = [&](bool ok, const char* name) {
    if (ok)
      ++passed;
    else {
      ++failed;
      std::cerr << "FAIL " << name << "\n";
    }
  };
  uint64_t hash = 0;
  check(!ParseHash("0123456789abcdef", nullptr), "null output rejected");
  check(ParseHash("0123456789abcdef", &hash) && hash == 0x0123456789abcdefULL, "valid hash");
  check(ParseHash("FFFFFFFFFFFFFFFF", &hash) && hash == UINT64_MAX, "full unsigned range");
  check(!ParseHash("../evil/file.dds", &hash), "path is not a hash");
  check(!ParseHash("000000000000000", &hash), "short hash");
  check(!ParseHash("00000000000000000", &hash), "long hash");
  check(!ParseHash("g000000000000000", &hash), "invalid hex");
  check(!ParseHash("-000000000000001", &hash), "negative hash");
  const std::string log =
      "[texpack] D:\\Pack: 6000 replacements\n"
      "[texpack] replace 0000000000000001 with 4096x4096 (13 mips)\n"
      "[texpack] replace 0000000000000002 refused: bad DDS\n"
      "[texpack] replace 0000000000000003 with 1024x1024 (11 mips)\n";
  check(ParseHistory(log, {"D:\\Pack"}) == std::vector<uint64_t>({1, 3}),
        "only actual replacement entries");
  check(ParseHistory(log, {"D:\\OtherPack"}).empty(), "different pack excluded");
  check(ParseHistory(log, {"D:\\Pac"}).empty(), "pack name prefix excluded");
  check(ParseHistory("[texpack] D:\\Pack: 1 replacements\n[texpack] replace", {"D:\\Pack"}).empty(),
        "truncated active log");
  check(ParseHistory("", {"D:\\Pack"}).empty(), "no previous sessions");
  check(Prioritize({{1, 2, 3}, {3, 4, 5}}) == std::vector<uint64_t>({3, 2, 1, 5, 4}),
        "recent sessions and resources first");
  check(Prioritize({{1, 1, 2, 1}}) == std::vector<uint64_t>({1, 2}),
        "duplicates use no extra budget");
  check(Prioritize({{1, 2, 3}}, 2) == std::vector<uint64_t>({3, 2}), "candidate count bound");
  check(Prioritize({{1, 2}}, 0).empty(), "zero count bound");
  check(Fits(0, 10, 10), "exact allocation");
  check(!Fits(1, 10, 10), "budget exceeded");
  check(!Fits(UINT64_MAX, 10, UINT64_MAX), "addition cannot overflow");
  check(!Fits(10, 1, 9), "invalid used value rejected");
  check(!Fits(0, 0, 10), "zero-size file excluded");
  check(Fits(8, 2, 10), "last remaining space");
  const uint64_t gb = uint64_t(1) << 30;
  check(BudgetBytes(3072, gb - 1) == 0, "low memory disables preparation");
  check(BudgetBytes(3072, 3 * gb) == gb, "one third of free memory");
  check(BudgetBytes(3072, 12 * gb) == 3 * gb, "configured 3GB cap");
  check(BudgetBytes(768, 12 * gb) == (uint64_t(768) << 20), "small configured budget kept");
  check(BudgetBytes(99999, 12 * gb) == 3 * gb, "untrusted configuration clamped");
  for (size_t pitch : {size_t(1), size_t(12), size_t(64), size_t(256), size_t(512), size_t(513)}) {
    const size_t aligned = (pitch + 255) & ~size_t(255), rows = 7;
    std::vector<uint8_t> source(pitch * rows);
    for (size_t n = 0; n < source.size(); ++n)
      source[n] = uint8_t(n * 17 + 3);
    std::vector<uint8_t> actual(aligned * rows + 16, 0xA5), expected = actual;
    for (size_t row = 0; row < rows; ++row)
      for (size_t col = 0; col < pitch; ++col)
        expected[row * aligned + col] = source[row * pitch + col];
    CopyMipRows(actual.data(), source.data(), pitch, aligned, rows);
    check(actual == expected, "staging copy preserves data, padding and bounds");
  }
  std::cout << passed << " checks passed, " << failed << " failed\n";
  return failed ? 1 : 0;
}
