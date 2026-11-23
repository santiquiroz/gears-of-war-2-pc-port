// gow2 - ReXGlue Recompiled Project
//
// The startup stack line of the co-op script patch groups; see the header.
#include "gow2_script_patch_stack.h"

#include <cstdio>

namespace rex::glue::script_patch {

std::string FormatGroupHash(uint64_t set_hash) {
  char out[17];
  std::snprintf(out, sizeof(out), "%016llx", static_cast<unsigned long long>(set_hash));
  return std::string(out);
}

std::string WireText(std::string_view coop_client_state) {
  return std::string("wire=2 coop-client=") + std::string(coop_client_state);
}

std::string StackText(int players, const std::vector<StackEntry>& entries) {
  std::string text = "players=" + std::to_string(players);
  for (const StackEntry& entry : entries) {
    text += " " + entry.name + "=" + entry.state;
  }
  return text;
}

uint64_t StackHash(std::string_view text) {
  // The true FNV-1a basis (FNV("a") == 0xaf63dc4c8601ec8c); not the patch-table variant.
  uint64_t hash = 14695981039346656037ull;
  for (unsigned char byte : text) {
    hash ^= byte;
    hash *= 1099511628211ull;
  }
  return hash;
}

std::string StackLine(int players, const std::vector<StackEntry>& entries) {
  const std::string text = StackText(players, entries);
  return text + " combined=" + FormatGroupHash(StackHash(text));
}

}  // namespace rex::glue::script_patch
