# Voice and subtitle language options: design

Owner request (2026-10-26): "algunos lo quieren jugar en español, otros en inglés … en las opciones demos
las opciones de elegir qué idioma o subtítulo usar … con tal de que le des la opción a la gente de cambiar
el idioma y subtítulo cuando quieran". Every player picks, from the in-game options, the language of the
voices and, separately, the language of subtitles and game text. Today everyone gets English.

## What the game does (measured 2026-10-26, retail USA multi-language disc)

- **The language comes from `user_language`.** The game reads it through `ExGetXConfigSetting`
  (XCONFIG_USER_LANGUAGE, `src/kernel/xboxkrnl/xboxkrnl_xconfig.cpp` in the SDK), not through
  `XGetLanguage` (that one is hard-wired to English in the SDK and a hook on it changed nothing).
  `user_language` is an existing runtime cvar (Kernel, default 1 = English). With `--user_language=5` the
  game opened `Coalesced_ESM.bin`, `GearGame_LOC_ESM.xxx` and `Startup_ESM.xxx`: Spanish always means the
  Latin American set (ESM); `user_country` (Mexico 71, Spain 31) made no difference.
- **What the disc has.** Full voice + text sets: `int` (English, 372 files), `ESM` (Latin American Spanish,
  371) and `FRA` (French, 371), each with its own TOC (`Xbox360TOC.txt`, `Xbox360TOC_ESM.txt`,
  `Xbox360TOC_FRA.txt`). Text-only (one `Coalesced_<lang>.bin` and some `Startup_<lang>.xxx`): deu, ita,
  pol, rus, kor, cht, esn, cze, hun. At start the game probes `Xbox360TOC_<lang>.txt` for every language.
- **Subtitles live inside the voice packages, for every language.** A level's `<Base>_LOC_<lang>.xxx`
  (e.g. `SP_Hospital_01_S_LOC_int.xxx`, ~400 KB) holds the dialogue and a `LocalizedSubtitles` list with
  the line in all languages: the English package has "Te informo más tarde, Dom. Debo regresar a
  Control." and "Ich er…", the ESM package has "I'll fill you in later, Dom…". The engine shows the
  subtitles of its current language from whichever voice package it loaded. So the subtitle/text language
  is `user_language`, and the voice language is a question of which `_LOC_` files the game is served.
- **No hash check on voice packages.** The XEX HashSec table names only `startup_*.xxx` and the Coalesced
  bundles (`basefile.exe` strings), so serving other bytes for a `_LOC_` package does not trip the
  "Disc Read Error" path. Startup packages ARE hashed: they are never swapped.
- **TOC lines** are `<size> <uncompressed size> <third field> ..\GearGame\CookedXenon\<file> <md5>`. A served
  file must match the size its TOC line gives (the same rule the Coalesced layer follows in
  `gow2_coalesced.cpp`), so a swapped package needs its TOC line swapped too.

## Goals

- Two rows in the options menu (PC Options on Windows, Device Options on Android), System tab, next to
  "Menu language", both `Apply::kRestart` (the game picks its language at boot; the menu's existing
  "Restart now" flow applies):
  - **Subtitles and text** (`user_language`, written to `gow2.toml`): English (1), Español (Latinoamérica)
    (5), Français (4), Deutsch (3), Italiano (6), Polski (11), Русский (12), 한국어 (7), 繁體中文 (8), all
    verified to boot in their language (see "Measured after the builder check"). Labels are written in
    each language; the menu's font must show them, or the label falls back to its English name.
  - **Voices** (`gow2_voice_language`, new string cvar): "Same as subtitles" (`auto`, default), English
    (`en`), Español (Latinoamérica) (`es`), Français (`fr`). `auto` means what the game does on its own:
    the subtitle language's dub when the disc has one, English otherwise.
- Any combination works: English voices + Spanish subtitles, Spanish voices + English subtitles, French
  voices + German text, and so on.
- The game folder is never modified (same rule as the Coalesced layer). Swapped voice packages are served
  straight from the other language's files in the game folder (no copy); only the edited TOCs are written,
  to the cache folder. (Designer's call after the builder check.)
- Works on PC and Android.

## Non-goals

- Changing language without a restart.
- Spanish from Spain voices (none on the disc) or Spain-Spanish text (`esn`; Spanish always loads ESM on
  this disc). Czech and Hungarian text (no XLanguage id for them) unless they turn out to be trivial.
- Japanese or any language the disc does not carry.

## Anti-goals

- A choice in the menu that silently does nothing, or one that breaks loading (missing voices, a hang at
  a level load, a "Disc Read Error"). A language that cannot be verified is left out of the list.
- Touching the player's game files or their Coalesced tweaks (mouse aim, key prompts, PC Options entry):
  those keep working in every language (the Coalesced layer already edits all 12 bundles).

## How it works

1. **Text/subtitles:** the row writes `user_language`. Nothing else is needed: the game loads
   `Coalesced_<T>`, `Startup_<T>` and picks the `<T>` subtitles inside each voice package.
2. **Voices:** in `OnPreSetup`, next to `PrepareCoalesced()`, a new pure planner decides, from the text
   language T (from `user_language`) and the voice choice V, which `_LOC_` files to serve:
   - effective voice of T = T if the disc has a voice set for T (int/ESM/FRA), else int.
   - If V is `auto` or equals that effective voice: no swap.
   - Otherwise, for every package base B other than `GearGame` that has `B_LOC_<V>` in V's TOC: the name
     the game will open is `B_LOC_<T>` when some TOC lists it, else `B_LOC_int` (measured: text-only
     languages fall back to int); when that name is `B_LOC_<V>` itself there is nothing to swap. Serve that
     name from the host file `B_LOC_<V>.xxx`, and
     serve the TOC that lists that name (`Xbox360TOC.txt` for int, `Xbox360TOC_<T>.txt` otherwise) as an
     edited copy in the cache folder in which each swapped line carries the size, the two middle fields
     and the md5 from `B_LOC_<V>`'s own TOC line, with the path left as the name the game opens.
   - A base with no `B_LOC_<V>` keeps its own package. Bases are matched case-insensitively (the TOCs mix
     `_LOC_int` and `_LOC_ESM`).
3. **Serving many files:** `GOW2_VFS_OVERRIDES` (SDK patch `0005-pad-deadzone-and-file-overrides.patch`,
   `GetVfsOverrides()` in `src/filesystem/devices/host_path_device.cpp`) is one environment variable of
   exact `guest=host` pairs separated by `;`; ~370 swapped packages plus TOCs do not fit in a Windows
   environment variable (32 767 characters). New SDK patch (in scope; next free number after 0060, built
   into the PC DLLs and the Android libs like every other patch): the same table also reads
   `GOW2_VFS_OVERRIDES_FILE`, a UTF-8 file with one `guest=host` pair per line, keys normalized exactly
   like the variable's keys (blank lines and lines starting with `#` ignored), merged with the variable
   (the variable wins on a duplicate key; in practice the two never share keys: the variable carries
   `Coalesced_*.bin`, the file carries `_LOC_` packages and `Xbox360TOC*.txt`). The game writes the file in
   the cache folder and sets the variable before the runtime mounts the game folder, the same moment
   `PrepareCoalesced()` sets `GOW2_VFS_OVERRIDES` today.
   - Which TOC to edit: at boot the game opens every TOC on the disc (`Xbox360TOC.txt`,
     `Xbox360TOC_esm.txt`, `Xbox360TOC_fra.txt`, measured in every language), so it builds one table from
     all of them. The edited copy is the TOC that lists the name the game opens; the swapped line takes all
     four fields (size, uncompressed size, third field, md5) from the source package's own line. Whether
     the third field matters is checked by the level-load test in "Open".
4. **Logging:** one INFO line at start: text language, voice choice, effective voices, how many packages
   were swapped, or why none were (same choice, missing TOC, unreadable file). The existing
   "vfs override:" lines show each served file.

## Constraints

- `user_language` is a runtime cvar already in `rexruntime.dll`: the row binds to it directly, like
  `resolution_scale` (an integer cvar of the GPU plugin with `Apply::kRestart`). Its value in `gow2.toml`
  must be an integer. The game reads it when its code asks the console config, after `gow2.toml` and the
  command line are applied (measured with `--user_language=5`).
- Other discs: only the retail USA multi-language disc was measured. At start, if the files of the chosen
  text language (`Coalesced_<T>.bin`) or voice set (its TOC) are not on the player's disc, that launch uses
  English for that part (without rewriting `gow2.toml`) and logs a warning. (Designer's call.)
- Fonts: Korean and Chinese glyphs come with the text language's own Startup/Coalesced files, which are
  never swapped, so a voice swap cannot take them away.
- Startup packages and Coalesced bundles are never swapped (hashed / handled by the Coalesced layer).
- The planner (names, TOC parsing and editing, which files to swap) is pure and gets a host test like the
  other `tools/tests/*_test.cpp`; the Windows/Android glue only reads files, writes the cache copies and
  sets the variables.
- Any failure (missing TOC, unreadable file, a write error in the cache) logs a warning and leaves the
  voices as the game would pick them; it never stops the game from starting.
- Co-op and System Link: each machine uses its own language; nothing about language goes into the co-op
  hash or the network.

## Measured after the builder check (2026-10-26)

Every `user_language` in the list boots to PRESS START in its own language (screenshots: French, German,
Italian, Polish, Russian, Korean, Traditional Chinese, all with their own glyphs), and the noisy file log
(`--log_verbose=true --log_noisy=true`, `[NtCreateFile] path=` lines) shows what each one opens:

| user_language | Coalesced | Startup | GearGame_LOC |
|---|---|---|---|
| 1 English | INT | int | int |
| 5 Spanish | ESM | ESM | ESM |
| 4 French | FRA | FRA | FRA |
| 3 German, 6 Italian | DEU / ITA | int | int |
| 11 Polish, 12 Russian | POL / RUS | POL / RUS | int |
| 7 Korean, 8 Trad. Chinese | KOR / CHT | KOR / CHT | KOR / CHT |

- Text-only languages fall back to the `int` package straight from the merged TOC table: there is no
  failed open of a `_LOC_DEU` name first. So for a text-only T the name to serve is `B_LOC_int`.
- `GearGame_LOC_KOR` and `GearGame_LOC_CHT` exist without any Korean or Chinese voices: `GearGame_LOC`
  carries UI content per language. **`GearGame_LOC_*` is never swapped** (like `Startup_*`); only the
  other `_LOC_` packages (levels, cinematics, `GUD*` battle chatter) follow the voice choice. The menu and
  announcer lines in `GearGame_LOC` may stay in the text language; that is accepted.

## Open (the builder resolves this by running the game)

- That a swapped voice set loads and plays: during a level load the log must show the swapped `_LOC_`
  names served from the other language's files, a cinematic with subtitles must show T's subtitles, and
  the level must load without a hang or a "Disc Read Error" (this also settles whether the third TOC field
  matters). Audio itself cannot be heard by the tests; the file log is the evidence. Check at least
  English voices + Spanish text and Spanish voices + English text.

## Left to the builder

- Names of files and functions, where the planner lives, the exact TOC edit code, the SDK patch number
  (next free after 0060), and how the voice row is hidden when the voice set for a language is missing.
- Help texts for the two rows (English and Spanish), docs (README, README.es, PC_OPTIONS.md,
  QUICKSTART.txt via `scripts/package.ps1`), and the release notes.
