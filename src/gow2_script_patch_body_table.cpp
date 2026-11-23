// gow2 - ReXGlue Recompiled Project
//
// The Stage 2 follow-up rows of docs/COOP_N_PLAYERS.md (items 8, 9 and 10) for players 3+. Printed
// by `python -I tools/script_patch_body.py --cpp`; edit the rows there, not here. Check them against
// your own decompressed packages with `python -I tools/script_patch_body.py --check <folder>`.

#include "gow2_script_patch_body.h"

namespace rex::glue::script_patch {

namespace {

// Default__GearPawn_COGGear.GearPawnMesh.SkeletalMesh: None -> Dom's body
ObjectFieldRow RowCB_1() {
  return {
      .id = "CB-1",
      .object = {"GearGame.Default__GearPawn_COGGear.GearPawnMesh", "SkeletalMeshComponent"},
      .offset = 480,
      .value = {"COG_Dom.Game.COG_Dom_Game_CamSkel", "SkeletalMesh"},
      .anchors = {
          {{"GearGame.Default__GearPawn_COGDom.GearPawnMesh", "SkeletalMeshComponent"}, 480, {"COG_Dom.Game.COG_Dom_Game_CamSkel", "SkeletalMesh"}},
          {{"GearGame.Default__GearPawn_COGMarcus.GearPawnMesh", "SkeletalMeshComponent"}, 480, {"COG_MarcusFenix.COG_Marcus_Fenix_CamSkel", "SkeletalMesh"}},
      },
  };
}

// Default__GearPawn_COGGear.GearPawnMesh.PhysicsAsset: None -> Dom's physics asset
ObjectFieldRow RowCB_2() {
  return {
      .id = "CB-2",
      .object = {"GearGame.Default__GearPawn_COGGear.GearPawnMesh", "SkeletalMeshComponent"},
      .offset = 508,
      .value = {"COG_Dom.COG_Dom_CamSkel_Physics", "PhysicsAsset"},
      .anchors = {
          {{"GearGame.Default__GearPawn_COGDom.GearPawnMesh", "SkeletalMeshComponent"}, 508, {"COG_Dom.COG_Dom_CamSkel_Physics", "PhysicsAsset"}},
          {{"GearGame.Default__GearPawn_COGMarcus.GearPawnMesh", "SkeletalMeshComponent"}, 508, {"COG_MarcusFenix.COG_Marcus_Fenix_CamSkel_Physics", "PhysicsAsset"}},
      },
  };
}

// players 3+ are always GearPawn_COGGear: goto 0x0100, return class'GearPawn_COGGear'
ScriptRow RowCB_3() {
  return {
      .id = "CB-3",
      .function = {"GearGame.GearGameSP_Base.SuggestPawnClassFor", "Function"},
      .script_size = 318,
      .checks = {
          {0x0061, {0x04, 0x20}},
          {0x007E, {0x04, 0x20}},
          {0x0085, {0x01, 0x00, 0x77, 0x1C}},
          {0x008D, {0x1F}},
          {0x008E, {0x47, 0x65, 0x61, 0x72, 0x47, 0x61, 0x6D, 0x65, 0x43, 0x6F, 0x6E, 0x74, 0x65, 0x6E, 0x74, 0x2E, 0x47, 0x65, 0x61, 0x72, 0x50, 0x61, 0x77, 0x6E, 0x5F, 0x43, 0x4F, 0x47, 0x42, 0x65, 0x6E, 0x6A, 0x61, 0x6D, 0x69, 0x6E, 0x43, 0x61, 0x72, 0x6D, 0x69, 0x6E, 0x65, 0x00}},
          {0x00BA, {0x20}},
          {0x00BF, {0x16, 0x2A, 0x16, 0x04, 0x13}},
          {0x00C8, {0x1C}},
          {0x0100, {0x04}},
          {0x0106, {0x1C}},
          {0x010B, {0x1F}},
          {0x010C, {0x47, 0x65, 0x61, 0x72, 0x47, 0x61, 0x6D, 0x65, 0x43, 0x6F, 0x6E, 0x74, 0x65, 0x6E, 0x74, 0x2E, 0x47, 0x65, 0x61, 0x72, 0x50, 0x61, 0x77, 0x6E, 0x5F, 0x43, 0x4F, 0x47, 0x52, 0x65, 0x64, 0x53, 0x68, 0x69, 0x72, 0x74, 0x00}},
          {0x0131, {0x20}},
          {0x0136, {0x16, 0x04, 0x3A}},
          {0x013D, {0x53}},
      },
      .operands = {
          {0x0063, {"GearGame.GearPawn_COGMarcus", "Class"}},
          {0x0080, {"GearGame.GearPawn_COGDom", "Class"}},
          {0x0089, {"Core.Object.FindObject", "Function"}},
          {0x00BB, {"Core.Class", "Class"}},
          {0x00C4, {"GearGame.GearPawn", "Class"}},
          {0x00C9, {"Core.Object.FindObject", "Function"}},
          {0x0107, {"Core.Object.FindObject", "Function"}},
          {0x0132, {"Core.Class", "Class"}},
          {0x0139, {"GearGame.GearGameSP_Base.SuggestPawnClassFor.ReturnValue", "ClassProperty"}},
      },
      .byte_edits = {
          {0x0084, 0x07, 0x06},
          {0x0101, 0x13, 0x20},
      },
      .operand_edits = {
          {0x0102, {"GearGame.GearPawn", "Class"}, {"GearGame.GearPawn_COGGear", "Class"}},
      },
  };
}

// players 3+ are Ben Carmine where FindObject finds him, else GearPawn_COGGear
ScriptRow RowCB_3c() {
  return {
      .id = "CB-3c",
      .function = {"GearGame.GearGameSP_Base.SuggestPawnClassFor", "Function"},
      .script_size = 318,
      .checks = {
          {0x0061, {0x04, 0x20}},
          {0x007E, {0x04, 0x20}},
          {0x0084, {0x07}},
          {0x0085, {0x01, 0x00, 0x77, 0x1C}},
          {0x008D, {0x1F}},
          {0x008E, {0x47, 0x65, 0x61, 0x72, 0x47, 0x61, 0x6D, 0x65, 0x43, 0x6F, 0x6E, 0x74, 0x65, 0x6E, 0x74, 0x2E, 0x47, 0x65, 0x61, 0x72, 0x50, 0x61, 0x77, 0x6E, 0x5F, 0x43, 0x4F, 0x47, 0x42, 0x65, 0x6E, 0x6A, 0x61, 0x6D, 0x69, 0x6E, 0x43, 0x61, 0x72, 0x6D, 0x69, 0x6E, 0x65, 0x00}},
          {0x00BA, {0x20}},
          {0x00BF, {0x16, 0x2A, 0x16, 0x04, 0x13}},
          {0x00C8, {0x1C}},
          {0x0100, {0x04}},
          {0x0106, {0x1C}},
          {0x010B, {0x1F}},
          {0x010C, {0x47, 0x65, 0x61, 0x72, 0x47, 0x61, 0x6D, 0x65, 0x43, 0x6F, 0x6E, 0x74, 0x65, 0x6E, 0x74, 0x2E, 0x47, 0x65, 0x61, 0x72, 0x50, 0x61, 0x77, 0x6E, 0x5F, 0x43, 0x4F, 0x47, 0x52, 0x65, 0x64, 0x53, 0x68, 0x69, 0x72, 0x74, 0x00}},
          {0x0131, {0x20}},
          {0x0136, {0x16, 0x04, 0x3A}},
          {0x013D, {0x53}},
      },
      .operands = {
          {0x0063, {"GearGame.GearPawn_COGMarcus", "Class"}},
          {0x0080, {"GearGame.GearPawn_COGDom", "Class"}},
          {0x0089, {"Core.Object.FindObject", "Function"}},
          {0x00BB, {"Core.Class", "Class"}},
          {0x00C4, {"GearGame.GearPawn", "Class"}},
          {0x00C9, {"Core.Object.FindObject", "Function"}},
          {0x0107, {"Core.Object.FindObject", "Function"}},
          {0x0132, {"Core.Class", "Class"}},
          {0x0139, {"GearGame.GearGameSP_Base.SuggestPawnClassFor.ReturnValue", "ClassProperty"}},
      },
      .byte_edits = {
          {0x0101, 0x13, 0x20},
      },
      .operand_edits = {
          {0x0102, {"GearGame.GearPawn", "Class"}, {"GearGame.GearPawn_COGGear", "Class"}},
      },
  };
}

// after a player 3+ spawns (a GearPC whose DefaultPawnClass is not None, Marcus or Dom): give GearWeap_COGPistol and GearWeap_Shotgun when missing
ScriptRow RowCB_4() {
  return {
      .id = "CB-4",
      .function = {"GearGame.GearGame.RestartPlayer", "Function"},
      .script_size = 387,
      .checks = {
          {0x0000, {0x1C}},
          {0x0005, {0x00}},
          {0x000A, {0x16}},
          {0x000B, {0x07, 0x01, 0x80, 0x77, 0x19, 0x00}},
          {0x0015, {0x00, 0x05, 0x00, 0x04, 0x01}},
          {0x001E, {0x2A, 0x16}},
          {0x00EC, {0x07, 0x01, 0x80}},
          {0x017E, {0x31, 0x30, 0x04, 0x0B, 0x53}},
      },
      .operands = {
          {0x0001, {"Engine.GameInfo.RestartPlayer", "Function"}},
          {0x0006, {"GearGame.GearGame.RestartPlayer.NewPlayer", "ObjectProperty"}},
          {0x0011, {"GearGame.GearGame.RestartPlayer.NewPlayer", "ObjectProperty"}},
          {0x001A, {"Engine.Controller.Pawn", "ObjectProperty"}},
      },
      .grow = GrowEdit{
          .keep = 0x0180,
          .new_size = 658,
          .tail = {
              {.bytes = {0x07, 0x02, 0x8F, 0x82, 0x82, 0x77, 0x2E}},
              {.object = {"GearGame.GearPC", "Class"}},
              {.bytes = {0x00}},
              {.object = {"GearGame.GearGame.RestartPlayer.NewPlayer", "ObjectProperty"}},
              {.bytes = {0x2A, 0x16, 0x18, 0x00, 0x58, 0x82, 0x82, 0x77, 0x19, 0x2E}},
              {.object = {"GearGame.GearPC", "Class"}},
              {.bytes = {0x00}},
              {.object = {"GearGame.GearGame.RestartPlayer.NewPlayer", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x2A, 0x16, 0x18, 0x00, 0x1C, 0x77, 0x19, 0x2E}},
              {.object = {"GearGame.GearPC", "Class"}},
              {.bytes = {0x00}},
              {.object = {"GearGame.GearGame.RestartPlayer.NewPlayer", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearPawn_COGMarcus", "Class"}},
              {.bytes = {0x16, 0x16, 0x18, 0x00, 0x1C, 0x77, 0x19, 0x2E}},
              {.object = {"GearGame.GearPC", "Class"}},
              {.bytes = {0x00}},
              {.object = {"GearGame.GearGame.RestartPlayer.NewPlayer", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearPawn_COGDom", "Class"}},
              {.bytes = {0x16, 0x16, 0x16, 0x18, 0x00, 0x13, 0x77, 0x19, 0x00}},
              {.object = {"GearGame.GearGame.RestartPlayer.NewPlayer", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"Engine.Controller.Pawn", "ObjectProperty"}},
              {.bytes = {0x2A, 0x16, 0x16, 0x07, 0x02, 0x49, 0x72, 0x19, 0x19, 0x00}},
              {.object = {"GearGame.GearGame.RestartPlayer.NewPlayer", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"Engine.Controller.Pawn", "ObjectProperty"}},
              {.bytes = {0x00, 0x0C, 0x00, 0x04, 0x1C}},
              {.object = {"Engine.Pawn.FindInventoryType", "Function"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearWeap_COGPistol", "Class"}},
              {.bytes = {0x4A, 0x16, 0x2A, 0x16, 0x19, 0x19, 0x00}},
              {.object = {"GearGame.GearGame.RestartPlayer.NewPlayer", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"Engine.Controller.Pawn", "ObjectProperty"}},
              {.bytes = {0x00, 0x0C, 0x00, 0x04, 0x1C}},
              {.object = {"Engine.Pawn.CreateInventory", "Function"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearWeap_COGPistol", "Class"}},
              {.bytes = {0x27, 0x16, 0x07, 0x02, 0x8F, 0x72, 0x19, 0x19, 0x00}},
              {.object = {"GearGame.GearGame.RestartPlayer.NewPlayer", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"Engine.Controller.Pawn", "ObjectProperty"}},
              {.bytes = {0x00, 0x0C, 0x00, 0x04, 0x1C}},
              {.object = {"Engine.Pawn.FindInventoryType", "Function"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearWeap_Shotgun", "Class"}},
              {.bytes = {0x4A, 0x16, 0x2A, 0x16, 0x19, 0x19, 0x00}},
              {.object = {"GearGame.GearGame.RestartPlayer.NewPlayer", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"Engine.Controller.Pawn", "ObjectProperty"}},
              {.bytes = {0x00, 0x0C, 0x00, 0x04, 0x1C}},
              {.object = {"Engine.Pawn.CreateInventory", "Function"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearWeap_Shotgun", "Class"}},
              {.bytes = {0x27, 0x16, 0x04, 0x0B, 0x53}},
          },
      },
  };
}

// a joiner who is not Dom spawns at once: LocalPlayer(PC.Player) != None -> class<GearPawn_COGDom>(PC.DefaultPawnClass) == None
ScriptRow RowCB_5() {
  return {
      .id = "CB-5",
      .function = {"GearGame.GearGameSP_Base.PostLogin", "Function"},
      .script_size = 1690,
      .checks = {
          {0x03A4, {0x07, 0x03, 0xDD, 0x84, 0x81, 0x2D, 0x00}},
          {0x03AF, {0x16, 0x18, 0x00, 0x18}},
          {0x03B9, {0x19, 0x00}},
          {0x03BF, {0x00, 0x05, 0x00, 0x04, 0x01}},
          {0x03C8, {0x2A, 0x16, 0x16, 0x1B}},
          {0x03DD, {0x07, 0x04, 0x0D, 0x19, 0x00}},
      },
      .operands = {
          {0x03AB, {"GearGame.GearGameSP_Base.PostLogin.bFoundPawn", "BoolProperty"}},
          {0x03BB, {"GearGame.GearGameSP_Base.PostLogin.PC", "ObjectProperty"}},
          {0x03E2, {"GearGame.GearGameSP_Base.PostLogin.PC", "ObjectProperty"}},
      },
      .byte_edits = {
          {0x03B3, 0x77, 0x72},
          {0x03B4, 0x2E, 0x13},
      },
      .operand_edits = {
          {0x03B5, {"Engine.LocalPlayer", "Class"}, {"GearGame.GearPawn_COGDom", "Class"}},
          {0x03C4, {"Engine.PlayerController.Player", "ObjectProperty"}, {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
      },
  };
}

// a hero enters a GearVehicle: god mode for every player 3+ on foot
ScriptRow RowCR_1() {
  return {
      .id = "CR-1",
      .function = {"GearGame.GearPawn.OnGearEnterVehicle", "Function"},
      .script_size = 169,
      .checks = {
          {0x0000, {0x0F, 0x00}},
          {0x0006, {0x01}},
          {0x000B, {0x07, 0x00, 0x5B, 0x77, 0x2E}},
          {0x0014, {0x19, 0x00}},
          {0x001A, {0x00, 0x05, 0x00, 0x04, 0x01}},
          {0x0025, {0x14, 0x2D, 0x00}},
          {0x0066, {0x07, 0x00, 0x87, 0x2D, 0x00}},
          {0x0084, {0x06, 0x00, 0xA6}},
          {0x00A6, {0x04, 0x0B, 0x53}},
      },
      .operands = {
          {0x0002, {"GearGame.GearPawn.OnGearEnterVehicle.AI", "ObjectProperty"}},
          {0x0007, {"GearGame.GearPawn.MyGearAI", "ObjectProperty"}},
          {0x0010, {"Engine.Vehicle", "Class"}},
          {0x0016, {"GearGame.GearPawn.OnGearEnterVehicle.inAction", "ObjectProperty"}},
          {0x001F, {"GearGame.SeqAct_GearEnterVehicle.TargetVehicle", "ObjectProperty"}},
          {0x0028, {"GearGame.GearPawn.OnGearEnterVehicle.bEntered", "BoolProperty"}},
          {0x006B, {"GearGame.GearPawn.OnGearEnterVehicle.bEntered", "BoolProperty"}},
      },
      .grow = GrowEdit{
          .keep = 0x00A6,
          .new_size = 369,
          .tail = {
              {.bytes = {0x07, 0x01, 0x6E, 0x2D, 0x00}},
              {.object = {"GearGame.GearPawn.OnGearEnterVehicle.bEntered", "BoolProperty"}},
              {.bytes = {0x07, 0x01, 0x6E, 0x77, 0x2E}},
              {.object = {"GearGame.GearVehicle", "Class"}},
              {.bytes = {0x19, 0x00}},
              {.object = {"GearGame.GearPawn.OnGearEnterVehicle.inAction", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"GearGame.SeqAct_GearEnterVehicle.TargetVehicle", "ObjectProperty"}},
              {.bytes = {0x2A, 0x16, 0x07, 0x01, 0x6E, 0x84, 0x77, 0x13}},
              {.object = {"GearGame.GearPawn_COGMarcus", "Class"}},
              {.bytes = {0x01}},
              {.object = {"Core.Object.Class", "ClassProperty"}},
              {.bytes = {0x2A, 0x16, 0x18, 0x00, 0x0E, 0x77, 0x13}},
              {.object = {"GearGame.GearPawn_COGDom", "Class"}},
              {.bytes = {0x01}},
              {.object = {"Core.Object.Class", "ClassProperty"}},
              {.bytes = {0x2A, 0x16, 0x16, 0x2F, 0x19, 0x01}},
              {.object = {"Engine.Actor.WorldInfo", "ObjectProperty"}},
              {.bytes = {0x00, 0x77, 0x00, 0x00, 0x1C}},
              {.object = {"Engine.WorldInfo.AllControllers", "Function"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearPC", "Class"}},
              {.bytes = {0x00}},
              {.object = {"GearGame.GearPawn.OnGearEnterVehicle.AI", "ObjectProperty"}},
              {.bytes = {0x16, 0x01, 0x6D, 0x07, 0x01, 0x6C, 0x82, 0x82, 0x72, 0x13}},
              {.object = {"GearGame.GearPawn_COGMarcus", "Class"}},
              {.bytes = {0x19, 0x00}},
              {.object = {"GearGame.GearPawn.OnGearEnterVehicle.AI", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x2A, 0x16, 0x18, 0x00, 0x18, 0x72, 0x13}},
              {.object = {"GearGame.GearPawn_COGDom", "Class"}},
              {.bytes = {0x19, 0x00}},
              {.object = {"GearGame.GearPawn.OnGearEnterVehicle.AI", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x2A, 0x16, 0x16, 0x18, 0x00, 0x18, 0x77, 0x2E}},
              {.object = {"GearGame.GearPawn", "Class"}},
              {.bytes = {0x19, 0x00}},
              {.object = {"GearGame.GearPawn.OnGearEnterVehicle.AI", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"Engine.Controller.Pawn", "ObjectProperty"}},
              {.bytes = {0x2A, 0x16, 0x16, 0x14, 0x19, 0x00}},
              {.object = {"GearGame.GearPawn.OnGearEnterVehicle.AI", "ObjectProperty"}},
              {.bytes = {0x00, 0x06, 0x00, 0x04, 0x2D, 0x01}},
              {.object = {"Engine.Controller.bGodMode", "BoolProperty"}},
              {.bytes = {0x27, 0x31, 0x30, 0x04, 0x0B, 0x53}},
          },
      },
  };
}

// a hero leaves a GearVehicle: players 3+ in god mode lose it and are placed next to him
ScriptRow RowCR_2() {
  return {
      .id = "CR-2",
      .function = {"GearGame.GearPawn.OnGearExitVehicle", "Function"},
      .script_size = 216,
      .checks = {
          {0x0000, {0x07, 0x00, 0xD5, 0x77, 0x19, 0x00}},
          {0x000A, {0x00, 0x05, 0x00, 0x04, 0x01}},
          {0x0013, {0x2A, 0x16}},
          {0x006D, {0x0F, 0x00}},
          {0x0073, {0x2E}},
          {0x0078, {0x19, 0x00}},
          {0x0087, {0x07, 0x00, 0xD5, 0x77, 0x00}},
          {0x0090, {0x2A, 0x16}},
          {0x00BC, {0x19, 0x00}},
          {0x00D5, {0x04, 0x0B, 0x53}},
      },
      .operands = {
          {0x0006, {"GearGame.GearPawn.OnGearExitVehicle.inAction", "ObjectProperty"}},
          {0x000F, {"GearGame.SeqAct_GearExitVehicle.TargetVehicle", "ObjectProperty"}},
          {0x006F, {"GearGame.GearPawn.OnGearExitVehicle.GV", "ObjectProperty"}},
          {0x0074, {"GearGame.GearVehicle", "Class"}},
          {0x007A, {"GearGame.GearPawn.OnGearExitVehicle.inAction", "ObjectProperty"}},
          {0x008C, {"GearGame.GearPawn.OnGearExitVehicle.GV", "ObjectProperty"}},
          {0x00BE, {"GearGame.GearPawn.OnGearExitVehicle.GV", "ObjectProperty"}},
      },
      .grow = GrowEdit{
          .keep = 0x00D5,
          .new_size = 451,
          .tail = {
              {.bytes = {0x07, 0x01, 0xC0, 0x77, 0x00}},
              {.object = {"GearGame.GearPawn.OnGearExitVehicle.GV", "ObjectProperty"}},
              {.bytes = {0x2A, 0x16, 0x07, 0x01, 0xC0, 0x84, 0x77, 0x13}},
              {.object = {"GearGame.GearPawn_COGMarcus", "Class"}},
              {.bytes = {0x01}},
              {.object = {"Core.Object.Class", "ClassProperty"}},
              {.bytes = {0x2A, 0x16, 0x18, 0x00, 0x0E, 0x77, 0x13}},
              {.object = {"GearGame.GearPawn_COGDom", "Class"}},
              {.bytes = {0x01}},
              {.object = {"Core.Object.Class", "ClassProperty"}},
              {.bytes = {0x2A, 0x16, 0x16, 0x2F, 0x19, 0x01}},
              {.object = {"Engine.Actor.WorldInfo", "ObjectProperty"}},
              {.bytes = {0x00, 0xB2, 0x00, 0x00, 0x1C}},
              {.object = {"Engine.WorldInfo.AllControllers", "Function"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearPC", "Class"}},
              {.bytes = {0x00}},
              {.object = {"GearGame.GearPawn.OnGearExitVehicle.GT", "ObjectProperty"}},
              {.bytes = {0x16, 0x01, 0xBF, 0x07, 0x01, 0xBE, 0x82, 0x82, 0x82, 0x19, 0x00}},
              {.object = {"GearGame.GearPawn.OnGearExitVehicle.GT", "ObjectProperty"}},
              {.bytes = {0x00, 0x06, 0x00, 0x04, 0x2D, 0x01}},
              {.object = {"Engine.Controller.bGodMode", "BoolProperty"}},
              {.bytes = {0x18, 0x00, 0x18, 0x72, 0x13}},
              {.object = {"GearGame.GearPawn_COGMarcus", "Class"}},
              {.bytes = {0x19, 0x00}},
              {.object = {"GearGame.GearPawn.OnGearExitVehicle.GT", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x2A, 0x16, 0x16, 0x18, 0x00, 0x18, 0x72, 0x13}},
              {.object = {"GearGame.GearPawn_COGDom", "Class"}},
              {.bytes = {0x19, 0x00}},
              {.object = {"GearGame.GearPawn.OnGearExitVehicle.GT", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x2A, 0x16, 0x16, 0x18, 0x00, 0x18, 0x77, 0x2E}},
              {.object = {"GearGame.GearPawn", "Class"}},
              {.bytes = {0x19, 0x00}},
              {.object = {"GearGame.GearPawn.OnGearExitVehicle.GT", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"Engine.Controller.Pawn", "ObjectProperty"}},
              {.bytes = {0x2A, 0x16, 0x16, 0x14, 0x19, 0x00}},
              {.object = {"GearGame.GearPawn.OnGearExitVehicle.GT", "ObjectProperty"}},
              {.bytes = {0x00, 0x06, 0x00, 0x04, 0x2D, 0x01}},
              {.object = {"Engine.Controller.bGodMode", "BoolProperty"}},
              {.bytes = {0x28, 0x19, 0x2E}},
              {.object = {"GearGame.GearPC", "Class"}},
              {.bytes = {0x01}},
              {.object = {"Engine.Pawn.Controller", "ObjectProperty"}},
              {.bytes = {0x00, 0x17, 0x00, 0x04, 0x1C}},
              {.object = {"GearGame.GearPC.FindAvailableTeleportSpot", "Function"}},
              {.bytes = {0x19, 0x00}},
              {.object = {"GearGame.GearPawn.OnGearExitVehicle.GT", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"Engine.Controller.Pawn", "ObjectProperty"}},
              {.bytes = {0x4A, 0x4A, 0x16, 0x31, 0x30, 0x04, 0x0B, 0x53}},
          },
      },
  };
}

// a joiner copies Kismet's cinematic and god-mode flags from Marcus or Dom, never from a player 3+
ScriptRow RowCR_3() {
  return {
      .id = "CR-3",
      .function = {"GearGame.GearGameSP_Base.SyncKismetSetProperties", "Function"},
      .script_size = 460,
      .checks = {
          {0x0000, {0x2F, 0x19, 0x01}},
          {0x0007, {0x01, 0xBD, 0x00, 0x00, 0x1C}},
          {0x0010, {0x20}},
          {0x0015, {0x00}},
          {0x001A, {0x16, 0x01, 0xC8}},
          {0x001E, {0x01}},
          {0x0020, {0x77, 0x00}},
          {0x0026, {0x00}},
          {0x002B, {0x16, 0x14}},
          {0x01C4, {0x06, 0x01, 0xC8, 0x31, 0x30, 0x04, 0x0B, 0x53}},
      },
      .operands = {
          {0x0003, {"Engine.Actor.WorldInfo", "ObjectProperty"}},
          {0x000C, {"Engine.WorldInfo.AllControllers", "Function"}},
          {0x0011, {"GearGame.GearPC", "Class"}},
          {0x0016, {"GearGame.GearGameSP_Base.SyncKismetSetProperties.OtherPC", "ObjectProperty"}},
          {0x0022, {"GearGame.GearGameSP_Base.SyncKismetSetProperties.OtherPC", "ObjectProperty"}},
          {0x0027, {"GearGame.GearGameSP_Base.SyncKismetSetProperties.PC", "ObjectProperty"}},
      },
      .byte_edits = {
          {0x001D, 0x07, 0x06},
          {0x001F, 0xC7, 0xCB},
      },
      .grow = GrowEdit{
          .keep = 0x01CB,
          .new_size = 532,
          .tail = {
              {.bytes = {0x07, 0x01, 0xC7, 0x82, 0x77, 0x00}},
              {.object = {"GearGame.GearGameSP_Base.SyncKismetSetProperties.OtherPC", "ObjectProperty"}},
              {.bytes = {0x00}},
              {.object = {"GearGame.GearGameSP_Base.SyncKismetSetProperties.PC", "ObjectProperty"}},
              {.bytes = {0x16, 0x18, 0x00, 0x32, 0x84, 0x72, 0x19, 0x00}},
              {.object = {"GearGame.GearGameSP_Base.SyncKismetSetProperties.OtherPC", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearPawn_COGMarcus", "Class"}},
              {.bytes = {0x16, 0x18, 0x00, 0x17, 0x72, 0x19, 0x00}},
              {.object = {"GearGame.GearGameSP_Base.SyncKismetSetProperties.OtherPC", "ObjectProperty"}},
              {.bytes = {0x00, 0x05, 0x00, 0x04, 0x01}},
              {.object = {"GearGame.GearPC.DefaultPawnClass", "ClassProperty"}},
              {.bytes = {0x20}},
              {.object = {"GearGame.GearPawn_COGDom", "Class"}},
              {.bytes = {0x16, 0x16, 0x16, 0x06, 0x00, 0x2C, 0x53}},
          },
      },
  };
}

}  // namespace

PatchGroup CoopClientGroup() {
  return {.name = "coop-client", .object_fields = {RowCB_1(), RowCB_2()}};
}

PatchGroup CoopBodyGroup(bool carmine, bool marcus_clones) {
  PatchGroup group{.name = "coop-body"};
  if (!marcus_clones) group.scripts.push_back(carmine ? RowCB_3c() : RowCB_3());
  group.scripts.push_back(RowCB_4());
  group.scripts.push_back(RowCB_5());
  return group;
}

PatchGroup CoopRideGroup() {
  return {
      .name = "coop-ride",
      .scripts = {RowCR_1(), RowCR_2(), RowCR_3()},
  };
}

}  // namespace rex::glue::script_patch
