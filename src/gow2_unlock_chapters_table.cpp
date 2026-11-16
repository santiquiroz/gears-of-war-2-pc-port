// gow2 - ReXGlue Recompiled Project
//
// The unlockchapters rows: every campaign act and chapter can be picked in the
// chapter-select and co-op lobby menus, and Insane can be picked without
// finishing the campaign. Printed by
// `python -I tools/unlock_chapters_patch.py --cpp`; edit the rows there, not here. Check them
// against your own decompressed packages with `python -I tools/unlock_chapters_patch.py --check <folder>`.

#include "gow2_unlock_chapters.h"

namespace rex::glue::script_patch {

namespace {

// HasActBeenUnlockedForAccess -> return true (act list: every act can be picked)
ScriptRow RowUNL_1() {
  return {
      .id = "UNL-1",
      .function = {"GearGame.GearProfileSettings.HasActBeenUnlockedForAccess", "Function"},
      .script_size = 98,
      .checks = {
          {0x0006, {0x00, 0x14, 0x00, 0x04, 0x1B}},
          {0x001E, {0x0F, 0x00}},
          {0x0035, {0x07, 0x00, 0x4F, 0x1B}},
          {0x0056, {0x06, 0x00, 0x25}},
          {0x0061, {0x53}},
      },
      .operands = {
          {0x0002, {"GearGame.GearUIDataStore_GameResource", "Class"}},
          {0x0030, {"GearGame.GearProfileSettings.HasActBeenUnlockedForAccess.ChapterList", "ArrayProperty"}},
          {0x005D, {"GearGame.GearProfileSettings.HasActBeenUnlockedForAccess.ReturnValue", "BoolProperty"}},
      },
      .byte_edits = {
          {0x0000, 0x12, 0x04},
          {0x0001, 0x20, 0x27},
      },
  };
}

// CanAccessChapter -> return true (every chapter can be picked)
ScriptRow RowUNL_2() {
  return {
      .id = "UNL-2",
      .function = {"GearGame.GearUISceneFE_SelectChapter.CanAccessChapter", "Function"},
      .script_size = 145,
      .checks = {
          {0x000C, {0x0F, 0x00}},
          {0x0023, {0x19, 0x10, 0x00}},
          {0x0042, {0x07, 0x00, 0x88, 0x82, 0x77}},
          {0x0064, {0x04, 0x19, 0x19, 0x00}},
          {0x0090, {0x53}},
      },
      .operands = {
          {0x0007, {"Engine.UIScreenObject.GetBestPlayerIndex", "Function"}},
          {0x0035, {"GearGame.GearCampaignActData.ChapterProviders", "ArrayProperty"}},
          {0x008C, {"GearGame.GearUISceneFE_SelectChapter.CanAccessChapter.ReturnValue", "BoolProperty"}},
      },
      .byte_edits = {
          {0x0000, 0x0F, 0x04},
          {0x0001, 0x00, 0x27},
      },
  };
}

// ShouldDisableElementChapter -> return false (no chapter is greyed out in the co-op lobby)
ScriptRow RowUNL_3() {
  return {
      .id = "UNL-3",
      .function = {"GearGame.GearUISceneFELobby_Campaign.ShouldDisableElementChapter", "Function"},
      .script_size = 297,
      .checks = {
          {0x0002, {0x20, 0x2D, 0x01}},
          {0x0035, {0x07, 0x01, 0x20, 0x77, 0x00}},
          {0x0071, {0x07, 0x01, 0x20, 0x96, 0x00}},
          {0x00CE, {0x07, 0x01, 0x20, 0x82, 0x77}},
          {0x00F0, {0x04, 0x81, 0x19, 0x19, 0x00}},
          {0x0128, {0x53}},
      },
      .operands = {
          {0x0005, {"GearGame.GearUISceneFELobby_Campaign.bConstrainChapterToProfile", "BoolProperty"}},
          {0x00B8, {"Engine.UIScreenObject.GetBestPlayerIndex", "Function"}},
          {0x0124, {"GearGame.GearUISceneFELobby_Campaign.ShouldDisableElementChapter.ReturnValue", "BoolProperty"}},
      },
      .byte_edits = {
          {0x0000, 0x07, 0x04},
          {0x0001, 0x01, 0x28},
      },
  };
}

// InitializeWidgetReferences -> skip the Insane lock (Insane can be picked without finishing the campaign)
ScriptRow RowUNL_4() {
  return {
      .id = "UNL-4",
      .function = {"GearGame.GearUISceneFE_Difficulty.InitializeWidgetReferences", "Function"},
      .script_size = 889,
      .checks = {
          {0x009F, {0x06, 0x00, 0x1B}},
          {0x00B5, {0x01, 0x27, 0x82, 0x82, 0x77}},
          {0x00EF, {0x1B}},
          {0x00F8, {0x24, 0x00, 0x16, 0x16, 0x16, 0x0F}},
          {0x0127, {0x19}},
      },
      .operands = {
          {0x00AE, {"Engine.UIScreenObject.GetBestPlayerIndex", "Function"}},
          {0x00BB, {"GearGame.GearUISceneFE_Difficulty.InitializeWidgetReferences.PC", "ObjectProperty"}},
      },
      .byte_edits = {
          {0x00B4, 0x07, 0x06},
      },
  };
}

}  // namespace

PatchGroup UnlockChaptersGroup() {
  return {
      .name = "unlockchapters",
      .scripts = {RowUNL_1(), RowUNL_2(), RowUNL_3(), RowUNL_4()},
      .defaults = {},
  };
}

}  // namespace rex::glue::script_patch
