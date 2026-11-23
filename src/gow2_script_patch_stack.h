// gow2 - ReXGlue Recompiled Project
//
// One comparable line for the co-op script patch groups applied at startup, so peers can tell
// they run the same set. Pure text and hashing; no guest access, no logging.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace rex::glue::script_patch {

struct StackEntry {
  std::string name;
  std::string state;  // 16 lowercase hex digits (applied), "off", or "skipped"
};

std::string FormatGroupHash(uint64_t set_hash);

// Wire text: what every peer, host or client, runs. Only the coop-client state goes on the
// wire; host-only settings and the player count stay out on purpose.
std::string WireText(std::string_view coop_client_state);

std::string StackText(int players, const std::vector<StackEntry>& entries);

uint64_t StackHash(std::string_view text);

std::string StackLine(int players, const std::vector<StackEntry>& entries);

}  // namespace rex::glue::script_patch
