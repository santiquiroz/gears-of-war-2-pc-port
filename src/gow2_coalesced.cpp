// Adapted from gears-judgement-recomp (BSD-3-Clause).
// GoW2's Coalesced tweak layer, see gow2_coalesced.h.

#include "gow2_coalesced.h"
#include "gow2_coalesced_text.h"
#include "gow2_graphics_quality.h"
#include "gow2_native_bindings.h"

#include "gow2_mouse_aim.h"

#ifdef _WIN32
#include <windows.h>
#endif
#if defined(__ANDROID__)
#include "gow2_android_devices.h"
#endif

#include <rex/cvar.h>
#include <rex/logging.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

#if defined(__ANDROID__)
#define GOW2_PROMPTS_HELP                                                                   \
  "Menu button prompts: auto (controller glyphs unless a physical keyboard is connected and " \
  "mnk_mode is on), keyboard or controller."
#else
#define GOW2_PROMPTS_HELP                                                         \
  "Menu button prompts: auto (keyboard labels while no controller is connected, switching " \
  "live when one is plugged in or removed), keyboard or controller (both fixed at launch)."
#endif
REXCVAR_DEFINE_STRING(gow2_prompts, "auto", "input", GOW2_PROMPTS_HELP);
REXCVAR_DEFINE_BOOL(gow2_keycaps, true, "input",
                    "Show keyboard prompts as keycap pictures when the keycap pack is installed "
                    "(tools/keycaps, scripts/build-keycaps.ps1), else as text such as [Space]. "
                    "Takes effect on the next start.");
#undef GOW2_PROMPTS_HELP

REXCVAR_DEFINE_STRING(gow2_system_settings, "", "debug",
                      "Raw engine [SystemSettings] Key=Value pairs separated by ';'. Overrides "
                      "Game quality rows and platform defaults. Only existing keys are changed; "
                      "oversized edits keep the retail engine settings.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

REXCVAR_DEFINE_STRING(gow2_gfx_light_environment_shadows, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "on", "off"});
REXCVAR_DEFINE_STRING(gow2_gfx_dynamic_shadows, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "on", "off"});
REXCVAR_DEFINE_STRING(gow2_gfx_dynamic_lights, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "on", "off"});
REXCVAR_DEFINE_STRING(gow2_gfx_composite_dynamic_lights, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "on", "off"});
REXCVAR_DEFINE_STRING(gow2_gfx_bloom, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "on", "off"});
REXCVAR_DEFINE_STRING(gow2_gfx_use_high_quality_bloom, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "on", "off"});
REXCVAR_DEFINE_STRING(gow2_gfx_motion_blur, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "on", "off"});
REXCVAR_DEFINE_STRING(gow2_gfx_depth_of_field, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "on", "off"});
REXCVAR_DEFINE_STRING(gow2_gfx_ambient_occlusion, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "on", "off"});
REXCVAR_DEFINE_STRING(gow2_gfx_lens_flares, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "on", "off"});
REXCVAR_DEFINE_STRING(gow2_gfx_distortion, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "on", "off"});
REXCVAR_DEFINE_STRING(gow2_gfx_static_decals, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "on", "off"});
REXCVAR_DEFINE_STRING(gow2_gfx_dynamic_decals, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "on", "off"});
REXCVAR_DEFINE_STRING(gow2_gfx_fog_volumes, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "on", "off"});
REXCVAR_DEFINE_STRING(gow2_gfx_speed_tree_leaves, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "on", "off"});
REXCVAR_DEFINE_STRING(gow2_gfx_speed_tree_fronds, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "on", "off"});
REXCVAR_DEFINE_STRING(gow2_gfx_shadow_filter_quality_bias, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "-1", "0", "1"});
REXCVAR_DEFINE_STRING(gow2_gfx_detail_mode, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "0", "1", "2"});
REXCVAR_DEFINE_STRING(gow2_gfx_skeletal_mesh_lod_bias, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "0", "1", "2"});
REXCVAR_DEFINE_STRING(gow2_gfx_particle_lod_bias, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "0", "1", "2"});
REXCVAR_DEFINE_STRING(gow2_gfx_foliage_draw_radius_multiplier, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "0.25", "0.5", "1.000000"});
REXCVAR_DEFINE_STRING(gow2_gfx_max_shadow_resolution, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "256", "512", "800"});
REXCVAR_DEFINE_STRING(gow2_gfx_shadow_texels_per_pixel, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "0.5", "1", "1.27324"});
REXCVAR_DEFINE_STRING(gow2_gfx_decal_cull_distance_scale, "auto", "graphics",
                      "Game quality: auto keeps the platform default; changed at next start.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart)
    .allowed({"auto", "0.4", "0.7", "1.0"});

REXCVAR_DEFINE_BOOL(gow2_pc_options_entry, true, "UI",
                    "The game's Options > Storage Location button opens the port's PC Options "
                    "menu (renamed PC Options). Takes effect on the next start.");

namespace rex::glue {

namespace {

constexpr int kVersion = 10;  // bump when the edit definitions change (invalidates cached copies)

// Bundle layout: BE u32 file count; per file a name, BE u32 section count; per section a name and
// BE u32 key count; per key a key and a value. Strings are a BE int32 length, negative = UTF-16LE
// with terminator.
struct Entry {
  std::u16string key, value;
};
struct Section {
  std::u16string name;
  std::vector<Entry> entries;
};
struct IniFile {
  std::u16string name;
  std::vector<Section> sections;
};

struct Reader {
  const std::vector<uint8_t>& b;
  size_t pos = 0;
  bool ok = true;
  uint32_t U32() {
    if (pos + 4 > b.size()) {
      ok = false;
      return 0;
    }
    uint32_t v = (uint32_t(b[pos]) << 24) | (uint32_t(b[pos + 1]) << 16) | (uint32_t(b[pos + 2]) << 8) |
                 b[pos + 3];
    pos += 4;
    return v;
  }
  std::u16string Str() {
    int32_t len = int32_t(U32());
    if (!ok || len == 0) return {};
    if (len > 0 || len < -(1 << 20)) {
      ok = false;  // narrow strings are not expected in this file
      return {};
    }
    const size_t chars = size_t(-len);
    if (pos + chars * 2 > b.size()) {
      ok = false;
      return {};
    }
    std::u16string s(chars, u'\0');
    for (size_t i = 0; i < chars; ++i) s[i] = char16_t(b[pos + 2 * i] | (b[pos + 2 * i + 1] << 8));
    pos += chars * 2;
    if (!s.empty() && s.back() == u'\0') s.pop_back();
    return s;
  }
};

bool Parse(const std::vector<uint8_t>& bytes, std::vector<IniFile>* out) {
  Reader r{bytes};
  const uint32_t nfiles = r.U32();
  if (!r.ok || nfiles == 0 || nfiles > 4096) return false;
  for (uint32_t f = 0; f < nfiles && r.ok; ++f) {
    IniFile file;
    file.name = r.Str();
    const uint32_t nsec = r.U32();
    if (!r.ok || nsec > (1u << 20)) return false;
    for (uint32_t s = 0; s < nsec && r.ok; ++s) {
      Section sec;
      sec.name = r.Str();
      const uint32_t nk = r.U32();
      if (!r.ok || nk > (1u << 24)) return false;
      sec.entries.reserve(nk);
      for (uint32_t k = 0; k < nk && r.ok; ++k) {
        Entry e;
        e.key = r.Str();
        e.value = r.Str();
        sec.entries.push_back(std::move(e));
      }
      file.sections.push_back(std::move(sec));
    }
    out->push_back(std::move(file));
  }
  return r.ok && r.pos == bytes.size();  // the whole file must be understood
}

void PutU32(std::vector<uint8_t>* o, uint32_t v) {
  o->push_back(uint8_t(v >> 24));
  o->push_back(uint8_t(v >> 16));
  o->push_back(uint8_t(v >> 8));
  o->push_back(uint8_t(v));
}
void PutStr(std::vector<uint8_t>* o, const std::u16string& s) {
  if (s.empty()) {
    PutU32(o, 0);
    return;
  }
  PutU32(o, uint32_t(-int32_t(s.size() + 1)));
  for (char16_t c : s) {
    o->push_back(uint8_t(c & 0xFF));
    o->push_back(uint8_t(c >> 8));
  }
  o->push_back(0);
  o->push_back(0);
}
std::vector<uint8_t> Serialize(const std::vector<IniFile>& files) {
  std::vector<uint8_t> o;
  PutU32(&o, uint32_t(files.size()));
  for (const IniFile& f : files) {
    PutStr(&o, f.name);
    PutU32(&o, uint32_t(f.sections.size()));
    for (const Section& s : f.sections) {
      PutStr(&o, s.name);
      PutU32(&o, uint32_t(s.entries.size()));
      for (const Entry& e : s.entries) {
        PutStr(&o, e.key);
        PutStr(&o, e.value);
      }
    }
  }
  return o;
}

std::string LowerA(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  return s;
}

// Xbox360TOC.txt lines are "FileSize UncompressedSize StartSector Path MD5"; lower-case file name
// -> FileSize. The first entry for a name wins, as in the game's own parser.
std::unordered_map<std::string, uint64_t> ReadTocFileSizes(const std::filesystem::path& toc) {
  std::unordered_map<std::string, uint64_t> sizes;
  std::ifstream in(toc, std::ios::binary);
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const size_t a = line.find(' ');
    const size_t b = a == std::string::npos ? a : line.find(' ', a + 1);
    const size_t c = b == std::string::npos ? b : line.find(' ', b + 1);
    if (c == std::string::npos) continue;
    std::string path = line.substr(c + 1, line.find(' ', c + 1) - c - 1);
    const size_t slash = path.find_last_of("\\/");
    if (slash != std::string::npos) path.erase(0, slash + 1);
    sizes.emplace(LowerA(path), std::strtoull(line.c_str(), nullptr, 10));
  }
  return sizes;
}

// The game reads a bundle at its TOC FileSize (the file reader takes its size from the TOC,
// sub_827F5508) and checks the SHA-1 of exactly those bytes against HashSec (sub_82225018 wraps them
// in FBufferReaderWithSHA). A served file of that exact size keeps the read and the hash right; the
// parser is count driven, so the zero padding is never read as data. A longer edit cannot be served.
bool PadToSize(std::vector<uint8_t>* bytes, uint64_t size) {
  return graphics_quality::PadBundleToSize(bytes, size);
}

std::u16string W(const std::string& s) {
  std::u16string r;
  r = Utf8ToUtf16(s);  // UTF-8 decoded; ASCII converts byte for byte as before.
  return r;
}
bool EndsWith(const std::u16string& s, const std::u16string& suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::u16string LowerW(std::u16string s) {
  for (char16_t& c : s)
    if (c >= u'A' && c <= u'Z') c = char16_t(c - u'A' + u'a');
  return s;
}

// The English bundle names its text files "GearGameUI.int"; every other language bundle names the
// same file "geargameui.FRA", "geargameui.KOR" and so on (and the case differs). An edit written
// for the ".int" name therefore also applies to the bundle's own language file.
bool FileMatches(const std::u16string& name, const std::string& edit_file, const std::u16string& locale) {
  const std::u16string n = LowerW(name), e = LowerW(W(edit_file));
  if (EndsWith(n, e)) return true;
  if (!locale.empty() && EndsWith(e, u".int")) {
    const std::u16string localized = e.substr(0, e.size() - 3) + LowerW(locale);
    return EndsWith(n, localized);
  }
  return false;
}

// Config section/key lookups ignore case, so the edits match that way too.
char16_t Fold(char16_t c) { return (c >= u'A' && c <= u'Z') ? char16_t(c + 32) : c; }
bool EqFold(const std::u16string& wide, const std::string& narrow) {
  if (wide.size() != narrow.size()) return false;
  for (size_t i = 0; i < wide.size(); ++i)
    if (Fold(wide[i]) != Fold(char16_t((unsigned char)narrow[i]))) return false;
  return true;
}
bool StartsWithFold(const std::u16string& wide, const std::string& prefix) {
  if (wide.size() < prefix.size()) return false;
  for (size_t i = 0; i < prefix.size(); ++i)
    if (Fold(wide[i]) != Fold(char16_t((unsigned char)prefix[i]))) return false;
  return true;
}

bool ReadAll(const std::filesystem::path& p, std::vector<uint8_t>* out) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return false;
  out->assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}

struct Edit {
  enum Kind { kSet, kReplaceSub, kAppend, kRemove } kind;
  std::string file;     // file name suffix inside the bundle
  std::string section;  // section name
  std::string key;      // kSet and kAppend: the key; kReplaceSub: a key prefix; kRemove: the key
  std::string value;    // kSet: the new value; kReplaceSub: "from|to"; kAppend: a new entry;
                        // kRemove: the entries whose value starts with this (empty = all)
  std::string match;    // kReplaceSub only: only values containing this text (empty = all)
  std::string locale;   // only bundles with this locale apply the edit (empty = every locale)
  bool optional = false;
  bool pc_options_entry = false;
  bool system_settings = false;
};

const char* const kUI = "GearGameUI.int";

void AddPcOptionsEntry(std::vector<Edit>* e) {
#if defined(__ANDROID__)
  constexpr bool kAndroid = true;
#else
  constexpr bool kAndroid = false;
#endif
  static const char* const kLocales[] = {"int", "esn", "esm", "deu", "fra", "ita",
                                         "pol", "cze", "hun", "rus", "kor", "cht"};
  // The fallback pair (English text, no filter) comes first so each bundle ends with exactly one
  // value: it writes everywhere, then at most one locale pair overwrites it.
  const LocalizedPair fallback = PcOptionsEntryText("int", kAndroid);
  e->push_back({Edit::kSet, "GearGame.int", "OptionsMenu", "StorageLocation",
                std::string(fallback.label), "", "", false, true});
  e->push_back({Edit::kSet, "GearGame.int", "OptionsMenu", "StorageLocation_Desc",
                std::string(fallback.description), "", "", true, true});
  for (const char* loc : kLocales) {
    const LocalizedPair t = PcOptionsEntryText(loc, kAndroid);
    e->push_back({Edit::kSet, "GearGame.int", "OptionsMenu", "StorageLocation",
                  std::string(t.label), "", loc, false, true});
    e->push_back({Edit::kSet, "GearGame.int", "OptionsMenu", "StorageLocation_Desc",
                  std::string(t.description), "", loc, true, true});
  }
}

void AddMouseLook(std::vector<Edit>* e) {
  for (const char* look : {"GBA_LookX", "GBA_LookY", "GBA_LookY_Flip"})
    e->push_back({Edit::kReplaceSub, "Xe-GearInput.ini", "Engine.PlayerInput", "Bindings",
                  "DeadZone=0.3|DeadZone=0.0", look});
}

// Keyboard bindings of the engine itself (SDK-independent): the game's own debug bindings of the
// keys in use go (an F that freezes rendering, a P that stops physics), then each key gets the
// command of the pad button it stands for. The retail editor and debug-camera bindings go too:
// they are unused and make room, because a bundle may not grow past its size in the TOC.
void AddNativeKeys(std::vector<Edit>* e, const CoalescedOptions& o) {
  const char* const kFile = "Xe-GearInput.ini";
  for (const char* section : {"Editor.EditorViewportInput", "UnrealEd.ObjectEditorViewportInput",
                              "Engine.DebugCameraInput"})
    e->push_back({Edit::kRemove, kFile, section, "Bindings", "", "", "", false, false});
  for (const std::string& key : NativeBindingKeys(o.native_bindings))
    e->push_back({Edit::kRemove, kFile, "Engine.PlayerInput", "Bindings",
                  "(Name=\"" + key + "\",", "", "", false, false});
  for (const NativeBinding& b : o.native_bindings)
    e->push_back({Edit::kAppend, kFile, "Engine.PlayerInput", "Bindings", BindingIniValue(b), "", "",
                  false, false});
}

void AddNativeMouse(std::vector<Edit>* e) {
  e->push_back({Edit::kSet, "Xe-GearInput.ini", "Engine.PlayerInput", "bEnableMouseSmoothing",
                "false"});
}

void AddPrompts(std::vector<Edit>* e, const CoalescedOptions& o) {
  for (const auto& kv : o.prompt_labels)
    e->push_back({Edit::kSet, kUI, "ControllerFont", kv.first, kv.second});
}

void AddSystemSettings(std::vector<Edit>* e, const CoalescedOptions& o) {
  for (const auto& kv : o.system_settings)
    e->push_back({Edit::kSet, "Xe-GearEngine.ini", "SystemSettings", kv.first, kv.second, "", "", false, false, true});
}

uint64_t Fnv(const std::string& s, uint64_t h = 1469598103934665603ull) {
  for (unsigned char c : s) {
    h ^= c;
    h *= 1099511628211ull;
  }
  return h;
}

int Apply(std::vector<IniFile>* files, const std::vector<Edit>& edits, const std::u16string& locale,
          bool include_optional = true, bool include_pc_options_entry = true,
          bool include_system_settings = true) {
  int changed = 0;
  for (const Edit& ed : edits) {
    if (ed.optional && !include_optional) continue;
    if (ed.pc_options_entry && !include_pc_options_entry) continue;
    if (ed.system_settings && !include_system_settings) continue;
    if (!ed.locale.empty() && !EqFold(locale, ed.locale)) continue;
    for (IniFile& f : *files) {
      if (!FileMatches(f.name, ed.file, locale)) continue;
      for (Section& s : f.sections) {
        if (!EqFold(s.name, ed.section)) continue;
        if (ed.kind == Edit::kAppend) {
          const std::u16string value = W(ed.value);
          bool present = false;
          for (const Entry& en : s.entries)
            if (EqFold(en.key, ed.key) && en.value == value) present = true;
          if (!present) {
            s.entries.push_back({W(ed.key), value});
            ++changed;
          }
          continue;
        }
        if (ed.kind == Edit::kRemove) {
          const std::u16string prefix = W(ed.value);
          const auto removed = std::remove_if(s.entries.begin(), s.entries.end(), [&](const Entry& en) {
            if (!EqFold(en.key, ed.key) || en.value.size() < prefix.size()) return false;
            for (size_t i = 0; i < prefix.size(); ++i)
              if (Fold(en.value[i]) != Fold(prefix[i])) return false;
            return true;
          });
          changed += int(s.entries.end() - removed);
          s.entries.erase(removed, s.entries.end());
          continue;
        }
        if (ed.kind == Edit::kReplaceSub) {
          const size_t bar = ed.value.find('|');
          const std::u16string from = W(ed.value.substr(0, bar)), to = W(ed.value.substr(bar + 1));
          const std::u16string match = W(ed.match);
          for (Entry& en : s.entries) {
            if (!StartsWithFold(en.key, ed.key)) continue;
            if (!ed.match.empty() && en.value.find(match) == std::u16string::npos) continue;
            size_t p = 0;
            while ((p = en.value.find(from, p)) != std::u16string::npos) {
              en.value.replace(p, from.size(), to);
              p += to.size();
              ++changed;
            }
          }
          continue;
        }
        for (Entry& en : s.entries) {
          if (!EqFold(en.key, ed.key)) continue;
          if (en.value != W(ed.value)) {
            en.value = W(ed.value);
            ++changed;
          }
          break;
        }
      }
    }
  }
  return changed;
}

bool RebuildWithoutOptionalEdits(const std::vector<uint8_t>& bytes, const std::vector<Edit>& edits,
                                 const std::u16string& locale, uint64_t size,
                                 std::vector<uint8_t>* patched, int* changed,
                                 bool include_pc_options_entry = true,
                                 bool include_system_settings = true) {
  std::vector<IniFile> files;
  if (!Parse(bytes, &files)) return false;
  *changed = Apply(&files, edits, locale, false, include_pc_options_entry, include_system_settings);
  *patched = Serialize(files);
  return PadToSize(patched, size);
}

}  // namespace

CoalescedResult PrepareCoalescedOverrides(const std::filesystem::path& game_root,
                                          const std::filesystem::path& cache_dir,
                                          const CoalescedOptions& options) {
  CoalescedResult res;
  if (const char* off = std::getenv("GOW2_COALESCED"); off && off[0] == '0') {
    res.summary = "COALESCED: disabled by GOW2_COALESCED=0";
    return res;
  }
  std::vector<Edit> edits;
  std::string groups;
  if (options.mouse_look) {
    if (options.native_mouse) {
      AddNativeMouse(&edits);
      groups += "mouse-aim ";
    } else {
      AddMouseLook(&edits);
      groups += "mouse-look ";
    }
  }
  if (!options.native_bindings.empty()) {
    AddNativeKeys(&edits, options);
    groups += "native-keys ";
  }
  if (!options.prompt_labels.empty()) {
    AddPrompts(&edits, options);
    groups += "key-prompts ";
  }
  if (options.pc_options_entry) {
    AddPcOptionsEntry(&edits);
    groups += "pc-options-entry ";
  }
  if (!options.system_settings.empty()) {
    AddSystemSettings(&edits, options);
    groups += "system-settings ";
  }
  if (edits.empty()) return res;

  uint64_t h = Fnv("v" + std::to_string(kVersion));
  for (const Edit& e : edits)
    h = Fnv(e.file + "|" + e.section + "|" + e.key + "|" + e.value + "|" + e.match + "|" +
                e.locale + "|" +
                char('0' + e.kind),
            h);

  std::error_code ec;
  const std::filesystem::path dir = game_root / "GearGame" / "CookedXenon";
  const std::filesystem::path out_dir = cache_dir / "coalesced";
  std::filesystem::create_directories(out_dir, ec);
  const std::unordered_map<std::string, uint64_t> toc_sizes =
      ReadTocFileSizes(game_root / "GearGame" / "Xbox360TOC.txt");
  int files_done = 0, values = 0, reused = 0, unreadable = 0, unmatched = 0, too_long = 0;
  int without_optional = 0;
  int without_menu_entry = 0;
  int without_system_settings = 0;
  for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
    const std::filesystem::path src = it->path();
    const std::string name = src.filename().string();
    if (name.rfind("Coalesced_", 0) != 0 || src.extension() != ".bin") continue;
    const std::filesystem::path dst = out_dir / name;
    const std::filesystem::path stamp_path = out_dir / (name + ".stamp");
    const auto size = std::filesystem::file_size(src, ec);
#if defined(__ANDROID__)
    // libc++ counts file times in __int128, which std::to_string has no overload for.
    const auto time = static_cast<long long>(
        std::filesystem::last_write_time(src, ec).time_since_epoch().count());
#else
    const auto time = std::filesystem::last_write_time(src, ec).time_since_epoch().count();
#endif
    const std::string stamp =
        std::to_string(h) + " " + std::to_string(size) + " " + std::to_string(time);
    bool fresh = false;
    bool omitted_optional = false;
    bool omitted_menu_entry = false;
    bool omitted_system_settings = false;
    if (std::filesystem::exists(dst, ec)) {
      std::ifstream sf(stamp_path);
      std::string have;
      std::getline(sf, have);
      fresh = (have == stamp);
      if (fresh) sf >> omitted_optional >> omitted_menu_entry >> omitted_system_settings;
    }
    if (fresh) {
      ++reused;
    } else {
      std::vector<uint8_t> bytes;
      std::vector<IniFile> files;
      if (!ReadAll(src, &bytes) || !Parse(bytes, &files)) {  // unknown layout: leave alone
        ++unreadable;
        continue;
      }
      const std::u16string locale = W(name.substr(10, name.size() - 14));  // Coalesced_XXX.bin -> XXX
      int changed = Apply(&files, edits, locale);
      if (changed == 0) {  // this locale file has none of the sections
        ++unmatched;
        continue;
      }
      std::vector<uint8_t> patched = Serialize(files);
      const auto toc = toc_sizes.find(LowerA(name));
      const uint64_t target_size = toc != toc_sizes.end() ? toc->second : uint64_t(size);
      if (!PadToSize(&patched, target_size)) {
        omitted_optional = true;
        bool fits = RebuildWithoutOptionalEdits(bytes, edits, locale, target_size, &patched, &changed);
        if (!fits && options.pc_options_entry) {
          omitted_menu_entry = true;
          fits = RebuildWithoutOptionalEdits(bytes, edits, locale, target_size, &patched, &changed,
                                             false);
        }
        if (!fits && !options.system_settings.empty()) {
          omitted_system_settings = true;
          fits = graphics_quality::PadOrKeepRetail(&patched, target_size, [&] {
            std::vector<uint8_t> retail;
            RebuildWithoutOptionalEdits(bytes, edits, locale, target_size, &retail, &changed,
                                       !omitted_menu_entry, false);
            return retail;
          });
        }
        if (!fits) {
          ++too_long;
          continue;
        }
      }
      {
        std::ofstream of(dst, std::ios::binary | std::ios::trunc);
        if (!of) continue;
        of.write(reinterpret_cast<const char*>(patched.data()), std::streamsize(patched.size()));
        if (!of) continue;
      }
      std::ofstream sf(stamp_path, std::ios::trunc);
      sf << stamp << "\n" << omitted_optional << " " << omitted_menu_entry << " " << omitted_system_settings << "\n";
      values += changed;
    }
    ++files_done;
    if (omitted_optional) ++without_optional;
    if (omitted_menu_entry) ++without_menu_entry;
    if (omitted_system_settings) ++without_system_settings;
    if (!res.overrides.empty()) res.overrides += ";";
    res.overrides += "GearGame/CookedXenon/" + name + "=" + dst.string();
    HashSecEntry entry;
    entry.name = name;
    std::transform(entry.name.begin(), entry.name.end(), entry.name.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    if (FileSha1(dst, &entry.sha1)) res.hashes.push_back(std::move(entry));
  }
  if (options.mouse_look && !options.native_mouse && files_done > 0) {
    // The mouse is fed to the game as the right stick; physical pads keep the 30% dead zone the
    // bundle no longer applies, through the runtime. Only when the bundle edit is really in use,
    // or pads would get it twice.
    // A value the player already set (for example saved by the menu) wins over this default.
    if (rex::cvar::GetFlagSource("pad_right_stick_deadzone") == rex::cvar::Source::kDefault)
      (void)rex::cvar::SetFlagByName("pad_right_stick_deadzone", "0.3");
  }
  if (files_done == 0) {
    res.summary = "COALESCED: no bundle could be prepared (" + std::to_string(unreadable) +
                  " unreadable, " + std::to_string(too_long) + " longer than their TOC size, " +
                  std::to_string(unmatched) +
                  " without the edited settings); the game files are used unchanged";
  } else {
    res.summary = "COALESCED: " + groups + "applied to " + std::to_string(files_done) + " file(s) (" +
                  std::to_string(values) + " values changed, " + std::to_string(reused) +
                  " reused from cache); the game folder is not modified";
  }
  if (without_optional > 0)
    res.summary += " (" + std::to_string(without_optional) + " without optional descriptions)";
  if (without_menu_entry > 0)
    res.summary += " (" + std::to_string(without_menu_entry) + " without the menu entry)";
  if (without_system_settings > 0)
    res.summary += " (" + std::to_string(without_system_settings) +
                   " without system settings: longer than their TOC size; retail values retained)";
  return res;
}

namespace {

#ifdef _WIN32
bool AnyPhysicalGamepad() {
  struct State {
    DWORD packet;
    WORD buttons;
    BYTE lt, rt;
    SHORT lx, ly, rx, ry;
  };
  using Fn = DWORD(WINAPI*)(DWORD, State*);
  // Loaded once and kept: the live prompts poll this about twice a second.
  static const Fn fn = [] {
    HMODULE m = LoadLibraryW(L"xinput1_4.dll");
    if (!m) m = LoadLibraryW(L"xinput9_1_0.dll");
    return m ? reinterpret_cast<Fn>(GetProcAddress(m, "XInputGetState")) : nullptr;
  }();
  if (!fn) return false;
  for (DWORD i = 0; i < 4; ++i) {
    State s{};
    if (fn(i, &s) == ERROR_SUCCESS) return true;
  }
  return false;
}
#endif

#if defined(__ANDROID__)
bool AndroidKeyboardPromptsWanted() {
  const bool keyboard = PhysicalKeyboardConnected();
  // Without mnk_mode the keyboard does not reach the game, so its labels would be wrong.
  const bool mnk = rex::cvar::Query<bool>("mnk_mode");
  REXLOG_INFO("PROMPTS gow2_prompts=auto: physical keyboard {}, mnk_mode {} -> {} labels",
              keyboard ? "connected" : "not connected", mnk ? "on" : "off",
              keyboard && mnk ? "keyboard" : "controller");
  return keyboard && mnk;
}
#endif

bool AutoPromptsUseKeyboard() {
#ifdef _WIN32
  return !AnyPhysicalGamepad();
#elif defined(__ANDROID__)
  return AndroidKeyboardPromptsWanted();
#else
  return true;
#endif
}

// First entry of a comma-separated keybind list, as the player wrote it in gow2.toml.
std::string FirstKey(const std::string& list) {
  const std::string first = list.substr(0, list.find(','));
  const size_t a = first.find_first_not_of(' ');
  if (a == std::string::npos) return {};
  return first.substr(a, first.find_last_not_of(' ') - a + 1);
}

std::string FriendlyKeyName(const std::string& k) {
  // Keybind values are already display names ("W", "LMB"); only engine names need mapping.
  static const struct { const char* n; const char* f; } kMap[] = {
      {"LeftMouseButton", "LMB"},
      {"RightMouseButton", "RMB"},
      {"MiddleMouseButton", "MMB"},
      {"Return", "Enter"},
      {"Escape", "Esc"},
      {"Control", "Ctrl"},
  };
  for (const auto& m : kMap)
    if (k == m.n) return m.f;
  return k;
}

std::string KeybindLabel(const char* name, const char* fallback) {
  std::string v = rex::cvar::Query<std::string>(name);
  if (v.empty()) v = fallback;
  return "[" + FriendlyKeyName(FirstKey(v)) + "]";
}


}  // namespace

bool HostUiHooksAvailable() {
  return rex::cvar::GetFlagInfo("xam_device_selector_ui_command") != nullptr;
}

CoalescedOptions CoalescedOptionsFromCvars() {
  CoalescedOptions o;
  o.mouse_look = rex::cvar::Query<bool>("mnk_mouse");
  o.native_mouse = NativeMouseAimWanted();
  if (rex::cvar::Query<bool>("gow2_native_keys") && o.native_mouse) {
    o.native_bindings = NativeKeyBindings([](const char* name) { return rex::cvar::Query<std::string>(name); });
  }
  o.pc_options_entry = REXCVAR_GET(gow2_pc_options_entry) && HostUiHooksAvailable();
#if defined(__ANDROID__)
  constexpr bool android = true;
#else
  constexpr bool android = false;
#endif
  o.system_settings = graphics_quality::Compose(android, [](std::string_view id) {
    return rex::cvar::Query<std::string>(id);
  }, REXCVAR_GET(gow2_system_settings));
  // The live swap (gow2_prompt_swap.cpp) rewrites the strings in memory: leave the bundles alone.
  if (PromptsFollowController()) return o;
  const std::string mode = REXCVAR_GET(gow2_prompts);
  const bool keyboard = mode == "keyboard" || (mode != "controller" && AutoPromptsUseKeyboard());
  if (!keyboard) return o;
  o.prompt_labels = KeyboardPromptLabels();
  return o;
}

bool PromptsFollowController() {
#ifdef _WIN32
  return REXCVAR_GET(gow2_prompts) != "controller";  // auto follows the pad, keyboard stays put
#else
  return false;
#endif
}

bool PhysicalGamepadConnected() {
#ifdef _WIN32
  return AnyPhysicalGamepad();
#else
  return false;
#endif
}

std::vector<ControllerFontSet> ReadControllerFontOriginals(const std::filesystem::path& game_root) {
  std::vector<ControllerFontSet> sets;
  std::error_code ec;
  const std::filesystem::path dir = game_root / "GearGame" / "CookedXenon";
  for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
    const std::string name = it->path().filename().string();
    if (name.rfind("Coalesced_", 0) != 0 || it->path().extension() != ".bin") continue;
    std::vector<uint8_t> bytes;
    std::vector<IniFile> files;
    if (!ReadAll(it->path(), &bytes) || !Parse(bytes, &files)) continue;
    ControllerFontSet set;
    set.locale = LowerA(name.substr(10, name.size() - 14));
    for (const IniFile& f : files) {
      if (f.name.find(u"GearGameUI") == std::u16string::npos) continue;
      for (const Section& s : f.sections) {
        if (!EqFold(s.name, "ControllerFont")) continue;
        for (const Entry& e : s.entries) {
          std::string key;
          for (char16_t c : e.key) key.push_back(char(c));
          set.values.emplace_back(std::move(key), e.value);
        }
      }
    }
    if (!set.values.empty()) sets.push_back(std::move(set));
  }
  return sets;
}

std::vector<std::pair<std::string, std::string>> KeyboardPromptLabels() {
  return {
      {"Controller_A", KeybindLabel("keybind_a", "Space")},
      {"Controller_B", KeybindLabel("keybind_b", "F,V")},
      {"Controller_X", KeybindLabel("keybind_x", "E")},
      {"Controller_Y", KeybindLabel("keybind_y", "Q")},
      {"Controller_LeftStick", "[WASD]"},
      {"Controller_LeftStickDown", "[S]"},
      {"Controller_RightStick", "[Mouse]"},
      {"Controller_LeftStickClick", KeybindLabel("keybind_lstick_press", "C")},
      {"Controller_RightStickClick", KeybindLabel("keybind_rstick_press", "MMB,Z")},
      {"Controller_LeftTrigger", KeybindLabel("keybind_left_trigger", "RMB")},
      {"Controller_RightTrigger", KeybindLabel("keybind_right_trigger", "LMB")},
      {"Controller_DPad", "[1-4]"},
      {"Controller_DPadUp", KeybindLabel("keybind_dpad_up", "1")},
      {"Controller_DPadRight", KeybindLabel("keybind_dpad_right", "2")},
      {"Controller_DPadDown", KeybindLabel("keybind_dpad_down", "3")},
      {"Controller_DPadLeft", KeybindLabel("keybind_dpad_left", "4")},
      {"Controller_ArrowUp", "[W]"},
      {"Controller_ArrowDown", "[S]"},
      {"Controller_ArrowLeft", "[A]"},
      {"Controller_ArrowRight", "[D]"},
      {"Controller_LeftBumper", KeybindLabel("keybind_left_shoulder", "G")},
      {"Controller_RightBumper", KeybindLabel("keybind_right_shoulder", "R")},
      {"Controller_Start", KeybindLabel("keybind_start", "Return,Escape")},
      {"Controller_Back", KeybindLabel("keybind_back", "Tab")},
      {"Controller_UpDown", "[W/S]"},
      {"Controller_UpDownLeftRight", "[WASD]"},
  };
}

}  // namespace rex::glue
