// Adapted from gears-judgement-recomp (BSD-3-Clause).
// Coalesced tweak layer.
//
// The game keeps its input bindings and UI strings in the Coalesced_*.bin bundles inside
// GearGame\CookedXenon (an archive of ini files). The edits below are applied to a cached copy
// of the player's own files and the runtime is told to read the copy instead
// (GOW2_VFS_OVERRIDES). The game folder is never modified.
//
// Edits are grouped by purpose and each group is switched on by an option:
//   mouse-look   right-stick dead zone off for the mouse-fed look axes (cvar mnk_mouse)
//   mouse-aim    PC mouse axes with no smoothing for native mouse look (cvars mnk_mouse, gow2_mouse_aim)
//   key-prompts  menu prompts show keyboard labels ([Space], [LMB]...) (cvar gow2_prompts)
//   pc-options-entry  Options > Storage Location becomes the PC Options entry
//                     (cvar gow2_pc_options_entry, needs SDK patch 0013)
//   native-keys  the keyboard's own engine bindings (cvar gow2_native_keys, src/gow2_native_keys.cpp)
//   system-settings  the game's own [SystemSettings] values (Game quality cvars and gow2_system_settings)
#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "gow2_native_bindings.h"
#include "gow2_sha_table.h"

namespace rex::glue {

struct CoalescedOptions {
  bool mouse_look = false;
  bool native_mouse = false;
  bool pc_options_entry = false;
  // Keyboard bindings of the engine (cvar gow2_native_keys); empty leaves the game's bindings alone.
  std::vector<NativeBinding> native_bindings;
  // Key = a name in [ControllerFont] of GearGameUI ("Controller_A"), value = the label shown
  // ("[Space]"). Empty means "leave the pad glyphs alone".
  std::vector<std::pair<std::string, std::string>> prompt_labels;
  // Key = a name in [SystemSettings] of Xe-GearEngine.ini ("AmbientOcclusion"), value = its new
  // value ("False"). Empty means "leave the game's settings alone".
  std::vector<std::pair<std::string, std::string>> system_settings;
};

struct CoalescedResult {
  std::string overrides;  // value for GOW2_VFS_OVERRIDES, empty when nothing was prepared
  std::string summary;    // one log line
  // The served copies' names and SHA-1, for the game's hash table (gow2_sha_table.h). Computed
  // here, before the runtime exists, because hashing later stalls the boot.
  std::vector<HashSecEntry> hashes;
};

CoalescedResult PrepareCoalescedOverrides(const std::filesystem::path& game_root,
                                          const std::filesystem::path& cache_dir,
                                          const CoalescedOptions& options);

// Reads mnk_mouse, gow2_prompts, gow2_pc_options_entry and the keybind_* labels.
CoalescedOptions CoalescedOptionsFromCvars();
// True when the runtime routes the device selector to the PC Options menu (patch 0013).
bool HostUiHooksAvailable();

// Live prompts (gow2_prompt_swap.cpp), Windows with gow2_prompts = auto: the bundles keep the
// pad glyphs and the strings are rewritten in memory as controllers come and go.
bool PromptsFollowController();
bool PhysicalGamepadConnected();
// Keyboard label per [ControllerFont] key ("Controller_A" -> "[Space]"), from the keybind_* settings.
std::vector<std::pair<std::string, std::string>> KeyboardPromptLabels();
// The [ControllerFont] values of every Coalesced bundle in the game folder, by locale. The glyphs
// differ between locales, so the swap needs the set the running game really loaded.
struct ControllerFontSet {
  std::string locale;
  std::vector<std::pair<std::string, std::u16string>> values;  // key, original text
};
std::vector<ControllerFontSet> ReadControllerFontOriginals(const std::filesystem::path& game_root);

}  // namespace rex::glue
