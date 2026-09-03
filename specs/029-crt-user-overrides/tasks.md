# Tasks: Per-field CRT user overrides

**Input**: Design documents from `/specs/029-crt-user-overrides/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/](contracts/)

**Tests**: Included and mandatory. The spec requires it (FR-013), the constitution requires it (Principle II and the Testability Litmus in VI), and the duplication this feature removes has already shipped two defects that tests would have caught.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to
- Every task gives exact file paths

## Path Conventions

Paths are repository-relative, per [plan.md](plan.md). Production code in `Casso/`, tests in `UnitTest/UiTests/`.

## Two things that differ from the template's assumptions

**Read these before planning around the phases below.**

**1. User Stories 1, 2 and 4 are not independently deliverable.** The template
asks each story to be a separate testable increment. Three of these cannot be,
and pretending otherwise would produce commits that build, pass the full suite,
and render the wrong picture. They share one cause: the storage shape. Deleting
the override flag, replacing the fixed array with the keyed map, and converting
existing files are the same edit seen from three angles. Splitting it leaves the
settings sliders writing to a model the renderer no longer reads.

This is not a decomposition failure. It was tested: three independent slicing
strategies were scored against a compile lens, a full-suite-green lens and a
dependency lens, and the "fewest viable commits" slice won precisely on this
argument. Phase 3 is therefore one increment serving three stories.

**2. Tasks are units of work, not commits.** Phase 3's tasks land as one commit,
because the tree is not green between them. Phases 1, 2, 4 and 5 commit per task
or per small group. Each phase says which it is.

---

## Phase 1: Setup (Independent Prerequisites)

**Purpose**: Three fixes that are correct on their own, are green in isolation, and remove hazards the rest of the work would otherwise inherit.

**Commits**: one per task. Each is independently landable and independently revertable.

- [ ] T001 Add `"monitorTilt"` to `s_kKnownTopLevel` in `Casso/Config/GlobalUserPrefs.cpp:38-67`, and cover it in `UnitTest/UiTests/GlobalUserPrefsTests.cpp` with three tests: a document holding two `monitorTilt` members loads and re-saves with exactly one, a load-save-load-save cycle is byte-identical, and a genuinely unknown top-level key still round-trips. Assert on the reparsed saved document, never a substring search.
- [ ] T002 [P] Create `UnitTest/UiTests/MonitorCatalogTests.cpp` and add its `<ClCompile>` entry to `UnitTest/UnitTest.vcxproj`, pinning the frozen identifiers `AppleMonitorII` and `AppleMonitorIIc`, that `Default()` is the first entry, and that `ByName` on an unknown name returns `Default()`. Assert presence of the two names, never an exact catalog size, so adding a third monitor is a decision rather than a red build.
- [ ] T003 [P] Fix `UserConfigStore::MigrateLegacyFiles` in `Casso/Config/UserConfigStore.cpp:1542` to assign `prefs.ToJson()` rather than the raw parsed document, matching its sibling branch at `:1550`. Cover in `UnitTest/UiTests/UserConfigStoreTests.cpp`: the global section of a unified file produced by a legacy upgrade equals `prefs.ToJson()` of what the upgrade loaded, and an unknown top-level key still survives the upgrade.

**Checkpoint**: Three latent defects closed. None of the remaining work depends on these landing, but all of it is safer with them in.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The value vocabulary and the pure resolver, added alongside the existing code and fully tested, before anything is deleted.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

**Commits**: one per task. Nothing here changes behavior, because nothing calls the new resolver yet. This is deliberate: it puts the rules under test before the swap that depends on them.

- [ ] T004 Add `#include <optional>` to `Casso/Pch.h` between `<numbers>` and `<set>`. No `#include <optional>` exists anywhere in the tree and current `std::optional` users compile on a transitive pull, which is not a dependency to make load-bearing. `UnitTest/Pch.h` includes `Casso/Pch.h`, so this one edit covers both projects.
- [ ] T005 Create `Casso/Config/CrtTypes.h` declaring `CrtValues` (the eleven fields per [data-model.md](data-model.md), no override flag), `CrtOverrides` (the same eleven as `std::optional`, plus `IsEmpty()` and a defaulted `operator==`), `enum class CrtField : uint8_t` with eleven values and `Count`, and `enum class CrtSource : uint8_t { Preset, Theme, User }`. Include `Pch.h` and nothing else, so `GlobalUserPrefs.h` and `DisplayPage.h` can both take it without a cycle. The defaulted equality is required rather than decorative: T016's `map != baseline` does not compile without it.
- [ ] T006 Create `Casso/Config/CrtResolver.h` and `.cpp` implementing `ResolveCrt (preset, themeDefaults, overrides)` returning `CrtResolved`, and `MakeCrtOverrideKey (monitorConfigName, mode)` with the mode-token table beside it, per [contracts/resolver.md](contracts/resolver.md). Add both files to `Casso/Casso.vcxproj` and the `.cpp` to `UnitTest/UnitTest.vcxproj`. Add `Touch`, `Clear (CrtField)` and `ClearAll` to `CrtOverrides`. Take the preset by value, never a mode index, and take no geometry. The key builder lives here rather than at its call sites because the format is a contract carrying the identifier freeze, the sort order and per-monitor distinctness, and neither call site is compiled by the test project.
- [ ] T007 Create `UnitTest/UiTests/CrtResolverTests.cpp` and add its project entry, covering every case in the Required Coverage section of [contracts/resolver.md](contracts/resolver.md): the four-row matrix for each of the nine fields with a theme group, the two-row matrix for gamma and persistence with an assertion that `Theme` never appears for either, a theme change leaving user fields intact, `Clear` falling back theme-then-preset, `Touch` flipping provenance for one field only, and an override equal to the resolved default still reporting `User`. Also cover `MakeCrtOverrideKey`: every monitor and mode combination yields a distinct key (this is what makes SC-002 automatable), the shipped identifiers appear exactly as the catalog spells them, and sorted order is amber, color, green, white within a monitor rather than `SettingsColorMode` order.

**Checkpoint**: The resolution rules exist in one place, are compiled by the test project, and are covered. The tree still behaves exactly as before.

---

## Phase 3: User Stories 1, 2 and 4 (Priority: P1) 🎯 MVP

**Goal**: A tweak becomes one remembered field, keyed by monitor and mode, and existing files convert without visible change.

**Independent Test**: Change one value on the Display page, switch themes, confirm the changed value survived and the other ten followed the new theme. Then switch to a machine with a different monitor and confirm it shows its own defaults.

**Why three stories in one phase**: see the note at the top. The storage shape is their common cause and cannot be split without a misleading green.

**Commits**: T008 through T017 land as ONE commit. The tree does not build between them. This is the unavoidably large step, and splitting it would produce commits that pass the suite while the picture is wrong.

- [ ] T008 [US1] Change the preset table element type in `Casso/Config/CrtPresets.h` from `GlobalUserPrefs::Crt` to `CrtValues` and remove the `/* userOverride */ false` row from all four aggregate initializers at `:68`, `:83`, `:98` and `:113`. They name the member positionally, so this breaks in the same commit. Preserve column alignment. Update the layering prose at `:21-23`.
- [ ] T009 [US1] In `Casso/Config/GlobalUserPrefs.h`, delete the nested `struct Crt`, `crtByMode[kCrtModeCount]` and `kCrtModeCount`, and add `std::map<std::string, CrtOverrides> crtOverrides`. Include `CrtTypes.h`. Update the two static helper declarations at `:249` and `:260`. Correct the stale `colorBleedWidth` comment at `:132`, which says output pixels where commit `967f02f2` made it emulated pixels.
- [ ] T010 [US2] Rewrite the CRT serialization in `Casso/Config/GlobalUserPrefs.cpp` per [contracts/prefs-json.md](contracts/prefs-json.md): emit `crtOverrides` sparsely with sorted keys and reuse the existing group structure, emit the object even when the map is empty, add `"crtOverrides"` to `s_kKnownTopLevel`, keep `"crt"` in that set as consumed-but-not-emitted following the `printerAudioMuted` precedent at `:61`, and stop emitting the `crt` block at `:969`. Clamp each present field on read as `CrtModeFromJson` does today.
- [ ] T011 [US4] Implement the conversion in `Casso/Config/GlobalUserPrefs.cpp` as a pure function of the parsed document, per [contracts/prefs-json.md](contracts/prefs-json.md). Trigger on shape, never on the version stamp. Do not use `HasObject` for the absence half, because it is type-checked and a hand-edited `null` or `[]` reads as absent and re-fires over live data. Fan each flag-set block onto `AppleMonitorII/<mode>` and `AppleMonitorIIc/<mode>` as hardcoded string literals, never read from `s_kMonitors`. Leave `s_kCurrentVersion` at 1 and comment that the stamp is reserved for changes of meaning.
- [ ] T012 [US1] Rewrite `MakeCrtParams` in `Casso/CrtPostProcess.h` and `.cpp` to take a `CrtResolved` and add geometry, removing the layering copy at `:103-125` and the `!prefsCrt.userOverride` gate at `:103`. Update the doc block at `CrtPostProcess.h:79-106`, which describes the flag.
- [ ] T013 [US2] In `Casso/EmulatorShell.h` and `.cpp`, cache the four keys built by `MakeCrtOverrideKey` as a `std::array<std::string, 4>`, never assembling the string inline, rather than one key, which removes the color-mode invalidation trigger entirely and keeps the map lookup allocation-free. Seed at `ApplyPersistedChromePrefs` (`:3051`) and at the `WM_APP_DXUI_UPDATE_TITLE` handler (`:7138`), NOT at `LoadDeskSceneModelsForMachine`, which only runs when a machine switch crosses the //c boundary. Never refresh on the CPU thread in `SwitchMachine`. At both resolve sites (`:7273` and `:12409`) load the color mode once into a local, because a preemption between the two current loads could pair one mode's overrides with another mode's preset.
- [ ] T014 [US1] In `Casso/Ui/Settings/SettingsDisplayCrtBridge.h` and `.cpp`, replace the three layering copies with one `ResolveCrt` call each in `ReseedFromActiveMode` (`:127`) and `PublishDefaultsHint` (`:217`), delete `ApplyActiveDefaults` (`:394`) and `PromoteActiveToOverride` (`:341`), and rewrite the eleven setter lambdas to call `Touch` for exactly one field with no seeding, and obtain the key from `MakeCrtOverrideKey` rather than building it here. Restore Defaults becomes `ClearAll` plus the existing color-monitor text reset at `:569-585`. Move each `////` banner comment with the function it documents.
- [ ] T015 [US1] In `Casso/Ui/Settings/SettingsApplyController.h` and `.cpp`, replace `m_baselineCrt[kCrtModeCount]` (`.h:99`) with a whole-map snapshot taken at Show, assign it back on Cancel, and make dirty detection `map != baseline` so presence of an override counts as a change. Replaces the four index loops at `:59-61`, `:306-309`, `:347-349` and `:436-438`.
- [ ] T016 [US1] In `Casso/Ui/Settings/SettingsSheet.cpp`, remove the write side from the chrome-theme-changed hook at `:224-228`, leaving reseed and publish only. This also fixes a live ordering bug where Cancel restores CRT state and then re-applies the theme, whose hook currently clears every flag.
- [ ] T017 [P] [US1] Rewrite `UnitTest/UiTests/CrtParameterMappingTests.cpp`. Ten methods survive untouched, every `ComputeCrtPixelScale`, `ComputeCrtPictureUvRect` and `ComputeLetterboxRect` test at `:345-466` and `:483-611`. Nine need rewriting for the new signature. Delete `MakeCrtParams_ThemeOverride_OnlyAppliesWhenUserHasNoOverride` at `:158-217`, whose Case 2 asserts the behavior this feature retires. Remove the unused `InMemoryFileSystem` at `:34` and its include. Add coverage for the non-positive output guard at `CrtPostProcess.cpp:153-154`, which has none and whose signature is changing anyway.
- [ ] T018 [P] [US4] Add the conversion and persistence tests to `UnitTest/UiTests/GlobalUserPrefsTests.cpp` per the Required Coverage section of [contracts/prefs-json.md](contracts/prefs-json.md). Note that map order is amber, color, green, white, not `SettingsColorMode` order, so any expected-order assertion written from the enum will be wrong. Write the downgrade round trip as a hand-built JSON literal holding both keys rather than by simulating an older binary. Rewrite the existing `crtByMode` assertions at `:52`, `:83-91`, `:117-125` and `:212-213`.

**Checkpoint**: User Stories 1, 2 and 4 are all functional. The picture is correct, existing files convert, and per-monitor separation works. The Display page still reports provenance the old way, by comparing values.

---

## Phase 4: User Story 3 - The page says where a value came from (Priority: P2)

**Goal**: Every Display row states its source and can be reset on its own.

**Independent Test**: Edit some rows and not others, confirm each row's stated source matches where its value actually came from, reset one row and confirm only that row changed.

**Commits**: one per task. Each is independently landable on top of Phase 3.

- [ ] T019 [US3] Add `DisplayPage.cpp` to `UnitTest/UnitTest.vcxproj` under the same four-line entry shape as `HardwarePage.cpp` at `:490`. This pulls in nothing new: `SettingsPanelState.cpp` and `ColorUtil.cpp` are already project entries, `CassoTheme.h` is header-only, and `Dxui` is already linked. Expose the row-to-field mapping and the badge strings as class statics so they are reachable.
- [ ] T020 [US3] In `Casso/Ui/Settings/DisplayPage.cpp`, replace the value-comparison badge with a source lookup. This DELETES code: the early return at `:850-853`, the `FloatMatches` lambda at `:868-871`, `s_kFloatEpsilon` at `:810`, eight float comparisons with their `/100.0f` rescaling, three bool comparisons, and `DisplayDefaultsHint::values` with all five `*FromTheme` bools at `DisplayPage.h:63-68`. Three labels rather than two: monitor default, theme default, custom. Gamma and persistence must never show a theme default, because no theme group carries them.
- [ ] T021 [US3] Add eleven per-row reset controls to `Casso/Ui/Settings/DisplayPage.cpp`. They fit in the 28 DIP gap that already exists between each slider's right edge and the badge column at `:400`; placing them anywhere else forces a sheet widening. Add a SECOND control-id enum rather than extending `kControl*` at `DisplayPage.h:97-105`, which is pinned by a direct cast to `SettingsPreviewController::Focus`. Add each button explicitly to `DisplayPage::Paint` at `:877-991`, because this page hand-paints its children and never calls `DxuiPanel::Paint`, so the buttons would otherwise be invisible while their clicks worked. Add eleven `SetAccessibleName` calls following `DiskPage.cpp:97-101`, since no control on this page has one today and a bare glyph button announces as an unnamed Button. Rebuild the focus ring when override state changes, because `CollectFocusables` prunes disabled subtrees.
- [ ] T022 [P] [US3] Create `UnitTest/UiTests/DisplayPageTests.cpp` and add its project entry, covering the row-to-field mapping and the three badge strings for every field, including that gamma and persistence never map to the theme-default string.
- [ ] T023 [P] [US3] Rename the `L"Monitor:"` label at `Casso/Ui/Settings/DisplayPage.cpp:240`. That row lists phosphors, and this feature introduces a real monitor axis that makes the existing mislabel actively wrong.

**Checkpoint**: All four user stories functional. Never decide whether an override exists by reading a value back out of a widget: the color bleed slider has a floor of 1.0 while three presets carry 0.0, so a round trip through the widget returns a different number.

---

## Phase 5: Polish & Cross-Cutting Concerns

- [ ] T024 [P] Correct the README sentence at `README.md:284`, which promises that tweaks persist on top of presets and themes. It becomes true in Phase 3 and should be checked rather than assumed.
- [ ] T025 [P] Add a precedence paragraph to `docs/themes/AUTHORING.md:22`, which calls `crtDefaults` "preferred CRT post-processing presets" and states no precedence at all.
- [ ] T026 Add the `CHANGELOG.md` entry under `[Unreleased]`. User-visible, so required. One entry covering the net effect, not the path taken through the phases. Do not add entries for the spec and plan commits, which are `docs`.
- [ ] T027 [P] Confirm no stale prose survives the deletions. The layering description at `Casso/Config/CrtPresets.h:21-23` is rewritten in T008 and the comment at `Casso/Config/GlobalUserPrefs.h:118-120` is deleted with the members it documents in T009, so this task verifies rather than edits. Note that `specs/007-ui-overhaul/contracts/global-user-prefs.schema.json` is two generations stale already and is a historical artifact, not an edit site.
- [ ] T028 Run every manual scenario in [quickstart.md](quickstart.md), especially scenario 4 against a real prefs file. Launch in the background, minimized, without activation, and select the process by path rather than window title.
- [ ] T029 Run the full gate: `scripts\Build.ps1 -Configuration Debug -Platform x64`, then `scripts\RunTests.ps1 -Configuration Debug -Platform x64`, confirming `x64\Debug\UnitTest.dll` is newer than the build before trusting the result. Repeat for Release, which is where an incremental build after a vtable change surfaces stale-object failures. Constitution Quality Gates require both before a merge.
- [ ] T030 Run `scripts\CheckStyle.ps1 -Against origin/master -Revision HEAD`. CI runs the checker in tree mode on every master push and this feature rewrites several function banner comments, which is exactly what it polices.
- [ ] T031 Watch the CI run after the merge to master. Code analysis uses settings that are not reproducible locally, so it is a gate to observe rather than one to pre-clear, and a local run must never be reported as having passed it.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies. All three tasks are independent of each other and of everything below.
- **Phase 2 (Foundational)**: T004 before T005 before T006 before T007. Does not depend on Phase 1.
- **Phase 3 (US1, US2, US4)**: Requires Phase 2. One commit.
- **Phase 4 (US3)**: Requires Phase 3.
- **Phase 5 (Polish)**: Requires Phase 4, except T024 and T025 which only require Phase 3.

### The external dependency

`UserConfigStore::BuildCombinedJson` emits the global section from whichever
prefs object it is handed, and three call sites hand it a default-constructed
one, so a conversion this feature performs can be silently reverted to defaults.
**That fix is owned by a separate work item and is not in this task list.**

Phase 3 should land after it. If it has not landed by then, Phase 3 may still
proceed, because the exposure predates this feature and is not made worse by it,
but T028's scenario 4 will be unreliable until it lands. Do not fix it here; two
sessions editing `UserConfigStore.cpp` is worse than waiting.

### Within Phase 3

T008 and T009 are the type changes and come first. T010 and T011 are
persistence. T012 and T013 are the render path. T014 through T016 are the
settings UI. T017 and T018 are the tests and are marked [P] because they are
different files, but all of it is one commit and nothing compiles until every
item is done.

### Parallel Opportunities

- T002 and T003 run in parallel with each other and with T001.
- T017 and T018 are different files and can be written in parallel within Phase 3.
- T022 and T023 are independent of T020 and T021 once T019 lands.
- T024, T025 and T027 are three different files and run in parallel.

---

## Implementation Strategy

### MVP

Phase 1, Phase 2, Phase 3. That delivers three of the four user stories and
makes the README sentence true. The Display page still infers provenance by
comparing values, which is wrong in the case where a user sets a value equal to
a default, but it is no worse than today.

**Stop and validate here.** Run quickstart scenarios 1, 2, 4 and 5 before
starting Phase 4.

### Incremental Delivery

1. Phase 1 → three latent defects closed, each independently revertable
2. Phase 2 → the rules under test, no behavior change
3. Phase 3 → US1, US2 and US4 (MVP)
4. Phase 4 → US3
5. Phase 5 → docs, changelog, manual validation, style gate

### Not a parallel-team feature

Phase 3 is one commit touching eleven files across persistence, rendering and
settings. Splitting it across people means splitting a commit that does not
build in the middle. Phases 1 and 2 can be staffed separately from each other.

---

## Notes

- Every commit that lands on master must build and pass the full suite. Phase 3
  satisfies this as a whole and not between its tasks, which is why it is one
  commit.
- `RunTests.ps1` does not build. Check `x64\Debug\UnitTest.dll` is newer than
  your build before trusting a green run.
- Commit subjects are a short label or an imperative phrase, parentheses
  optional, scope always required. No Claude attribution.
- `.specify/feature.json` and the `CLAUDE.md` speckit block are per-checkout
  state. Keep both out of every commit and beware `git add -A`.
