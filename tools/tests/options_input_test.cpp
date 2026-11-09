// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// Exercises controller timing, command edges, chord suppression, and reset isolation.
#include "gow2_options_input.h"

#include <cstdio>
#include <limits>
#include <string_view>

using namespace rex::glue::options;

int g_checks = 0;
int g_failures = 0;

void Expect(bool ok, std::string_view what) {
  ++g_checks;
  if (ok) return;
  ++g_failures;
  std::printf("FAIL: %.*s\n", static_cast<int>(what.size()), what.data());
}

bool IsCommand(const std::vector<Command>& commands, Command command) {
  return commands.size() == 1 && commands[0] == command;
}

void TestRepeatGate() {
  RepeatGate gate;
  Expect(!gate.Update(false, 0), "released repeat gate does not fire");
  Expect(gate.Update(true, 0), "repeat fires on press at zero");
  Expect(!gate.Update(true, 0), "same timestamp does not fire twice");
  Expect(!gate.Update(true, 399), "repeat waits the first 399 ms");
  Expect(gate.Update(true, 400), "repeat fires at 400 ms");
  Expect(!gate.Update(true, 489), "repeat waits another 89 ms");
  Expect(gate.Update(true, 490), "repeat fires after another 90 ms");
  Expect(gate.Update(true, 760), "late frame produces one repeat");
  Expect(!gate.Update(true, 760), "late frame does not accumulate repeats");
  Expect(!gate.Update(false, 761), "release does not fire");
  Expect(gate.Update(true, 762), "release rearms repeat");
  Expect(!gate.Update(true, 10), "clock rewind does not produce another press");
  Expect(!gate.Update(true, 409), "clock rewind restarts repeat delay");
  Expect(gate.Update(true, 410), "repeat resumes after rewind delay");
  RepeatGate extremes;
  Expect(extremes.Update(true, std::numeric_limits<int64_t>::min()), "negative timestamps support presses");
  Expect(extremes.Update(true, std::numeric_limits<int64_t>::max()), "large elapsed interval avoids signed overflow");
}

void TestChordGate() {
  ChordGate gate;
  Expect(!gate.Update(false, false, 0), "empty chord does not fire");
  Expect(!gate.Update(true, false, 10), "one chord button does not start timer");
  Expect(!gate.Update(true, true, 100), "chord starts when both become held");
  Expect(!gate.Update(true, true, 399), "chord waits 299 ms");
  Expect(gate.Update(true, true, 400), "chord fires after 300 ms");
  Expect(!gate.Update(true, true, 700), "held chord fires once");
  Expect(!gate.Update(false, true, 710), "partial release does not rearm");
  Expect(!gate.Update(true, true, 720), "repressing only one button remains latched");
  Expect(!gate.Update(true, true, 1020), "partial release cannot trigger a second chord");
  Expect(!gate.Update(false, false, 1030), "complete release rearms without firing");
  Expect(!gate.Update(true, true, 1040), "rearmed chord starts a fresh timer");
  Expect(gate.Update(true, true, 1340), "rearmed chord fires");
  ChordGate interrupted;
  Expect(!interrupted.Update(true, true, 0), "interrupted chord starts");
  Expect(!interrupted.Update(true, false, 200), "interruption resets duration");
  Expect(!interrupted.Update(true, true, 250), "both held starts timer again");
  Expect(!interrupted.Update(true, true, 549), "continuous overlap must last 300 ms");
  Expect(interrupted.Update(true, true, 550), "continuous overlap fires at threshold");
  ChordGate rewound;
  Expect(!rewound.Update(true, true, 100), "rewind chord starts");
  Expect(!rewound.Update(true, true, 0), "rewind restarts chord timer");
  Expect(rewound.Update(true, true, 300), "rewound chord reaches threshold");
}

void TestButtonEdges() {
  constexpr uint16_t buttons[]{0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200};
  constexpr Command commands[]{Command::kAccept, Command::kBack, Command::kResetRow,
                               Command::kSave, Command::kTabPrev, Command::kTabNext};
  PadReader reader;
  Expect(reader.Update({}, 0).empty(), "empty pad has no commands");
  for (std::size_t i = 0; i < 6; ++i) {
    Expect(IsCommand(reader.Update({buttons[i]}, 10), commands[i]), "button press maps to expected command");
    Expect(reader.Update({buttons[i]}, 1000).empty(), "held action button does not repeat");
    Expect(reader.Update({}, 1010).empty(), "button release emits nothing");
    Expect(IsCommand(reader.Update({buttons[i]}, 1020), commands[i]), "button repress fires again");
    reader.Update({}, 1030);
  }
  Expect(reader.Update({0x0030}, 1100).empty(), "start and back do not map to commands");
}

void TestDirections() {
  constexpr uint16_t bits[]{0x0001, 0x0002, 0x0004, 0x0008};
  constexpr Command commands[]{Command::kUp, Command::kDown, Command::kLeft, Command::kRight};
  for (std::size_t i = 0; i < 4; ++i) {
    PadReader reader;
    Expect(IsCommand(reader.Update({bits[i]}, 0), commands[i]), "dpad press maps to direction");
    Expect(reader.Update({bits[i]}, 399).empty(), "held direction waits repeat delay");
    Expect(IsCommand(reader.Update({bits[i]}, 400), commands[i]), "held direction repeats at 400 ms");
    Expect(IsCommand(reader.Update({bits[i]}, 490), commands[i]), "held direction repeats every 90 ms");
    Expect(reader.Update({}, 500).empty(), "direction release emits nothing");
    Expect(IsCommand(reader.Update({bits[i]}, 501), commands[i]), "direction repress fires immediately");
  }
  PadReader stick;
  Expect(stick.Update({0, 0.5f, -0.5f}, 0).empty(), "stick exactly at threshold is neutral");
  Expect(IsCommand(stick.Update({0, 0.5001f, 0}, 1), Command::kRight), "positive x past threshold moves right");
  Expect(IsCommand(stick.Update({0, -0.5001f, 0}, 2), Command::kLeft), "negative x past threshold moves left");
  Expect(IsCommand(stick.Update({0, 0, 0.5001f}, 3), Command::kUp), "positive y moves up");
  Expect(IsCommand(stick.Update({0, 0, -0.5001f}, 4), Command::kDown), "negative y moves down");
  PadReader combined;
  Expect(IsCommand(combined.Update({0x0001, 0, 1}, 0), Command::kUp), "dpad and stick combine into one direction");
  Expect(combined.Update({0, 0, 1}, 399).empty(), "switching direction source preserves repeat delay");
  Expect(IsCommand(combined.Update({0, 0, 1}, 400), Command::kUp), "stick repeats after dpad source disappears");
}

void TestSuppressionAndReset() {
  PadReader reader;
  Expect(reader.Update({0xF33F, 1, 1}, 0).empty(), "chord suppresses all commands");
  Expect(reader.Update({0xF33F, 1, 1}, 400).empty(), "chord suppresses direction repeats");
  Expect(reader.Update({0xF30F, 1, 1}, 401).empty(), "held actions consumed during suppression do not leak");
  Expect(IsCommand(reader.Update({0x0001}, 490), Command::kUp), "directions resume their repeat cadence after chord");
  reader.Reset({0xF30F, 1, 1});
  Expect(reader.Update({0xF30F, 1, 1}, 1000).empty(), "reset ignores all held buttons and directions");
  Expect(reader.Update({0xF30F, 1, 1}, 2000).empty(), "reset directions stay blocked until release");
  Expect(reader.Update({}, 2010).empty(), "release after reset emits nothing");
  Expect(IsCommand(reader.Update({0x1000}, 2020), Command::kAccept), "released reset button fires on new press");
  Expect(IsCommand(reader.Update({0, 1, 0}, 2030), Command::kRight), "released reset stick fires on new tilt");
  reader.Reset({0x0030});
  Expect(!reader.ToggleChord({0x0030}, 0), "reset suppresses held toggle chord");
  Expect(!reader.ToggleChord({0x0030}, 300), "held reset chord cannot fire later");
  Expect(!reader.ToggleChord({0x0010}, 400), "partial reset chord release stays latched");
  Expect(!reader.ToggleChord({0x0030}, 700), "partial release cannot rearm reset chord");
  Expect(!reader.ToggleChord({}, 800), "full release rearms reset chord");
  Expect(!reader.ToggleChord({0x0030}, 900), "new chord starts after reset release");
  Expect(reader.ToggleChord({0x0030}, 1200), "new chord fires after reset release");
  Expect(!reader.ToggleChord({0x0030}, 1600), "pad chord fires only once");
}

void TestQuitHold() {
  PadReader held;
  Expect(held.Update({0x0020}, 0).empty(), "view hold starts without command");
  Expect(held.Update({0x0020}, 699).empty(), "view hold waits 700 ms");
  Expect(IsCommand(held.Update({0x0020}, 700), Command::kQuit), "view held 700 ms emits quit");
  Expect(held.Update({0x0020}, 2000).empty(), "long view hold emits quit only once");

  PadReader released;
  released.Update({0x0020}, 0);
  Expect(released.Update({}, 500).empty(), "early view release emits nothing");

  PadReader cancelled;
  cancelled.Update({0x0020}, 0);
  Expect(cancelled.Update({0x0030}, 350).empty(), "start during view hold cancels quit");
  Expect(cancelled.Update({0x0020}, 800).empty(), "released start cannot revive cancelled hold");

  PadReader reset;
  reset.Reset({0x0020});
  Expect(reset.Update({0x0020}, 0).empty(), "reset view hold is latched");
  Expect(reset.Update({0x0020}, 900).empty(), "reset view hold never emits");
  reset.Update({}, 910);
  reset.Update({0x0020}, 920);
  Expect(IsCommand(reset.Update({0x0020}, 1620), Command::kQuit), "view emits after release and fresh hold");

  PadReader progress;
  progress.Update({0x0020}, 100);
  Expect(progress.QuitHoldProgress(450) > .49f && progress.QuitHoldProgress(450) < .51f,
         "quit hold progress is about one half at 350 ms");
  progress.Update({}, 800);
  Expect(progress.QuitHoldProgress(800) == 0.f, "quit hold progress is zero when released");
}

int main() {
  TestRepeatGate();
  TestChordGate();
  TestButtonEdges();
  TestDirections();
  TestSuppressionAndReset();
  TestQuitHold();
  std::printf("options_input_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
