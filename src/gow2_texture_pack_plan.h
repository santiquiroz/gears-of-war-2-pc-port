// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Texture pack builder planning: which dumped textures get upscaled, to what size and format. Pure, no SDK.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rex::glue::texture_pack_plan {

struct DumpName {
  std::string hash;
  std::string format;
  uint32_t width = 0;
  uint32_t height = 0;
};

enum class Step { kSkip, kUpscale };

struct Plan {
  Step step = Step::kSkip;
  uint32_t width = 0, height = 0;
  std::string dxgi;
};

constexpr uint32_t kMaxSide = 4096;
constexpr uint32_t kMinSide = 128;

inline bool IsLowerHex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }

inline bool IsKnownFormat(std::string_view format) {
  for (std::string_view known : {"bc1", "bc2", "bc3", "bc4", "bc5", "rgba8", "r8", "rg8"}) {
    if (format == known) return true;
  }
  return false;
}

// [1-9][0-9]{0,4}
inline std::optional<uint32_t> ParseSide(std::string_view text) {
  if (text.empty() || text.size() > 5 || text[0] < '1' || text[0] > '9') return std::nullopt;
  uint32_t value = 0;
  for (char c : text) {
    if (c < '0' || c > '9') return std::nullopt;
    value = value * 10 + static_cast<uint32_t>(c - '0');
  }
  return value;
}

// File names become command-line arguments: only exactly what the game writes gets through.
inline std::optional<DumpName> ParseDumpName(std::string_view file_name) {
  constexpr std::string_view kExtension = ".dds";
  if (file_name.size() < 17 + kExtension.size() || !file_name.ends_with(kExtension)) return std::nullopt;
  file_name.remove_suffix(kExtension.size());
  for (size_t i = 0; i < 16; ++i) {
    if (!IsLowerHex(file_name[i])) return std::nullopt;
  }
  if (file_name[16] != '_') return std::nullopt;
  const std::string_view rest = file_name.substr(17);
  const size_t format_end = rest.find('_');
  if (format_end == std::string_view::npos || !IsKnownFormat(rest.substr(0, format_end))) return std::nullopt;
  const std::string_view size = rest.substr(format_end + 1);
  const size_t x = size.find('x');
  if (x == std::string_view::npos) return std::nullopt;
  const auto width = ParseSide(size.substr(0, x));
  const auto height = ParseSide(size.substr(x + 1));
  if (!width || !height) return std::nullopt;
  return DumpName{std::string(file_name.substr(0, 16)), std::string(rest.substr(0, format_end)), *width, *height};
}

inline std::string SourceDxgi(std::string_view format) {
  if (format == "bc1") return "BC1_UNORM";
  if (format == "bc2") return "BC2_UNORM";
  if (format == "bc3") return "BC3_UNORM";
  return "R8G8B8A8_UNORM";
}

// rgba8 dumps are dither/jitter noise that Real-ESRGAN smooths flat.
inline bool IsColorFormat(std::string_view format) { return format == "bc1" || format == "bc2" || format == "bc3"; }

enum class Content { kColor, kNormalMap, kGrayscale };

// Normal maps, grayscale masks and specular maps, noise and single-channel masks are not photos: Real-ESRGAN
// would invent detail in them and a plain resize adds memory without detail, so the disc texture stays.
inline Plan PlanTexture(const DumpName& dump, Content content, uint32_t scale, bool high_quality) {
  // Parentheses keep windows.h's min macro out.
  if ((std::min)(dump.width, dump.height) < kMinSide) return {};
  if (content != Content::kColor || !IsColorFormat(dump.format)) return {};
  uint32_t width = dump.width * scale, height = dump.height * scale;
  while (width > kMaxSide || height > kMaxSide) {
    width /= 2;
    height /= 2;
  }
  if (width <= dump.width && height <= dump.height) return {};
  return {Step::kUpscale, width, height, high_quality ? "BC7_UNORM" : SourceDxgi(dump.format)};
}

// gow2.toml can hold any number; the builder offers 2x and 4x only.
inline uint32_t BuildScale(int32_t configured) { return configured >= 4 ? 4 : 2; }

// Video memory of the upscaled textures against the disc ones: area times the BC7 doubling.
inline uint32_t MemoryMultiplier(uint32_t scale, bool high_quality) {
  return scale * scale * (high_quality ? 2 : 1);
}

inline std::string PackName(uint32_t scale, bool high_quality) {
  return "dump-" + std::to_string(scale) + "x" + (high_quality ? "-bc7" : "");
}

struct ColorSum {
  uint64_t r = 0, g = 0, b = 0, chroma = 0, count = 0;
  void Add(uint32_t red, uint32_t green, uint32_t blue) {
    r += red;
    g += green;
    b += blue;
    chroma += (std::max)({red, green, blue}) - (std::min)({red, green, blue});
    ++count;
  }
};

inline void AddRgb565(ColorSum& sum, uint16_t color) {
  const uint32_t r5 = color >> 11, g6 = (color >> 5) & 0x3f, b5 = color & 0x1f;
  sum.Add((r5 * 255 + 15) / 31, (g6 * 255 + 31) / 63, (b5 * 255 + 15) / 31);
}

inline uint16_t ReadLe16(std::span<const uint8_t> data, size_t offset) {
  return static_cast<uint16_t>(data[offset] | (data[offset + 1] << 8));
}

inline ColorSum SumBlockEndpoints(std::span<const uint8_t> mip0, size_t block_size, size_t color_offset) {
  ColorSum sum;
  for (size_t block = 0; block + block_size <= mip0.size(); block += block_size) {
    AddRgb565(sum, ReadLe16(mip0, block + color_offset));
    AddRgb565(sum, ReadLe16(mip0, block + color_offset + 2));
  }
  return sum;
}

inline ColorSum SumTexels(std::span<const uint8_t> mip0) {
  ColorSum sum;
  for (size_t texel = 0; texel + 4 <= mip0.size(); texel += 4) sum.Add(mip0[texel], mip0[texel + 1], mip0[texel + 2]);
  return sum;
}

inline size_t BlockSize(std::string_view format) {
  if (format == "bc1") return 8;
  if (format == "bc2" || format == "bc3") return 16;
  if (format == "rgba8") return 4;
  return 0;
}

// Measured on the chapter 2 hospital dump: 166 textures are tangent-space normal maps stored as BC1,
// mean color about R127 G127 B250.
inline Content ClassifyTexture(std::string_view format, std::span<const uint8_t> mip0) {
  const size_t block = BlockSize(format);
  if (block == 0 || mip0.size() < block || mip0.size() % block != 0) return Content::kColor;
  const ColorSum sum = format == "rgba8" ? SumTexels(mip0) : SumBlockEndpoints(mip0, block, block == 16 ? 8 : 0);
  if (sum.count == 0) return Content::kColor;
  const double count = double(sum.count);
  const double r = double(sum.r) / count, g = double(sum.g) / count, b = double(sum.b) / count;
  if (r > 100 && r < 160 && g > 100 && g < 160 && b > 200) return Content::kNormalMap;
  // RGB565 endpoints of a gray texture carry up to about 6 of chroma from rounding; measured on the hospital
  // dump, 7 catches 151 of 161 grayscale textures and calls 4 of 268 colored ones gray.
  if (double(sum.chroma) / count < 7) return Content::kGrayscale;
  return Content::kColor;
}

inline size_t Mip0Bytes(std::string_view format, uint32_t width, uint32_t height) {
  const size_t block = BlockSize(format);
  if (format == "rgba8") return size_t{width} * height * 4;
  return size_t{(width + 3) / 4} * ((height + 3) / 4) * block;
}

constexpr size_t kDdsHeaderBytes = 128;
constexpr size_t kDx10HeaderBytes = 20;
constexpr size_t kFourCcOffset = 84;

// Mip 0 of a dumped DDS (the game writes DX10 headers); empty when the file is shorter than its name says.
inline std::span<const uint8_t> DdsMip0(std::span<const uint8_t> file, const DumpName& dump) {
  if (file.size() < kDdsHeaderBytes || file[0] != 'D' || file[1] != 'D' || file[2] != 'S' || file[3] != ' ') return {};
  const bool dx10 = file[kFourCcOffset] == 'D' && file[kFourCcOffset + 1] == 'X' && file[kFourCcOffset + 2] == '1' &&
                    file[kFourCcOffset + 3] == '0';
  const size_t offset = kDdsHeaderBytes + (dx10 ? kDx10HeaderBytes : 0);
  const size_t bytes = Mip0Bytes(dump.format, dump.width, dump.height);
  if (bytes == 0 || file.size() < offset + bytes) return {};
  return file.subspan(offset, bytes);
}

// One argument quoted the way the MSVC C runtime splits a command line back into argv.
inline std::string QuoteArgument(std::string_view arg) {
  if (!arg.empty() && arg.find_first_of(" \t\n\v\"") == std::string_view::npos) return std::string(arg);
  std::string out = "\"";
  size_t backslashes = 0;
  for (char c : arg) {
    if (c == '\\') { ++backslashes; continue; }
    // Backslashes count literally unless a quote follows them.
    out.append(c == '"' ? backslashes * 2 + 1 : backslashes, '\\');
    backslashes = 0;
    out += c;
  }
  out.append(backslashes * 2, '\\');
  return out + "\"";
}

inline std::string CommandLine(const std::vector<std::string>& args) {
  std::string line;
  for (const auto& arg : args) {
    if (!line.empty()) line += ' ';
    line += QuoteArgument(arg);
  }
  return line;
}

inline std::string Lower(std::string_view text) {
  std::string out(text);
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return out;
}

// Vulkan names devices like DXGI, so the game's GPU name finds ncnn's -g index.
inline std::optional<uint32_t> VulkanIndexForName(const std::vector<std::string>& vulkan_names,
                                                  std::string_view gpu_name) {
  if (gpu_name.empty()) return std::nullopt;
  for (size_t i = 0; i < vulkan_names.size(); ++i) {
    if (vulkan_names[i] == gpu_name) return static_cast<uint32_t>(i);
  }
  const std::string gpu = Lower(gpu_name);
  for (size_t i = 0; i < vulkan_names.size(); ++i) {
    const std::string name = Lower(vulkan_names[i]);
    if (!name.empty() && (name.find(gpu) != std::string::npos || gpu.find(name) != std::string::npos)) {
      return static_cast<uint32_t>(i);
    }
  }
  return std::nullopt;
}

inline bool IsAsciiLetter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

inline std::string_view Trim(std::string_view text) {
  constexpr std::string_view kSpace = " \t\r\n";
  const size_t first = text.find_first_not_of(kSpace);
  if (first == std::string_view::npos) return {};
  return text.substr(first, text.find_last_not_of(kSpace) - first + 1);
}

// vk<letters> failed, case-insensitive, anywhere in the line.
inline bool IsVulkanFailureLine(std::string_view line) {
  const std::string lower = Lower(line);
  for (size_t at = lower.find("vk"); at != std::string::npos; at = lower.find("vk", at + 1)) {
    size_t end = at + 2;
    while (end < lower.size() && IsAsciiLetter(lower[end])) ++end;
    if (end > at + 2 && std::string_view(lower).substr(end).starts_with(" failed")) return true;
  }
  return false;
}

// ncnn can report a failed Vulkan call and still exit 0.
inline std::optional<std::string> VulkanFailureLine(std::string_view log) {
  while (!log.empty()) {
    const size_t end = log.find('\n');
    const std::string_view line = log.substr(0, end);
    if (IsVulkanFailureLine(line)) return std::string(Trim(line));
    if (end == std::string_view::npos) break;
    log.remove_prefix(end + 1);
  }
  return std::nullopt;
}

}  // namespace rex::glue::texture_pack_plan
