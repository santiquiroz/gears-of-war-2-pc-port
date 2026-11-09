// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Host-side checks for the SDK-free PC Options text conversions.
#include "gow2_options_values_text.h"

#include <cstdio>
#include <string>
#include <string_view>

namespace {
namespace text = rex::glue::options::values_text;
int checks = 0;
int failures = 0;

void Expect(bool ok, std::string_view name) {
  ++checks;
  if (ok) return;
  ++failures;
  std::printf("FAIL: %.*s\n", static_cast<int>(name.size()), name.data());
}

void ExpectPair(const std::optional<text::Pair>& pair, std::string_view first,
                std::string_view second, std::string_view name) {
  Expect(pair && pair->first == first && pair->second == second, name);
}

void TestWindowParsing() {
  ExpectPair(text::ParseWindowSize("auto"), "0", "0", "auto dimensions");
  ExpectPair(text::ParseWindowSize("1920x1080"), "1920", "1080", "window dimensions");
  ExpectPair(text::ParseWindowSize("0x0"), "0", "0", "zero dimensions");
  ExpectPair(text::ParseWindowSize("001x002"), "001", "002", "leading zero dimensions");
  ExpectPair(text::ParseWindowSize("2147483647x1"), "2147483647", "1", "maximum width");
  for (const auto value : {"", "AUTO", "1920X1080", "1920x", "x1080", "1x2x3",
                            "-1x2", "+1x2", " 1x2", "1x2 ", "1.5x2", "1x2px",
                            "2147483648x1", "1x2147483648", "18446744073709551616x1"}) {
    Expect(!text::ParseWindowSize(value), value);
  }
}

void TestMemoryParsing() {
  ExpectPair(text::ParseTextureMemory("512/1024"), "512", "1024", "memory limits");
  ExpectPair(text::ParseTextureMemory("0/0"), "0", "0", "zero memory limits");
  ExpectPair(text::ParseTextureMemory("18446744073709551615/1"),
             "18446744073709551615", "1", "maximum memory limit");
  for (const auto value : {"", "1", "/2", "1/", "1/2/3", "-1/2", "1/+2",
                            "1 /2", "1/2MB", "18446744073709551616/1",
                            "1/18446744073709551616"}) {
    Expect(!text::ParseTextureMemory(value), value);
  }
}

void TestWindowMapping() {
  Expect(text::WindowMode("false", "false") == "windowed", "windowed mode");
  Expect(text::WindowMode("false", "true") == "windowed", "windowed ignores exclusive");
  Expect(text::WindowMode("true", "false") == "borderless", "borderless mode");
  Expect(text::WindowMode("true", "true") == "exclusive", "exclusive mode");
  Expect(text::WindowMode("1", "1") == "exclusive", "numeric bool mode");
  Expect(text::WindowMode("", "") == "windowed", "missing bool mode");
  Expect(text::WindowSize("1920", "1080") == "1920x1080", "window size text");
  Expect(text::WindowSize("0", "1080") == "auto", "zero width is auto");
  Expect(text::WindowSize("1920", "0") == "auto", "zero height is auto");
  Expect(text::WindowSize("-1", "720") == "auto", "negative dimension is auto");
  Expect(text::WindowSize("oops", "720") == "auto", "invalid dimension is auto");
  Expect(text::WindowSize("2147483648", "720") == "auto", "overflow dimension is auto");
}

void TestFloatText() {
  Expect(text::FloatText("0.600000") == "0.6", "float trailing zeros");
  Expect(text::FloatText("1.000000") == "1.0", "whole float keeps fraction");
  Expect(text::FloatText("0.050000") == "0.05", "float leading fractional zero");
  Expect(text::FloatText("-2.500000") == "-2.5", "negative float trailing zeros");
  Expect(text::FloatText("3") == "3.0", "integer float gets fraction");
  Expect(text::FloatText("0.000000") == "0.0", "zero float keeps fraction");
  Expect(text::FloatText("0.6") == "0.6", "short float stays short");
  Expect(text::FloatText("1.500000e+10") == "1.5e+10", "float exponent stays intact");
  Expect(text::FloatText("3E-10") == "3.0E-10", "integer mantissa gets fraction");
}

void TestUpscaleRatio() {
  const std::string_view arrow = " \xe2\x86\x92 ";
  Expect(text::UpscaleRatio("1", "", "") == "1280x720", "internal size alone");
  Expect(text::UpscaleRatio("1.5", "", "") == "1920x1080", "scaled internal size");
  Expect(text::UpscaleRatio("0.5", "", "") == "640x360", "fractional internal size");
  Expect(text::UpscaleRatio("1", "1920", "1080") ==
             std::string("1280x720") + std::string(arrow) + "1920x1080 (1.5x)", "ratio text");
  Expect(text::UpscaleRatio("1.5", "2560", "1440") ==
             std::string("1920x1080") + std::string(arrow) + "2560x1440 (1.3x)", "ratio rounds");
  Expect(text::UpscaleRatio("2", "1920", "") == "2560x1440", "missing target height");
  Expect(text::UpscaleRatio("2", "", "1080") == "2560x1440", "missing target width");
  Expect(text::UpscaleRatio("1", "1920", "bad") == "1280x720", "invalid target is safe");
  for (const auto value : {"", "bad", "1junk", "0", "-2", "nan", "inf", "1e999", "1e308"}) {
    Expect(text::UpscaleRatio(value, "", "") == "1280x720", value);
  }
}

void TestLeaseTime() {
  Expect(text::LeaseTime("2026-10-16T09:07:00") == "09:07", "lease time");
  Expect(text::LeaseTime("2026-10-16T23:59:59Z") == "23:59", "UTC lease time");
  for (const auto value : {"", "short", "2026-10-16T09:0", "2026-10-16 09:07:00",
                            "xxxx-10-16T09:07:00", "2026-10-16Txx:07:00"}) {
    Expect(text::LeaseTime(value).empty(), value);
  }
}

rex::glue::options_actions::HostResult FailedHost(std::string_view code) {
  rex::glue::options_actions::HostResult result;
  result.mode = "Start";
  result.ok = false;
  result.error = "boom";
  result.error_code = std::string(code);
  result.error_router = "192.168.1.1";
  result.ports = {14001, 41000};
  return result;
}

struct ErrorTextCase {
  std::string_view code;
  std::string_view english;
  std::string_view spanish;
};

void TestHostErrorTexts() {
  const ErrorTextCase cases[] = {
      {"upnp_off", "Router 192.168.1.1 has UPnP off: forward UDP 14001 and 41000 by hand",
       "El router 192.168.1.1 tiene UPnP apagado: reenvía UDP 14001 y 41000 a mano"},
      {"upnp_unresponsive",
       "Router 192.168.1.1 did not answer UPnP: restart it or forward the ports by hand",
       "El router 192.168.1.1 no respondió por UPnP: reinícialo o reenvía los puertos a mano"},
      {"outer_no_upnp", "Router 192.168.1.1 has no UPnP: point its DMZ here or forward the ports",
       "El router 192.168.1.1 no tiene UPnP: apunta su DMZ aquí o reenvía los puertos"},
      {"cgnat", "Your ISP shares your address (CGNAT): use Tailscale or ZeroTier",
       "Tu proveedor comparte tu dirección (CGNAT): usa Tailscale o ZeroTier"},
      {"refused", "Router 192.168.1.1 does not allow UPnP changes: forward the ports by hand",
       "El router 192.168.1.1 no permite cambios por UPnP: reenvía los puertos a mano"},
      {"mapping_failed", "Router 192.168.1.1 refused the ports: forward them by hand",
       "El router 192.168.1.1 rechazó los puertos: reenvíalos a mano"},
      {"conflict", "The ports already go to another device on 192.168.1.1",
       "Los puertos ya van a otro equipo en 192.168.1.1"},
      {"no_route", "Could not follow the route to the internet",
       "No se pudo seguir la ruta a internet"},
      {"other", "Error: boom", "Error: boom"},
      {"", "Error: boom", "Error: boom"},
      {"from_a_newer_script", "Error: boom", "Error: boom"},
  };
  for (const auto& item : cases) {
    const auto result = FailedHost(item.code);
    Expect(text::HostResultText(result, false, "22:40") == item.english, item.english);
    Expect(text::HostResultText(result, true, "22:40") == item.spanish, item.spanish);
  }
}

void TestHostErrorPorts() {
  auto offset = FailedHost("upnp_off");
  offset.ports = {14001, 42000};
  Expect(text::HostResultText(offset, false, "") ==
             "Router 192.168.1.1 has UPnP off: forward UDP 14001 and 42000 by hand",
         "upnp_off uses the reported ports");
  offset.ports.clear();
  Expect(text::HostResultText(offset, false, "") ==
             "Router 192.168.1.1 has UPnP off: forward UDP 14001 and 41000 by hand",
         "upnp_off falls back to the default ports");
}

void TestHostOkTexts() {
  rex::glue::options_actions::HostResult start;
  start.mode = "Start";
  start.ok = true;
  start.peer_name = "203.0.113.7";
  Expect(text::HostResultText(start, false, "22:40") == "Open until 22:40 at 203.0.113.7",
         "start en");
  Expect(text::HostResultText(start, true, "22:40") == "Abierto hasta 22:40 en 203.0.113.7",
         "start es");
  start.partial = true;
  start.unmanaged_router = "192.0.2.1";
  start.unmanaged_target = "192.168.0.2";
  Expect(text::HostResultText(start, false, "22:40") ==
             "Open until 22:40; router 192.0.2.1 must send the ports to 192.168.0.2 (DMZ)",
         "partial start en");
  Expect(text::HostResultText(start, true, "22:40") ==
             "Abierto hasta 22:40; el router 192.0.2.1 debe enviar los puertos a 192.168.0.2 (DMZ)",
         "partial start es");
  rex::glue::options_actions::HostResult status;
  status.mode = "Status";
  status.ok = true;
  status.open_ports = 4;
  Expect(text::HostResultText(status, false, "") == "Open (4 ports)", "status open en");
  Expect(text::HostResultText(status, true, "") == "Abierto (4 puertos)", "status open es");
  status.mode = "Stop";
  Expect(text::HostResultText(status, false, "") == "Closed", "stop en");
  Expect(text::HostResultText(status, true, "") == "Cerrado", "stop es");
}
}

int main() {
  TestWindowParsing();
  TestMemoryParsing();
  TestWindowMapping();
  TestFloatText();
  TestUpscaleRatio();
  TestLeaseTime();
  TestHostErrorTexts();
  TestHostErrorPorts();
  TestHostOkTexts();
  std::printf("options_values_test: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
