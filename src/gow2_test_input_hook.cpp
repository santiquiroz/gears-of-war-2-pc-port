// gow2 - ReXGlue Recompiled Project
//
// Reads the gow2_test_input file and merges it into guest user 0's pad (gow2_test_input.h).

#include "gow2_test_input.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>

#include <rex/cvar.h>
#include <rex/logging.h>

#include "gow2_guest.h"
#include "gow2_host_ui.h"

REXCVAR_DEFINE_STRING(gow2_test_input, "", "Testing",
                      "Path of a text file whose first line names the pad buttons and stick values "
                      "to hold for guest user 0 (format in src/gow2_test_input.h). Read even when the "
                      "window has no focus; for automated tests. Empty = off");
REXCVAR_DEFINE_STRING(gow2_test_input_p2, "", "Testing",
                      "Like gow2_test_input, for guest user 1 (split-screen tests).");

namespace rex::glue {
namespace {

using test_input::PadState;

// The guest polls about 125 times a second from several threads; one read per frame is enough.
constexpr auto kRereadEvery = std::chrono::milliseconds(15);

struct PadFile {
  std::mutex lock;
  std::chrono::steady_clock::time_point next_read{};
  PadState pad;
  uint32_t packet_bump = 0;
  bool logged = false;
};

PadFile& TheFile() {
  static PadFile file;
  return file;
}
PadFile& TheFileP2() {
  static PadFile file;
  return file;
}

bool ReadFirstLine(const std::string& path, std::string* line) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  std::getline(in, *line);
  return true;
}

void Refresh(PadFile* file, const std::string& path, uint32_t user_index) {
  std::string line;
  const bool read = ReadFirstLine(path, &line);
  const PadState pad = read ? test_input::ParsePadLine(line) : PadState{};
  if (read && !file->logged) {
    file->logged = true;
    REXLOG_INFO("TESTINPUT pad state for user {} from {}", user_index, path);
  }
  if (!(pad == file->pad)) ++file->packet_bump;
  file->pad = pad;
}

void RefreshIfDue(PadFile* file, const std::string& path, uint32_t user_index) {
  const auto now = std::chrono::steady_clock::now();
  if (now < file->next_read) return;
  Refresh(file, path, user_index);
  file->next_read = now + kRereadEvery;
}

void ApplyForFile(uint8_t* base, uint32_t user_index, uint32_t state_addr, uint32_t* result,
                  const std::string& path, PadFile& file) {
  std::lock_guard<std::mutex> hold(file.lock);
  RefreshIfDue(&file, path, user_index);
  if (test_input::IsNeutral(file.pad)) return;
  test_input::OverlayPad(guest::HostPtr(base, state_addr), file.pad, file.packet_bump, result);
}

}  // namespace

bool test_input::CurrentPad(PadState* out) {
  const std::string path = REXCVAR_GET(gow2_test_input);
  if (path.empty()) return false;
  PadFile& file = TheFile();
  std::lock_guard<std::mutex> hold(file.lock);
  RefreshIfDue(&file, path, 0);
  if (IsNeutral(file.pad)) return false;
  *out = file.pad;
  return true;
}

void ApplyTestInput(uint8_t* base, uint32_t user_index, uint32_t state_addr, uint32_t* result) {
  if (HostUiOpen()) return;
  if (user_index == 0) {
    const std::string path = REXCVAR_GET(gow2_test_input);
    if (path.empty()) return;
    ApplyForFile(base, user_index, state_addr, result, path, TheFile());
  } else if (user_index == 1) {
    const std::string path = REXCVAR_GET(gow2_test_input_p2);
    if (path.empty()) return;
    ApplyForFile(base, user_index, state_addr, result, path, TheFileP2());
  }
}

}  // namespace rex::glue
