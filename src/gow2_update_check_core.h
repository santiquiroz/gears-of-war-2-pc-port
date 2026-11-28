// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#pragma once

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rex::glue::update {

struct Release {
  std::string tag, html_url, name, download_url, checksum_url;
  bool prerelease = false;
  bool draft = false;
  uint64_t download_size = 0;
};

#ifdef _WIN32
inline constexpr std::string_view kAssetSuffix = "-win64.zip";
#else
inline constexpr std::string_view kAssetSuffix = "-linux-x86_64.tar.gz";
#endif

struct Cache {
  int64_t checked_at = 0;
  Release release;
  std::string dismissed;
  bool failed = false;
};

namespace detail {

inline bool IsDigit(char c) { return c >= '0' && c <= '9'; }
inline bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

struct Json {
  enum Kind { Null, String, Number, Bool, Object, Array } kind = Null;
  std::string text;
  std::map<std::string, Json, std::less<>> fields;
  std::vector<Json> items;
};

class JsonReader {
 public:
  explicit JsonReader(std::string_view input) : input_(input) {}

  std::optional<Json> Read() {
    Json value;
    if (!ReadValue(value, 0)) return std::nullopt;
    Space();
    if (pos_ != input_.size()) return std::nullopt;
    return value;
  }

 private:
  void Space() {
    while (pos_ < input_.size() && IsSpace(input_[pos_])) ++pos_;
  }

  bool Take(char c) {
    if (pos_ == input_.size() || input_[pos_] != c) return false;
    ++pos_;
    return true;
  }

  std::optional<uint32_t> Hex4() {
    uint32_t code = 0;
    for (int i = 0; i < 4; ++i) {
      if (pos_ == input_.size()) return std::nullopt;
      const char c = input_[pos_++];
      const auto index = std::string_view("0123456789abcdef").find(c >= 'A' && c <= 'F' ? c + 32 : c);
      if (index == std::string_view::npos) return std::nullopt;
      code = code * 16 + static_cast<uint32_t>(index);
    }
    return code;
  }

  static void AppendUtf8(std::string& text, uint32_t code) {
    if (code <= 0x7f) { text += static_cast<char>(code); return; }
    if (code <= 0x7ff) {
      text += static_cast<char>(0xc0 | (code >> 6));
    } else {
      if (code > 0xffff) text += static_cast<char>(0xf0 | (code >> 18));
      else text += static_cast<char>(0xe0 | (code >> 12));
      if (code > 0xffff) text += static_cast<char>(0x80 | ((code >> 12) & 63));
      text += static_cast<char>(0x80 | ((code >> 6) & 63));
    }
    text += static_cast<char>(0x80 | (code & 63));
  }

  bool Unicode(std::string& text) {
    auto code = Hex4();
    if (!code) return false;
    if (*code >= 0xdc00 && *code <= 0xdfff) return false;
    if (*code < 0xd800 || *code > 0xdbff) { AppendUtf8(text, *code); return true; }
    if (!Take('\\') || !Take('u')) return false;
    const auto low = Hex4();
    if (!low || *low < 0xdc00 || *low > 0xdfff) return false;
    AppendUtf8(text, 0x10000 + ((*code - 0xd800) << 10) + (*low - 0xdc00));
    return true;
  }

  bool Escape(std::string& text) {
    if (pos_ == input_.size()) return false;
    const char c = input_[pos_++];
    const auto index = std::string_view("\"\\/bfnrt").find(c);
    if (index != std::string_view::npos) {
      text += std::string_view("\"\\/\b\f\n\r\t")[index];
      return true;
    }
    return c == 'u' && Unicode(text);
  }

  bool ReadUtf8(std::string& text, unsigned char first) {
    unsigned remaining = 0;
    uint32_t code = 0;
    uint32_t minimum = 0;
    if (first >= 0xc2 && first <= 0xdf) { remaining = 1; code = first & 31; minimum = 0x80; }
    else if (first >= 0xe0 && first <= 0xef) { remaining = 2; code = first & 15; minimum = 0x800; }
    else if (first >= 0xf0 && first <= 0xf4) { remaining = 3; code = first & 7; minimum = 0x10000; }
    else return false;
    for (unsigned i = 0; i < remaining; ++i) {
      if (pos_ == input_.size()) return false;
      const auto next = static_cast<unsigned char>(input_[pos_++]);
      if ((next & 0xc0) != 0x80) return false;
      code = (code << 6) | (next & 63);
    }
    if (code < minimum || code > 0x10ffff) return false;
    if (code >= 0xd800 && code <= 0xdfff) return false;
    AppendUtf8(text, code);
    return true;
  }

  bool ReadString(std::string& text) {
    if (!Take('"')) return false;
    while (pos_ < input_.size()) {
      const char c = input_[pos_++];
      if (c == '"') return true;
      if (static_cast<unsigned char>(c) < 0x20) return false;
      if (static_cast<unsigned char>(c) >= 0x80) {
        if (!ReadUtf8(text, static_cast<unsigned char>(c))) return false;
        continue;
      }
      if (c == '\\') { if (!Escape(text)) return false; }
      else text += c;
    }
    return false;
  }

  bool Digits() {
    const size_t start = pos_;
    while (pos_ < input_.size() && IsDigit(input_[pos_])) ++pos_;
    return pos_ != start;
  }

  bool ReadNumber(Json& value) {
    const size_t start = pos_;
    Take('-');
    if (!Take('0') && !Digits()) return false;
    if (Take('.') && !Digits()) return false;
    if (Take('e') || Take('E')) {
      if (!Take('+')) Take('-');
      if (!Digits()) return false;
    }
    value.kind = Json::Number;
    value.text = input_.substr(start, pos_ - start);
    return true;
  }

  bool ReadObject(Json& value, unsigned depth) {
    value.kind = Json::Object;
    Space();
    if (Take('}')) return true;
    do {
      Space();
      std::string key;
      if (!ReadString(key)) return false;
      Space();
      if (!Take(':')) return false;
      Json child;
      if (!ReadValue(child, depth + 1)) return false;
      if (!value.fields.emplace(std::move(key), std::move(child)).second) return false;
      Space();
      if (Take('}')) return true;
    } while (Take(','));
    return false;
  }

  bool ReadArray(Json& value, unsigned depth) {
    value.kind = Json::Array;
    Space();
    if (Take(']')) return true;
    do {
      Json child;
      if (!ReadValue(child, depth + 1)) return false;
      value.items.push_back(std::move(child));
      Space();
      if (Take(']')) return true;
    } while (Take(','));
    return false;
  }

  bool ReadValue(Json& value, unsigned depth) {
    if (depth > 64) return false;
    Space();
    if (pos_ == input_.size()) return false;
    if (Take('{')) return ReadObject(value, depth);
    if (Take('[')) return ReadArray(value, depth);
    if (input_[pos_] == '"') { value.kind = Json::String; return ReadString(value.text); }
    for (const auto literal : {"true", "false", "null"}) {
      if (!input_.substr(pos_).starts_with(literal)) continue;
      value.kind = literal[0] == 'n' ? Json::Null : Json::Bool;
      value.text = literal;
      pos_ += value.text.size();
      return true;
    }
    return ReadNumber(value);
  }

  std::string_view input_;
  size_t pos_ = 0;
};

inline const Json* Field(const Json& object, std::string_view key, Json::Kind kind) {
  const auto it = object.fields.find(key);
  if (it == object.fields.end() || it->second.kind != kind) return nullptr;
  return &it->second;
}

inline bool StringField(const Json& object, std::string_view key, std::string& result) {
  const auto field = Field(object, key, Json::String);
  if (!field) return !object.fields.contains(key);
  result = field->text;
  return true;
}

inline bool BoolField(const Json& object, std::string_view key, bool& result) {
  const auto field = Field(object, key, Json::Bool);
  if (!field) return !object.fields.contains(key);
  result = field->text == "true";
  return true;
}

inline bool UInt64Field(const Json& object, std::string_view key, uint64_t& result) {
  const auto it = object.fields.find(key);
  if (it == object.fields.end()) return true;
  if (it->second.kind != Json::Number) return false;
  uint64_t value = 0;
  const auto& text = it->second.text;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size()) return false;
  result = value;
  return true;
}

inline std::optional<Release> ReleaseFields(const Json& object) {
  if (object.kind != Json::Object) return std::nullopt;
  Release release;
  if (!StringField(object, "tag_name", release.tag)) return std::nullopt;
  if (!StringField(object, "html_url", release.html_url)) return std::nullopt;
  if (!Field(object, "name", Json::Null) && !StringField(object, "name", release.name)) return std::nullopt;
  if (!BoolField(object, "prerelease", release.prerelease)) return std::nullopt;
  if (!BoolField(object, "draft", release.draft)) return std::nullopt;
  return release;
}

struct Version {
  std::vector<std::string_view> parts;
  bool suffix = false;
};

inline std::optional<Version> ParseVersion(std::string_view text) {
  while (!text.empty() && IsSpace(text.front())) text.remove_prefix(1);
  while (!text.empty() && IsSpace(text.back())) text.remove_suffix(1);
  if (text.starts_with('v') || text.starts_with('V')) text.remove_prefix(1);
  Version version;
  size_t pos = 0;
  while (pos < text.size()) {
    const size_t start = pos;
    while (pos < text.size() && IsDigit(text[pos])) ++pos;
    if (pos == start) return std::nullopt;
    auto part = text.substr(start, pos - start);
    while (part.size() > 1 && part.front() == '0') part.remove_prefix(1);
    version.parts.push_back(part);
    if (pos == text.size()) return version;
    if (text[pos] == '.') { ++pos; if (pos == text.size()) return std::nullopt; continue; }
    const auto suffix = text.substr(pos);
    if (suffix == "-" || suffix == "+") return std::nullopt;
    for (const char c : suffix) {
      if (IsDigit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) continue;
      if (c != '-' && c != '+' && c != '.') return std::nullopt;
    }
    version.suffix = true;
    return version;
  }
  return std::nullopt;
}

inline std::string Quote(std::string_view text) {
  std::string result = "\"";
  constexpr char hex[] = "0123456789abcdef";
  for (const unsigned char c : text) {
    if (c == '"' || c == '\\') { result += '\\'; result += c; continue; }
    if (c >= 0x20) { result += c; continue; }
    result += "\\u00";
    result += hex[c >> 4];
    result += hex[c & 15];
  }
  return result + '"';
}

}  // namespace detail

inline std::optional<Release> ParseRelease(std::string_view input, std::string_view asset_suffix = kAssetSuffix) {
  const auto object = detail::JsonReader(input).Read();
  if (!object) return std::nullopt;
  auto release = detail::ReleaseFields(*object);
  if (!release || release->tag.empty()) return std::nullopt;
  const auto assets = detail::Field(*object, "assets", detail::Json::Array);
  if (!assets) {
    if (object->fields.contains("assets")) return std::nullopt;
    return release;
  }
  std::string selected_asset;
  for (const auto& asset : assets->items) {
    const auto name = detail::Field(asset, "name", detail::Json::String);
    const auto url = detail::Field(asset, "browser_download_url", detail::Json::String);
    if (!name || !url || !name->text.ends_with(asset_suffix)) continue;
    uint64_t download_size = 0;
    if (!detail::UInt64Field(asset, "size", download_size)) continue;
    selected_asset = name->text;
    release->download_url = url->text;
    release->download_size = download_size;
    break;
  }
  if (!selected_asset.empty()) {
    const std::string checksum_name = selected_asset + ".sha256";
    for (const auto& asset : assets->items) {
      const auto name = detail::Field(asset, "name", detail::Json::String);
      const auto url = detail::Field(asset, "browser_download_url", detail::Json::String);
      if (!name || !url || name->text != checksum_name) continue;
      release->checksum_url = url->text;
      break;
    }
  }
  return release;
}

inline bool IsNewer(std::string_view latest, std::string_view current) {
  const auto a = detail::ParseVersion(latest);
  const auto b = detail::ParseVersion(current);
  if (!a || !b) return false;
  for (size_t i = 0; i < std::max(a->parts.size(), b->parts.size()); ++i) {
    const auto x = i < a->parts.size() ? a->parts[i] : std::string_view("0");
    const auto y = i < b->parts.size() ? b->parts[i] : std::string_view("0");
    if (x.size() != y.size()) return x.size() > y.size();
    if (x != y) return x > y;
  }
  return !a->suffix && b->suffix;
}

inline bool ShouldCheck(int64_t now_seconds, int64_t last_seconds, bool force = false) {
  if (force || last_seconds <= 0) return true;
  if (now_seconds < last_seconds) return false;
  return static_cast<uint64_t>(now_seconds) - static_cast<uint64_t>(last_seconds) >= 43'200;
}

inline bool ShouldNotify(const Release& release, std::string_view current, std::string_view dismissed) {
  if (release.draft || release.prerelease || release.tag == dismissed) return false;
  return IsNewer(release.tag, current);
}

inline std::string SerializeCache(const Cache& cache) {
  using detail::Quote;
  const auto& release = cache.release;
  return "{\"checked_at\":" + std::to_string(cache.checked_at) +
         ",\"failed\":" + (cache.failed ? "true" : "false") +
         ",\"dismissed\":" + Quote(cache.dismissed) + ",\"release\":{\"tag_name\":" + Quote(release.tag) +
         ",\"html_url\":" + Quote(release.html_url) + ",\"name\":" + Quote(release.name) +
         ",\"download_url\":" + Quote(release.download_url) + ",\"checksum_url\":" + Quote(release.checksum_url) +
         ",\"download_size\":" + std::to_string(release.download_size) +
         ",\"prerelease\":" + (release.prerelease ? "true" : "false") +
         ",\"draft\":" + (release.draft ? "true" : "false") + "}}";
}

inline std::optional<Cache> ParseCache(std::string_view input) {
  const auto object = detail::JsonReader(input).Read();
  if (!object || object->kind != detail::Json::Object) return std::nullopt;
  const auto checked = detail::Field(*object, "checked_at", detail::Json::Number);
  const auto release = detail::Field(*object, "release", detail::Json::Object);
  if (!checked || !release) return std::nullopt;
  Cache cache;
  const auto [end, error] = std::from_chars(checked->text.data(), checked->text.data() + checked->text.size(), cache.checked_at);
  if (error != std::errc{} || end != checked->text.data() + checked->text.size() || cache.checked_at < 0) return std::nullopt;
  const auto fields = detail::ReleaseFields(*release);
  if (!fields) return std::nullopt;
  cache.release = *fields;
  if (!detail::StringField(*release, "download_url", cache.release.download_url)) return std::nullopt;
  if (!detail::StringField(*release, "checksum_url", cache.release.checksum_url)) return std::nullopt;
  if (!detail::UInt64Field(*release, "download_size", cache.release.download_size)) return std::nullopt;
  if (!detail::StringField(*object, "dismissed", cache.dismissed)) return std::nullopt;
  if (!detail::BoolField(*object, "failed", cache.failed)) return std::nullopt;
  return cache;
}

}  // namespace rex::glue::update
