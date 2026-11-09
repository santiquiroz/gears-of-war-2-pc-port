// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
//
// What the PC Options menu does outside the cvars: lists of monitors, GPUs and save folders, folder
// picking, restarting the game, and hosting over the internet through scripts/host-online.ps1.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rex::glue::options_actions {

struct NamedValue {
  std::string value;
  std::string label;
};

// "0" is the main display; "N" is the runtime's display N (the main display first, then the rest).
// Empty outside Windows.
std::vector<NamedValue> ListMonitors();

// "auto" first, then each hardware adapter's index with its name. Empty outside Windows.
std::vector<NamedValue> ListGpus();

// "" (default folder), "saves" (portable, next to gow2.toml), the cloud folders found on this PC
// (OneDrive, Google Drive, Dropbox, each as "<root>\GearsOfWar2\saves"), and `current` when it is none
// of them. Labels name the place and show the folder.
std::vector<NamedValue> ListSaveFolders(std::string_view current);

// The standard folder picker. Empty when cancelled or outside Windows. UTF-8.
std::optional<std::string> PickFolder(std::string_view title);

bool OpenFolder(const std::filesystem::path& dir);

// Starts this exe again with the same command line plus --gow2_wait_pid=<this pid>. True when it
// started; the caller then closes the game.
bool RestartGame();

// At start-up: when gow2_wait_pid names a running process, waits up to 20 s for it to exit.
void WaitForPreviousInstance();

struct HostRouter {
  std::string address;
  std::string closing;
};

struct HostResult {
  std::string mode;  // "Start", "Stop" or "Status"
  bool ok = false;
  std::string error;
  std::string public_address;
  std::string peer_name;
  std::vector<int> ports;
  double lease_hours = 0;
  std::string lease_end;
  std::string stop_scheduled_at;
  std::vector<HostRouter> routers;
  int open_ports = 0;
  std::vector<std::string> warnings;
  std::string error_code;
  std::string error_router;
  bool partial = false;
  std::string unmanaged_router;
  std::string unmanaged_target;
};

// The script's last stdout line, "GOW2HOST {...}"; nullopt when it is not one.
std::optional<HostResult> ParseHostLine(std::string_view line);

struct HostRequest {
  int lease_hours = 6;
  std::string hostname;     // net_public_address, may be empty
  std::string allow_peers;  // net_allow_peers, comma-separated, may be empty
  int players = 2;          // gow2_coop_players
  int port_offset = 40000;  // net_port_offset
};

// Runs scripts/host-online.ps1 hidden on a worker thread (Windows only; everything returns false or
// empty elsewhere). One run at a time.
class HostRunner {
 public:
  bool Start(const HostRequest& request);
  bool Stop();
  bool Status();
  bool Busy() const;
  std::optional<HostResult> Last() const;
  std::string ScriptPath() const;  // empty when the script is not found
  bool OpenedThisSession() const;  // a Start succeeded and no Stop succeeded since
  // At exit: runs -Stop and waits for it, at most timeout_ms.
  void StopBlocking(int timeout_ms);
};

HostRunner& Host();

}  // namespace rex::glue::options_actions
