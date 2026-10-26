# Voice and subtitle languages

PC Options > System has two rows, both applied after a restart:

- **Subtitles and text** (`user_language`): English (1), Español (Latinoamérica) (5), Français (4),
  Deutsch (3), Italiano (6), Polski (11), Russian (12), Korean (7), Traditional Chinese (8).
- **Voices** (`gow2_voice_language`): Same as subtitles (`auto`), English (`en`), Español
  (Latinoamérica) (`es`), Français (`fr`).

Design: [superpowers/specs/2026-10-26-voice-and-subtitle-language-design.md](superpowers/specs/2026-10-26-voice-and-subtitle-language-design.md).

## How it works

- The game takes its language from `user_language` (the console's XCONFIG language), read through
  `ExGetXConfigSetting`. `XGetLanguage` is not what decides it.
- Each level's voice package `<Base>_LOC_<lang>.xxx` carries the dialogue and the subtitles of every
  language. The engine shows the subtitles of its current language from whichever package it loaded.
- So the voices follow which `_LOC_` files are served. At start `PrepareVoiceLanguage`
  (`src/gow2_voice_language_setup.cpp`) reads the disc's `Xbox360TOC*.txt` files.
  `PlanVoiceSwap` (`src/gow2_voice_language.cpp`) picks the packages to serve and their TOC lines. The
  edited TOCs and an overrides list are written to `<cache>/voice/`, and SDK patch 0061
  (`GOW2_VFS_OVERRIDES_FILE`) serves them.
- `GearGame_LOC_*` (UI content, with its own Korean and Chinese versions), `Startup_*` (hash-checked)
  and the Coalesced bundles are never swapped.

## Measured on the retail USA multi-language disc (2026-10-26)

What each `user_language` opens at boot (noisy file log), and the title screen it shows:

| user_language | Coalesced | Startup | GearGame_LOC | Title screen |
|---|---|---|---|---|
| 1 English | INT | int | int | PRESS START |
| 5 Spanish | ESM | ESM | ESM | (files only) |
| 4 French | FRA | FRA | FRA | APPUYEZ SUR START |
| 3 German | DEU | int | int | START DRÜCKEN |
| 6 Italian | ITA | int | int | PREMI START |
| 11 Polish | POL | POL | int | WCISNIJ START |
| 12 Russian | RUS | RUS | int | НАЖМИТЕ START |
| 7 Korean | KOR | KOR | KOR | START(…)를 누르십시오. |
| 8 Trad. Chinese | CHT | CHT | CHT | 按下 START 鍵 |

- Spanish always loads the Latin American set (ESM). `user_country` (Mexico 71, Spain 31) changes
  nothing.
- Text-only languages fall back to the `int` voice packages directly from the merged TOC table,
  without a failed open first.
- The XEX hash table names only `startup_*.xxx` and the Coalesced bundles, so voice packages can be
  served from another language without a "Disc Read Error".

Chapter 1 runs (`scripts/chapter-sweep.ps1 -From 1 -To 1`):

| Setup | Log | Result |
|---|---|---|
| `--user_language=5 --gow2_voice_language=en` | `VOICES text ESM, voices INT: 369 packages swapped`, `vfs overrides file: 370 entries` | chapter loads, ~80 fps, Spanish loading tip ("Sugerencia: pulse X para rematar a un enemigo derribado.") |
| `--user_language=1 --gow2_voice_language=es` | `VOICES text INT, voices ESM: 369 packages swapped` | chapter loads, English subtitle on screen ("They're getting bolder. Those Gears were guarding Pomeroy Depot."), drawn from a Latin American Spanish voice package |
| `--user_language=5 --gow2_voice_language=en`, cache under a non-ASCII folder | 369 swapped, the overrides file read from that folder | boots |
| same folder, then `--gow2_voice_language=auto` | `voices same as text: nothing swapped` | no overrides file line, `overrides.txt` deleted |

Not checked yet: a Spanish subtitle on screen while English voices play. The English package does
carry the Latin American lines ("Te informo más tarde, Dom. Debo regresar a Control."), and the English
subtitle drawn from a Spanish package shows the engine picks by the text language. The game's sound
itself cannot be heard by these tests: the file log is the evidence for the voices.

## Reading a log

- `VOICES ...`: the plan at start. A failure is logged as a warning, and the voices stay as the game
  picks them.
- `vfs overrides file: N entries from ...`: the SDK read the list (N = swapped packages + edited TOCs).
- `vfs override: GearGame\CookedXenon\<name> -> <other language's file>`: one line per served file.
- `LANGUAGE user_language N is not on this disc`: the chosen text language is missing, so that launch
  runs in English.
