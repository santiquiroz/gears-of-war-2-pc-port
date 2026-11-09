// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Current cinematic and host state for the overlay hint.
#pragma once

namespace rex::glue {

struct CineNow {
  bool cinematic = false;
  bool host = false;
};

CineNow CurrentCinematic();

}  // namespace rex::glue
