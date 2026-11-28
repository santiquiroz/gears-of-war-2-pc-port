// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_update_check_core.h"

#include <cstdio>
#include <limits>
#include <string>
#include <string_view>

namespace {

using namespace rex::glue::update;
int g_checks = 0;
int g_failures = 0;

void Expect(bool ok, std::string_view what) {
  ++g_checks;
  if (ok) return;
  ++g_failures;
  std::printf("FAIL: %.*s\n", int(what.size()), what.data());
}

void TestVersionNumbers() {
  Expect(IsNewer("v0.2.10", "v0.2.9"), "numeric components beat lexical order");
  Expect(!IsNewer("0.2.9", "0.2.10"), "older version is not newer");
  Expect(!IsNewer("v1.2", "1.2.0.0"), "missing components are zero");
  Expect(IsNewer("1.2.0.1", "1.2"), "additional nonzero components matter");
  Expect(!IsNewer("001.02.000", "1.2"), "leading zeroes preserve numeric equality");
  Expect(IsNewer("  V1.3\n", "v1.2"), "leading v and surrounding whitespace are tolerated");
  Expect(IsNewer("1.99999999999999999999999999999", "1.9999999999999999999999999999"),
         "arbitrary numeric components cannot overflow");
  Expect(IsNewer("v1.2.3", "v1.2.3-rc1"), "plain release follows suffix");
  Expect(!IsNewer("v1.2.3-rc1", "v1.2.3"), "suffix precedes plain release");
  Expect(!IsNewer("v1.2.3+build", "v1.2.3"), "non-numeric build suffix precedes plain release");
  Expect(IsNewer("1.3.0-rc1", "1.2.0"), "numeric core is compared before suffix");
  Expect(!IsNewer("1.2.3-rc2", "1.2.3-rc1"), "suffix ordering is conservatively equal");
  for (const auto invalid : {"", "v", "1..2", "1.", ".2", "-1.2", "1.2-", "1.2+", "1.2/3", "1. 2", "banana"}) {
    Expect(!IsNewer(invalid, "0"), "invalid latest version is rejected");
    Expect(!IsNewer("999", invalid), "invalid installed version is rejected");
  }
}

void TestReleaseFieldsAndAssets() {
  const auto release = ParseRelease(R"({
    "nested":{"tag_name":"v99","html_url":"decoy"},
    "tag_name":"v0.2.10", "html_url":"https://github.com/owner/repo/releases/v0.2.10",
    "name":"Release \"ready\" \\ \u00e9 \ud83d\ude80", "prerelease":false, "draft":false,
    "assets":[
      {"name":"source.zip","browser_download_url":"wrong"},
      {"name":"game-win64.zip.sig","browser_download_url":"wrong"},
      {"name":"game-win64.zip","nested":{"browser_download_url":"decoy"},"browser_download_url":"https:\/\/example.com\/game-win64.zip"},
      {"name":"later-win64.zip","browser_download_url":"later"}
    ], "unused":[null,true,false,-1.25e+2,{"quoted":"} [ \""}]
  })");
  Expect(release.has_value(), "complete GitHub payload parses");
  if (!release) return;
  Expect(release->tag == "v0.2.10", "only top-level tag is extracted");
  Expect(release->html_url.starts_with("https://github.com/"), "release page is extracted");
  Expect(release->name == "Release \"ready\" \\ \xc3\xa9 \xf0\x9f\x9a\x80", "escapes and surrogate pairs decode to UTF-8");
  Expect(release->download_url == "https://example.com/game-win64.zip", "first exact Windows asset suffix is selected");
  Expect(release->checksum_url.empty(), "missing checksum sibling is empty");
  Expect(!release->prerelease && !release->draft, "release booleans are extracted");
  const auto minimal = ParseRelease(R"({"tag_name":"v1"})");
  Expect(minimal && minimal->download_url.empty(), "release without matching assets remains usable");
  const auto flags = ParseRelease(R"({"tag_name":"v2","prerelease":true,"draft":true})");
  Expect(flags && flags->prerelease && flags->draft, "draft and prerelease flags remain visible to policy");
  const auto untitled = ParseRelease(R"({"tag_name":"v1","name":null})");
  Expect(untitled && untitled->name.empty(), "untitled GitHub releases accept null name");
  const auto utf8 = ParseRelease("{\"tag_name\":\"v1\",\"name\":\"\xc3\xa9 \xf0\x9f\x9a\x80\"}");
  Expect(utf8 && utf8->name == "\xc3\xa9 \xf0\x9f\x9a\x80", "raw UTF-8 strings preserve Unicode");
}

void TestPlatformAssetsAndChecksums() {
  constexpr std::string_view payload = R"({
    "tag_name":"v2", "assets":[
      {"name":"game-linux-x86_64.tar.gz","browser_download_url":"https://example.com/game.tar.gz"},
      {"name":"game-linux-x86_64.tar.gz.sha256","browser_download_url":"https://example.com/game.tar.gz.sha256"},
      {"name":"game-win64.zip","browser_download_url":"https://example.com/game.zip"},
      {"name":"game-win64.zip.sha256","browser_download_url":"https://example.com/game.zip.sha256"}
    ]
  })";
  const auto linux = ParseRelease(payload, "-linux-x86_64.tar.gz");
  Expect(linux && linux->download_url == "https://example.com/game.tar.gz",
         "Linux asset suffix selects tarball");
  Expect(linux && linux->checksum_url == "https://example.com/game.tar.gz.sha256",
         "Linux asset finds its checksum sibling");
  const auto windows = ParseRelease(payload, "-win64.zip");
  Expect(windows && windows->download_url == "https://example.com/game.zip",
         "Windows asset suffix selects zip");
  Expect(windows && windows->checksum_url == "https://example.com/game.zip.sha256",
         "Windows asset finds its checksum sibling");
  const auto no_checksum = ParseRelease(
      R"({"tag_name":"v2","assets":[{"name":"game-linux-x86_64.tar.gz","browser_download_url":"download"}]})",
      "-linux-x86_64.tar.gz");
  Expect(no_checksum && no_checksum->checksum_url.empty(), "missing selected asset checksum is empty");
}

void TestMalformedJson() {
  for (const auto invalid : {
      R"({"tag_name":"v1",})", R"({"tag_name":"v1"} trailing)",
      R"({"tag_name":"v1","extra":[1,]})", R"({"tag_name":"v1","extra":01})",
      R"({"tag_name":"v1","extra":1.})", R"({"tag_name":"v1","extra":1e})",
      R"({"tag_name":"v1","extra":truefalse})", R"({"tag_name":"v1","name":"\q"})",
      R"({"tag_name":"v1","name":"\uD800"})", R"({"tag_name":"v1","name":"\uDC00"})",
      R"({"tag_name":"v1","name":"\uD800\u1234"})", R"({"tag_name":"v1","name":"\uZZZZ"})",
      R"({"tag_name":"v1","tag_name":"v9"})", R"({"tag_name":123})",
      R"({"tag_name":"v1","prerelease":"false"})", R"({"tag_name":"v1","assets":{}})",
      R"({"tag_name":""})", R"({"nested":{"tag_name":"v1"}})", "[]", "null", "{}", "{", ""}) {
    Expect(!ParseRelease(invalid), "malformed or wrong-shaped JSON is rejected");
  }
  Expect(!ParseRelease("{\"tag_name\":\"v1\",\"name\":\"bad\nstring\"}"), "raw control characters are rejected");
  const std::string deep = "{\"tag_name\":\"v1\",\"extra\":" + std::string(70, '[') + "0" + std::string(70, ']') + "}";
  Expect(!ParseRelease(deep), "excessive nesting is rejected");
  for (const auto invalid_utf8 : {"\xff", "\xc0\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2"}) {
    const std::string invalid = "{\"tag_name\":\"v1\",\"name\":\"" + std::string(invalid_utf8) + "\"}";
    Expect(!ParseRelease(invalid), "malformed UTF-8 is rejected");
  }
}

void TestThrottleAndDismissal() {
  Expect(ShouldCheck(100, 0), "first launch checks immediately");
  Expect(!ShouldCheck(43'299, 100), "automatic request waits full twelve hours");
  Expect(ShouldCheck(43'300, 100), "exact twelve-hour boundary permits checking");
  Expect(!ShouldCheck(99, 100), "clock rollback does not trigger another automatic request");
  Expect(ShouldCheck(101, 100, true), "manual request bypasses throttle");
  Expect(ShouldCheck(std::numeric_limits<int64_t>::max(), 1), "large clock values do not overflow");
  Release release{"v2", "https://example.com", "Two", "", "", false, false};
  Expect(ShouldNotify(release, "v1", ""), "newer undismissed release notifies");
  Expect(!ShouldNotify(release, "v1", "v2"), "skipped release stays silent");
  Expect(ShouldNotify(release, "v1", "v1.5"), "newer release after skipped tag notifies again");
  Expect(!ShouldNotify(release, "v2", ""), "installed release does not notify");
  release.prerelease = true;
  Expect(!ShouldNotify(release, "v1", ""), "prerelease never notifies");
  release.prerelease = false;
  release.draft = true;
  Expect(!ShouldNotify(release, "v1", ""), "draft never notifies");
}

void TestCacheRoundTripAndFailedAttempt() {
  Cache cache{12345, {"v2", "https://example.com/\"quoted\"", "A\nrelease\t\\",
                     "https://example.com/download", "https://example.com/download.sha256", false, false},
              "v1.5", false};
  const auto saved = ParseCache(SerializeCache(cache));
  Expect(saved && saved->checked_at == 12345 && !saved->failed, "cache timestamp and successful status round-trip");
  if (!saved) return;
  Expect(saved->release.tag == cache.release.tag && saved->release.name == cache.release.name,
         "cache release strings round-trip with control escapes");
  Expect(saved->release.html_url == cache.release.html_url && saved->release.download_url == cache.release.download_url,
         "cache retains release and asset URLs");
  Expect(saved->release.checksum_url == cache.release.checksum_url, "cache retains checksum URL");
  Expect(saved->dismissed == "v1.5", "cache remembers dismissed release");
  cache.checked_at = 12346;
  cache.failed = true;
  const auto failed = ParseCache(SerializeCache(cache));
  Expect(failed && failed->failed && failed->release.tag == "v2", "failed attempts retain last good release");
  Expect(failed && !ShouldCheck(12347, failed->checked_at), "failed attempts also throttle next launch");
  const auto empty = ParseCache(SerializeCache(Cache{}));
  Expect(empty && empty->release.tag.empty(), "cache can preserve dismissal and errors without a known release");
  const auto old_cache = ParseCache(
      R"({"checked_at":12,"failed":false,"dismissed":"","release":{"tag_name":"v1","html_url":"page","name":"Old","download_url":"download","prerelease":false,"draft":false}})");
  Expect(old_cache && old_cache->release.checksum_url.empty(), "old cache without checksum remains readable");
  for (const auto invalid : {"{}", "[]", R"({"checked_at":-1,"release":{}})",
      R"({"checked_at":1.5,"release":{}})", R"({"checked_at":9223372036854775808,"release":{}})",
      R"({"checked_at":"123","release":{}})", R"({"checked_at":1,"release":{},"failed":1})",
      R"({"checked_at":1,"release":{"tag_name":null}})"}) {
    Expect(!ParseCache(invalid), "corrupt cache is rejected");
  }
}

}  // namespace

int main() {
  TestVersionNumbers();
  TestReleaseFieldsAndAssets();
  TestPlatformAssetsAndChecksums();
  TestMalformedJson();
  TestThrottleAndDismissal();
  TestCacheRoundTripAndFailedAttempt();
  std::printf("update_check_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
