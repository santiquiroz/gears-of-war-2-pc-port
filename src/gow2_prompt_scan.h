// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Finds the game's [ControllerFont] strings in a block of guest memory: first the text itself,
// then the FString headers (data pointer, Num, Max) that own it. Pure functions over a byte range,
// so the host test runs them on synthetic memory.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace rex::glue::promptscan {

struct StringHit {
  uint32_t addr;   // guest address of the first character
  size_t index;    // which entry of `wanted`
};

// Guest memory is big-endian: the text is UTF-16BE, ending with a NUL, starting at an even
// address. `mem` is the host copy of guest address `guest_addr`.
std::vector<StringHit> FindStrings(const uint8_t* mem, size_t size, uint32_t guest_addr,
                                   const std::vector<std::u16string>& wanted);

struct HeaderHit {
  uint32_t header;  // guest address of the FString (Data, Num, Max)
  uint32_t chars;
  uint32_t num;     // characters including the NUL
  uint32_t max;     // capacity in characters
};

// `strings` maps a text's guest address to its length in characters including the NUL. A header
// counts when it is 4-aligned, points at that address and has Num equal to that length and
// Max >= Num.
std::vector<HeaderHit> FindHeaders(const uint8_t* mem, size_t size, uint32_t guest_addr,
                                   const std::unordered_map<uint32_t, uint32_t>& strings);

// The in-memory character table of Warfare_HUD.Xbox360_18pt: UFont::Characters, an array of
// FFontCharacter {StartU, StartV, USize, VSize, TextureIndex, VerticalOffset}. Found through the
// cells of the glyphs 'A'..'E' (retail values), which sit one record apart in a 256-entry table.
struct CharTableHit {
  uint32_t table;   // guest address of the record for character 0
  uint32_t stride;  // bytes per record
};
std::vector<CharTableHit> FindCharTable(const uint8_t* mem, size_t size, uint32_t guest_addr);

}  // namespace rex::glue::promptscan
