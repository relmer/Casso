# Implementation Plan: Per-field CRT user overrides

**Branch**: `claude/theme-crt-defaults-handoff-f2c5b1` | **Date**: 2026-09-03 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/029-crt-user-overrides/spec.md`

## Summary

Replace the one-boolean-per-monitor CRT override with sparse per-field
overrides, keyed by the pair of monitor and color mode, resolved by a single
pure function that reports where each value came from.

The technical approach has three parts. A new leaf header carries the CRT value
vocabulary, moved out of `GlobalUserPrefs` so the prefs class does not own a
type it no longer stores. A pure `ResolveCrt` in a new core file replaces four
separate copies of the preset-then-theme layering chain, one in the renderer and
three in the settings bridge, and returns a per-field source alongside the
values. Persistence moves from a fixed four-element array under the JSON key
`crt` to a variable-length map under a new key `crtOverrides`, with a
shape-triggered conversion that is a pure function of the parsed document.

## Technical Context

**Language/Version**: C++ `stdcpplatest`, MSVC v145+ (Visual Studio 2026)

**Primary Dependencies**: In-repo only. `CassoCore` for `JsonValue`, `JsonParser`
and `JsonWriter`; `Dxui` for the settings widgets; Direct3D 11 for the CRT
post-process chain. No external libraries added.

**Storage**: `%LOCALAPPDATA%\Casso\UserPrefs.json`, a single JSON document
holding a `global` section and a `machines` section, written through
`UserConfigStore`. Writes are staged to a temporary file and committed with
`MoveFileExW (REPLACE_EXISTING | WRITE_THROUGH)`, so a half-written file is not
a failure mode this plan must handle.

**Testing**: Microsoft C++ Unit Test Framework, in the `UnitTest` project. The
relevant existing suites are `UiTests/CrtParameterMappingTests.cpp` (20 test
methods), `UiTests/GlobalUserPrefsTests.cpp` and `UiTests/UserConfigStoreTests.cpp`.

**Target Platform**: Windows 11 x64. ARM64 is build-only.

**Project Type**: Desktop application with a linked core library. `Casso.exe`
over `CassoCore`, `CassoEmuCore` and `Dxui`, all of which the `UnitTest` project
also links.

**Performance Goals**: The resolution path runs once per presented frame in
`EmulatorShell`, so it must stay allocation-free and must not touch the file
system. The existing post-process is a nine-pass chain, against which one lookup
in a small map is not measurable, but a per-frame string join or a machine JSON
re-parse would be.

**Constraints**: One `UserPrefs.json` is shared by several worktrees running
builds of different ages, so a file written by this version must survive being
read and rewritten by an older build. The conversion must not depend on
`$cassoGlobalPrefsVersion`, which nothing branches on today. Monitor identifiers
become durable user data and are frozen once shipped.

**Scale/Scope**: 256 change sites across ten areas, established by inventory
rather than estimate. 122 of those are references to `crtByMode` or
`kCrtModeCount` across 11 files, and 80 of the 122 are inside the two test
files, so the tests are a substantial part of the work rather than an addendum
to it. Two new source files, one new test file, 25 references to the moved value
type across 6 files.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### I. Code Quality (NON-NEGOTIABLE) — PASS

Ordinary compliance work, no structural tension. The specific traps this feature
walks into: the four preset aggregate initializers in `CrtPresets.h` name
`userOverride` positionally and their column alignment must survive the member's
removal; several functions in the settings bridge are being replaced wholesale
and their `////` banner comments must move with them; `MonitorCatalog.h` needs a
missing include added rather than continuing to rely on a transitive one.

### II. Testing Discipline — PASS, and this feature improves it

The conversion is specified as a pure function from a parsed document to a map,
with no theme access, no catalog access and no file system, which satisfies Test
Isolation without a mock. The resolver is likewise data-in, data-out. Both are
directly drivable from `UnitTest`.

The feature also closes existing coverage gaps rather than only avoiding new
ones. `monitorTilt` has no test at all today and is visibly malfunctioning. No
test references `MonitorCatalog`, so the frozen-identifier rule is currently
enforced by nothing.

### III. User Experience Consistency — PASS

No command-line surface changes. The Display page gains per-row source labels
and per-row reset controls, which is additive. One existing label is corrected:
the row reading `Monitor:` lists phosphors rather than monitors, and this feature
makes that mislabel worse by introducing a real monitor axis.

Backward compatibility is a settings-file concern here rather than a
command-line one, and is covered by FR-009 through FR-012.

### IV. Performance Requirements — PASS, with one design constraint recorded

Resolving a monitor identity from machine JSON costs a `PathResolver::FindFile`,
a file read and a JSON parse. That cannot run per frame. The design caches the
joined key strings and invalidates them only on machine switch and color-mode
change. Resolved values are deliberately not cached, because that would add
eleven more invalidation sources in the settings setters and a missed one shows
up as a slider that stops moving the picture.

### V. Simplicity & Maintainability — PASS, with one File Scope note

The feature is a net deletion in the settings bridge: `PromoteActiveToOverride`
and `ApplyActiveDefaults` both disappear, and the Display page's badge machinery
shrinks because a source lookup replaces eight float comparisons with epsilon
handling.

**File Scope**: two changes touch files this feature would not otherwise need.
Both are recorded in Complexity Tracking below rather than made silently.

### VI. Thin Executable, Testable Core (NON-NEGOTIABLE) — PASS, and this is the point

This feature exists partly because the current shape violates this principle.
The preset-then-theme layering chain is written four times. One copy is in
`CrtPostProcess.cpp`, which the `UnitTest` project compiles. Three copies are in
`Casso/Ui/Settings/SettingsDisplayCrtBridge.cpp`, which it does not. Those three
copies have already produced two shipped defects: a resize that changed
brightness, and a Restore Defaults that read the wrong tier, fixed on this
branch in `98d37eb2`.

The plan moves the rules into one core file the tests link, and reduces the
settings bridge to calling it. That is the Testability Litmus applied directly.

The same litmus applies to the override key, and applying it caught a violation
this plan originally contained. The key format `<configName>/<mode>` carries the
identifier freeze, the file's sort order and the per-monitor distinctness that
SC-002 asserts. Building it inline in `EmulatorShell.cpp` and
`SettingsDisplayCrtBridge.cpp`, neither of which the test project compiles,
would put a contract where no test can reach it. `MakeCrtOverrideKey` therefore
lives beside `ResolveCrt`, and the mode-token table moves with it.

One residual: `DisplayPage.cpp` is not in the `UnitTest` project today. The
inventory established that `HardwarePage.cpp` already is, under a four-line
project entry, and that adding `DisplayPage.cpp` pulls in no new dependency.
This plan adds it, so the badge strings and the row-to-field mapping are
covered. The parts that remain untestable are the widget layout and paint calls,
which is what the principle means by what cannot exist without the process.

**Result**: no gate fails. Two justified scope expansions are tracked below.

## Project Structure

### Documentation (this feature)

```text
specs/029-crt-user-overrides/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/
│   ├── prefs-json.md    # The on-disk shape and the conversion contract
│   └── resolver.md      # The resolution rules as a decision table
└── tasks.md             # Phase 2 output, /speckit-tasks
```

### Source Code (repository root)

```text
Casso/
├── Config/
│   ├── CrtTypes.h                     # NEW. CrtValues, CrtOverrides, CrtField, CrtSource
│   ├── CrtResolver.h                  # NEW. ResolveCrt declaration
│   ├── CrtResolver.cpp                # NEW. The one copy of the layering rules
│   ├── CrtPresets.h                   # Preset table, element type becomes CrtValues
│   ├── GlobalUserPrefs.h              # crtByMode and Crt removed, crtOverrides map added
│   ├── GlobalUserPrefs.cpp            # Field table, serialization, the conversion
│   ├── MonitorCatalog.h               # Missing include; frozen identifiers
│   └── UserConfigStore.cpp            # Legacy upgrade writes the converted document
├── CrtPostProcess.h / .cpp            # MakeCrtParams becomes a projection of ResolveCrt
├── EmulatorShell.h / .cpp             # Cached keys, two resolve call sites
└── Ui/Settings/
    ├── SettingsDisplayCrtBridge.h/.cpp  # Three chain copies collapse to one call each
    ├── SettingsApplyController.h/.cpp   # Fixed-arity baseline becomes a map snapshot
    ├── SettingsSheet.cpp                # Theme hook loses its write side
    └── DisplayPage.h / .cpp             # Per-row source labels and reset controls

UnitTest/
├── UnitTest.vcxproj                   # Adds CrtResolver.cpp, DisplayPage.cpp, two test files
└── UiTests/
    ├── CrtParameterMappingTests.cpp   # 10 methods survive, 9 rewritten, 1 deleted
    ├── CrtResolverTests.cpp           # NEW. The resolution matrix and provenance
    ├── MonitorCatalogTests.cpp        # NEW. Frozen identifiers
    ├── GlobalUserPrefsTests.cpp       # Conversion, round-trip, idempotence
    └── UserConfigStoreTests.cpp       # Legacy upgrade writes converted output
```

**Structure Decision**: The new files go in `Casso/Config/` beside
`GlobalUserPrefs.h` and `CrtPresets.h`, not in `CassoCore`. `Casso/Config/` is
already compiled into the `UnitTest` project (`GlobalUserPrefs.cpp` and
`UserConfigStore.cpp` are both project entries), so it satisfies the Testability
Litmus without a new library boundary. `CassoCore` is the emulator core and
holds no UI or preferences code, so putting CRT preferences there would be a
worse fit for no testability gain.

`CrtTypes.h` follows the existing `UiCommandTypes.h` precedent for a header that
carries a vocabulary rather than behavior. It includes `Pch.h` and nothing else,
so `GlobalUserPrefs.h` and `DisplayPage.h` can both take it without a cycle and
without either dragging in the other.

## Complexity Tracking

> Two changes touch files outside this feature's strict need. Both are recorded
> rather than made silently, per Principle V File Scope.

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|--------------------------------------|
| Fixing `monitorTilt`'s absence from `s_kKnownTopLevel` in `GlobalUserPrefs.cpp` | It is the exact defect this feature must avoid for its own new key, one level away in the same function. The key is parsed live and also captured as unknown, so every save emits one more stale copy. The user's live file currently holds twelve `monitorTilt` members. Leaving it means shipping a feature whose correctness argument is contradicted by the line above it. | Filing it separately was considered. Rejected because the new key's test ("every top-level key appears exactly once") covers both, and writing that test while a known violation sits beside it means either a failing test or a test written to avoid the truth. |
| Fixing the legacy-file upgrade in `UserConfigStore.cpp` to write the converted document | `MigrateLegacyFiles` keeps the raw parsed document and writes that, discarding what `FromJson` produced, while the sibling branch eight lines away writes `prefs.ToJson()`. Under a shape-triggered conversion this means a legacy upgrade writes a file that still needs converting. | Deferring it was considered. Rejected because it maximizes the window in which a file on disk carries the old key and not the new one, which is exactly the state the conversion is designed to be the last reader of. The change is correct under the current schema too, so it does not depend on this feature. |
| Adding a missing `#include "Core/JsonValue.h"` to `Casso/Config/MonitorCatalog.h` | The header names `JsonValue` at `:105` and `JsonType` at `:109` while including neither, and compiles today only because both current includers reach the definition first. This feature adds a new translation unit that includes it, so the latent break becomes a real one. | Leaving it and having the new test file include `Core/JsonValue.h` first was considered. Rejected because it propagates the fragility rather than fixing it, and the next includer hits the same wall with no clue why. |
| Renaming the `L"Monitor:"` label at `Casso/Ui/Settings/DisplayPage.cpp:240` | That row lists phosphors, not monitors. The mislabel is pre-existing and harmless while there is one monitor axis; this feature introduces a real one, so the row would name the wrong axis of a distinction the same page now depends on. | Leaving it was considered. Rejected because the feature makes an existing mislabel actively misleading, and the fix is one string. |

> A third, larger dependency is **not** this feature's to fix and is tracked as a
> sequencing constraint instead. `UserConfigStore::BuildCombinedJson` emits the
> `global` section from whichever prefs object it is handed, and three call sites
> hand it a default-constructed one. A conversion this feature performs can
> therefore be silently reverted to defaults. That fix is owned by a separate
> work item. Task ordering must place this feature's persistence work after it,
> or accept that a specific launch sequence discards the result.
