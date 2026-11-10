// gow2 - ReXGlue Recompiled Project
//
// The game checks some files it reads (the Coalesced_*.bin bundles among them) against SHA-1
// hashes in its HashSec resource ("<name>\0" followed by 20 hash bytes, one entry per file) and
// treats a mismatch as a disc read error. Files the port serves in place of the originals (the
// edited Coalesced copies) need their own hashes written into that table before the game runs.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace rex::glue {

struct HashSecEntry {
  std::string name;                 // file name as the table spells it, lower case
  std::array<uint8_t, 20> sha1{};  // SHA-1 of the file the game will actually read
  bool optional = false;           // mod files: most are not in the table, so no warning for those
};

// SHA-1 of a host file. Slow enough (seconds for a few MB) that it must not run once the XEX is
// loaded: a stall of about two seconds there makes the game report a disc read error.
bool FileSha1(const std::filesystem::path& path, std::array<uint8_t, 20>* out);

// Writes each entry's hash into the table in guest memory. Call from OnPostLoadXexImage; only
// copies bytes. Returns the entries updated. GOW2_HASHSEC=0 turns it off for troubleshooting.
int PatchHashSec(const std::vector<HashSecEntry>& entries);

}  // namespace rex::glue
