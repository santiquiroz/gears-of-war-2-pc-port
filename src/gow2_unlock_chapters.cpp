// gow2 - ReXGlue Recompiled Project
//
// Campaign chapters always unlocked, applied once at startup: the chapter-select menu checks
// pass without touching the saved profile.
#include "gow2_unlock_chapters.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <cstdint>
#include <string>

REXCVAR_DEFINE_BOOL(gow2_unlock_all_chapters, true, "Campaign",
                    "Every campaign act and chapter can be picked in Campaign > Chapter select and in the co-op lobby. Only the menu checks change; the saved profile is not touched");

namespace rex::glue {
namespace {

using script_patch::GroupResult;
using script_patch::PatchGroup;

constexpr uint32_t kGNames = 0x83136834u;
constexpr uint32_t kGObjObjects = 0x83136860u;

struct ApplyCall {
  uint8_t* base;
  const PatchGroup* group;
  const script_patch::GuestAlloc* alloc;
  GroupResult* result;
};

void RunApply(ApplyCall* call) {
  *call->result = script_patch::ApplyGroup(call->base, {kGNames, kGObjObjects}, *call->group,
                                           0, *call->alloc);
}

bool GuardedApply(ApplyCall* call) {
#ifdef _WIN32
  __try {
    RunApply(call);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
#else
  RunApply(call);
  return true;
#endif
}

void LogResult(const GroupResult& result) {
  if (!result.applied) {
    REXLOG_WARN("SCRIPTPATCH unlockchapters: skipped ({}: {}); chapters stay locked",
                result.failed_row, result.reason);
    return;
  }
  for (const std::string& line : result.lines) REXLOG_INFO("SCRIPTPATCH unlockchapters {}", line);
  REXLOG_INFO("SCRIPTPATCH unlockchapters: {} rows applied", result.lines.size());
}

}  // namespace

void ApplyUnlockChaptersPatch(PPCContext&, uint8_t* base) {
  static bool done = false;
  if (done) return;
  done = true;
  if (!REXCVAR_GET(gow2_unlock_all_chapters)) {
    REXLOG_INFO("SCRIPTPATCH unlockchapters: off (gow2_unlock_all_chapters=false)");
    return;
  }
  PatchGroup group = script_patch::UnlockChaptersGroup();
  // the rows never grow
  const script_patch::GuestAlloc alloc = [](uint32_t) { return 0u; };
  GroupResult result;
  ApplyCall call{base, &group, &alloc, &result};
  if (!GuardedApply(&call)) {
    REXLOG_WARN("SCRIPTPATCH unlockchapters: skipped (fault while reading guest objects); chapters stay locked");
    return;
  }
  LogResult(result);
}

}  // namespace rex::glue
