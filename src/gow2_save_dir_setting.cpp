// gow2 - ReXGlue Recompiled Project
#include "gow2_save_dir.h"

#include <rex/cvar.h>

REXCVAR_DEFINE_STRING(gow2_save_dir, "", "Storage",
    "Folder for saves, profile settings and achievement unlocks, for example a OneDrive, Google Drive or Dropbox folder. Relative paths start at the game's folder (on Android, the folder with gow2.toml); %VARIABLES% are expanded on Windows. Empty keeps the default. Takes effect on the next start")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

namespace rex::glue { std::string SaveDirSetting() { return REXCVAR_GET(gow2_save_dir); } }
