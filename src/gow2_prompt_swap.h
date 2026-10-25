// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Live button prompts: keyboard labels after keyboard or mouse input, the game's own pad glyphs (Xbox, or
// PlayStation through the keycap pack's font page) after controller input. The game keeps the glyph strings
// it loaded from the Coalesced bundle; this finds them in guest memory once and rewrites them in place
// when the device used last changes.
#pragma once

#include <filesystem>

namespace rex::glue {

// Windows with gow2_prompts = auto, keyboard or playstation; anywhere else (or controller) it does nothing.
void InstallPromptSwap(const std::filesystem::path& game_root);
// Turns the texture pack on when the keycap pages are installed. Call before the runtime starts.
void PrepareKeycapPack();

}  // namespace rex::glue
