# Voice and Subtitle Language Options Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Two rows in the options menu let each player pick the voice language and, separately, the subtitle/text language of the game.

**Architecture:** The subtitle/text row writes the runtime cvar `user_language`, which the game already reads at boot. The voice row (`gow2_voice_language`) makes the port serve the other language's `_LOC_` voice packages under the names the game opens, with matching TOC lines, through the existing VFS override table; a small SDK patch lets that table read a long list from a file. A pure planner decides the swaps from the disc's TOC files.

**Tech Stack:** C++23, ReXGlue SDK 923c1a5 + `sdk/patches`, CMake/Ninja (clang), PowerShell build and test scripts.

**Spec:** `docs/superpowers/specs/2026-10-26-voice-and-subtitle-language-design.md`

## Global Constraints

- The game folder is never modified; swapped voice packages are served from the other language's files in the game folder, edited TOCs from the cache folder.
- `GearGame_LOC_*` and `Startup_*` are never swapped; Coalesced bundles stay with the Coalesced layer.
- `user_language` values: English 1, Español (Latinoamérica) 5, Français 4, Deutsch 3, Italiano 6, Polski 11, Russian 12, Korean 7, Traditional Chinese 8. Default stays 1 (English).
- `gow2_voice_language` values: `auto` (default, same as subtitles), `en`, `es`, `fr`.
- Both rows: System tab, right after "Menu language" (`gow2_menu_language`), `Apply::kRestart`, `kAllPlatforms`.
- Any failure logs a warning and leaves the voices as the game picks them; the game always starts.
- A launch whose chosen text language has no `Coalesced_<T>.bin` on the disc runs in English, without rewriting `gow2.toml`.
- Code style of the repo: SPDX BSD-3 header on new files, 2-space indent, no doc comments, one-line comments only for a non-obvious why, small single-purpose functions. Commit messages in Spanish, conventional style (`feat(...)`, `fix(...)`), no `Co-Authored-By`.
- Do not run `git push`; the orchestrator publishes.

## Review Focus

1. Cache folder with non-ASCII characters (a Windows user named José, a Chinese user name): the overrides file path must reach the SDK intact. Pinned in Task 1 (wide environment variable on Windows) and Task 4 (run with `--cache_root` under a folder named `caché-ñ`).
2. Changing the voice choice back to `auto` or to another language between launches: stale overrides must not survive. Pinned in Task 3 (the file and variable are rewritten or removed on every launch) and Task 4 (an `auto` run logs no overrides file).
3. A disc without the chosen voice set (another region): no swap, a warning, the game starts. Pinned in Task 2 (`voice set missing` case).
4. TOC files with CRLF or LF line ends, and names spelled `_LOC_int` / `_LOC_ESM` / `Xbox360TOC_esm.txt` in mixed case: edits keep the file's line ends and match names case-insensitively. Pinned in Task 2.
5. Android's case-sensitive file system: TOCs and `Coalesced_<T>.bin` are found by scanning the folder case-insensitively, never by building a name. Pinned in Task 3.

---

### Task 1: SDK patch `0061-vfs-overrides-file.patch`

**Files:**
- Modify (then extract the patch): `out/sdk-build/rexglue-sdk/src/filesystem/devices/host_path_device.cpp` (`GetVfsOverrides()`, ~line 47)
- Create: `sdk/patches/0061-vfs-overrides-file.patch`
- Modify: `sdk/patches/README.md` (one table row after `0060-pad-activity.patch`)

**Interfaces:**
- Produces: the environment variable `GOW2_VFS_OVERRIDES_FILE` = path of a UTF-8 text file, one `guest=host` pair per line (guest relative to the game folder, `\` or `/`, any case; host absolute, UTF-8). Blank lines and lines starting with `#` are ignored; a trailing `\r` is stripped. Entries from `GOW2_VFS_OVERRIDES` win on a duplicate key. On Windows the path is read with `_wgetenv(L"GOW2_VFS_OVERRIDES_FILE")`, elsewhere with `getenv`. One `REXLOG_INFO("vfs overrides file: {} entries from {}", count, path_utf8)` when the file was read.

- [ ] **Step 1: Snapshot the patched file in the index**

Run: `git -C out/sdk-build/rexglue-sdk add src/filesystem/devices/host_path_device.cpp`
Expected: no output (the file as patches 0002-0060 left it is now staged, so the next diff holds only this change).

- [ ] **Step 2: Add `void AddOverridesFromFile(std::unordered_map<std::string, std::string>& table)` in the anonymous namespace and call it in `GetVfsOverrides()` before the variable is parsed**

Keys go through `NormalizeOverrideKey` exactly like the variable's keys; values are kept as given (they are turned into paths with `rex::to_path`, like today).

- [ ] **Step 3: Extract the patch and unstage**

Run: `git -C out/sdk-build/rexglue-sdk diff -- src/filesystem/devices/host_path_device.cpp > sdk/patches/0061-vfs-overrides-file.patch` then `git -C out/sdk-build/rexglue-sdk reset -q -- src/filesystem/devices/host_path_device.cpp`
Expected: the patch has only the new function and its call; `git apply --check` of it fails on the build tree (already applied), which is fine.

- [ ] **Step 4: Build the SDK the release uses**

Run: `pwsh -File scripts/build-sdk.ps1 -NoDlss` (it re-prepares `out/sdk-build/rexglue-sdk` from 923c1a5 + every patch because a patch changed)
Expected: ends without errors and installs `rexruntime.dll` in `out/sdk-nodlss`.

- [ ] **Step 5: Commit**

```bash
git add sdk/patches/0061-vfs-overrides-file.patch sdk/patches/README.md
git commit -m "feat(sdk): GOW2_VFS_OVERRIDES_FILE lee la tabla de archivos servidos desde un archivo (parche 0061)"
```

### Task 2: Pure voice-swap planner

**Files:**
- Create: `src/gow2_voice_language.h`, `src/gow2_voice_language.cpp`
- Create: `tools/tests/voice_language_test.cpp`
- Modify: `CMakeLists.txt` (host test target next to `gow2_key_names_test`, and add it to the `foreach` that sets `target_include_directories(... PRIVATE src)`), `scripts/test-host.ps1` (`$targets` list and the test-name comment)

**Interfaces:**
- Produces (namespace `rex::glue::voice_language`, no SDK or OS includes):
  - `struct TocEntry { std::string size, uncompressed, third, path, md5; };`
  - `std::optional<TocEntry> ParseTocLine(std::string_view line);` — exactly five space-separated fields, else `std::nullopt`; `line` has no terminator.
  - `std::string FormatTocLine(const TocEntry& entry);` — the five fields joined by single spaces.
  - `std::string TextLanguageCode(uint32_t user_language);` — 1 `INT`, 5 `ESM`, 4 `FRA`, 3 `DEU`, 6 `ITA`, 11 `POL`, 12 `RUS`, 7 `KOR`, 8 `CHT`, anything else `INT`.
  - `std::string VoiceChoiceCode(std::string_view choice);` — `en` `INT`, `es` `ESM`, `fr` `FRA`, anything else (including `auto`) empty.
  - `struct TocFile { std::string name; std::string text; };` — `name` as on disc (`Xbox360TOC_ESM.txt`), `text` the whole file.
  - `struct FileSwap { std::string guest; std::string source; };` — `guest` = the TOC path without its leading `..\` (`GearGame\CookedXenon\SP_A_LOC_ESM.xxx`), `source` = the file name to serve instead, in the same folder, spelled as its own TOC spells it (`SP_A_LOC_int.xxx`).
  - `struct VoicePlan { std::vector<FileSwap> swaps; std::vector<TocFile> edited_tocs; std::string summary; };`
  - `VoicePlan PlanVoiceSwap(const std::vector<TocFile>& tocs, std::string_view text_code, std::string_view voice_code);`

Rules `PlanVoiceSwap` implements (from the spec): a voice package is a TOC entry whose file name matches `<B>_LOC_<L>.xxx` (case-insensitive) with `B` not equal to `GearGame`. The effective voice of the text language is `text_code` when some TOC lists a voice package with `L == text_code`, else `INT`. No swaps when `voice_code` is empty or equals the effective voice, or when no TOC lists a voice package with `L == voice_code` (summary says the voice set is missing). Otherwise, for each voice package `B_LOC_<voice_code>`: target = `B_LOC_<text_code>` if some TOC lists it, else `B_LOC_int` if some TOC lists it, else skip; skip when target is the source itself. Each swap replaces the target's line, in the TOC that lists it, with the source's size, uncompressed, third and md5 and the target's path. Edited TOCs keep every other line byte for byte and the file's line ends (`\r\n` when the input used them). Summary formats: `VOICES text <T>, voices <V>: <n> packages swapped`, `VOICES text <T>, voices same as text: nothing swapped`, `VOICES voice set <V> is not on this disc: nothing swapped`.

- [ ] **Step 1: Write the failing test `tools/tests/voice_language_test.cpp`**

Same style as `tools/tests/prompt_device_test.cpp` (`Expect`, `g_checks`, `g_failures`, same summary print and return code). Fixture: three TOCs with CRLF line ends —
`Xbox360TOC.txt`: `SP_A_LOC_int.xxx` (size 100, md5 aaa), `GUDS_X_SF_LOC_int.xxx` (110, bbb), `GearGame_LOC_int.xxx`, `GearGame_LOC_KOR.xxx`, `Startup_int.xxx`, `SP_OnlyInt_LOC_int.xxx`, one malformed line `garbage`;
`Xbox360TOC_ESM.txt`: `SP_A_LOC_ESM.xxx` (200, ccc), `GUDS_X_SF_LOC_ESM.xxx` (210, ddd), `GearGame_LOC_ESM.xxx`, `Startup_ESM.xxx`;
`Xbox360TOC_FRA.txt`: `SP_A_LOC_FRA.xxx` (300, eee).
Every path written `..\GearGame\CookedXenon\<name>`. Cases:
- `TextLanguageCode`: 1 `INT`, 5 `ESM`, 4 `FRA`, 3 `DEU`, 6 `ITA`, 11 `POL`, 12 `RUS`, 7 `KOR`, 8 `CHT`, 99 `INT`. `VoiceChoiceCode`: `auto` empty, `en` `INT`, `es` `ESM`, `fr` `FRA`, `xx` empty.
- `ParseTocLine("100 0 5 ..\\GearGame\\CookedXenon\\A.xxx aaa")` gives the five fields; `ParseTocLine("garbage")` and a six-field line give nullopt; `FormatTocLine(*ParseTocLine(s)) == s`.
- Text ESM, voices INT: swaps exactly `GearGame\CookedXenon\SP_A_LOC_ESM.xxx` from `SP_A_LOC_int.xxx` and `GearGame\CookedXenon\GUDS_X_SF_LOC_ESM.xxx` from `GUDS_X_SF_LOC_int.xxx`; one edited TOC `Xbox360TOC_ESM.txt` whose SP_A line is `100 <unc> <third> ..\GearGame\CookedXenon\SP_A_LOC_ESM.xxx aaa`, its GearGame and Startup lines unchanged, CRLF kept; summary `VOICES text ESM, voices INT: 2 packages swapped`.
- Text INT, voices ESM: swaps `SP_A_LOC_int.xxx` and `GUDS_X_SF_LOC_int.xxx` targets from the ESM files, edited `Xbox360TOC.txt`; `SP_OnlyInt_LOC_int.xxx` and `garbage` lines unchanged.
- Text DEU, voices ESM: same swaps as text INT (text-only languages fall back to int).
- Text FRA, voices ESM: two swaps — `SP_A_LOC_FRA.xxx` from `SP_A_LOC_ESM.xxx` (edits `Xbox360TOC_FRA.txt`) and `GUDS_X_SF_LOC_int.xxx` from `GUDS_X_SF_LOC_ESM.xxx` (no FRA package, so the game falls back to int; edits `Xbox360TOC.txt`).
- No swap: text DEU voices INT; text KOR voices empty; text ESM voices ESM; text ESM voices empty — each with summary `... voices same as text: nothing swapped` and no edited TOCs.
- Voice set missing: only the int TOC given, text INT, voices ESM: no swaps, summary `VOICES voice set ESM is not on this disc: nothing swapped`.
- Case: the same fixture with names written `sp_a_loc_INT.xxx` / `Xbox360TOC_esm.txt` still yields the text-ESM/voices-INT swaps, keeping each TOC's own spelling in `guest` and `source`.
- LF input: the int TOC with `\n` line ends stays `\n` after editing.

- [ ] **Step 2: Run it and see it fail**

Run: `cmake --build out/build/release --target gow2_voice_language_test` (from a shell set up the way `scripts/test-host.ps1` sets it up, or run `pwsh -File scripts/test-host.ps1`)
Expected: build error, `gow2_voice_language.h` not found.

- [ ] **Step 3: Implement `src/gow2_voice_language.h` / `.cpp` with the interfaces above**

- [ ] **Step 4: Run the host tests**

Run: `pwsh -File scripts/test-host.ps1`
Expected: `gow2_voice_language_test` prints 0 failures; the script ends with `All 5 steps passed.`

- [ ] **Step 5: Commit**

```bash
git add src/gow2_voice_language.h src/gow2_voice_language.cpp tools/tests/voice_language_test.cpp CMakeLists.txt scripts/test-host.ps1
git commit -m "feat(lang): planificador puro de voces: qué paquetes _LOC_ servir y sus líneas de TOC"
```

### Task 3: Options rows and boot glue

**Files:**
- Create: `src/gow2_voice_language_setup.h`, `src/gow2_voice_language_setup.cpp` (picked up by both builds: the root `GOW2_SOURCES` list in `CMakeLists.txt` needs the `.cpp` added; Android globs `src/*.cpp`)
- Modify: `src/gow2_app.h` (`OnPreSetup`, right after `PrepareCoalesced();`)
- Modify: `src/gow2_options_catalog.cpp` (two rows after `gow2_menu_language`), `tools/tests/options_catalog_test.cpp` (`system has 13 rows` becomes 15, plus checks below)
- Modify: `config/gow2.toml` (commented examples next to `gow2_menu_language`), `docs/PC_OPTIONS.md` (System table rows), `README.md`, `README.es.md` (settings list), `scripts/package.ps1` (QUICKSTART: one line per language on how to change voices and subtitles)

**Interfaces:**
- Consumes: Task 2's `TextLanguageCode`, `VoiceChoiceCode`, `PlanVoiceSwap`, `TocFile`, `VoicePlan`; Task 1's `GOW2_VFS_OVERRIDES_FILE`.
- Produces:
  - cvar `REXCVAR_DEFINE_STRING(gow2_voice_language, "auto", "UI", "Voice language: auto (same as subtitles), en, es or fr")` in `gow2_voice_language_setup.cpp`.
  - `struct VoiceLanguageSetup { std::filesystem::path overrides_file; std::string summary; bool text_missing = false; };`
  - `VoiceLanguageSetup rex::glue::PrepareVoiceLanguage(const std::filesystem::path& game_root, const std::filesystem::path& cache_dir, uint32_t user_language, std::string_view voice_choice);` — scans `game_root/GearGame` for `xbox360toc*.txt` case-insensitively and reads them; `text_missing` when no file in `game_root/GearGame/CookedXenon` matches `coalesced_<t>.bin` case-insensitively (t = lower-case text code); writes each edited TOC to `cache_dir/voice/<name>` and `cache_dir/voice/overrides.txt` (`guest=host` lines: TOC keys `GearGame\<toc name>` to the cache copy, package keys to `game_root/GearGame/CookedXenon/<source>`), deletes `cache_dir/voice/overrides.txt` when there is nothing to swap, returns an empty `overrides_file` then. Errors (unreadable TOC, write failure) give an empty `overrides_file` and a summary that says why.
  - In `gow2_app.h`, `void PrepareLanguage()` called from `OnPreSetup` right after `PrepareCoalesced();`: reads `user_language` with `std::strtoul(rex::cvar::GetFlagByName("user_language").c_str(), nullptr, 10)` and `rex::cvar::Query<std::string>("gow2_voice_language")`; when `text_missing`, `SetFlagByName("user_language", "1")` for this launch and `REXLOG_WARN`; sets `GOW2_VFS_OVERRIDES_FILE` to the file (Windows `_wputenv_s` with the wide path, elsewhere `setenv`) or to an empty value when there is none; logs the summary with `REXLOG_INFO`.
  - Rows: `user_language` "Subtitles and text" / "Subtítulos y textos", choices `C("1","English")`, `C("5","Español (Latinoamérica)")`, `C("4","Français")`, `C("3","Deutsch")`, `C("6","Italiano")`, `C("11","Polski")`, `C("12","Russian","Ruso")`, `C("7","Korean","Coreano")`, `C("8","Traditional Chinese","Chino tradicional")` (non-Latin names in Latin letters: the menu font is Latin-1 only); `gow2_voice_language` "Voices" / "Voces", choices `C("auto","Same as subtitles","Igual que los subtítulos")`, `C("en","English")`, `C("es","Español (Latinoamérica)")`, `C("fr","Français")`. Help texts say the voices exist in English, Latin American Spanish and French on the retail disc, and that both rows apply after a restart.

- [ ] **Step 1: Extend the failing catalog test**

In `tools/tests/options_catalog_test.cpp`: `CountTab(options::Tab::kSystem) == 15`; `Find("user_language")` and `Find("gow2_voice_language")` exist, are `Tab::kSystem`, `Apply::kRestart`, `kAllPlatforms`, have 9 and 4 choices, and their first choice values are `"1"` and `"auto"`. If `tools/tests/options_model_test.cpp` has a save case for an integer cvar (as `resolution_scale`), add the same for `user_language` = `5`, so the menu writes `user_language = 5` as an integer to `gow2.toml`.

- [ ] **Step 2: Run it and see it fail**

Run: `pwsh -File scripts/test-host.ps1`
Expected: `options_catalog_test` reports failures for the new checks.

- [ ] **Step 3: Add the rows, the setup files and `PrepareLanguage()` as specified above; add the docs lines**

- [ ] **Step 4: Run the host tests and build the game**

Run: `pwsh -File scripts/test-host.ps1` then `pwsh -File scripts/build.ps1 -Sdk C:\personal\tools\rexglue\0.10.0.9\win-amd64 -SkipCodegen`
Expected: `All 5 steps passed.`; build ends with `Done: ...\out\build\release\gow2.exe` and the last step says it used the self-built SDK DLLs from `out/sdk-nodlss`.

- [ ] **Step 5: Commit**

```bash
git add src/gow2_voice_language_setup.h src/gow2_voice_language_setup.cpp src/gow2_app.h src/gow2_options_catalog.cpp tools/tests/options_catalog_test.cpp CMakeLists.txt config/gow2.toml docs/PC_OPTIONS.md README.md README.es.md scripts/package.ps1
git commit -m "feat(lang): opciones de idioma de voces y de subtítulos en Opciones de PC"
```

### Task 4: End-to-end verification in the game

**Files:**
- Create: `docs/LANGUAGES.md` (what was measured: the table from the spec, the E2E results below, how to read the evidence in a log)

**Interfaces:**
- Consumes: the build from Task 3.

Every run goes through the idle gate (`out/idle-gate/wait-idle.ps1`, exit 75 = call again) with `out/idle-gate/guard-user.ps1` running, unless the owner says to run now. Use `scripts/chapter-sweep.ps1 -From 1 -To 1 -LoadSec 90 -PlaySec 60 -OutDir out\lang-e2e\<name> -Common "<args>"`; screenshots are taken by the script (PrintWindow).

- [ ] **Step 1: English voices + Spanish subtitles**

`-Common "--user_language=5 --gow2_voice_language=en"`
Expected: the log has `VOICES text ESM, voices INT: <n> packages swapped` with n > 300, `vfs overrides file: <n + edited TOCs> entries`, `vfs override: GearGame\CookedXenon\SP_Hospital_01_S_LOC_ESM.xxx -> ...SP_Hospital_01_S_LOC_int.xxx`; the chapter loads (no hang, no `DISCERR` box line other than the usual Coalesced one); a screenshot shows Spanish HUD or subtitle text.

- [ ] **Step 2: Spanish voices + English subtitles**

`-Common "--user_language=1 --gow2_voice_language=es"`
Expected: `VOICES text INT, voices ESM: <n> packages swapped`, an override line for `SP_Hospital_01_S_LOC_int.xxx -> ...SP_Hospital_01_S_LOC_ESM.xxx`, the chapter loads, English text on screen.

- [ ] **Step 3: Back to `auto`, and a non-ASCII cache folder**

`-Common "--user_language=5 --gow2_voice_language=auto --cache_root=<scratch>\caché-ñ"`
Expected: `VOICES text ESM, voices same as text: nothing swapped`, no `vfs overrides file` line, no `_LOC_` override lines; then once more with `--gow2_voice_language=en` and the same cache root: the `vfs overrides file` line appears with the `caché-ñ` path.

- [ ] **Step 4: Android build**

Run: `pwsh -File scripts/build-android.ps1 -LowPriority`
Expected: ends with `Done: ...app-debug.apk` and `check_android_libs: ... all checks passed` (the SDK tree is prepared again with 0061; the setup code compiles for Android).

- [ ] **Step 5: Write `docs/LANGUAGES.md` and commit**

```bash
git add docs/LANGUAGES.md
git commit -m "docs(lang): mediciones y verificación de idiomas de voces y subtítulos"
```
