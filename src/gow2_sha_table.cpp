// gow2 - ReXGlue Recompiled Project
#include "gow2_sha_table.h"

#include <cstdlib>
#include <cstring>
#include <fstream>

#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <rex/runtime.h>

#include "third_party/TinySHA1.hpp"

namespace rex::glue {

namespace {

// HashSec resource of this XEX (title 4D53082D, module hash BDB97753374D520A), from its resource
// directory: 83220280 - 83220858.
constexpr uint32_t kHashSecBegin = 0x83220280;
constexpr uint32_t kHashSecEnd = 0x83220858;

// Offset of the hash bytes that follow "<name>\0" in the table, or SIZE_MAX.
size_t FindEntry(const uint8_t* table, size_t size, const std::string& name) {
  const size_t need = name.size() + 1 + 20;
  for (size_t i = 0; i + need <= size; ++i) {
    if (std::memcmp(table + i, name.c_str(), name.size() + 1) == 0) return i + name.size() + 1;
  }
  return SIZE_MAX;
}

}  // namespace

bool FileSha1(const std::filesystem::path& path, std::array<uint8_t, 20>* out) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return false;
  sha1::SHA1 sha;
  std::vector<char> buffer(1 << 16);
  while (file) {
    file.read(buffer.data(), std::streamsize(buffer.size()));
    if (file.gcount() > 0) sha.processBytes(buffer.data(), size_t(file.gcount()));
  }
  const uint32_t* words = sha.getDigest();
  for (size_t i = 0; i < 5; ++i) {
    (*out)[i * 4 + 0] = uint8_t(words[i] >> 24);
    (*out)[i * 4 + 1] = uint8_t(words[i] >> 16);
    (*out)[i * 4 + 2] = uint8_t(words[i] >> 8);
    (*out)[i * 4 + 3] = uint8_t(words[i]);
  }
  return true;
}

int PatchHashSec(const std::vector<HashSecEntry>& entries) {
  auto* runtime = rex::Runtime::instance();
  if (entries.empty() || !runtime || !runtime->memory()) return 0;
  if (const char* off = std::getenv("GOW2_HASHSEC"); off && off[0] == '0') {
    REXLOG_INFO("HASHSEC: disabled by GOW2_HASHSEC=0");
    return 0;
  }
  uint8_t* table = runtime->memory()->virtual_membase() + kHashSecBegin;
  const uint32_t size = kHashSecEnd - kHashSecBegin;
  // Host pages only, and only when they are not writable already: restoring through the guest
  // heap applied its read-only page-table state to a page the runtime keeps writable (the table
  // shares it with the ContSrvr resource) and the game then failed with a disc read error.
  // mprotect needs a page-aligned range (VirtualProtect rounds by itself).
  const uintptr_t page = rex::memory::page_size();
  uint8_t* page_base = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(table) & ~(page - 1));
  const size_t page_length = ((table + size - page_base) + page - 1) & ~(page - 1);
  size_t length = page_length;
  rex::memory::PageAccess access = rex::memory::PageAccess::kNoAccess;
  rex::memory::QueryProtect(page_base, length, access);
  const bool writable = access == rex::memory::PageAccess::kReadWrite ||
                        access == rex::memory::PageAccess::kExecuteReadWrite;
  rex::memory::PageAccess old_access = access;
  if (!writable && !rex::memory::Protect(page_base, page_length,
                                         rex::memory::PageAccess::kReadWrite, &old_access)) {
    REXLOG_WARN("HASHSEC: cannot make the hash table writable; edited files will fail the check");
    return 0;
  }
  int patched = 0;
  for (const HashSecEntry& entry : entries) {
    const size_t at = FindEntry(table, size, entry.name);
    if (at == SIZE_MAX) {
      if (!entry.optional) REXLOG_WARN("HASHSEC: no table entry for {}", entry.name);
      continue;
    }
    std::memcpy(table + at, entry.sha1.data(), entry.sha1.size());
    ++patched;
  }
  if (!writable) rex::memory::Protect(page_base, page_length, old_access, nullptr);
  REXLOG_INFO("HASHSEC: {} of {} file hashes replaced with the edited copies' (page was {})", patched,
              entries.size(), writable ? "writable" : "read-only");
  return patched;
}

}  // namespace rex::glue
