// gow2 - ReXGlue Recompiled Project
//
// The co-op request channel and friendly-fire rows of docs/COOP_N_PLAYERS.md. Printed by
// `python -I tools/script_patch_requests.py --cpp`; edit the rows there, not here. Check them
// against your own decompressed packages with `python -I tools/script_patch_requests.py --check <folder>`.

#include "gow2_script_patch_requests.h"

namespace rex::glue::script_patch {

namespace {

// ServerMutate on the server stores its string in LastBroadcastString[0] for the port to read: goto 0x0042, LastBroadcastString[0] = MutateString
ScriptRow RowRQ_1() {
  return {
      .id = "RQ-1",
      .function = {"Engine.PlayerController.ServerMutate", "Function"},
      .script_size = 67,
      .checks = {
          {0x0000, {0x07, 0x00}},
          {0x0003, {0x9A, 0x38, 0x3A, 0x19, 0x01}},
          {0x000C, {0x00, 0x05, 0x00, 0x01, 0x01}},
          {0x0015, {0x38, 0x3A, 0x24, 0x03, 0x16}},
          {0x001A, {0x04, 0x0B}},
          {0x001C, {0x19, 0x19, 0x01}},
          {0x0023, {0x00, 0x05, 0x00, 0x04, 0x01}},
          {0x002C, {0x00, 0x10, 0x00, 0x00, 0x1B}},
          {0x0039, {0x00}},
          {0x003E, {0x17, 0x16, 0x04, 0x0B}},
          {0x0042, {0x53}},
      },
      .operands = {
          {0x0008, {"Engine.Actor.WorldInfo", "ObjectProperty"}},
          {0x0011, {"Engine.WorldInfo.NetMode", "ByteProperty"}},
          {0x001F, {"Engine.Actor.WorldInfo", "ObjectProperty"}},
          {0x0028, {"Engine.WorldInfo.Game", "ObjectProperty"}},
          {0x003A, {"Engine.PlayerController.ServerMutate.MutateString", "StrProperty"}},
      },
      .byte_edits = {
          {0x0002, 0x1C, 0x42},
      },
      .grow = GrowEdit{
          .keep = 0x0042,
          .new_size = 82,
          .tail = {
              {.bytes = {0x0F, 0x1A, 0x25, 0x01}},
              {.object = {"Engine.PlayerController.LastBroadcastString", "StrProperty"}},
              {.bytes = {0x00}},
              {.object = {"Engine.PlayerController.ServerMutate.MutateString", "StrProperty"}},
              {.bytes = {0x04, 0x0B, 0x53}},
          },
      },
  };
}

// with bAllowFriendlyFire on outside versus, it is off for one call unless a human player hurts a human-controlled pawn: goto 0x0259 and 0x01D9 -> the restore tail
ScriptRow RowFF_1() {
  return {
      .id = "FF-1",
      .function = {"GearGame.GearGame.ReduceDamage", "Function"},
      .script_size = 602,
      .checks = {
          {0x0013, {0x77, 0x00}},
          {0x0019, {0x2A, 0x16, 0x07, 0x00, 0x4F, 0x77, 0x19, 0x00}},
          {0x006F, {0x0F, 0x00}},
          {0x0075, {0x13}},
          {0x007A, {0x00}},
          {0x007F, {0x07, 0x01, 0xD9}},
          {0x00A8, {0x81, 0x2D, 0x01}},
          {0x00AF, {0x16}},
          {0x018F, {0x06, 0x01, 0xD9, 0x07, 0x01, 0xD9}},
          {0x01A7, {0x07, 0x01, 0xD9}},
          {0x01DC, {0x97, 0x48}},
          {0x01E2, {0x25, 0x16, 0x0F, 0x00}},
          {0x01EA, {0x25}},
          {0x0257, {0x04, 0x0B}},
          {0x0259, {0x53}},
          {0x01DA, {0x02}},
      },
      .operands = {
          {0x0015, {"GearGame.GearGame.ReduceDamage.injured", "ObjectProperty"}},
          {0x0021, {"GearGame.GearGame.ReduceDamage.injured", "ObjectProperty"}},
          {0x0071, {"GearGame.GearGame.ReduceDamage.GDT", "ClassProperty"}},
          {0x0076, {"GearGame.GearDamageType", "Class"}},
          {0x007B, {"GearGame.GearGame.ReduceDamage.DamageType", "ClassProperty"}},
          {0x00AB, {"GearGame.GearGame.bAllowFriendlyFire", "BoolProperty"}},
          {0x01DE, {"GearGame.GearGame.ReduceDamage.Damage", "IntProperty"}},
          {0x01E6, {"GearGame.GearGame.ReduceDamage.Idx", "IntProperty"}},
      },
      .byte_edits = {
          {0x0010, 0x07, 0x06},
          {0x0011, 0x00, 0x02},
          {0x0012, 0x6F, 0x59},
          {0x01D9, 0x07, 0x06},
          {0x01DB, 0x57, 0xD0},
      },
      .grow = GrowEdit{
          .keep = 0x0259,
          .new_size = 754,
          .tail = {
              {.bytes = {0x07, 0x02, 0xC2, 0x82, 0x82, 0x2D, 0x01}},
              {.object = {"GearGame.GearGame.bAllowFriendlyFire", "BoolProperty"}},
              {.bytes = {0x18, 0x00, 0x0A, 0x72, 0x2E}},
              {.object = {"GearGame.GearGameMP_Base", "Class"}},
              {.bytes = {0x17, 0x2A, 0x16, 0x16, 0x18, 0x00, 0x3F, 0x81, 0x82, 0x82, 0x82, 0x77, 0x00}},
              {.object = {"GearGame.GearGame.ReduceDamage.injured", "ObjectProperty"}},
              {.bytes = {0x2A, 0x16, 0x18, 0x00, 0x09, 0x77, 0x00}},
              {.object = {"GearGame.GearGame.ReduceDamage.InstigatedBy", "ObjectProperty"}},
              {.bytes = {0x2A, 0x16, 0x16, 0x18, 0x00, 0x11, 0x19, 0x00}},
              {.object = {"GearGame.GearGame.ReduceDamage.injured", "ObjectProperty"}},
              {.bytes = {0x00, 0x06, 0x00, 0x04, 0x1C}},
              {.object = {"Engine.Pawn.IsHumanControlled", "Function"}},
              {.bytes = {0x16, 0x16, 0x18, 0x00, 0x0E, 0x77, 0x2E}},
              {.object = {"Engine.PlayerController", "Class"}},
              {.bytes = {0x00}},
              {.object = {"GearGame.GearGame.ReduceDamage.InstigatedBy", "ObjectProperty"}},
              {.bytes = {0x2A, 0x16, 0x16, 0x16, 0x16, 0x14, 0x2D, 0x01}},
              {.object = {"GearGame.GearGame.bAllowFriendlyFire", "BoolProperty"}},
              {.bytes = {0x28, 0x0F, 0x00}},
              {.object = {"GearGame.GearGame.ReduceDamage.Idx", "IntProperty"}},
              {.bytes = {0x26, 0x07, 0x00, 0x6F, 0x77, 0x00}},
              {.object = {"GearGame.GearGame.ReduceDamage.injured", "ObjectProperty"}},
              {.bytes = {0x2A, 0x16, 0x06, 0x00, 0x1B, 0x07, 0x02, 0xE3, 0x9A, 0x00}},
              {.object = {"GearGame.GearGame.ReduceDamage.Idx", "IntProperty"}},
              {.bytes = {0x26, 0x16, 0x14, 0x2D, 0x01}},
              {.object = {"GearGame.GearGame.bAllowFriendlyFire", "BoolProperty"}},
              {.bytes = {0x27, 0x07, 0x02, 0x57, 0x97, 0x48}},
              {.object = {"GearGame.GearGame.ReduceDamage.Damage", "IntProperty"}},
              {.bytes = {0x25, 0x16, 0x06, 0x01, 0xE4, 0x53}},
          },
      },
  };
}

}  // namespace

PatchGroup CoopRequestsGroup() {
  return {
      .name = "coop-requests",
      .scripts = {RowRQ_1()},
  };
}

PatchGroup CoopFriendlyFireGroup() {
  return {
      .name = "coop-ff",
      .scripts = {RowFF_1()},
  };
}

}  // namespace rex::glue::script_patch
