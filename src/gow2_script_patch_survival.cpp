// gow2 - ReXGlue Recompiled Project
//
// The Stage 2 group A (survival) rows of docs/COOP_STAGE2_SURVIVAL.md. Printed by
// `python -I tools/script_patch_survival.py --cpp`; edit the rows there, not here. Check them
// against your own decompressed packages with `python -I tools/script_patch_survival.py --check <folder>`.

#include "gow2_script_patch_survival.h"

namespace rex::glue::script_patch {

namespace {

// player pawns survive chapter travel: ResultPawn.bKillDuringLevelTransition = false
ScriptRow RowS2A_1() {
  return {
      .id = "S2A-1",
      .function = {"GearGame.GearGameSP_Base.SpawnDefaultPawnFor", "Function"},
      .script_size = 162,
      .checks = {
          {0x0061, {0x0F, 0x00}},
          {0x0067, {0x1C}},
          {0x008A, {0x07, 0x00}},
          {0x008D, {0x72, 0x00}},
          {0x0093, {0x2A, 0x16, 0x04, 0x00}},
          {0x009B, {0x04, 0x3A}},
          {0x00A1, {0x53}},
      },
      .operands = {
          {0x0063, {"GearGame.GearGameSP_Base.SpawnDefaultPawnFor.ResultPawn", "ObjectProperty"}},
          {0x0068, {"Engine.Actor.Spawn", "Function"}},
          {0x008F, {"GearGame.GearGameSP_Base.SpawnDefaultPawnFor.ResultPawn", "ObjectProperty"}},
          {0x0097, {"GearGame.GearGameSP_Base.SpawnDefaultPawnFor.ResultPawn", "ObjectProperty"}},
          {0x009D, {"GearGame.GearGameSP_Base.SpawnDefaultPawnFor.ReturnValue", "ObjectProperty"}},
      },
      .byte_edits = {
          {0x008C, 0x95, 0xA1},
      },
      .grow = GrowEdit{
          .keep = 0x00A1,
          .new_size = 183,
          .tail = {
              {.bytes = {0x14, 0x19, 0x00}},
              {.object = {"GearGame.GearGameSP_Base.SpawnDefaultPawnFor.ResultPawn", "ObjectProperty"}},
              {.bytes = {0x00, 0x06, 0x00, 0x04, 0x2D, 0x01}},
              {.object = {"Engine.Actor.bKillDuringLevelTransition", "BoolProperty"}},
              {.bytes = {0x28, 0x06, 0x00, 0x95, 0x53}},
          },
      },
  };
}

// game over: a dead player 3+ respawns near a teammate instead; Marcus and Dom keep the retail rule
ScriptRow RowS2A_2() {
  return {
      .id = "S2A-2",
      .function = {"GearGame.GearPC.ProcessGameOver", "Function"},
      .script_size = 64,
      .checks = {
          {0x0000, {0x1B}},
          {0x0009, {0x16, 0x2F, 0x19, 0x01}},
          {0x0011, {0x00, 0x27, 0x00, 0x00, 0x1C}},
          {0x001A, {0x20}},
          {0x001F, {0x00}},
          {0x0024, {0x16, 0x00, 0x3C, 0x19, 0x00}},
          {0x002D, {0x00, 0x0A, 0x00, 0x00, 0x1B}},
          {0x003A, {0x16, 0x31, 0x30, 0x04, 0x0B, 0x53}},
      },
      .operands = {
          {0x000D, {"Engine.Actor.WorldInfo", "ObjectProperty"}},
          {0x0016, {"Engine.WorldInfo.AllControllers", "Function"}},
          {0x001B, {"GearGame.GearPC", "Class"}},
          {0x0020, {"GearGame.GearPC.ProcessGameOver.PC", "ObjectProperty"}},
          {0x0029, {"GearGame.GearPC.ProcessGameOver.PC", "ObjectProperty"}},
      },
      .grow = GrowEdit{
          .keep = 0x0000,
          .new_size = 159,
          .tail = {
              {.bytes = {0x07, 0x00, 0x67, 0x82, 0x82, 0x77, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x2A, 0x16, 0x18, 0x00, 0x0D, 0x77, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearPawn_COGMarcus", "Class"}},
              {.bytes = {0x16, 0x16, 0x18, 0x00, 0x0D, 0x77, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearPawn_COGDom", "Class"}},
              {.bytes = {0x16, 0x16, 0x07, 0x00, 0x65, 0x82, 0x81, 0x2D, 0x01}},
              {.object = {"Engine.Actor.bPendingDelete", "BoolProperty"}},
              {.bytes = {0x16, 0x18, 0x00, 0x09, 0x72, 0x01}},
              {.object = {"Engine.Controller.Pawn", "ObjectProperty"}},
              {.bytes = {0x2A, 0x16, 0x16, 0x19, 0x2E}},
              {.object = {"GearGame.GearGameSP_Base", "Class"}},
              {.bytes = {0x19, 0x01}},
              {.object = {"Engine.Actor.WorldInfo", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"Engine.WorldInfo.Game", "ObjectProperty"}},
              {.bytes = {0x00, 0x07, 0x00, 0x00, 0x1C}},
              {.object = {"GearGame.GearGameSP_Base.RestartPlayer", "Function"}},
              {.bytes = {0x17, 0x16, 0x04, 0x0B, 0x1C}},
              {.object = {"GearGame.GearPC.PlayAnyaCoopDeathSound", "Function"}},
              {.bytes = {0x16, 0x2F, 0x19, 0x01}},
              {.object = {"Engine.Actor.WorldInfo", "ObjectProperty"}},
              {.bytes = {0x00, 0x23, 0x00, 0x00, 0x1C}},
              {.object = {"Engine.WorldInfo.AllControllers", "Function"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearPC", "Class"}},
              {.bytes = {0x00}},
              {.object = {"GearGame.GearPC.ProcessGameOver.PC", "ObjectProperty"}},
              {.bytes = {0x16, 0x00, 0x9B, 0x19, 0x00}},
              {.object = {"GearGame.GearPC.ProcessGameOver.PC", "ObjectProperty"}},
              {.bytes = {0x00, 0x06, 0x00, 0x00, 0x1C}},
              {.object = {"GearGame.GearPC.ClientGameOver", "Function"}},
              {.bytes = {0x16, 0x31, 0x30, 0x04, 0x0B, 0x53}},
          },
      },
  };
}

// a leaving player 3+, or a checkpoint dummy holding a player 3+'s record: UnPossess and Destroy his pawn; no AI Dom, no SeqEvent_CoopPlayerLeft
ScriptRow RowS2A_3() {
  return {
      .id = "S2A-3",
      .function = {"GearGame.GearPC.Destroyed", "Function"},
      .script_size = 1080,
      .checks = {
          {0x0063, {0x07}},
          {0x0066, {0x82, 0x81, 0x2D, 0x01}},
          {0x00A4, {0x1C}},
          {0x00A9, {0x16, 0x06, 0x04, 0x35, 0x07, 0x01, 0x16, 0x84, 0x19, 0x00}},
          {0x01B2, {0x0F, 0x00}},
          {0x01B8, {0x01}},
          {0x01BD, {0x1B}},
          {0x01C6, {0x16, 0x0F, 0x00}},
          {0x01CD, {0x1C}},
          {0x01D2, {0x20}},
          {0x0435, {0x04, 0x0B, 0x53}},
      },
      .operands = {
          {0x006A, {"GearGame.GearPC.bCheckpointDummy", "BoolProperty"}},
          {0x00A5, {"Engine.PlayerController.Destroyed", "Function"}},
          {0x00B3, {"GearGame.GearPC.Destroyed.Gear", "ObjectProperty"}},
          {0x00BC, {"GearGame.GearPawn.IsDoingSpecialMove", "Function"}},
          {0x01B4, {"GearGame.GearPC.Destroyed.OldPawn", "ObjectProperty"}},
          {0x01B9, {"Engine.Controller.Pawn", "ObjectProperty"}},
          {0x01C9, {"GearGame.GearPC.Destroyed.AI", "ObjectProperty"}},
          {0x01CE, {"Engine.Actor.Spawn", "Function"}},
          {0x01D3, {"GearGame.GearAI_Dom", "Class"}},
      },
      .byte_edits = {
          {0x0064, 0x00, 0x04},
          {0x0065, 0xAD, 0x37},
      },
      .grow = GrowEdit{
          .keep = 0x0437,
          .new_size = 1275,
          .tail = {
              {.bytes = {0x07, 0x00, 0xAD, 0x82, 0x82, 0x82, 0x82, 0x77, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x2A, 0x16, 0x18, 0x00, 0x0D, 0x77, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearPawn_COGMarcus", "Class"}},
              {.bytes = {0x16, 0x16, 0x18, 0x00, 0x0D, 0x77, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearPawn_COGDom", "Class"}},
              {.bytes = {0x16, 0x16, 0x18, 0x00, 0x17, 0x77, 0x19, 0x00}},
              {.object = {"GearGame.GearPC.Destroyed.Gear", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"Core.Object.Class", "ClassProperty"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearPawn_COGMarcus", "Class"}},
              {.bytes = {0x16, 0x16, 0x18, 0x00, 0x17, 0x77, 0x19, 0x00}},
              {.object = {"GearGame.GearPC.Destroyed.Gear", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"Core.Object.Class", "ClassProperty"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearPawn_COGDom", "Class"}},
              {.bytes = {0x16, 0x16, 0x07, 0x04, 0xF7, 0x77, 0x2E}},
              {.object = {"GearGame.GearPawn", "Class"}},
              {.bytes = {0x01}},
              {.object = {"Engine.Controller.Pawn", "ObjectProperty"}},
              {.bytes = {0x2A, 0x16, 0x07, 0x04, 0xD9, 0x84, 0x19, 0x00}},
              {.object = {"GearGame.GearPC.Destroyed.Gear", "ObjectProperty"}},
              {.bytes = {0x00, 0x08, 0x00, 0x04, 0x1C}},
              {.object = {"GearGame.GearPawn.IsDoingSpecialMove", "Function"}},
              {.bytes = {0x24, 0x2A, 0x16, 0x18, 0x00, 0x13, 0x19, 0x00}},
              {.object = {"GearGame.GearPC.Destroyed.Gear", "ObjectProperty"}},
              {.bytes = {0x00, 0x08, 0x00, 0x04, 0x1C}},
              {.object = {"GearGame.GearPawn.IsDoingSpecialMove", "Function"}},
              {.bytes = {0x24, 0x2B, 0x16, 0x16, 0x06, 0x00, 0xAD, 0x0F, 0x00}},
              {.object = {"GearGame.GearPC.Destroyed.OldPawn", "ObjectProperty"}},
              {.bytes = {0x01}},
              {.object = {"Engine.Controller.Pawn", "ObjectProperty"}},
              {.bytes = {0x1C}},
              {.object = {"GearGame.GearPC.UnPossess", "Function"}},
              {.bytes = {0x16, 0x19, 0x00}},
              {.object = {"GearGame.GearPC.Destroyed.OldPawn", "ObjectProperty"}},
              {.bytes = {0x00, 0x03, 0x00, 0x04, 0x61, 0x17, 0x16, 0x06, 0x00, 0xA4, 0x53}},
          },
      },
  };
}

// checkpoints: a dead player 3+ waiting to respawn does not block the save
ScriptRow RowS2A_5() {
  return {
      .id = "S2A-5",
      .function = {"GearGame.GearPC.SaveCheckpoint", "Function"},
      .script_size = 392,
      .checks = {
          {0x0065, {0x2F, 0x19, 0x01}},
          {0x006C, {0x00, 0x6B, 0x00, 0x00, 0x1C}},
          {0x0075, {0x20}},
          {0x007A, {0x00}},
          {0x007F, {0x16, 0x00, 0xDB, 0x07, 0x00, 0xDA, 0x84, 0x19, 0x00}},
          {0x00DA, {0x31, 0x30, 0x0F, 0x00}},
          {0x0185, {0x04, 0x0B, 0x53}},
      },
      .operands = {
          {0x0068, {"Engine.Actor.WorldInfo", "ObjectProperty"}},
          {0x0071, {"Engine.WorldInfo.AllControllers", "Function"}},
          {0x0076, {"GearGame.GearPC", "Class"}},
          {0x007B, {"GearGame.GearPC.SaveCheckpoint.PC", "ObjectProperty"}},
          {0x0088, {"GearGame.GearPC.SaveCheckpoint.PC", "ObjectProperty"}},
          {0x0091, {"Engine.Actor.IsTimerActive", "Function"}},
      },
      .byte_edits = {
          {0x00D7, 0x30, 0x06},
          {0x00D8, 0x04, 0x01},
          {0x00D9, 0x0B, 0x87},
      },
      .grow = GrowEdit{
          .keep = 0x0187,
          .new_size = 496,
          .tail = {
              {.bytes = {0x07, 0x01, 0xEC, 0x82, 0x82, 0x82, 0x77, 0x19, 0x00}},
              {.object = {"GearGame.GearPC.SaveCheckpoint.PC", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x2A, 0x16, 0x18, 0x00, 0x17, 0x77, 0x19, 0x00}},
              {.object = {"GearGame.GearPC.SaveCheckpoint.PC", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearPawn_COGMarcus", "Class"}},
              {.bytes = {0x16, 0x16, 0x18, 0x00, 0x17, 0x77, 0x19, 0x00}},
              {.object = {"GearGame.GearPC.SaveCheckpoint.PC", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearPawn_COGDom", "Class"}},
              {.bytes = {0x16, 0x16, 0x18, 0x00, 0x13, 0x72, 0x19, 0x00}},
              {.object = {"GearGame.GearPC.SaveCheckpoint.PC", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"Engine.Controller.Pawn", "ObjectProperty"}},
              {.bytes = {0x2A, 0x16, 0x16, 0x06, 0x00, 0xDA, 0x30, 0x04, 0x0B, 0x53}},
          },
      },
  };
}

// co-op splits: a standing player 3+ in the downed player's squad counts as a reviver
ScriptRow RowS2A_4() {
  return {
      .id = "S2A-4",
      .function = {"GearGame.GearGame.TeamHasAnyChanceOfSurvival", "Function"},
      .script_size = 664,
      .checks = {
          {0x0248, {0x0F, 0x00}},
          {0x024E, {0x2E}},
          {0x0253, {0x00}},
          {0x0258, {0x07, 0x02}},
          {0x025B, {0x82, 0x82, 0x77, 0x00}},
          {0x0263, {0x2A, 0x16, 0x18, 0x00, 0x09, 0x81, 0x2D, 0x01}},
          {0x026F, {0x16, 0x16, 0x18, 0x00, 0x09, 0x81, 0x2D, 0x01}},
          {0x027B, {0x16, 0x16, 0x14, 0x2D, 0x00}},
          {0x0284, {0x27, 0x06, 0x02, 0x89, 0x31, 0x30, 0x04, 0x2D, 0x00}},
          {0x0291, {0x04, 0x3A}},
          {0x0297, {0x53}},
      },
      .operands = {
          {0x024A, {"GearGame.GearGame.TeamHasAnyChanceOfSurvival.PC", "ObjectProperty"}},
          {0x024F, {"GearGame.GearPC", "Class"}},
          {0x0254, {"GearGame.GearGame.TeamHasAnyChanceOfSurvival.C", "ObjectProperty"}},
          {0x025F, {"GearGame.GearGame.TeamHasAnyChanceOfSurvival.PC", "ObjectProperty"}},
          {0x026B, {"GearGame.GearGame.bInCoopSplit", "BoolProperty"}},
          {0x0277, {"GearGame.GearGame.bInSoloSplit", "BoolProperty"}},
          {0x0280, {"GearGame.GearGame.TeamHasAnyChanceOfSurvival.bFoundReviver", "BoolProperty"}},
          {0x028D, {"GearGame.GearGame.TeamHasAnyChanceOfSurvival.bFoundReviver", "BoolProperty"}},
          {0x0293, {"GearGame.GearGame.TeamHasAnyChanceOfSurvival.ReturnValue", "BoolProperty"}},
      },
      .byte_edits = {
          {0x025A, 0x88, 0x97},
      },
      .grow = GrowEdit{
          .keep = 0x0297,
          .new_size = 854,
          .tail = {
              {.bytes = {0x07, 0x02, 0x88, 0x82, 0x82, 0x82, 0x82, 0x77, 0x00}},
              {.object = {"GearGame.GearGame.TeamHasAnyChanceOfSurvival.PC", "ObjectProperty"}},
              {.bytes = {0x2A, 0x16, 0x18, 0x00, 0x09, 0x81, 0x2D, 0x01}},
              {.object = {"GearGame.GearGame.bInSoloSplit", "BoolProperty"}},
              {.bytes = {0x16, 0x16, 0x18, 0x00, 0x07, 0x2D, 0x01}},
              {.object = {"GearGame.GearGame.bInCoopSplit", "BoolProperty"}},
              {.bytes = {0x16, 0x18, 0x00, 0x49, 0x82, 0x82, 0x77, 0x19, 0x00}},
              {.object = {"GearGame.GearGame.TeamHasAnyChanceOfSurvival.PC", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x2A, 0x16, 0x18, 0x00, 0x17, 0x77, 0x19, 0x00}},
              {.object = {"GearGame.GearGame.TeamHasAnyChanceOfSurvival.PC", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearPawn_COGMarcus", "Class"}},
              {.bytes = {0x16, 0x16, 0x18, 0x00, 0x17, 0x77, 0x19, 0x00}},
              {.object = {"GearGame.GearGame.TeamHasAnyChanceOfSurvival.PC", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearPawn_COGDom", "Class"}},
              {.bytes = {0x16, 0x16, 0x16, 0x18, 0x00, 0x3F, 0xFE, 0x19, 0x2E}},
              {.object = {"GearGame.GearPRI", "Class"}},
              {.bytes = {0x19, 0x00}},
              {.object = {"GearGame.GearGame.TeamHasAnyChanceOfSurvival.PC", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"Engine.Controller.PlayerReplicationInfo", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x08, 0x01}},
              {.object = {"GearGame.GearPRI.SquadName", "NameProperty"}},
              {.bytes = {0x19, 0x2E}},
              {.object = {"GearGame.GearPRI", "Class"}},
              {.bytes = {0x19, 0x00}},
              {.object = {"GearGame.GearGame.TeamHasAnyChanceOfSurvival.DBNOPawn", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"Engine.Pawn.PlayerReplicationInfo", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x08, 0x01}},
              {.object = {"GearGame.GearPRI.SquadName", "NameProperty"}},
              {.bytes = {0x16, 0x16, 0x14, 0x2D, 0x00}},
              {.object = {"GearGame.GearGame.TeamHasAnyChanceOfSurvival.bFoundReviver", "BoolProperty"}},
              {.bytes = {0x27, 0x06, 0x02, 0x89, 0x53}},
          },
      },
  };
}

}  // namespace

PatchGroup CoopSurvivalGroup(bool extra_players_end_game) {
  PatchGroup group{.name = "survival"};
  group.scripts.push_back(RowS2A_1());
  if (!extra_players_end_game) group.scripts.push_back(RowS2A_2());
  group.scripts.push_back(RowS2A_3());
  group.scripts.push_back(RowS2A_5());
  return group;
}

PatchGroup CoopSplitReviveGroup() {
  return {
      .name = "split-revive",
      .scripts = {RowS2A_4()},
  };
}

}  // namespace rex::glue::script_patch
