// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Strict JSON reader for the host-online.ps1 result line, no third-party code.
#include "gow2_host_line.h"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace rex::glue::host_line {

namespace {

constexpr int kMaxDepth = 32;

struct Value {
  enum class Kind { Null, Bool, Number, String, Array, Object };
  Kind kind = Kind::Null;
  bool boolean = false;
  double number = 0;
  std::string text;
  std::vector<Value> items;
  std::vector<std::pair<std::string, Value>> fields;

  Value() = default;
  Value(const Value&) = default;
  Value(Value&&) noexcept = default;
  Value& operator=(const Value&) = default;
  Value& operator=(Value&&) noexcept = default;
  ~Value();
};

// Out of line: libstdc++ instantiates ~vector<pair<string, Value>> while Value is still incomplete.
Value::~Value() = default;

bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

bool IsDigit(char c) { return c >= '0' && c <= '9'; }

bool IsAlphanumeric(char c) {
  return IsDigit(c) || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

std::string_view TrimAddress(std::string_view value) {
  constexpr std::string_view kSpace = " \t\r\n\f\v";
  const size_t first = value.find_first_not_of(kSpace);
  if (first == std::string_view::npos) return {};
  return value.substr(first, value.find_last_not_of(kSpace) - first + 1);
}

bool IsValidPeer(std::string_view value) {
  value = TrimAddress(value);
  if (value.empty() || value.size() > 253 || value.front() == '-') return false;
  for (char c : value) {
    if (!IsAlphanumeric(c) && c != '.' && c != ':' && c != '/' && c != '-') return false;
  }
  return true;
}

void AppendUtf8(std::string* out, uint32_t code) {
  if (code < 0x80) {
    out->push_back(char(code));
  } else if (code < 0x800) {
    out->push_back(char(0xC0 | (code >> 6)));
    out->push_back(char(0x80 | (code & 0x3F)));
  } else if (code < 0x10000) {
    out->push_back(char(0xE0 | (code >> 12)));
    out->push_back(char(0x80 | ((code >> 6) & 0x3F)));
    out->push_back(char(0x80 | (code & 0x3F)));
  } else {
    out->push_back(char(0xF0 | (code >> 18)));
    out->push_back(char(0x80 | ((code >> 12) & 0x3F)));
    out->push_back(char(0x80 | ((code >> 6) & 0x3F)));
    out->push_back(char(0x80 | (code & 0x3F)));
  }
}

class Reader {
 public:
  explicit Reader(std::string_view input) : input_(input) {}

  bool Read(Value* out) {
    if (!ReadValue(out, 0)) return false;
    SkipSpace();
    return pos_ == input_.size();
  }

 private:
  char Peek() const { return pos_ < input_.size() ? input_[pos_] : '\0'; }

  void SkipSpace() {
    while (pos_ < input_.size() && IsSpace(input_[pos_])) ++pos_;
  }

  bool ReadWord(std::string_view word) {
    if (input_.substr(pos_, word.size()) != word) return false;
    pos_ += word.size();
    return true;
  }

  bool ReadLiteral(Value* out) {
    if (ReadWord("true")) {
      out->kind = Value::Kind::Bool;
      out->boolean = true;
      return true;
    }
    if (ReadWord("false")) {
      out->kind = Value::Kind::Bool;
      out->boolean = false;
      return true;
    }
    if (ReadWord("null")) {
      out->kind = Value::Kind::Null;
      return true;
    }
    return false;
  }

  bool ReadValue(Value* out, int depth) {
    if (depth > kMaxDepth) return false;
    SkipSpace();
    const char c = Peek();
    switch (c) {
      case '{': return ReadObject(out, depth);
      case '[': return ReadArray(out, depth);
      case '"': return ReadString(out);
      default: break;
    }
    if (c == '-' || IsDigit(c)) return ReadNumber(out);
    return ReadLiteral(out);
  }

  bool ReadObject(Value* out, int depth) {
    ++pos_;
    Value value;
    value.kind = Value::Kind::Object;
    SkipSpace();
    if (Peek() == '}') {
      ++pos_;
      *out = std::move(value);
      return true;
    }
    while (true) {
      SkipSpace();
      if (Peek() != '"') return false;
      Value key;
      if (!ReadString(&key)) return false;
      SkipSpace();
      if (Peek() != ':') return false;
      ++pos_;
      Value item;
      if (!ReadValue(&item, depth + 1)) return false;
      value.fields.emplace_back(std::move(key.text), std::move(item));
      SkipSpace();
      if (Peek() == ',') {
        ++pos_;
        continue;
      }
      if (Peek() != '}') return false;
      ++pos_;
      *out = std::move(value);
      return true;
    }
  }

  bool ReadArray(Value* out, int depth) {
    ++pos_;
    Value value;
    value.kind = Value::Kind::Array;
    SkipSpace();
    if (Peek() == ']') {
      ++pos_;
      *out = std::move(value);
      return true;
    }
    while (true) {
      Value item;
      if (!ReadValue(&item, depth + 1)) return false;
      value.items.push_back(std::move(item));
      SkipSpace();
      if (Peek() == ',') {
        ++pos_;
        continue;
      }
      if (Peek() != ']') return false;
      ++pos_;
      *out = std::move(value);
      return true;
    }
  }

  bool ReadDigits() {
    const size_t start = pos_;
    while (pos_ < input_.size() && IsDigit(input_[pos_])) ++pos_;
    return pos_ > start;
  }

  bool ReadNumber(Value* out) {
    const size_t start = pos_;
    if (Peek() == '-') ++pos_;
    if (Peek() == '0') {
      ++pos_;
    } else if (IsDigit(Peek())) {
      ++pos_;
      while (IsDigit(Peek())) ++pos_;
    } else {
      return false;
    }
    if (Peek() == '.') {
      ++pos_;
      if (!ReadDigits()) return false;
    }
    if (Peek() == 'e' || Peek() == 'E') {
      ++pos_;
      if (Peek() == '+' || Peek() == '-') ++pos_;
      if (!ReadDigits()) return false;
    }
    const std::string token(input_.substr(start, pos_ - start));
    char* end = nullptr;
    const double number = std::strtod(token.c_str(), &end);
    if (end == nullptr || end != token.c_str() + token.size()) return false;
    out->kind = Value::Kind::Number;
    out->number = number;
    return true;
  }

  bool ReadHex4(uint32_t* out) {
    if (pos_ + 4 > input_.size()) return false;
    uint32_t code = 0;
    for (int i = 0; i < 4; ++i) {
      const char c = input_[pos_++];
      code <<= 4;
      if (c >= '0' && c <= '9') {
        code |= uint32_t(c - '0');
      } else if (c >= 'a' && c <= 'f') {
        code |= uint32_t(c - 'a' + 10);
      } else if (c >= 'A' && c <= 'F') {
        code |= uint32_t(c - 'A' + 10);
      } else {
        return false;
      }
    }
    *out = code;
    return true;
  }

  bool ReadUnicode(std::string* text) {
    uint32_t code = 0;
    if (!ReadHex4(&code)) return false;
    if (code >= 0xD800 && code <= 0xDBFF) {
      if (pos_ + 2 > input_.size() || input_[pos_] != '\\' || input_[pos_ + 1] != 'u') return false;
      pos_ += 2;
      uint32_t low = 0;
      if (!ReadHex4(&low)) return false;
      if (low < 0xDC00 || low > 0xDFFF) return false;
      code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
    } else if (code >= 0xDC00 && code <= 0xDFFF) {
      return false;
    }
    AppendUtf8(text, code);
    return true;
  }

  bool ReadEscape(std::string* text) {
    if (pos_ >= input_.size()) return false;
    const char c = input_[pos_++];
    switch (c) {
      case '"': text->push_back('"'); return true;
      case '\\': text->push_back('\\'); return true;
      case '/': text->push_back('/'); return true;
      case 'b': text->push_back('\b'); return true;
      case 'f': text->push_back('\f'); return true;
      case 'n': text->push_back('\n'); return true;
      case 'r': text->push_back('\r'); return true;
      case 't': text->push_back('\t'); return true;
      case 'u': return ReadUnicode(text);
      default: break;
    }
    return false;
  }

  bool ReadString(Value* out) {
    ++pos_;
    std::string text;
    while (true) {
      if (pos_ >= input_.size()) return false;
      const char c = input_[pos_++];
      if (c == '"') {
        out->kind = Value::Kind::String;
        out->text = std::move(text);
        return true;
      }
      if (c == '\\') {
        if (!ReadEscape(&text)) return false;
      } else if (uint8_t(c) < 0x20) {
        return false;
      } else {
        text.push_back(c);
      }
    }
  }

  std::string_view input_;
  size_t pos_ = 0;
};

const Value* FindField(const Value& object, std::string_view key) {
  for (const auto& field : object.fields) {
    if (field.first == key) return &field.second;
  }
  return nullptr;
}

bool TakeString(const Value* value, std::string* out) {
  if (value == nullptr || value->kind == Value::Kind::Null) {
    out->clear();
    return true;
  }
  if (value->kind != Value::Kind::String) return false;
  *out = value->text;
  return true;
}

bool TakeBool(const Value* value, bool* out) {
  if (value == nullptr) {
    *out = false;
    return true;
  }
  if (value->kind != Value::Kind::Bool) return false;
  *out = value->boolean;
  return true;
}

bool TakeDouble(const Value* value, double* out) {
  if (value == nullptr || value->kind == Value::Kind::Null) {
    *out = 0;
    return true;
  }
  if (value->kind != Value::Kind::Number) return false;
  *out = value->number;
  return true;
}

bool TakeInt(const Value* value, int* out) {
  if (value == nullptr) {
    *out = 0;
    return true;
  }
  if (value->kind != Value::Kind::Number) return false;
  const double number = value->number;
  if (number < -2147483648.0 || number > 2147483647.0) return false;
  const int whole = int(number);
  if (double(whole) != number) return false;
  *out = whole;
  return true;
}

bool TakeIntList(const Value* value, std::vector<int>* out) {
  out->clear();
  if (value == nullptr) return true;
  if (value->kind != Value::Kind::Array) return false;
  for (const Value& item : value->items) {
    int number = 0;
    if (!TakeInt(&item, &number)) return false;
    out->push_back(number);
  }
  return true;
}

bool TakeStringList(const Value* value, std::vector<std::string>* out) {
  out->clear();
  if (value == nullptr) return true;
  if (value->kind != Value::Kind::Array) return false;
  for (const Value& item : value->items) {
    std::string text;
    if (!TakeString(&item, &text)) return false;
    out->push_back(std::move(text));
  }
  return true;
}

bool TakeRouters(const Value* value, std::vector<options_actions::HostRouter>* out) {
  out->clear();
  if (value == nullptr) return true;
  if (value->kind != Value::Kind::Array) return false;
  for (const Value& item : value->items) {
    if (item.kind != Value::Kind::Object) return false;
    options_actions::HostRouter router;
    if (!TakeString(FindField(item, "address"), &router.address)) return false;
    if (!TakeString(FindField(item, "closing"), &router.closing)) return false;
    out->push_back(std::move(router));
  }
  return true;
}

}  // namespace

std::wstring QuoteWindowsArg(std::wstring_view arg) {
  std::wstring quoted(1, L'"');
  size_t slashes = 0;
  for (wchar_t c : arg) {
    if (c == L'\\') {
      ++slashes;
      continue;
    }
    quoted.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
    quoted.push_back(c);
    slashes = 0;
  }
  quoted.append(slashes * 2, L'\\');
  quoted.push_back(L'"');
  return quoted;
}

bool IsValidHostname(std::string_view value) {
  if (value.empty()) return true;
  if (!IsAlphanumeric(value.front())) return false;
  const size_t colon = value.find(':');
  const std::string_view host = value.substr(0, colon);
  if (host.size() > 253) return false;
  for (char c : host) {
    if (!IsAlphanumeric(c) && c != '.' && c != '-') return false;
  }
  if (colon == std::string_view::npos) return true;
  const std::string_view port = value.substr(colon + 1);
  if (port.empty() || port.size() > 5) return false;
  for (char c : port) {
    if (!IsDigit(c)) return false;
  }
  return true;
}

bool IsValidAllowPeers(std::string_view value) {
  if (value.empty()) return true;
  while (true) {
    const size_t comma = value.find(',');
    if (!IsValidPeer(value.substr(0, comma))) return false;
    if (comma == std::string_view::npos) return true;
    value.remove_prefix(comma + 1);
  }
}

std::optional<options_actions::HostResult> ParseHostLine(std::string_view line) {
  constexpr std::string_view kPrefix = "GOW2HOST ";
  if (line.size() < kPrefix.size() || line.substr(0, kPrefix.size()) != kPrefix) {
    return std::nullopt;
  }
  Reader reader(line.substr(kPrefix.size()));
  Value root;
  if (!reader.Read(&root) || root.kind != Value::Kind::Object) return std::nullopt;
  options_actions::HostResult result;
  if (!TakeString(FindField(root, "mode"), &result.mode)) return std::nullopt;
  if (!TakeBool(FindField(root, "ok"), &result.ok)) return std::nullopt;
  if (!TakeString(FindField(root, "error"), &result.error)) return std::nullopt;
  if (!TakeString(FindField(root, "public_address"), &result.public_address)) return std::nullopt;
  if (!TakeString(FindField(root, "peer_name"), &result.peer_name)) return std::nullopt;
  if (!TakeIntList(FindField(root, "ports"), &result.ports)) return std::nullopt;
  if (!TakeDouble(FindField(root, "lease_hours"), &result.lease_hours)) return std::nullopt;
  if (!TakeString(FindField(root, "lease_end"), &result.lease_end)) return std::nullopt;
  if (!TakeString(FindField(root, "stop_scheduled_at"), &result.stop_scheduled_at)) {
    return std::nullopt;
  }
  if (!TakeRouters(FindField(root, "routers"), &result.routers)) return std::nullopt;
  if (!TakeInt(FindField(root, "open_ports"), &result.open_ports)) return std::nullopt;
  if (!TakeStringList(FindField(root, "warnings"), &result.warnings)) return std::nullopt;
  if (!TakeString(FindField(root, "error_code"), &result.error_code)) return std::nullopt;
  if (!TakeString(FindField(root, "error_router"), &result.error_router)) return std::nullopt;
  if (!TakeBool(FindField(root, "partial"), &result.partial)) return std::nullopt;
  if (!TakeString(FindField(root, "unmanaged_router"), &result.unmanaged_router)) return std::nullopt;
  if (!TakeString(FindField(root, "unmanaged_target"), &result.unmanaged_target)) return std::nullopt;
  return result;
}

}  // namespace rex::glue::host_line
