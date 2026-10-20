// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
#include "gow2_prompt_swap.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/ppc.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>

#include "gow2_atlas_image.h"
#include "gow2_coalesced.h"
#include "gow2_coalesced_text.h"
#include "gow2_engine_tick.h"
#include "gow2_guest.h"
#include "gow2_keycap_layout.inc"
#include "gow2_keycaps.h"
#include "gow2_prompt_scan.h"

namespace rex::glue {
namespace {

using Clock = std::chrono::steady_clock;

struct Slot {
  uint32_t header = 0;
  uint32_t chars = 0;
  uint32_t max = 0;
  std::string key;
  std::u16string original;
};

struct State {
  std::filesystem::path game_root;
  bool installed = false;
  bool worker_started = false;
  std::atomic<bool> ready{false};
  std::atomic<bool> keycaps{false};  // the font's cells hold keycaps (set by the worker)
  std::mutex mutex;
  std::vector<Slot> slots;  // written once by the worker, then read by the game thread
  // Game thread only:
  Clock::time_point next_poll{};
  int applied = -1;  // 1 keyboard, 0 controller, -1 nothing written yet
  std::string applied_signature;
  int pending = -1;  // a reading seen once, not yet confirmed
  int atlas_slot = -1;  // the variant folder the tutorial pictograms were last composed into
};

State& S() {
  static State* state = new State;  // leaked: the worker may outlive static destruction
  return *state;
}

// Calls fn(guest address, host pointer, size) for each committed, readable range of the guest
// virtual heaps below the executable image.
template <typename Fn>
void ForEachReadableRange(Fn fn) {
  rex::Runtime* runtime = rex::Runtime::instance();
  auto* memory = runtime ? runtime->memory() : nullptr;
  if (!memory) return;
  constexpr uint64_t kEnd = 0x80000000ull;
  uint64_t addr = 0;
  while (addr < kEnd) {
    auto* heap = memory->LookupHeap(uint32_t(addr));
    if (!heap) {
      addr += 0x10000;
      continue;
    }
    rex::memory::HeapAllocationInfo info{};
    uint64_t step = heap->page_size();
    if (heap->QueryRegionInfo(uint32_t(addr), &info)) {
      if (info.region_size) step = info.region_size;
      if ((info.state & rex::memory::kMemoryAllocationCommit) &&
          (info.protect & rex::memory::kMemoryProtectRead)) {
        const uint64_t len = std::min<uint64_t>(step, kEnd - addr);
        fn(uint32_t(addr), memory->TranslateVirtual(uint32_t(addr)), size_t(len));
      }
    }
    addr += step;
  }
}

// Finds the FStrings of the loaded [ControllerFont] and picks the locale whose values they match.
bool Scan(std::vector<Slot>* out, std::string* locale) {
  const std::vector<ControllerFontSet> sets = ReadControllerFontOriginals(S().game_root);
  std::vector<std::u16string> wanted;
  for (const auto& set : sets)
    for (const auto& kv : set.values)
      if (std::find(wanted.begin(), wanted.end(), kv.second) == wanted.end())
        wanted.push_back(kv.second);
  if (wanted.empty()) return false;

  std::vector<promptscan::StringHit> strings;
  ForEachReadableRange([&](uint32_t addr, const uint8_t* mem, size_t size) {
    for (const auto& h : promptscan::FindStrings(mem, size, addr, wanted)) strings.push_back(h);
  });
  if (strings.empty()) return false;

  std::unordered_map<uint32_t, uint32_t> lengths;  // chars address -> characters with the NUL
  for (const auto& h : strings) lengths[h.addr] = uint32_t(wanted[h.index].size() + 1);
  std::vector<promptscan::HeaderHit> headers;
  ForEachReadableRange([&](uint32_t addr, const uint8_t* mem, size_t size) {
    for (const auto& h : promptscan::FindHeaders(mem, size, addr, lengths)) headers.push_back(h);
  });

  // A string with no header is a piece of some larger text or a raw file buffer: not ours.
  std::map<std::string, std::vector<Slot>> by_locale;
  for (const auto& set : sets) {
    for (const auto& kv : set.values) {
      for (const auto& s : strings) {
        if (wanted[s.index] != kv.second) continue;
        for (const auto& h : headers) {
          if (h.chars != s.addr) continue;
          by_locale[set.locale].push_back({h.header, h.chars, h.max, kv.first, kv.second});
        }
      }
    }
  }
  size_t best = 0;
  for (const auto& kv : by_locale) {
    std::set<std::string> keys;
    for (const Slot& s : kv.second) keys.insert(s.key);
    if (keys.size() > best) {
      best = keys.size();
      *locale = kv.first;
      *out = kv.second;
    }
  }
  return best > 0;
}

// ---- keycaps ------------------------------------------------------------------------------

std::filesystem::path PackDir() {
  const std::string override = rex::cvar::Query<std::string>("texture_pack_dir");
  const std::filesystem::path root =
      override.empty() ? rex::filesystem::GetExecutableFolder() / "textures" : std::filesystem::path(override);
  return root / "4D53082D" / "replace";
}

// The pack writes one replacement per font page; without both the cells would point at art that
// is not there.
bool KeycapFilesPresent();

bool KeycapPackInstalled() {
  return KeycapFilesPresent() && rex::cvar::Query<bool>("texture_replace");
}

void StoreCell(uint8_t* base, uint32_t record, const keycap::Cell& c) {
  guest::StoreU32(base, record + 0, uint32_t(c.u));
  guest::StoreU32(base, record + 4, uint32_t(c.v));
  guest::StoreU32(base, record + 8, uint32_t(c.w));
  guest::StoreU32(base, record + 12, uint32_t(c.h));
  guest::StoreU8(base, record + 16, c.page);  // TextureIndex; the bytes after it are padding
  guest::StoreU32(base, record + 20, uint32_t(c.vo));
}

// Points the characters that show keycaps at their cells. Characters nothing else uses.
bool PatchFontCells(uint8_t* base) {
  std::vector<promptscan::CharTableHit> tables;
  ForEachReadableRange([&](uint32_t addr, const uint8_t* mem, size_t size) {
    for (const auto& h : promptscan::FindCharTable(mem, size, addr)) tables.push_back(h);
  });
  if (tables.size() != 1 || tables[0].stride < 24) {
    REXLOG_WARN("PROMPTS keycaps: the font's cell table was not found exactly once ({} hits)",
                tables.size());
    return false;
  }
  const auto& t = tables[0];
  for (const keycap::Cell& c : keycap::kCells) StoreCell(base, t.table + uint32_t(c.ch) * t.stride, c);
  REXLOG_INFO("PROMPTS keycaps: {} cells patched into the font table at 0x{:08X}",
              sizeof(keycap::kCells) / sizeof(keycap::kCells[0]), t.table);
  return true;
}

bool KeycapFilesPresent() {
  if (!rex::cvar::Query<bool>("gow2_keycaps")) return false;
  std::error_code ec;
  for (const char* hash : keycap::kPageHash)
    if (!std::filesystem::exists(PackDir() / (std::string(hash) + ".dds"), ec)) return false;
  // The pages must be the ones this build's cell table was made for.
  std::ifstream id(PackDir() / "keycaps.id");
  std::string installed;
  std::getline(id, installed);
  if (installed != keycap::kLayoutId) {
    REXLOG_WARN("PROMPTS keycaps: the pack in {} is for another layout ({} vs {}): run scripts/build-keycaps.ps1",
                PackDir().string(), installed, keycap::kLayoutId);
    return false;
  }
  return true;
}

// The tutorial pictograms have Xbox buttons painted into them. The pack holds that picture without
// them (replace_kbd_base); while no controller is connected the keycaps of the player's bindings
// are drawn onto it, the result goes to replace_kbdA or replace_kbdB (alternating, so the variant
// name changes and the texture pack reloads the picture) and that variant is turned on.
std::filesystem::path AtlasBaseFile() {
  return PackDir().parent_path() / "replace_kbd_base" / (std::string(keycap::kAtlasHash) + ".dds");
}

bool VariantPresent() {
  if (!rex::cvar::Query<bool>("gow2_keycaps")) return false;
  std::error_code ec;
  return std::filesystem::exists(AtlasBaseFile(), ec);
}

void SetVariant(const char* name) {
  (void)rex::cvar::SetFlagByName("texture_replace_variant", name);
}

const keycap::Cell* CellFor(char16_t ch) {
  for (const keycap::Cell& c : keycap::kCells)
    if (c.ch == ch) return &c;
  return nullptr;
}

// The one character of a one-glyph keycap markup ("<Fonts:...>X<Fonts:/>"), 0 for anything else.
char16_t MarkupGlyph(const std::u16string& markup) {
  const std::u16string open = keycap::Markup(u"");
  const size_t tag = open.find(u'>');
  const size_t close = markup.rfind(u'<');
  if (tag == std::u16string::npos || close == std::u16string::npos || close != tag + 2) return 0;
  return markup[tag + 1];
}

struct PageImages {
  atlas::Image page[2];
  bool loaded = false;
};

bool LoadPages(PageImages* pages) {
  if (pages->loaded) return true;
  for (int i = 0; i < 2; ++i)
    if (!atlas::ReadDds(PackDir() / (std::string(keycap::kPageHash[i]) + ".dds"), &pages->page[i], nullptr))
      return false;
  pages->loaded = true;
  return true;
}

// Draws the keycap of each button sprite onto the keyboard base of the atlas.
bool ComposeAtlas(const std::map<std::string, keycap::Prompt>& caps, bool mouse_look, int slot) {
  static PageImages pages;
  if (!LoadPages(&pages)) return false;
  atlas::Image image;
  std::vector<uint8_t> header;
  if (!atlas::ReadDds(AtlasBaseFile(), &image, &header)) return false;
  for (const keycap::AtlasSprite& sprite : keycap::kAtlasSprites) {
    char16_t glyph = 0;
    const std::string button = sprite.button;
    if (button == "RS" && mouse_look) {
      glyph = keycap::CharFor("IC_MOUSE");
    } else {
      const auto it = caps.find(button == "RS" ? "Controller_RightStick" : "Controller_" + button);
      if (it != caps.end()) glyph = MarkupGlyph(it->second.brief);
    }
    const keycap::Cell* cell = glyph ? CellFor(glyph) : nullptr;
    if (!cell) continue;
    const atlas::Image& src = pages.page[cell->page];
    const int scale = src.width / 256;
    const int cw = (cell->w - 1) * scale, ch = (cell->h - 1) * scale;
    // A square-ish cap the size of the disc it replaces; a wide cap keeps its shape and shrinks
    // until it fits the sprite.
    const int box_w = sprite.box[2] - sprite.box[0] + 1;
    float th = 24.0f, tw = th * cw / ch;
    if (tw > box_w) {
      th *= box_w / tw;
      tw = float(box_w);
    }
    const int cx = (sprite.disc[0] + sprite.disc[2]) / 2, cy = (sprite.disc[1] + sprite.disc[3]) / 2;
    atlas::Blit(image, src, cell->u * scale + scale / 2, cell->v * scale + scale / 2, cw, ch,
                cx - int(tw / 2), cy - int(th / 2), std::max(1, int(tw)), std::max(1, int(th)));
  }
  const std::filesystem::path out =
      PackDir().parent_path() / (slot ? "replace_kbdB" : "replace_kbdA") / (std::string(keycap::kAtlasHash) + ".dds");
  return atlas::WriteDds(out, header, image);
}

void Worker() {
  // The bundle is parsed during start-up; give the engine a moment, then retry while it loads.
  for (int attempt = 1; attempt <= 20; ++attempt) {
    std::this_thread::sleep_for(std::chrono::seconds(attempt == 1 ? 1 : 3));
    std::vector<Slot> slots;
    std::string locale;
    const auto t0 = Clock::now();
    if (!Scan(&slots, &locale)) continue;
    std::set<std::string> keys;
    for (const Slot& s : slots) keys.insert(s.key);
    REXLOG_INFO("PROMPTS live: found {} strings for {} keys (locale {}) in {} ms, attempt {}",
                slots.size(), keys.size(), locale,
                std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count(), attempt);
    {
      std::lock_guard<std::mutex> lock(S().mutex);
      S().slots = std::move(slots);
    }
    if (KeycapPackInstalled()) {
      uint8_t* base = rex::Runtime::instance()->memory()->virtual_membase();
      S().keycaps = PatchFontCells(base);
    } else {
      REXLOG_INFO("PROMPTS keycaps: no keycap pack in {}; prompts use text labels",
                  PackDir().string());
    }
    S().ready = true;
    return;
  }
  REXLOG_WARN("PROMPTS live: the [ControllerFont] strings were not found; prompts stay as loaded");
}

void Write(uint8_t* base, const Slot& slot, const std::u16string& text) {
  if (text.size() + 1 > slot.max) return;
  for (size_t i = 0; i < text.size(); ++i) {
    guest::StoreU8(base, slot.chars + uint32_t(i * 2), uint8_t(text[i] >> 8));
    guest::StoreU8(base, slot.chars + uint32_t(i * 2 + 1), uint8_t(text[i]));
  }
  const uint32_t end = slot.chars + uint32_t(text.size() * 2);
  guest::StoreU8(base, end, 0);
  guest::StoreU8(base, end + 1, 0);
  guest::StoreU32(base, slot.header + 4, uint32_t(text.size() + 1));
}

// Test aid: GOW2_PROMPTS_PAD_FILE names a file whose first character, 1 or 0, stands in for
// "a controller is connected" and can be changed while the game runs.
bool PadConnected() {
  static const char* file = std::getenv("GOW2_PROMPTS_PAD_FILE");
  if (file && *file) {
    std::ifstream in(file);
    char c = 0;
    if (in.get(c) && (c == '1' || c == '0')) return c == '1';
  }
  return PhysicalGamepadConnected();
}

// What a slot shows in keyboard mode: keycaps when the font has them and the string has room,
// else the bracketed text.
std::u16string KeyboardText(const Slot& slot, const std::map<std::string, keycap::Prompt>& caps,
                            const std::map<std::string, std::string>& text) {
  const auto cap = caps.find(slot.key);
  if (cap != caps.end()) {
    if (!cap->second.full.empty() && cap->second.full.size() + 1 <= slot.max) return cap->second.full;
    if (!cap->second.brief.empty() && cap->second.brief.size() + 1 <= slot.max) return cap->second.brief;
  }
  const auto t = text.find(slot.key);
  return t != text.end() ? Utf8ToUtf16(t->second) : slot.original;
}

// Test aid: GOW2_TICK_LOG=1 logs the engine's frame times every five seconds (TICKS lines).
void LogTickRate() {
  static const bool on = std::getenv("GOW2_TICK_LOG") != nullptr;
  if (!on) return;
  static Clock::time_point start{}, last{};
  static std::vector<double> ms;
  const auto now = Clock::now();
  if (start == Clock::time_point{}) start = last = now;
  ms.push_back(std::chrono::duration<double, std::milli>(now - last).count());
  last = now;
  if (now - start < std::chrono::seconds(5)) return;
  std::sort(ms.begin(), ms.end());
  double sum = 0;
  for (double v : ms) sum += v;
  REXLOG_INFO("TICKS {} frames in {:.1f} s: avg {:.1f} ms, median {:.1f}, p99 {:.1f}, max {:.1f}", ms.size(),
              std::chrono::duration<double>(now - start).count(), sum / ms.size(), ms[ms.size() / 2],
              ms[size_t(ms.size() * 0.99)], ms.back());
  ms.clear();
  start = now;
}

void PromptSwapTick(const PPCContext& ctx, uint8_t* base) {
  (void)ctx;
  LogTickRate();
  State& s = S();
  if (!s.installed) return;
  if (!s.worker_started) {
    s.worker_started = true;
    std::thread(Worker).detach();
    return;
  }
  if (!s.ready) return;
  const auto now = Clock::now();
  if (now < s.next_poll) return;
  s.next_poll = now + std::chrono::milliseconds(500);

  // gow2_prompts = keyboard keeps the keyboard labels whatever is plugged in.
  const int keyboard = (rex::cvar::Query<std::string>("gow2_prompts") == "keyboard" || !PadConnected()) ? 1 : 0;
  std::map<std::string, keycap::Prompt> caps;
  std::map<std::string, std::string> text;
  std::string signature = keyboard ? (s.keycaps ? "K|" : "T|") : "C|";
  if (keyboard) {
    for (const auto& kv : KeyboardPromptLabels()) {
      text[kv.first] = kv.second;
      signature += kv.first + "=" + kv.second + ";";
    }
    if (s.keycaps) {
      const auto get = [](const char* name) { return rex::cvar::Query<std::string>(name); };
      for (const std::string& key : keycap::ControllerKeys()) {
        keycap::Prompt p = keycap::PromptFor(key, get);
        signature += std::string(p.full.begin(), p.full.end()) + ";";  // narrow copy: change detection only
        caps[key] = std::move(p);
      }
    }
  }
  if (keyboard == s.applied && signature == s.applied_signature) {
    s.pending = -1;
    return;
  }
  // Two equal readings in a row, so a controller that blinks out for a poll does not flip the text.
  if (s.pending != keyboard && keyboard != s.applied) {
    s.pending = keyboard;
    return;
  }
  s.pending = -1;

  std::lock_guard<std::mutex> lock(s.mutex);
  int written = 0;
  for (const Slot& slot : s.slots) {
    Write(base, slot, keyboard ? KeyboardText(slot, caps, text) : slot.original);
    ++written;
  }
  s.applied = keyboard;
  s.applied_signature = signature;
  if (keyboard && s.keycaps && VariantPresent()) {
    const int next = s.atlas_slot == 0 ? 1 : 0;
    if (ComposeAtlas(caps, rex::cvar::Query<bool>("mnk_mouse"), next)) {
      s.atlas_slot = next;
      SetVariant(next ? "kbdB" : "kbdA");
    }
  } else {
    SetVariant("");
  }
  REXLOG_INFO("PROMPTS live: {} labels ({} strings written)",
              keyboard ? (s.keycaps ? "keycap" : "keyboard text") : "controller", written);
}

[[maybe_unused]] const bool kPromptSwapRegistered = (AddEngineTickListener(&PromptSwapTick), true);

}  // namespace

void PrepareKeycapPack() {
  if (!PromptsFollowController() || (!KeycapFilesPresent() && !VariantPresent())) return;
  // The pages are swapped in by the texture pack. The GPU plugin registers texture_replace after
  // this runs, so the setting goes in as the environment variable it reads when it does.
#ifdef _WIN32
  if (!std::getenv("REX_TEXTURE_REPLACE")) _putenv_s("REX_TEXTURE_REPLACE", "true");
#endif
}

void InstallPromptSwap(const std::filesystem::path& game_root) {
  if (!PromptsFollowController()) return;
  S().game_root = game_root;
  S().installed = true;
  REXLOG_INFO("PROMPTS live: prompts follow the connected controller");
}

}  // namespace rex::glue
