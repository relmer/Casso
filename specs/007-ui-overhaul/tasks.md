# Tasks: 007 UI Overhaul (Native Reset)

**Input**: `spec.md`, `plan.md`, `research.md`, `data-model.md`, `contracts/`, `quickstart.md`  
**Feature Dir**: `C:\Users\relmer\repos\relmer\Casso\specs\007-ui-overhaul`  
**Reset Note**: This redo pass revalidates scope and resets completion state; all checkboxes are intentionally unchecked until re-verified in code/tests.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Finalize reset bootstrap and explicitly retire remaining legacy dialog entry points.

- [X] T001 Retire remaining Win32 settings-dialog entry points for FR-027 in C:\Users\relmer\repos\relmer\Casso\Casso\EmulatorShell.cpp
- [X] T002 Remove legacy startup machine-picker dialog path for FR-027 in C:\Users\relmer\repos\relmer\Casso\Casso\Main.cpp
- [X] T003 Update theme metadata contract details for family/variant + drive profile in C:\Users\relmer\repos\relmer\Casso\specs\007-ui-overhaul\contracts\theme-metadata.schema.json
- [X] T004 Update native ThemeManager contract for family/variant behavior in C:\Users\relmer\repos\relmer\Casso\specs\007-ui-overhaul\contracts\theme-manager.h
- [X] T005 Create runtime screenshot validation matrix checklist in C:\Users\relmer\repos\relmer\Casso\specs\007-ui-overhaul\quickstart.md

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Remove legacy Rml-era ownership/build paths and establish native-only baseline before story work.

**⚠️ CRITICAL**: No user story work starts until this phase is complete.

- [X] T006 Implement native-only UI ownership bootstrap and routing in C:\Users\relmer\repos\relmer\Casso\Casso\EmulatorShell.cpp
- [X] T007 Remove Rml project wiring from solution in C:\Users\relmer\repos\relmer\Casso\Casso.sln
- [X] T008 Remove Rml includes/compile units/project references from app build in C:\Users\relmer\repos\relmer\Casso\Casso\Casso.vcxproj
- [X] T009 Remove Rml includes/compile units/project references from tests build in C:\Users\relmer\repos\relmer\Casso\UnitTest\UnitTest.vcxproj
- [X] T010 Delete obsolete Rml runtime files in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\RmlBackend_D3D11.cpp
- [X] T011 [P] Delete obsolete Rml runtime files in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\RmlInputBridge.cpp
- [X] T012 [P] Delete obsolete Rml runtime files in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\RmlSystemInterface.cpp
- [X] T013 [P] Delete obsolete Rml runtime files in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\UiShell.cpp
- [X] T014 [P] Delete obsolete Rml-era UI tests in C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\RmlBackendSmokeTests.cpp
- [X] T015 Add UT isolation guards for registry/filesystem/environment access in C:\Users\relmer\repos\relmer\Casso\UnitTest\ModuleSetup.cpp

**Checkpoint**: Native-only ownership/build baseline is in place and legacy runtime/build paths are removed.

---

## Phase 3: User Story 1 - Change emulation speed from unified settings (Priority: P1) 🎯 MVP

**Goal**: Machine selector drives immediate settings-state swap and per-machine speed persistence in one panel.

**Independent Test**: Open settings with 2+ machines, switch machines, verify controls update within one frame and speed changes stay machine-specific.

- [X] T016 [P] [US1] Add machine-switch + speed persistence unit coverage in C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\SettingsPanelStateTests.cpp
- [X] T017 [P] [US1] Add user-config speed shadow/fallthrough coverage in C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\UserConfigStoreTests.cpp
- [X] T018 [US1] Implement machine-selector state swap logic in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\SettingsPanelState.cpp
- [X] T019 [US1] Implement settings UI refresh-on-machine-change wiring in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\SettingsPanel.cpp
- [X] T020 [US1] Implement per-machine speed apply/commit path in C:\Users\relmer\repos\relmer\Casso\Casso\Config\UserConfigStore.cpp
- [X] T021 [US1] Integrate speed apply path with runtime shell command dispatch in C:\Users\relmer\repos\relmer\Casso\Casso\EmulatorShell.cpp
- [X] T022 [US1] Verify machine/scoped speed requirements and test notes in C:\Users\relmer\repos\relmer\Casso\specs\007-ui-overhaul\quickstart.md

**Checkpoint**: US1 is independently functional and testable.

---

## Phase 4: User Story 2 - Enable/disable hardware components per machine (Priority: P1)

**Goal**: Hardware tree capability flags and enable-state behavior are machine-scoped and reset-safe.

**Independent Test**: Disable Disk II controller via settings tree, apply+reset to verify removal; re-enable and verify restoration.

- [X] T023 [P] [US2] Add hardware capability flag extraction tests in C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\HardwareTreeTests.cpp
- [X] T024 [P] [US2] Add hardware enable/disable persistence tests in C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\UserConfigStoreTests.cpp
- [X] T025 [US2] Implement capability-flag mapping and lock-reason handling in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\SettingsPanelState.cpp
- [X] T026 [US2] Implement hardware tree checkbox interactivity + tooltip behavior in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\SettingsPanel.cpp
- [X] T027 [US2] Implement component enable-state delta persistence in C:\Users\relmer\repos\relmer\Casso\Casso\Config\UserConfigStore.cpp
- [X] T028 [US2] Integrate component-apply reset prompt + command dispatch in C:\Users\relmer\repos\relmer\Casso\Casso\EmulatorShell.cpp
- [X] T029 [US2] Align machine user config contract notes for component toggles in C:\Users\relmer\repos\relmer\Casso\specs\007-ui-overhaul\contracts\machine-user-config.schema.json

**Checkpoint**: US2 is independently functional and testable.

---

## Phase 5: User Story 3 - Insert/eject disk with realistic drive widgets (Priority: P1)

**Goal**: Drive widgets read as Disk ][ hardware and keep door animation + sound synchronized.

**Independent Test**: Drag valid image to each drive and verify door-close, present LED, label, spin while active, and door-open on eject with synced audio.

- [X] T030 [P] [US3] Add drive state-transition tests for insert/eject/open/close in C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\DriveWidgetStateTests.cpp
- [X] T031 [P] [US3] Add timing-bound sync tests for drive animation/audio events in C:\Users\relmer\repos\relmer\Casso\UnitTest\Audio\DiskIIAudioSourceEventSinkTests.cpp
- [X] T032 [US3] Implement Disk ][ geometry/state visual model in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\DriveWidgetElement.cpp
- [X] T033 [US3] Implement door-open/door-close animation state machine in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\DriveWidgetState.h
- [X] T034 [US3] Implement insert/eject action orchestration + optimistic UI updates in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\DriveWidgetController.cpp
- [X] T035 [US3] Implement shared drive sync-event publish/consume path in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\DriveWidgetController.h
- [X] T036 [US3] Integrate drive-door sync timeline with floppy audio playback in C:\Users\relmer\repos\relmer\Casso\Casso\WasapiAudio.cpp
- [X] T037 [US3] Add visual acceptance checklist for Disk ][ fidelity + sync in C:\Users\relmer\repos\relmer\Casso\specs\007-ui-overhaul\quickstart.md

**Checkpoint**: US3 is independently functional and testable.

---

## Phase 6: User Story 4 - Apply themes at runtime with Apple variant coverage (Priority: P2)

**Goal**: Theme hot-swap remains immediate while shipping Apple II/II+/IIe//c variants and future family extensibility.

**Independent Test**: Switch among built-in themes and Apple variants at runtime; verify full chrome updates within one frame and persists across restart.

- [X] T038 [P] [US4] Add theme discovery/activation regression coverage for familyId+variantId in C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\ThemeManagerTests.cpp
- [X] T039 [P] [US4] Add schema validation tests for Apple + non-Apple families in C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\ThemeLoaderTests.cpp
- [X] T040 [US4] Implement family/variant activation + persistence logic in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\ThemeManager.cpp
- [X] T041 [US4] Implement theme token ingestion for runtime hot-swap in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\ThemeLoader.cpp
- [X] T042 [US4] Ship Apple II variant metadata/assets update in C:\Users\relmer\repos\relmer\Casso\Resources\Themes\Skeuomorphic\theme.json
- [X] T043 [US4] Ship Apple II+ and Apple IIe variant metadata/assets update in C:\Users\relmer\repos\relmer\Casso\Resources\Themes\DarkModern\theme.json
- [X] T044 [US4] Ship Apple //c distinct palette + distinct drive style metadata in C:\Users\relmer\repos\relmer\Casso\Resources\Themes\RetroTerminal\theme.json

**Checkpoint**: US4 is independently functional and testable.

---

## Phase 7: User Story 5 - Interact with native title bar and nav layer (Priority: P2)

**Goal**: Native title/nav is sole runtime owner with Windows-system-identical title/nav font behavior and NC parity.

**Independent Test**: Verify drag, fullscreen toggle, min/max/close, and menu open/dismiss behavior with borderless NC hit-testing parity.

- [X] T045 [P] [US5] Add NC hit-test parity tests for title/nav interactions in C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\TitleBarHitTestTests.cpp
- [X] T046 [P] [US5] Add title/nav layout + font policy tests in C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\TitleBarLayoutTests.cpp
- [X] T047 [P] [US5] Add nav command parity tests against command map in C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\NavLayerTraceabilityTests.cpp
- [X] T048 [US5] Implement native title bar rendering + system button states in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\TitleBar.cpp
- [X] T049 [US5] Implement WM_NCHITTEST rect mapping + drag/resize/fullscreen behavior in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\TitleBarHitTest.cpp
- [X] T050 [US5] Enforce Windows-system-identical font selection for title/nav text in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\TitleBar.h
- [X] T051 [US5] Implement D3D nav layer command routing parity in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\NavLayer.cpp
- [X] T052 [US5] Remove remaining Win32 menu fallback dispatch from C:\Users\relmer\repos\relmer\Casso\Casso\MenuSystem.cpp

**Checkpoint**: US5 is independently functional and testable.

---

## Phase 8: User Story 6 - Preserve per-machine JSON settings across upgrades (Priority: P3)

**Goal**: User JSON migration is silent/lossless and canonicalizes version fields.

**Independent Test**: Load legacy and canonical versioned `_user.json`, verify migration rewrite + merged config behavior without losing overrides.

- [ ] T053 [P] [US6] Add canonical-vs-legacy version field migration tests in C:\Users\relmer\repos\relmer\Casso\UnitTest\EmuTests\MachineConfigUpgradeTests.cpp
- [ ] T054 [P] [US6] Add user/default merge fallback tests for new fields in C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\UserConfigStoreTests.cpp
- [ ] T055 [US6] Implement `$cassoMachineVersion` canonicalization and rewrite logic in C:\Users\relmer\repos\relmer\Casso\Casso\Config\UserConfigStore.cpp
- [ ] T056 [US6] Implement migration-time `$cassoDefault` alias read behavior in C:\Users\relmer\repos\relmer\Casso\Casso\Config\UserConfigStore.h
- [ ] T057 [US6] Implement merge/fallthrough preservation for new settings fields in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\SettingsPanelState.cpp
- [ ] T058 [US6] Align migration guarantees in machine user contract notes in C:\Users\relmer\repos\relmer\Casso\specs\007-ui-overhaul\contracts\machine-user-config.schema.json

**Checkpoint**: US6 is independently functional and testable.

---

## Phase 9: Polish & Cross-Cutting Concerns

**Purpose**: Final teardown verification, matrix validation, and cross-story hardening.

- [ ] T059 [P] Remove obsolete Rml-era theme markup/runtime assets in C:\Users\relmer\repos\relmer\Casso\Resources\Themes\Skeuomorphic\*.rml
- [ ] T060 [P] Remove obsolete Rml-era theme style sheets in C:\Users\relmer\repos\relmer\Casso\Resources\Themes\Skeuomorphic\*.rcss
- [ ] T061 Execute UT isolation audit (no real registry/filesystem state) in C:\Users\relmer\repos\relmer\Casso\UnitTest\EmuTests\RegistryTests.cpp
- [ ] T062 [P] Add/refresh runtime screenshot capture matrix instructions in C:\Users\relmer\repos\relmer\Casso\specs\007-ui-overhaul\quickstart.md
- [ ] T063 Capture screenshot matrix for startup/menu/NC/settings/drive states in C:\Users\relmer\repos\relmer\Casso\TestResults\007-ui-overhaul\
- [ ] T064 Validate screenshot matrix against acceptance criteria in C:\Users\relmer\repos\relmer\Casso\specs\007-ui-overhaul\spec.md
- [ ] T065 Implement CRT brightness/effect controls in settings UI for FR-038/039 in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\SettingsPanel.cpp
- [ ] T066 Implement CRT toggle and parameter renderer wiring for FR-039/040 in C:\Users\relmer\repos\relmer\Casso\Casso\CrtPostProcess.cpp
- [ ] T067 Implement CRT defaults + user-override persistence for FR-040 in C:\Users\relmer\repos\relmer\Casso\Casso\Config\GlobalUserPrefs.cpp
- [ ] T068 Implement full keyboard navigation/focus-visible behavior for FR-044 in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\SettingsPanel.cpp
- [ ] T069 Implement mounted-image persistence and auto-remount behavior for FR-047 in C:\Users\relmer\repos\relmer\Casso\Casso\Config\UserConfigStore.cpp
- [ ] T070 Implement per-monitor window placement persistence for FR-048 in C:\Users\relmer\repos\relmer\Casso\Casso\Config\GlobalUserPrefs.cpp
- [ ] T071 Add runtime theme-discovery, malformed-theme exclusion, and re-extract coverage for FR-035/036/037 in C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\ThemeManagerTests.cpp
- [ ] T072 Add three-consecutive-upgrade migration matrix coverage for SC-003 in C:\Users\relmer\repos\relmer\Casso\UnitTest\EmuTests\MachineConfigUpgradeTests.cpp
- [X] T073 Execute required Code Analysis gate in C:\Users\relmer\repos\relmer\Casso\scripts\Build.ps1
- [ ] T074 Capture and validate SC-002 evidence on integrated Intel/AMD iGPU matrix (1280x960 + 1920x1080 at 100% + 150% scale), requiring first post-switch frame with zero mixed-theme regions in C:\Users\relmer\repos\relmer\Casso\TestResults\007-ui-overhaul\
- [ ] T075 Implement click-to-browse drive mount flow for FR-022b in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\DriveWidgetController.cpp
- [ ] T076 Add run-while-open behavior coverage for FR-041 in C:\Users\relmer\repos\relmer\Casso\UnitTest\UiTests\SettingsPanelStateTests.cpp
- [ ] T077 Implement Win10/Win11 runtime gating for FR-042 in C:\Users\relmer\repos\relmer\Casso\Casso\Main.cpp
- [ ] T078 Implement and test 4:3 viewport invariant for FR-043 in C:\Users\relmer\repos\relmer\Casso\Casso\D3DRenderer.cpp
- [ ] T079 Implement and test theme metadata version-upgrade path for FR-045 in C:\Users\relmer\repos\relmer\Casso\Casso\Ui\ThemeLoader.cpp
- [ ] T080 Execute SC-004 200-attempt drag/drop reliability validation matrix (per-format success-rate evidence + threshold checks) in C:\Users\relmer\repos\relmer\Casso\TestResults\007-ui-overhaul\

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: starts immediately.
- **Phase 2 (Foundational)**: depends on Phase 1; blocks all stories.
- **Phases 3-8 (User Stories)**: depend on Phase 2 completion.
- **Phase 9 (Polish)**: depends on selected story completion (minimum MVP requires US1-3 + US5 for ownership validation).

### User Story Dependencies

- **US1 (P1)**: starts after Phase 2; no story dependency.
- **US2 (P1)**: starts after Phase 2; no story dependency.
- **US3 (P1)**: starts after Phase 2; no story dependency.
- **US4 (P2)**: starts after Phase 2; should consume US3 drive visual profile outputs.
- **US5 (P2)**: starts after Phase 2; no story dependency.
- **US6 (P3)**: starts after Phase 2; best validated after US1/US2 persistence paths exist.

### Delivery Order Graph

`Setup -> Foundational -> (US1 || US2 || US3 || US5) -> US4 -> US6 -> Polish`

### Within Each User Story

- Write/add tests first and confirm failure mode.
- Implement core state/models before UI event wiring.
- Implement runtime integration before acceptance pass updates.

---

## Parallel Opportunities

- **Foundational**: T011-T014 can run in parallel after T007-T010 starts.
- **US1**: T016 and T017 parallel; implementation T018-T021 then T022.
- **US2**: T023 and T024 parallel; implementation T025-T028 then T029.
- **US3**: T030 and T031 parallel; T032 and T033 parallel before T034-T037.
- **US4**: T038 and T039 parallel; T042-T044 can run in parallel after T040-T041.
- **US5**: T045-T047 parallel; T048-T051 can be split by component owner.
- **US6**: T053 and T054 parallel; T055-T057 then T058.
- **Polish**: T059 and T060 parallel; T062 can run while T061 executes; T065-T080 can be parallelized by subsystem.

### Parallel Example: User Story 3

```bash
Task: "T030 [US3] Drive state-transition tests in UnitTest/UiTests/DriveWidgetStateTests.cpp"
Task: "T031 [US3] Sync timing tests in UnitTest/Audio/DiskIIAudioSourceEventSinkTests.cpp"
Task: "T032 [US3] Disk ][ visuals in Casso/Ui/DriveWidgetElement.cpp"
Task: "T033 [US3] Door animation state machine in Casso/Ui/DriveWidgetState.h"
```

### Parallel Example: User Story 5

```bash
Task: "T045 [US5] NC hit-test tests in UnitTest/UiTests/TitleBarHitTestTests.cpp"
Task: "T046 [US5] Font/layout policy tests in UnitTest/UiTests/TitleBarLayoutTests.cpp"
Task: "T047 [US5] Nav command parity tests in UnitTest/UiTests/NavLayerTraceabilityTests.cpp"
```

---

## Implementation Strategy

### MVP First (Recommended)

1. Complete Setup + Foundational (native-only ownership + Rml teardown).
2. Deliver **US1 + US2 + US3** (core settings + hardware + drive realism/sync).
3. Deliver **US5** (native title/nav ownership + Windows font policy).
4. Run UT isolation checks + screenshot matrix before expanding scope.

### Incremental Delivery

1. Foundation reset complete (no legacy runtime/build path).
2. Ship P1 stories (US1, US2, US3) and validate independently.
3. Add P2 stories (US4, US5) and re-run runtime matrix.
4. Add P3 story (US6), then execute polish/audit tasks.

### Validation Gates

- Gate A: Build graph clean of `External\RmlUi` references.
- Gate B: UT suites for ownership/hit-test/drive-sync/migration pass with no host-state side effects.
- Gate C: Screenshot matrix passes for startup, menus, NC controls, settings, and drive door open/close states.
- Gate D: `scripts\Build.ps1 -RunCodeAnalysis` passes with zero analyzer failures.
- Gate E: Theme-switch consecutive-frame evidence passes SC-002 (zero mixed-theme regions).
- Gate F: SC-004 drag/drop reliability matrix meets >=99% success over 200 attempts.
