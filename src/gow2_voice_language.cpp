// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_voice_language.h"

#include <algorithm>
#include <cstddef>

namespace rex::glue::voice_language {

namespace {

std::string Lower(std::string_view s) {
  std::string out(s);
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  return out;
}

std::string_view FileName(std::string_view path) {
  size_t slash = path.find_last_of("\\/");
  return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

std::string_view GuestPath(std::string_view path) {
  if (path.starts_with("..\\") || path.starts_with("../")) path.remove_prefix(3);
  return path;
}

struct Package {
  size_t toc = 0;
  size_t offset = 0;
  size_t length = 0;
  TocEntry entry;
  std::string base;
  std::string lang;
};

// <B>_LOC_<L>.xxx with B not GearGame; base and lang come back lowercased.
bool ParseVoiceName(std::string_view name, std::string& base, std::string& lang) {
  std::string lower = Lower(name);
  if (!lower.ends_with(".xxx")) return false;
  lower.resize(lower.size() - 4);
  size_t loc = lower.rfind("_loc_");
  if (loc == std::string::npos || loc == 0 || loc + 5 == lower.size()) return false;
  base = lower.substr(0, loc);
  lang = lower.substr(loc + 5);
  return base != "geargame";
}

std::vector<Package> CollectPackages(const std::vector<TocFile>& tocs) {
  std::vector<Package> packages;
  for (size_t t = 0; t < tocs.size(); ++t) {
    std::string_view text = tocs[t].text;
    size_t begin = 0;
    while (begin < text.size()) {
      size_t end = text.find('\n', begin);
      if (end == std::string_view::npos) end = text.size();
      size_t length = end - begin;
      if (length > 0 && text[begin + length - 1] == '\r') --length;
      Package pkg;
      if (auto entry = ParseTocLine(text.substr(begin, length));
          entry && ParseVoiceName(FileName(entry->path), pkg.base, pkg.lang)) {
        pkg.toc = t;
        pkg.offset = begin;
        pkg.length = length;
        pkg.entry = std::move(*entry);
        packages.push_back(std::move(pkg));
      }
      begin = end + 1;
    }
  }
  return packages;
}

const Package* Find(const std::vector<Package>& packages, std::string_view base, std::string_view lang) {
  for (const auto& pkg : packages)
    if (pkg.base == base && pkg.lang == lang) return &pkg;
  return nullptr;
}

bool HasVoiceSet(const std::vector<Package>& packages, std::string_view lang) {
  return std::any_of(packages.begin(), packages.end(), [&](const Package& pkg) { return pkg.lang == lang; });
}

struct LineEdit {
  size_t offset;
  size_t length;
  std::string line;
};

std::string ApplyEdits(std::string_view text, std::vector<LineEdit> edits) {
  std::sort(edits.begin(), edits.end(), [](const LineEdit& a, const LineEdit& b) { return a.offset < b.offset; });
  std::string out;
  size_t pos = 0;
  for (const auto& edit : edits) {
    out.append(text.substr(pos, edit.offset - pos));
    out.append(edit.line);
    pos = edit.offset + edit.length;
  }
  out.append(text.substr(pos));
  return out;
}

}  // namespace

std::optional<TocEntry> ParseTocLine(std::string_view line) {
  std::vector<std::string> fields;
  size_t begin = 0;
  while (begin <= line.size()) {
    size_t end = line.find(' ', begin);
    if (end == std::string_view::npos) end = line.size();
    if (end == begin) return std::nullopt;
    fields.emplace_back(line.substr(begin, end - begin));
    begin = end + 1;
  }
  if (fields.size() != 5) return std::nullopt;
  return TocEntry{fields[0], fields[1], fields[2], fields[3], fields[4]};
}

std::string FormatTocLine(const TocEntry& entry) {
  return entry.size + " " + entry.uncompressed + " " + entry.third + " " + entry.path + " " + entry.md5;
}

std::string TextLanguageCode(uint32_t user_language) {
  switch (user_language) {
    case 5: return "ESM";
    case 4: return "FRA";
    case 3: return "DEU";
    case 6: return "ITA";
    case 11: return "POL";
    case 12: return "RUS";
    case 7: return "KOR";
    case 8: return "CHT";
    default: return "INT";
  }
}

std::string VoiceChoiceCode(std::string_view choice) {
  if (choice == "en") return "INT";
  if (choice == "es") return "ESM";
  if (choice == "fr") return "FRA";
  return {};
}

VoicePlan PlanVoiceSwap(const std::vector<TocFile>& tocs, std::string_view text_code, std::string_view voice_code) {
  VoicePlan plan;
  std::vector<Package> packages = CollectPackages(tocs);
  std::string text = Lower(text_code);
  std::string voice = Lower(voice_code);
  std::string effective = HasVoiceSet(packages, text) ? text : "int";

  if (voice.empty() || voice == effective) {
    plan.summary = "VOICES text " + std::string(text_code) + ", voices same as text: nothing swapped";
    return plan;
  }
  if (!HasVoiceSet(packages, voice)) {
    plan.summary = "VOICES voice set " + std::string(voice_code) + " is not on this disc: nothing swapped";
    return plan;
  }

  std::vector<std::vector<LineEdit>> edits(tocs.size());
  std::vector<const Package*> targeted;
  for (const auto& source : packages) {
    if (source.lang != voice) continue;
    const Package* target = Find(packages, source.base, text);
    if (!target) target = Find(packages, source.base, "int");
    if (!target || target == &source) continue;
    if (std::find(targeted.begin(), targeted.end(), target) != targeted.end()) continue;
    targeted.push_back(target);

    TocEntry line = source.entry;
    line.path = target->entry.path;
    edits[target->toc].push_back({target->offset, target->length, FormatTocLine(line)});
    plan.swaps.push_back({std::string(GuestPath(target->entry.path)), std::string(FileName(source.entry.path))});
  }

  for (size_t t = 0; t < tocs.size(); ++t)
    if (!edits[t].empty()) plan.edited_tocs.push_back({tocs[t].name, ApplyEdits(tocs[t].text, std::move(edits[t]))});
  plan.summary = "VOICES text " + std::string(text_code) + ", voices " + std::string(voice_code) + ": " +
                 std::to_string(plan.swaps.size()) + " packages swapped";
  return plan;
}

}  // namespace rex::glue::voice_language
