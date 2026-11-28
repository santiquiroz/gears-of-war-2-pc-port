// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_process.h"

#include <cstdio>

namespace {

int g_failures = 0;

void Expect(bool ok, const char* what) {
  if (ok) return;
  ++g_failures;
  std::fprintf(stderr, "FAIL: %s\n", what);
}

void TestQuoteWindowsArgument() {
  using rex::glue::process::QuoteWindowsArgument;
  Expect(QuoteWindowsArgument("plain") == "plain", "plain argument stays unquoted");
  Expect(QuoteWindowsArgument("two words") == "\"two words\"", "spaces are quoted");
  Expect(QuoteWindowsArgument("a\"b") == "\"a\\\"b\"", "embedded quote is escaped");
  Expect(QuoteWindowsArgument("path with space\\") == "\"path with space\\\\\"",
         "trailing backslash is doubled inside quotes");
  Expect(QuoteWindowsArgument("") == "\"\"", "empty argument is quoted");
}

}  // namespace

int main() {
  TestQuoteWindowsArgument();
  std::printf("process_test: %d failures\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}