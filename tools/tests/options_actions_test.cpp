// Host-side test of the GOW2HOST result-line reader (no game data, no SDK).
#include "gow2_host_line.h"

#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

using rex::glue::host_line::ParseHostLine;
using rex::glue::host_line::IsValidAllowPeers;
using rex::glue::host_line::IsValidHostname;
using rex::glue::host_line::QuoteWindowsArg;
using rex::glue::options_actions::HostResult;

int g_checks = 0;
int g_failures = 0;
void Expect(bool ok, std::string_view what) { ++g_checks; if (ok) return; ++g_failures; std::printf("FAIL: %.*s\n", int(what.size()), what.data()); }

void TestExample() {
  const std::string line =
      "GOW2HOST {\"mode\":\"Start\",\"ok\":true,\"error\":\"\",\"public_address\":\"203.0.113.7\","
      "\"peer_name\":\"gow2.example.com\",\"ports\":[14001,41000],\"lease_hours\":6,"
      "\"lease_end\":\"2026-10-16T22:40:00\",\"stop_scheduled_at\":\"2026-10-16T22:41:00\","
      "\"routers\":[{\"address\":\"192.168.0.1\",\"closing\":\"the lease ends at 2026-10-16 22:40\"}],"
      "\"open_ports\":0,\"warnings\":[\"a \\\"quoted\\\" note\"]}";
  const std::optional<HostResult> parsed = ParseHostLine(line);
  Expect(parsed.has_value(), "example parses");
  if (!parsed) return;
  Expect(parsed->mode == "Start", "example mode");
  Expect(parsed->ok, "example ok");
  Expect(parsed->error.empty(), "example error");
  Expect(parsed->public_address == "203.0.113.7", "example public_address");
  Expect(parsed->peer_name == "gow2.example.com", "example peer_name");
  Expect(parsed->ports.size() == 2u, "example ports size");
  if (parsed->ports.size() == 2u) {
    Expect(parsed->ports[0] == 14001 && parsed->ports[1] == 41000, "example ports values");
  }
  Expect(parsed->lease_hours == 6, "example lease_hours");
  Expect(parsed->lease_end == "2026-10-16T22:40:00", "example lease_end");
  Expect(parsed->stop_scheduled_at == "2026-10-16T22:41:00", "example stop_scheduled_at");
  Expect(parsed->routers.size() == 1u, "example routers size");
  if (parsed->routers.size() == 1u) {
    Expect(parsed->routers[0].address == "192.168.0.1", "example router address");
    Expect(parsed->routers[0].closing == "the lease ends at 2026-10-16 22:40",
           "example router closing");
  }
  Expect(parsed->open_ports == 0, "example open_ports");
  Expect(parsed->warnings.size() == 1u, "example warnings size");
  if (parsed->warnings.size() == 1u) {
    Expect(parsed->warnings[0] == "a \"quoted\" note", "example warning text");
  }
}

void TestStatusNullsAndEmptyArrays() {
  const std::string line =
      "GOW2HOST {\"mode\":\"Status\",\"ok\":true,\"error\":\"\",\"public_address\":\"\",\"peer_name\":\"\","
      "\"ports\":[14001,41000],\"lease_hours\":null,\"lease_end\":\"\",\"stop_scheduled_at\":\"\","
      "\"routers\":[],\"open_ports\":2,\"warnings\":[]}";
  const std::optional<HostResult> parsed = ParseHostLine(line);
  Expect(parsed.has_value(), "status parses");
  if (!parsed) return;
  Expect(parsed->mode == "Status", "status mode");
  Expect(parsed->ok, "status ok");
  Expect(parsed->lease_hours == 0, "status null lease_hours reads 0");
  Expect(parsed->routers.empty(), "status empty routers");
  Expect(parsed->warnings.empty(), "status empty warnings");
  Expect(parsed->open_ports == 2, "status open_ports");
  Expect(parsed->ports.size() == 2u, "status ports size");
}

void TestOneRouterUnknownFieldsAndNulls() {
  const std::string line =
      "GOW2HOST {\"mode\":\"Start\",\"ok\":true,\"error\":null,\"public_address\":\"198.51.100.9\","
      "\"peer_name\":null,\"ports\":[14001],\"lease_hours\":0.5,\"lease_end\":\"\",\"stop_scheduled_at\":\"\","
      "\"routers\":[{\"address\":\"10.0.0.1\",\"closing\":null,\"extra\":[1,2]}],\"open_ports\":1,"
      "\"warnings\":[],\"future\":{\"deep\":[true,null]},\"spare\":42}";
  const std::optional<HostResult> parsed = ParseHostLine(line);
  Expect(parsed.has_value(), "one-router parses");
  if (!parsed) return;
  Expect(parsed->routers.size() == 1u, "one-router size");
  if (parsed->routers.size() == 1u) {
    Expect(parsed->routers[0].address == "10.0.0.1", "one-router address");
    Expect(parsed->routers[0].closing.empty(), "one-router null closing reads empty");
  }
  Expect(parsed->lease_hours == 0.5, "one-router fractional lease_hours");
  Expect(parsed->error.empty(), "one-router null error reads empty");
  Expect(parsed->peer_name.empty(), "one-router null peer_name reads empty");
  Expect(parsed->ports.size() == 1u && parsed->ports[0] == 14001, "one-router ports");
}

void TestEscapes() {
  const std::string line =
      "GOW2HOST {\"mode\":\"Stop\",\"ok\":false,\"error\":\"caf\\u00e9 \\uD83D\\uDE00\\t\\n\\\"\\\\\\/"
      "\\b\\f\\r\",\"public_address\":\"\",\"peer_name\":\"\",\"ports\":[],\"lease_hours\":0,\"lease_end\":\"\","
      "\"stop_scheduled_at\":\"\",\"routers\":[],\"open_ports\":0,\"warnings\":[\"tab\\there\"]}";
  const std::optional<HostResult> parsed = ParseHostLine(line);
  Expect(parsed.has_value(), "escapes parse");
  if (!parsed) return;
  const std::string want = "caf\xC3\xA9 \xF0\x9F\x98\x80\t\n\"\\/\b\f\r";
  Expect(parsed->error == want, "escapes decode incl \\u00e9 and a surrogate pair");
  Expect(parsed->warnings.size() == 1u && parsed->warnings[0] == "tab\there",
         "escapes warning tab");
  Expect(!parsed->ok, "escapes ok false");
}

void TestBadStrings() {
  Expect(!ParseHostLine("GOW2HOST {\"mode\":\"x\",\"error\":\"\\uD83D\"}"), "lone high surrogate fails");
  Expect(!ParseHostLine("GOW2HOST {\"mode\":\"x\",\"error\":\"\\uDE00\"}"), "lone low surrogate fails");
  Expect(!ParseHostLine("GOW2HOST {\"mode\":\"x\",\"error\":\"\\uD83D\\u0041\"}"),
         "high surrogate plus non-low fails");
  Expect(!ParseHostLine("GOW2HOST {\"mode\":\"x\",\"error\":\"\\u12GZ\"}"), "bad hex fails");
  Expect(!ParseHostLine("GOW2HOST {\"mode\":\"x\",\"error\":\"\\x\"}"), "bad escape fails");
  Expect(!ParseHostLine("GOW2HOST {\"mode\":\"x\",\"error\":\"unclosed}"), "unclosed string fails");
  Expect(!ParseHostLine("GOW2HOST {\"mode\":\"x\",\"error\":\"a\x01" "b\"}"), "control char fails");
}

void TestMissingPrefix() {
  Expect(!ParseHostLine("{\"mode\":\"Start\",\"ok\":true}"), "no prefix fails");
  Expect(!ParseHostLine("GOW2HOST{\"mode\":\"Start\"}"), "prefix without space fails");
  Expect(!ParseHostLine("GOW2HOST"), "bare prefix fails");
  Expect(!ParseHostLine(""), "empty line fails");
  Expect(!ParseHostLine("  GOW2HOST {\"mode\":\"Start\"}"), "leading spaces fail");
}

void TestMalformed() {
  Expect(!ParseHostLine("GOW2HOST {\"mode\":\"Start\",\"ok\":tr"), "truncated fails");
  Expect(!ParseHostLine("GOW2HOST {\"mode\":\"Start\",}"), "trailing comma fails");
  Expect(!ParseHostLine("GOW2HOST {\"mode\":\"Start\"} trailing"), "trailing text fails");
  Expect(!ParseHostLine("GOW2HOST [1,2]"), "top-level array fails");
  Expect(!ParseHostLine("GOW2HOST "), "prefix only fails");
  Expect(!ParseHostLine("GOW2HOST {\"ports\":[01]}"), "leading zero fails");
  Expect(!ParseHostLine("GOW2HOST {\"mode\":}"), "missing value fails");
  Expect(!ParseHostLine("GOW2HOST {'mode':'Start'}"), "single quotes fail");
  Expect(!ParseHostLine("GOW2HOST {\"ok\":\"yes\"}"), "wrong ok type fails");
  Expect(!ParseHostLine("GOW2HOST {\"ports\":{}}"), "wrong ports type fails");
  Expect(!ParseHostLine("GOW2HOST {\"open_ports\":1.5}"), "fractional open_ports fails");
}

void TestTrailingSpaceAndDefaults() {
  const std::optional<HostResult> spaced =
      ParseHostLine("GOW2HOST {\"mode\":\"Status\",\"ok\":true}  \r");
  Expect(spaced.has_value(), "trailing spaces and CR parse");
  if (spaced) {
    Expect(spaced->mode == "Status", "trailing spaces mode");
    Expect(spaced->ok, "trailing spaces ok");
  }
  const std::optional<HostResult> empty = ParseHostLine("GOW2HOST {}");
  Expect(empty.has_value(), "empty object parses");
  if (empty) {
    Expect(empty->mode.empty() && !empty->ok && empty->lease_hours == 0, "empty object defaults");
    Expect(empty->ports.empty() && empty->routers.empty() && empty->warnings.empty(),
           "empty object empty lists");
  }
}

void TestWindowsArgumentQuoting() {
  Expect(QuoteWindowsArg(L"plain") == L"\"plain\"", "plain argument is quoted");
  Expect(QuoteWindowsArg(L"two words") == L"\"two words\"", "spaces stay in one argument");
  Expect(QuoteWindowsArg(L"say \"hi\"") == L"\"say \\\"hi\\\"\"", "embedded quotes are escaped");
  Expect(QuoteWindowsArg(L"path\\") == L"\"path\\\\\"", "trailing backslash is doubled");
  Expect(QuoteWindowsArg(L"") == L"\"\"", "empty argument is preserved");
  Expect(QuoteWindowsArg(L"a\\\"b") == L"\"a\\\\\\\"b\"", "backslash before quote is doubled");
  Expect(QuoteWindowsArg(L"a\\b") == L"\"a\\b\"", "ordinary backslash stays unchanged");
}

void TestHostnameValidation() {
  Expect(IsValidHostname(""), "empty hostname is allowed");
  Expect(IsValidHostname("example.com"), "hostname is allowed");
  Expect(IsValidHostname("example-1.com:41000"), "hostname with port is allowed");
  Expect(IsValidHostname("127.0.0.1:1"), "IPv4 with port is allowed");
  Expect(IsValidHostname(std::string(253, 'a') + ":12345"), "maximum hostname and port fit");
  Expect(!IsValidHostname(std::string(254, 'a')), "oversized hostname fails");
  Expect(!IsValidHostname("example.com -AllowPermanent"), "hostname switch injection fails");
  Expect(!IsValidHostname("-LogFile"), "hostname cannot start with hyphen");
  Expect(!IsValidHostname(".example.com"), "hostname starts with alphanumeric");
  Expect(!IsValidHostname("example.com:"), "empty port fails");
  Expect(!IsValidHostname("example.com:123456"), "oversized port fails");
  Expect(!IsValidHostname("example.com:abc"), "nonnumeric port fails");
  Expect(!IsValidHostname("example.com:12:34"), "multiple port separators fail");
  Expect(!IsValidHostname("example_com"), "underscore hostname fails");
  Expect(!IsValidHostname("example.com\n"), "hostname newline fails");
}

void TestPeerValidation() {
  Expect(IsValidAllowPeers(""), "empty peer list is allowed");
  Expect(IsValidAllowPeers("192.0.2.1"), "single peer is allowed");
  Expect(IsValidAllowPeers(" 192.0.2.0/24 ,\t2001:db8::1,example-host.com:14001 "),
         "trimmed comma list accepts CIDR IPv6 and hostname");
  Expect(IsValidAllowPeers(std::string(253, 'a')), "maximum peer size fits");
  Expect(!IsValidAllowPeers(std::string(254, 'a')), "oversized peer fails");
  Expect(!IsValidAllowPeers("-LogFile x"), "peer switch injection fails");
  Expect(!IsValidAllowPeers(" -LogFile"), "trimmed peer cannot start with hyphen");
  Expect(!IsValidAllowPeers("good.com,-AllowPermanent"), "injected second peer fails");
  Expect(!IsValidAllowPeers("a b"), "peer interior whitespace fails");
  Expect(!IsValidAllowPeers("a;whoami"), "peer semicolon fails");
  Expect(!IsValidAllowPeers("a\"b"), "peer quote fails");
  Expect(!IsValidAllowPeers("a,"), "trailing empty peer fails");
  Expect(!IsValidAllowPeers(",a"), "leading empty peer fails");
  Expect(!IsValidAllowPeers("a,,b"), "interior empty peer fails");
  Expect(!IsValidAllowPeers(" \t"), "blank peer fails");
}

void TestPartialFields() {
  const std::optional<HostResult> parsed = ParseHostLine(
      "GOW2HOST {\"mode\":\"Start\",\"ok\":true,\"error\":\"\",\"public_address\":\"203.0.113.7\","
      "\"peer_name\":\"203.0.113.7\",\"ports\":[14001,41000],\"lease_hours\":6,"
      "\"lease_end\":\"2026-10-16T22:40:00\",\"stop_scheduled_at\":\"\",\"routers\":[],"
      "\"open_ports\":0,\"warnings\":[],\"error_code\":\"\",\"error_router\":\"\","
      "\"partial\":true,\"unmanaged_router\":\"192.168.0.1\","
      "\"unmanaged_target\":\"192.168.0.20\"}");
  Expect(parsed.has_value(), "partial line parses");
  if (!parsed) return;
  Expect(parsed->error_code.empty() && parsed->error_router.empty(), "ok line has no error code");
  Expect(parsed->partial, "partial is read");
  Expect(parsed->unmanaged_router == "192.168.0.1", "unmanaged_router is read");
  Expect(parsed->unmanaged_target == "192.168.0.20", "unmanaged_target is read");
}

void TestErrorCodeFields() {
  const std::optional<HostResult> parsed = ParseHostLine(
      "GOW2HOST {\"mode\":\"Start\",\"ok\":false,\"error\":\"Router 192.168.50.1 refused\","
      "\"error_code\":\"mapping_failed\",\"error_router\":\"192.168.50.1\",\"partial\":false,"
      "\"unmanaged_router\":\"\",\"unmanaged_target\":\"\"}");
  Expect(parsed.has_value(), "error line parses");
  if (!parsed) return;
  Expect(parsed->error_code == "mapping_failed", "error_code is read");
  Expect(parsed->error_router == "192.168.50.1", "error_router is read");
  Expect(!parsed->partial, "partial false is read");
}

void TestLineWithoutNewFields() {
  const std::optional<HostResult> parsed = ParseHostLine("GOW2HOST {\"mode\":\"Status\",\"ok\":true}");
  Expect(parsed.has_value(), "line without the new fields parses");
  if (!parsed) return;
  Expect(parsed->error_code.empty() && parsed->error_router.empty(), "error fields default");
  Expect(!parsed->partial, "partial defaults to false");
  Expect(parsed->unmanaged_router.empty() && parsed->unmanaged_target.empty(),
         "unmanaged fields default");
}

void TestNewFieldWrongTypes() {
  Expect(!ParseHostLine("GOW2HOST {\"mode\":\"Start\",\"partial\":\"yes\"}"), "string partial fails");
  Expect(!ParseHostLine("GOW2HOST {\"mode\":\"Start\",\"error_code\":7}"), "numeric error_code fails");
  Expect(!ParseHostLine("GOW2HOST {\"mode\":\"Start\",\"unmanaged_target\":[]}"),
         "array unmanaged_target fails");
}

int main() {
  TestWindowsArgumentQuoting();
  TestHostnameValidation();
  TestPeerValidation();
  TestExample();
  TestStatusNullsAndEmptyArrays();
  TestOneRouterUnknownFieldsAndNulls();
  TestEscapes();
  TestBadStrings();
  TestMissingPrefix();
  TestMalformed();
  TestTrailingSpaceAndDefaults();
  TestPartialFields();
  TestErrorCodeFields();
  TestLineWithoutNewFields();
  TestNewFieldWrongTypes();
  std::printf("options_actions_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
