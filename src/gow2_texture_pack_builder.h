// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Builds a texture pack from the player's own dump with the bundled texconv and Real-ESRGAN (Windows).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace rex::glue::texture_pack_builder {

enum class State : uint8_t { kIdle, kRunning, kDone, kFailed, kCancelled };

struct Snapshot {
  State state = State::kIdle;
  uint32_t done = 0;
  uint32_t total = 0;
  std::string message;  // the failure for kFailed, the pack name for kDone
  uint64_t finished = 0;  // counts finished builds, so the menu notices one ending
};

bool ToolsPresent();
size_t DumpCount();  // valid dump names, rescanned at most every few seconds
bool Start(uint32_t scale, bool high_quality);
void Cancel();
Snapshot GetSnapshot();
// Quitting the app cancels the build and waits for its thread.
void Shutdown();

}  // namespace rex::glue::texture_pack_builder
