// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Pure text conversions for PC Options values.
#include "gow2_options_values_text.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <utility>

namespace rex::glue::options::values_text {
namespace {

bool IsDigits(std::string_view text) {
  return !text.empty() && text.find_first_not_of("0123456789") == text.npos;
}

bool IsUnsignedWithin(std::string_view text, std::uint64_t limit) {
  if (!IsDigits(text)) return false;
  std::uint64_t value = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  return result.ec == std::errc{} && value <= limit;
}

std::optional<Pair> ParsePair(std::string_view text, char separator, std::uint64_t limit) {
  const auto split = text.find(separator);
  if (split == text.npos) return std::nullopt;
  const auto first = text.substr(0, split);
  const auto second = text.substr(split + 1);
  if (!IsUnsignedWithin(first, limit)) return std::nullopt;
  if (!IsUnsignedWithin(second, limit)) return std::nullopt;
  return Pair{std::string(first), std::string(second)};
}

bool IsPositiveInteger(std::string_view text) {
  if (!IsUnsignedWithin(text, std::numeric_limits<std::int32_t>::max())) return false;
  return text.find_first_not_of('0') != text.npos;
}

std::optional<double> PositiveDouble(std::string_view text) {
  if (text.empty()) return std::nullopt;
  double value = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) return std::nullopt;
  if (!std::isfinite(value) || value <= 0) return std::nullopt;
  return value;
}

double ResolutionScale(std::string_view text) {
  const auto value = PositiveDouble(text);
  if (!value || *value > std::numeric_limits<double>::max() / 1280) return 1;
  return *value;
}

std::string InternalSize(double scale) {
  std::ostringstream text;
  text.imbue(std::locale::classic());
  text << std::fixed << std::setprecision(0) << 1280 * scale << 'x' << 720 * scale;
  return text.str();
}

bool IsIsoDateTime(std::string_view text) {
  if (text.size() < 19) return false;
  if (text[4] != '-' || text[7] != '-' || text[10] != 'T') return false;
  if (text[13] != ':' || text[16] != ':') return false;
  return IsDigits(text.substr(0, 4)) && IsDigits(text.substr(5, 2)) &&
         IsDigits(text.substr(8, 2)) && IsDigits(text.substr(11, 2)) &&
         IsDigits(text.substr(14, 2)) && IsDigits(text.substr(17, 2));
}

}

std::optional<Pair> ParseWindowSize(std::string_view value) {
  if (value == "auto") return Pair{"0", "0"};
  return ParsePair(value, 'x', std::numeric_limits<std::int32_t>::max());
}

std::optional<Pair> ParseTextureMemory(std::string_view value) {
  return ParsePair(value, '/', std::numeric_limits<std::uint64_t>::max());
}

std::string FloatText(std::string_view value) {
  const auto exponent = value.find_first_of("eE");
  auto mantissa = value.substr(0, exponent);
  const auto decimal = mantissa.find('.');
  if (decimal != mantissa.npos) {
    while (mantissa.size() > decimal + 2 && mantissa.back() == '0')
      mantissa.remove_suffix(1);
  }
  std::string text(mantissa);
  if (decimal == mantissa.npos) text += ".0";
  if (text.back() == '.') text += '0';
  if (exponent != value.npos) text += value.substr(exponent);
  return text;
}

std::string WindowMode(std::string_view fullscreen, std::string_view exclusive) {
  if (fullscreen != "true" && fullscreen != "1") return "windowed";
  if (exclusive == "true" || exclusive == "1") return "exclusive";
  return "borderless";
}

std::string WindowSize(std::string_view width, std::string_view height) {
  if (!IsPositiveInteger(width) || !IsPositiveInteger(height)) return "auto";
  return std::string(width) + "x" + std::string(height);
}

std::string UpscaleRatio(std::string_view scale, std::string_view target_w,
                         std::string_view target_h) {
  const double factor = ResolutionScale(scale);
  const auto internal = InternalSize(factor);
  if (target_w.empty() || target_h.empty()) return internal;
  const auto height = PositiveDouble(target_h);
  if (!height || !PositiveDouble(target_w)) return internal;
  std::ostringstream text;
  text.imbue(std::locale::classic());
  text << internal << " \xe2\x86\x92 " << target_w << 'x' << target_h << " ("
       << std::fixed << std::setprecision(1) << *height / (720 * factor) << "x)";
  return text.str();
}

namespace {

std::string Pick(bool spanish, std::string english, std::string spanish_text) {
  return spanish ? std::move(spanish_text) : std::move(english);
}

std::string PortPair(const options_actions::HostResult& result, std::string_view joiner) {
  const bool has_pair = result.ports.size() == 2;
  const int beacon = has_pair ? result.ports[0] : 14001;
  const int game = has_pair ? result.ports[1] : 41000;
  return std::to_string(beacon) + std::string(joiner) + std::to_string(game);
}

std::string HostStartText(const options_actions::HostResult& result, bool spanish,
                          const std::string& time) {
  if (!result.partial) {
    return Pick(spanish, "Open until " + time + " at " + result.peer_name,
                "Abierto hasta " + time + " en " + result.peer_name);
  }
  return Pick(spanish,
              "Open until " + time + "; router " + result.unmanaged_router +
                  " must send the ports to " + result.unmanaged_target + " (DMZ)",
              "Abierto hasta " + time + "; el router " + result.unmanaged_router +
                  " debe enviar los puertos a " + result.unmanaged_target + " (DMZ)");
}

std::string HostOkText(const options_actions::HostResult& result, bool spanish,
                       const std::string& time) {
  if (result.mode == "Start") return HostStartText(result, spanish, time);
  if (result.mode == "Status" && result.open_ports > 0) {
    const std::string count = std::to_string(result.open_ports);
    return Pick(spanish, "Open (" + count + " ports)", "Abierto (" + count + " puertos)");
  }
  return Pick(spanish, "Closed", "Cerrado");
}

std::string RouterErrorText(const std::string& code, const std::string& router,
                            const options_actions::HostResult& result, bool spanish) {
  if (code == "upnp_off") {
    return Pick(spanish,
                "Router " + router + " has UPnP off: forward UDP " + PortPair(result, " and ") +
                    " by hand",
                "El router " + router + " tiene UPnP apagado: reenvía UDP " +
                    PortPair(result, " y ") + " a mano");
  }
  if (code == "upnp_unresponsive") {
    return Pick(spanish,
                "Router " + router + " did not answer UPnP: restart it or forward the ports by hand",
                "El router " + router +
                    " no respondió por UPnP: reinícialo o reenvía los puertos a mano");
  }
  if (code == "outer_no_upnp") {
    return Pick(spanish,
                "Router " + router + " has no UPnP: point its DMZ here or forward the ports",
                "El router " + router + " no tiene UPnP: apunta su DMZ aquí o reenvía los puertos");
  }
  if (code == "refused") {
    return Pick(spanish,
                "Router " + router + " does not allow UPnP changes: forward the ports by hand",
                "El router " + router + " no permite cambios por UPnP: reenvía los puertos a mano");
  }
  if (code == "mapping_failed") {
    return Pick(spanish, "Router " + router + " refused the ports: forward them by hand",
                "El router " + router + " rechazó los puertos: reenvíalos a mano");
  }
  if (code == "conflict") {
    return Pick(spanish, "The ports already go to another device on " + router,
                "Los puertos ya van a otro equipo en " + router);
  }
  return {};
}

std::string HostErrorText(const options_actions::HostResult& result, bool spanish) {
  const std::string& code = result.error_code;
  if (code == "cgnat") {
    return Pick(spanish, "Your ISP shares your address (CGNAT): use Tailscale or ZeroTier",
                "Tu proveedor comparte tu dirección (CGNAT): usa Tailscale o ZeroTier");
  }
  if (code == "no_route") {
    return Pick(spanish, "Could not follow the route to the internet",
                "No se pudo seguir la ruta a internet");
  }
  std::string text = RouterErrorText(code, result.error_router, result, spanish);
  if (!text.empty()) return text;
  return "Error: " + result.error;
}

}  // namespace

std::string HostResultText(const options_actions::HostResult& result, bool spanish,
                           std::string_view lease_time) {
  if (result.ok) return HostOkText(result, spanish, std::string(lease_time));
  return HostErrorText(result, spanish);
}

std::string LeaseTime(std::string_view lease_end) {
  if (!IsIsoDateTime(lease_end)) return {};
  return std::string(lease_end.substr(11, 5));
}

}
