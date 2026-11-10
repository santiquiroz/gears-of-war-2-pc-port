// SPDX-License-Identifier: BSD-3-Clause
#include "gow2_mods.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <system_error>

#include "gow2_sha_table.h"
#include "gow2_texture_packs.h"

namespace rex::glue::mods {

namespace {

namespace fs = std::filesystem;
namespace vl = voice_language;
namespace tp = texture_packs;

std::string Utf8(const fs::path& path) {
  const std::u8string text = path.u8string();
  return std::string(text.begin(), text.end());
}

fs::path FromUtf8(const std::string& text) {
  return fs::path(std::u8string(text.begin(), text.end()));
}

std::string Lower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
  });
  return text;
}

std::string Trim(std::string text) {
  const size_t begin = text.find_first_not_of(" \t\r");
  if (begin == std::string::npos) return {};
  const size_t end = text.find_last_not_of(" \t\r");
  return text.substr(begin, end - begin + 1);
}

bool ReadWhole(const fs::path& path, std::string& text) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return !in.bad();
}

bool WriteWhole(const fs::path& path, const std::string& text) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) return false;
  out.write(text.data(), static_cast<std::streamsize>(text.size()));
  out.close();
  return !out.fail();
}

bool IsToc(const std::string& name) {
  const std::string lower = Lower(name);
  return lower.starts_with("xbox360toc") && lower.ends_with(".txt");
}

uint32_t ReadBig32(const uint8_t* bytes) {
  return (uint32_t(bytes[0]) << 24) | (uint32_t(bytes[1]) << 16) |
         (uint32_t(bytes[2]) << 8) | uint32_t(bytes[3]);
}

uint64_t CompressedUncompressedSize(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::array<uint8_t, 16> header{};
  if (!in.read(reinterpret_cast<char*>(header.data()), header.size())) return 0;
  if (ReadBig32(header.data()) != 0x9e2a83c1) return 0;
  return ReadBig32(header.data() + 12);
}

std::array<uint32_t, 64> Md5Shifts() {
  return {7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
          5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
          4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
          6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
}

std::array<uint32_t, 64> Md5Constants() {
  return {0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
          0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
          0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
          0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
          0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
          0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
          0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
          0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
}

uint32_t Rotate(uint32_t value, uint32_t shift) {
  return (value << shift) | (value >> (32 - shift));
}

std::string Md5(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return {};
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
  const uint64_t bit_length = uint64_t(bytes.size()) * 8;
  bytes.push_back(0x80);
  while (bytes.size() % 64 != 56) bytes.push_back(0);
  for (int i = 0; i < 8; ++i) bytes.push_back(uint8_t(bit_length >> (i * 8)));
  uint32_t a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;
  const auto shifts = Md5Shifts();
  const auto constants = Md5Constants();
  for (size_t block = 0; block < bytes.size(); block += 64) {
    std::array<uint32_t, 16> words{};
    for (size_t i = 0; i < words.size(); ++i)
      words[i] = uint32_t(bytes[block + 4 * i]) | (uint32_t(bytes[block + 4 * i + 1]) << 8) |
                 (uint32_t(bytes[block + 4 * i + 2]) << 16) | (uint32_t(bytes[block + 4 * i + 3]) << 24);
    uint32_t a = a0, b = b0, c = c0, d = d0;
    for (uint32_t i = 0; i < 64; ++i) {
      uint32_t f, g;
      if (i < 16) { f = (b & c) | (~b & d); g = i; }
      else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
      else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
      else { f = c ^ (b | ~d); g = (7 * i) % 16; }
      const uint32_t next = d;
      d = c;
      c = b;
      b += Rotate(a + f + constants[i] + words[g], shifts[i]);
      a = next;
    }
    a0 += a; b0 += b; c0 += c; d0 += d;
  }
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (uint32_t word : {a0, b0, c0, d0})
    for (int i = 0; i < 4; ++i) out << std::setw(2) << ((word >> (i * 8)) & 0xff);
  return out.str();
}

std::string Relative(const fs::path& path) {
  fs::path clean = path.lexically_normal();
  std::string text;
  for (const fs::path& part : clean) {
    if (!text.empty()) text += '\\';
    text += Utf8(part);
  }
  return text;
}

bool Ignored(const std::string& relative, std::string& warning) {
  const std::string lower = Lower(relative);
  const size_t slash = lower.find_last_of('\\');
  const std::string leaf = lower.substr(slash == std::string::npos ? 0 : slash + 1);
  const size_t dot = leaf.find_last_of('.');
  const std::string extension = dot == std::string::npos ? "" : leaf.substr(dot);
  if (extension == ".xex" || extension == ".exe" || extension == ".dll") {
    warning = "executable and library files are skipped";
    return true;
  }
  if (leaf.starts_with("coalesced") && extension == ".bin") {
    warning = "Coalesced overrides are not supported yet";
    return true;
  }
  return false;
}

void ReadMetadata(const fs::path& path, Mod& mod) {
  std::ifstream in(path, std::ios::binary);
  std::string line;
  while (std::getline(in, line)) {
    line = Trim(line);
    if (line.empty() || line.front() == '[' || line.front() == ';' || line.front() == '#') continue;
    const size_t equal = line.find('=');
    if (equal == std::string::npos) continue;
    const std::string key = Lower(Trim(line.substr(0, equal)));
    const std::string value = Trim(line.substr(equal + 1));
    if (key == "name") mod.name = value;
    if (key == "author") mod.author = value;
    if (key == "version") mod.version = value;
    if (key == "description") mod.description = value;
    if (key == "url") mod.url = value;
  }
}

}  // namespace

std::vector<Mod> FindMods(const fs::path& root) {
  std::vector<Mod> mods;
  std::error_code ec;
  for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
       !ec && it != end; it.increment(ec)) {
    std::error_code type_ec;
    if (!it->is_directory(type_ec)) continue;
    const std::string folder = Utf8(it->path().filename());
    if (folder.empty() || folder.front() == '_' || folder.front() == '.') continue;
    Mod mod;
    mod.folder = folder;
    mod.name = folder;
    ReadMetadata(it->path() / "mod.ini", mod);
    mod.textures = texture_packs::IsPackRoot(it->path());
    mods.push_back(std::move(mod));
  }
  std::sort(mods.begin(), mods.end(), [](const Mod& a, const Mod& b) { return Lower(a.folder) < Lower(b.folder); });
  return mods;
}

std::vector<File> FindFiles(const fs::path& root, const std::vector<Mod>& mods) {
  std::vector<File> files;
  for (const Mod& mod : mods) {
    const fs::path game_mod = root / FromUtf8(mod.folder) / "GearGame";
    std::error_code ec;
    for (fs::recursive_directory_iterator it(game_mod, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec)) {
      std::error_code type_ec;
      if (!it->is_regular_file(type_ec)) continue;
      File file;
      file.mod_folder = mod.folder;
      file.relative_path = Relative(it->path().lexically_relative(game_mod));
      file.source = Utf8(fs::absolute(it->path(), ec).lexically_normal());
      if (ec) { ec.clear(); file.source = Utf8(it->path()); }
      file.ignored = Ignored(file.relative_path, file.warning);
      if (!file.ignored) {
        file.size = it->file_size(type_ec);
        if (type_ec) continue;
        const auto modified = it->last_write_time(type_ec);
        if (!type_ec) file.modified = static_cast<uint64_t>(modified.time_since_epoch().count());
        const fs::path source = it->path();
        file.md5 = Md5(source);
        if (file.md5.empty() || !FileSha1(source, &file.sha1)) continue;
        file.uncompressed = CompressedUncompressedSize(source);
      }
      files.push_back(std::move(file));
    }
  }
  return files;
}

namespace {

std::vector<vl::TocFile> ReadTocs(const fs::path& game_root, const fs::path& voice_overrides) {
  std::vector<vl::TocFile> tocs;
  std::string voice_lines;
  ReadWhole(voice_overrides, voice_lines);
  std::istringstream overrides(voice_lines);
  std::string line;
  while (std::getline(overrides, line)) {
    const size_t equal = line.find('=');
    if (equal == std::string::npos || Lower(line.substr(0, equal)).find("xbox360toc") == std::string::npos) continue;
    const std::string name = line.substr(line.find_last_of("\\/") == std::string::npos ? 0 : line.find_last_of("\\/") + 1,
                                         equal - (line.find_last_of("\\/") == std::string::npos ? 0 : line.find_last_of("\\/") + 1));
    std::string text;
    if (ReadWhole(FromUtf8(line.substr(equal + 1)), text)) tocs.push_back({name, std::move(text)});
  }
  std::error_code ec;
  for (fs::directory_iterator it(game_root / "GearGame", ec), end; !ec && it != end; it.increment(ec)) {
    const std::string name = Utf8(it->path().filename());
    if (!IsToc(name)) continue;
    if (std::any_of(tocs.begin(), tocs.end(), [&](const vl::TocFile& toc) { return Lower(toc.name) == Lower(name); })) continue;
    std::string text;
    if (ReadWhole(it->path(), text)) tocs.push_back({name, std::move(text)});
  }
  return tocs;
}

std::vector<std::string> ModLogLines(const std::vector<Mod>& mods, const std::vector<File>& files) {
  std::vector<std::string> lines;
  for (const Mod& mod : mods) {
    const auto count = std::count_if(files.begin(), files.end(), [&](const File& file) {
      return file.mod_folder == mod.folder && !file.ignored;
    });
    lines.push_back("mods: " + mod.name + " " + mod.version + " by " + mod.author + " - " +
                    std::to_string(count) + " files, textures " + (mod.textures ? "yes" : "no"));
  }
  return lines;
}

bool AppendTocCopies(const fs::path& output_dir, const std::vector<vl::TocFile>& tocs, std::string& overrides) {
  for (const vl::TocFile& toc : tocs) {
    const fs::path copy = output_dir / FromUtf8(toc.name);
    if (!WriteWhole(copy, toc.text)) return false;
    overrides += "GearGame\\" + toc.name + "=" + Utf8(fs::absolute(copy).lexically_normal()) + "\n";
  }
  return true;
}

}  // namespace

Plan BuildPlan(const std::vector<Mod>& mods, const std::vector<File>& files,
               const std::vector<vl::TocFile>& tocs) {
  Plan plan;
  std::vector<vl::TocFile> edited = tocs;
  std::vector<std::pair<std::string, const File*>> winners;
  for (const File& file : files) {
    if (file.ignored) {
      plan.warnings.push_back("mods: " + file.mod_folder + "\\GearGame\\" + file.relative_path + " skipped: " + file.warning);
      continue;
    }
    const std::string guest = "GearGame\\" + file.relative_path;
    plan.override_lines.push_back(guest + "=" + file.source);
    const std::string key = Lower(file.relative_path);
    auto found = std::find_if(winners.begin(), winners.end(), [&](const auto& item) { return item.first == key; });
    if (found == winners.end()) winners.push_back({key, &file});
    else found->second = &file;
  }
  for (const Mod& mod : mods)
    if (mod.textures) plan.texture_folders.push_back(mod.folder);
  std::reverse(plan.texture_folders.begin(), plan.texture_folders.end());

  for (const auto& [key, file] : winners) {
    const std::string toc_path = "..\\GearGame\\" + file->relative_path;
    bool listed = false;
    for (vl::TocFile& toc : edited) {
      std::string output;
      size_t begin = 0;
      while (begin < toc.text.size()) {
        size_t end = toc.text.find('\n', begin);
        if (end == std::string::npos) end = toc.text.size();
        size_t length = end - begin;
        const bool crlf = length && toc.text[begin + length - 1] == '\r';
        if (crlf) --length;
        std::string_view raw(toc.text.data() + begin, length);
        auto entry = vl::ParseTocLine(raw);
        if (entry && Lower(entry->path) == Lower(toc_path)) {
          entry->size = std::to_string(file->size);
          entry->uncompressed = std::to_string(file->uncompressed);
          entry->md5 = file->md5;
          output += vl::FormatTocLine(*entry);
          listed = true;
        } else output.append(raw);
        if (crlf) output += '\r';
        if (end < toc.text.size()) output += '\n';
        begin = end + 1;
      }
      toc.text = std::move(output);
    }
    if (!listed && !edited.empty()) {
      vl::TocEntry entry{std::to_string(file->size), std::to_string(file->uncompressed), "0", toc_path, file->md5};
      auto it = std::find_if(edited.begin(), edited.end(), [](const vl::TocFile& toc) { return Lower(toc.name) == "xbox360toc.txt"; });
      if (it != edited.end()) {
        vl::TocFile& main = *it;
        if (!main.text.empty() && main.text.back() != '\n') main.text += '\n';
        main.text += vl::FormatTocLine(entry) + "\n";
      }
    }
  }
  plan.winners = std::move(winners);
  for (size_t i = 0; i < edited.size(); ++i)
    if (edited[i].text != tocs[i].text) plan.edited_tocs.push_back(std::move(edited[i]));
  return plan;
}

Setup Prepare(const fs::path& game_root, const fs::path& cache_root, const fs::path& mods_root,
              const fs::path& voice_overrides_file) {
  Setup setup;
  const std::vector<Mod> mods = FindMods(mods_root);
  if (mods.empty()) return setup;
  // ponytail: hashes every mod file at each start; cache by size and mtime if big mods slow it.
  const std::vector<File> files = FindFiles(mods_root, mods);
  const Plan plan = BuildPlan(mods, files, ReadTocs(game_root, voice_overrides_file));
  setup.log_lines = ModLogLines(mods, files);
  setup.warnings = plan.warnings;
  const fs::path output_dir = cache_root / "mods";
  std::error_code ec;
  fs::create_directories(output_dir, ec);
  std::string overrides;
  ReadWhole(voice_overrides_file, overrides);
  if (!overrides.empty() && overrides.back() != '\n') overrides += '\n';
  // A TOC copy that cannot be written would leave the sizes of the replaced files wrong: no mods then.
  if (ec || !AppendTocCopies(output_dir, plan.edited_tocs, overrides)) {
    setup.warnings.push_back("mods: cannot write to " + Utf8(output_dir) + "; mods are off");
    return setup;
  }
  for (const std::string& line : plan.override_lines) overrides += line + "\n";
  const fs::path overrides_file = output_dir / "overrides.txt";
  if (!WriteWhole(overrides_file, overrides)) {
    setup.warnings.push_back("mods: cannot write " + Utf8(overrides_file) + "; mods are off");
    return setup;
  }
  setup.active = true;
  setup.overrides_file = overrides_file;
  for (const std::string& folder : plan.texture_folders)
    setup.texture_folders.push_back(Utf8(mods_root / FromUtf8(folder)));
  for (const auto& [key, file] : plan.winners)
    setup.hashes.push_back({Lower(Utf8(FromUtf8(file->relative_path).filename())), file->sha1, true});
  return setup;
}

}  // namespace rex::glue::mods