# Tasks: Dxui Command Widgets

**Input**: Design documents from `/specs/032-dxui-command-widgets/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Required. The spec's FR-017 and SC-003 through SC-006 are proved only by headless tests, and US1 is proved by captures plus the existing suite with every assertion unchanged.

**Organization**: Phases follow the plan's build order, which is the dependency order: command, dropdown, menu bar, context menu, toolbar, emulator table. Each task carries the story it serves. US1 (emulator unchanged) is the bar every phase is held to, and gets its own validation phase at the end.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: US1 emulator unchanged, US2 one command every surface, US3 one dropdown three openers, US4 second app hosts a toolbar, US5 emulator content only in the emulator

## Path Conventions

- Library: `Dxui/Core/`, `Dxui/Widgets/`, `Dxui/Dxui.vcxproj`
- Emulator: `CassoEmuCore/Ui/Chrome/`, `CassoEmuCore/Ui/`, `CassoEmuCore/Shell/`
- Tests: `UnitTest/Dxui/`, `UnitTest/UnitTest.vcxproj`
- Mocks: `UnitTest/Dxui/MockDxuiTextRenderer.h`, `MockDxuiPainter.h`, `MockDxuiTheme.h`
- Widget skeleton and headless-test pattern: `specs/013-dxui-framework-extraction/quickstart.md` §2 and §3

## Standing rules for every task

- Build through `Casso.sln` with `scripts/Build.ps1`; `-Target Rebuild` after any virtual is added or removed.
- Function names VerbNoun; `OnXxx` exempt; `IDxuiTheme` color accessors keep bare nouns. Helpers are class statics. Non-ASCII glyphs via `Dxui/Core/UnicodeSymbols.h`.
- Moving a function ahead of another splices ahead of its `////` banner.
- No spec, FR or task references in code comments or identifiers.
- Commit per phase, subject in the form `refactor(dxui): ...` or `refactor(shell): ...`.

---

## Phase 1: Setup

**Purpose**: A master baseline to compare against, taken before any code moves.

- [ ] T001 Build master `Casso.exe` Release x64 from a second worktree or before checkout, and copy it to `scripts/out/baseline/Casso.exe`
- [ ] T002 Run `scripts/CaptureScreenshotMatrix.ps1 -Configuration Release -CassoPath scripts/out/baseline/Casso.exe -OutDir scripts/out/screenshots/master` and keep the output
- [ ] T003 With the baseline binary, capture by hand each open top-level menu and the toolbar band in Skeuomorphic, DarkModern and RetroTerminal, and record the client widths at which each toolbar entry loses its label, right to left, into `scripts/out/screenshots/master/collapse-widths.txt`

---

## Phase 2: Foundational: DxuiCommand (US2)

**Purpose**: The declaration every later surface references. Blocks all following phases.

- [ ] T004 [US2] Create `Dxui/Core/DxuiCommand.h` per `contracts/dxui-command.md`: fields id, label, shortLabel, glyph, tip, accelerator, dispatch, isChecked, isEnabled, labelText; accessors `IsChecked`, `IsEnabled`, `GetLabelText`, `GetShortText` with the absent-functor defaults
- [ ] T005 [US2] Add `Core\DxuiCommand.h` as a `ClInclude` to `Dxui/Dxui.vcxproj`
- [ ] T006 [P] [US2] Create `UnitTest/Dxui/DxuiCommandTests.cpp`: absent `isChecked` is false, absent `isEnabled` is true, `GetLabelText` prefers `labelText`, `GetShortText` prefers `shortLabel` then falls back to `GetLabelText`
- [ ] T007 [US2] Add `Dxui\DxuiCommandTests.cpp` to `UnitTest/UnitTest.vcxproj`; build Debug x64; run `scripts/RunTests.ps1 -Configuration Debug -Filter DxuiCommand`; confirm `UnitTest.dll` is newer than the build
- [ ] T008 Commit: `refactor(dxui): DxuiCommand, one declaration per action`

**Checkpoint**: A command exists with no consumer. Tree builds, all tests pass.

---

## Phase 3: User Story 3: One dropdown (DxuiMenuFlyout)

**Goal**: One dropdown widget with the menu bar's item model, opened under a rect or at a point, content-fitted.

**Independent Test**: `DxuiMenuFlyoutTests.cpp` opens one list from `ShowUnder` and `ShowAt` and asserts identical row layout, navigation and callbacks.

- [ ] T009 [US3] Create `Dxui/Widgets/DxuiMenuFlyout.h` per `contracts/dxui-menu-flyout.md`: `DxuiMenuFlyoutItem` with `ForCommand`, `ForSeparator`, `ForSubmenu`; the widget's setters, `ShowUnder`, `ShowAt`, `Hide`, `IsVisible`, `GetHighlight`, `HitTest`, the four input handlers, `Paint`, and the `IDxuiControl` overrides
- [ ] T010 [US3] Create `Dxui/Widgets/DxuiMenuFlyout.cpp` by copying `Dxui/Widgets/DxuiPopupMenu.cpp` and renaming the class: keep hosting via `DxuiHwndSource` popup pool, the three callbacks with closed-before-select, and the submenu chain through `DxuiPopupHost::SetParentPopup`
- [ ] T011 [US3] In `DxuiMenuFlyout.cpp`, replace the label-plus-checked item painting with the menu bar's row painting moved from `Dxui/Widgets/DxuiMenuBar.cpp`: 26 dp rows, 10 dp pad plus 18 dp check gutter, 14 dp font, separators 10 dp tall with 10 dp inset, accelerator text right aligned, submenu arrow, colors `BackgroundElevated`, `HoverBackground`, `Foreground`, `ForegroundDisabled`, `ForegroundMuted`, `Divider`, `Border`, all read from the command at paint time
- [ ] T012 [US3] In `DxuiMenuFlyout.cpp`, implement content-fitted width: widest `GetLabelText` across rows, plus an accelerator column only when some row has accelerator text, plus padding, 140 dp minimum; no fixed width and no setter for one
- [ ] T013 [US3] In `DxuiMenuFlyout.cpp`, implement navigation per contract: Up and Down skip separators and disabled rows and wrap; Right opens a submenu with its first enabled row highlighted; pointer dwell on a submenu row opens it unhighlighted and hovering another parent row closes it; Left or Escape with a child open closes only the child; Escape on the root hides uncommitted; Enter on an enabled command commits and calls its `dispatch`; Enter on a submenu row opens it; `ShowAt` clamps the dropdown inside the host client rect
- [ ] T014 [US3] In `DxuiMenuFlyout.cpp`, copy the reopen guard from `CassoEmuCore/Ui/Chrome/CommandToolbar.cpp` `IsReopenSuppressed` so a show inside the close window from the same anchor is ignored; leave the toolbar's copy in place until T038 deletes it, so the pickers keep click-to-toggle between commits
- [ ] T015 [US3] Add `Widgets\DxuiMenuFlyout.h/.cpp` to `Dxui/Dxui.vcxproj`; build with `-Target Rebuild`
- [ ] T016 [P] [US3] Create `UnitTest/Dxui/DxuiMenuFlyoutTests.cpp` on the three mocks: identical layout from `ShowUnder` and `ShowAt` for one list; width fits content and grows only when a row gains accelerator text; a `ShowAt` near the client edge is clamped inside the host rect; Down skips a separator and a disabled row and wraps; Right opens a submenu with its first enabled row highlighted; hover dwell opens it unhighlighted and hovering another parent row closes it; Left closes only the child; closed fires before select with the committed flag; a disabled command does not dispatch on Enter; the reopen guard swallows a show inside the close window
- [ ] T017 [US3] Add `Dxui\DxuiMenuFlyoutTests.cpp` to `UnitTest/UnitTest.vcxproj`; run `scripts/RunTests.ps1 -Configuration Debug -Filter DxuiMenuFlyout`
- [ ] T018 Commit: `refactor(dxui): DxuiMenuFlyout, one dropdown for every opener`

**Checkpoint**: The dropdown exists beside `DxuiPopupMenu`; nothing consumes it yet.

---

## Phase 4: User Story 3 continued: menu bar over the dropdown, and the context menu call

**Goal**: `DxuiMenuBar` opens `DxuiMenuFlyout` under each title and paints no dropdown of its own; the two debug panels open a context menu with one call.

**Independent Test**: `DxuiMenuBarTests.cpp` and `MainMenuDropdownTests.cpp` pass with every assertion unchanged; open menus match master's rows and states at content-fitted width.

- [ ] T019 [US3] In `Dxui/Widgets/DxuiMenuBar.h`, retype `DxuiMenuBarItem::submenu` to `std::vector<DxuiMenuFlyoutItem>` and delete `DxuiMenuBarSubitem`; keep every public method for titles, mnemonics, open, close, focus and keyboard; add an owned `DxuiMenuFlyout` member; in `UnitTest/Dxui/DxuiMenuBarTests.cpp` retype the positional subitem aggregates at lines 52-66 into `DxuiCommand` objects and `DxuiMenuFlyoutItem::ForCommand` calls, changing no assertion
- [ ] T020 [US3] In `Dxui/Widgets/DxuiMenuBar.cpp`, make `Open` show the owned dropdown under the title with that title's items, `Close` and `CloseAll` hide it, Left, Right and hover-swap replace its items, and Up, Down, Enter and Escape forward to it; delete the bar's own dropdown layout and painting and the `s_kDropdownWidthDip` and `s_kAccelOffsetDip` constants; forward `SetDropdownColors` to the dropdown
- [ ] T021 [US3] In `CassoEmuCore/Ui/Chrome/MainMenu.h/.cpp`, adapt `s_kEntries` into `DxuiCommand` objects at construction (id, label, accelerator, dispatch calling the existing `WORD` dispatch functor, `isChecked` and `labelText` from the existing check and label functors) and build each title's `DxuiMenuFlyoutItem` list from them; keep `MainMenu`'s public surface so the shell compiles unchanged
- [ ] T022 [US3] Build with `-Target Rebuild`; run `scripts/RunTests.ps1 -Configuration Debug -Filter MenuBar` and confirm `UnitTest/Dxui/DxuiMenuBarTests.cpp` and `UnitTest/UiTests/MainMenuDropdownTests.cpp` pass; `git diff master -- UnitTest/` shows construction-site retyping only
- [ ] T023 [US1] Run the emulator; open each top-level menu in each theme and compare against the Phase 1 captures: same rows, separators, disabled rows, checks and accelerator hints at the same row height and top edge; width fitted to content; exercise Alt+letter, Left, Right, Up, Down, Enter, Escape per quickstart §4
- [ ] T024 [P] [US3] Create `Dxui/Widgets/DxuiContextMenu.h/.cpp` with `static void Show (DxuiHwndSource & host, int x, int y, std::vector<DxuiMenuFlyoutItem> items)` driving a dropdown owned by the host window with the host's DPI, theme, text renderer and client rect; no completion callback, a picked row runs its command; keep it a short adapter with no logic of its own, since it is covered by the dropdown tests plus T027; add both files to `Dxui/Dxui.vcxproj`
- [ ] T025 [US3] In `CassoEmuCore/Ui/Disk2DebugPanel.h/.cpp`, replace the `DxuiPopupMenu` member and its `Show` with `DxuiCommand` objects for its rows and `DxuiContextMenu::Show` from the right-click handler
- [ ] T026 [US3] In `CassoEmuCore/Ui/InputDebugPanel.h/.cpp`, same replacement as T025
- [ ] T027 [US1] Run the emulator; right-click each debug panel, confirm the same rows appear and each row runs its action
- [ ] T028 Commit: `refactor(dxui): menu bar and context menus over DxuiMenuFlyout`

**Checkpoint**: `DxuiPopupMenu` has one consumer left, the toolbar.

---

## Phase 5: User Story 4: DxuiToolbar

**Goal**: The strip extracted from `CommandToolbar`, entries holding command references, pickers on `DxuiMenuFlyout`, decoration and custom-entry hooks.

**Independent Test**: `DxuiToolbarTests.cpp` builds a toolbar from commands with no emulator reference and verifies collapse, tooltips, dispatch, pickers, flyout and a custom entry.

- [ ] T029 [US4] Create `Dxui/Widgets/DxuiToolbar.h` per `contracts/dxui-toolbar.md`: `IDxuiToolbarCustomEntry`, `Kind`, `Entry`, `ChoiceFn`, `DecorationFn`, `SetEntries`, `SetIconFace`, planning, layout, paint, the pointer and keyboard handlers, `SetDropDownItems`, `SetDropDownSinks`, `SetFlyoutControl`, `IsFlyoutOpen`, `SetPopupHost`, `SetHostClientRect`, `SetTextRenderer`
- [ ] T030 [US4] Create `Dxui/Widgets/DxuiToolbar.cpp` by moving from `CassoEmuCore/Ui/Chrome/CommandToolbar.cpp`, bodies and metric constants intact, in today's order: `WireMenus`, `SetPopupHost`, `HideMenus`, `IsMenuOpen`, `HandleKey`, `OpenMenuFor`, `IsPointInRect`, `HitTest`, `GetBandDp`, `FlyoutKeepAliveRc`, `MeasureLabelPx`, `GetEntryWidthPx`, `GetTotalWidthPx`, `PlanForWidth`, `Layout`, `GetTooltipAt`, `OnToolbarMouseMove`, `OnToolbarMouseLeave`, `OnToolbarLButtonDown`, `OnToolbarLButtonUp`, `PaintEntryIcon`, `PaintButton`, `Paint`, `StrokeCircle`, `PaintVolumeFlyout` as `PaintFlyout`
- [ ] T031 [US4] In `DxuiToolbar.cpp`, replace the entry table's id, glyph, label and tip fields with reads from `Entry::command` at paint and click time; `Toggle` draws pressed while `IsChecked`; the label uses `GetShortText`; a disabled command draws disabled and does not dispatch
- [ ] T032 [US4] In `DxuiToolbar.cpp`, replace the three `DxuiPopupMenu` members with one `DxuiMenuFlyout` plus a per-picker item list, `openedOn`, preview and commit sinks keyed by command id; preview on highlight change, commit once on select, replay `openedOn` on uncommitted close
- [ ] T033 [US4] In `DxuiToolbar.cpp`, replace every input-cluster branch in `GetEntryWidthPx`, `Layout`, `GetTooltipAt`, `OnToolbarLButtonUp` and `PaintEntryIcon` with delegation to `Entry::custom` when set, and paint `Entry::decoration` over the icon rect after `PaintEntryIcon` when set
- [ ] T034 [US4] In `DxuiToolbar.cpp`, generalize the volume flyout: `SetFlyoutControl` stores the hosted `IDxuiControl` and panel size; the widget sizes and positions it, forwards mouse events while open, and paints it inside `PaintFlyout`; `Paint` reads `DxuiTheme` fields via `IDxuiTheme` with no downcast; the icon face is a member defaulting to Segoe MDL2 Assets
- [ ] T035 [US4] Add `Widgets\DxuiToolbar.h/.cpp` to `Dxui/Dxui.vcxproj`; build with `-Target Rebuild`; grep `Dxui/` for `CassoEmuCore`, `CassoTheme`, `Resource.h`, `IDM_` and confirm no new hits
- [ ] T036 [P] [US4] Create `UnitTest/Dxui/DxuiToolbarTests.cpp` on the three mocks with five commands: all labeled at a fitting width, only the rightmost collapsed one step narrower, none labeled far narrower; tooltip and anchor per entry and nullptr in a gap; dispatch fires once on down-and-up on one entry, not across entries, not when `isEnabled` is false; a `Toggle` draws pressed from `isChecked`; picker preview on highlight, snap-back on Escape, one commit on Enter; flyout opens on dwell and closes on leave; a flyout entry that collapses while open closes its flyout and reopens it on the next dwell; a stub `IDxuiToolbarCustomEntry` receives width, layout, paint, tooltip and click calls, and when its `OnClick` returns false on a `DropDown` entry the widget opens that entry's item list
- [ ] T037 [US4] Add `Dxui\DxuiToolbarTests.cpp` to `UnitTest/UnitTest.vcxproj`; run `scripts/RunTests.ps1 -Configuration Debug -Filter DxuiToolbar`
- [ ] T038 [US4] Reduce `CassoEmuCore/Ui/Chrome/CommandToolbar.h/.cpp` to a temporary wrapper owning a `DxuiToolbar` and forwarding its existing public surface, so the shell compiles unchanged for this commit; delete its `IsReopenSuppressed` and the three `DxuiPopupMenu` members now that the widget's dropdown carries the guard; the emulator-specific functions stay in it for now
- [ ] T039 [US1] Build; run the emulator; compare the toolbar band against the Phase 1 captures in each theme and re-record collapse widths against `collapse-widths.txt`; exercise pickers, volume flyout, input entry and printer light per quickstart §4
- [ ] T040 Commit: `refactor(dxui): DxuiToolbar extracted from CommandToolbar`

**Checkpoint**: The toolbar is a library widget with tests; the emulator still reaches it through a wrapper.

---

## Phase 6: User Story 2 and User Story 5: one emulator command table, CommandToolbar deleted

**Goal**: Every emulator action declared once; the shell holds `DxuiToolbar` directly; the printer LED and input cluster are two small classes; `CommandToolbar` and `DxuiPopupMenu` are gone.

**Independent Test**: `CommandToolbar.cpp` and `DxuiPopupMenu.cpp` do not exist; the shell builds; captures match; a cross-surface test shows one enabled change on three surfaces.

- [ ] T041 [P] [US5] Create `CassoEmuCore/Ui/Chrome/PrinterStatusLed.h/.cpp`: move `GetStatusCoreColor` and the free static `PaintStatusLed` from `CommandToolbar.cpp` as class statics; expose a `DecorationFn` factory bound to a `PrinterStatus` reference and the `present` flag; it may downcast `IDxuiTheme` to `CassoTheme` for `ledActive`
- [ ] T042 [P] [US5] Create `CassoEmuCore/Ui/Chrome/InputClusterEntry.h/.cpp` implementing `IDxuiToolbarCustomEntry`: move `SetInputState`, `InputSegSelected`, `IsInputExpanded`, `PaintInputCluster`, `GetGlyphStroke`, `PaintJoystickMono`, `PaintPaddleMono`, the segment rects, skeuo and monoline flags, and the `s_kInputRows` and `s_kInputModes` tables from `CommandToolbar.cpp`; segment clicks report the mode through an `InputFn` sink and `OnClick` returns true; while collapsed `OnClick` returns false so the toolbar opens the entry's picker list, which is the three modes as commands with `isChecked` reading the current mode
- [ ] T043 [US2] Create `CassoEmuCore/Ui/Chrome/EmulatorCommands.h/.cpp`: one static table of `DxuiCommand` for every `IDM_*` shown by `MainMenu` or the toolbar, built from `MainMenu`'s `s_kEntries` plus the toolbar's ten entries, with `dispatch` calling the shell's command handler via a sink, `isChecked`, `isEnabled` and `labelText` bound to shell state via sinks set at startup, the Reset and Power tips as label functors quoting the machine display name, and the theme and monitor color row lists as commands with `isChecked` reading the current choice; two placement tables beside it: menu title to item list, and the toolbar entry list with kinds and groups
- [ ] T044 [US2] Rewrite `CassoEmuCore/Ui/Chrome/MainMenu.h/.cpp` to build its titles from the placement table in `EmulatorCommands`; delete `s_kEntries`, `MainMenuCommandEntry`, `GetCommandEntries`, `IsSeparator`, `GetMenuName`, `EmitParityMarkdown` and the `WORD`-keyed dispatch, check and label maps; keep `SetDispatch (WORD)`, `Open (MainMenuId, bool)`, `HandleKey`, `HandleMouseMove`, `HandleMouseUp` and `GetHighlightIndex` with their current behavior so `UnitTest/UiTests/MainMenuDropdownTests.cpp` runs unchanged; give `EmulatorCommands` accessors for the command table and the menu placement, and re-point `UnitTest/UiTests/ChromeCommandRoutingTests.cpp` lines 73-149 at them so its every-`IDM_`-present and no-duplicate-id assertions survive with the same meaning
- [ ] T045 [US5] Add a `DxuiToolbar` member to `CassoEmuCore/Shell/EmulatorShell.h` in place of `CommandToolbar`, plus a `PrinterStatusLed` and an `InputClusterEntry`; add a static builder in `EmulatorCommands` that fills the toolbar from the placement table with the LED as the printer entry's decoration and the cluster as the input entry's custom entry
- [ ] T046 [US5] Update call sites in `CassoEmuCore/Shell/EmulatorShell.cpp` (text renderer, popup host, theme and monitor sinks via `SetDropDownSinks`), `EmulatorShellChrome.cpp` (`PlanForWidth`, `GetBandDp`, `Layout`, `SetVisible`, `GetBounds`, `SetHostClientRect`, theme rows via `SetDropDownItems`, machine name and fullscreen via the commands, skeuo flag on the cluster), `EmulatorShellPresent.cpp` (`IsMenuOpen`), `EmulatorShellPrinter.cpp` (LED state), `Shell/Window/EmulatorWindow.cpp` (adopt, dispatch, volume sink, `SetFlyoutControl` with the shell's `DxuiSlider`, volume seeding), `Shell/Window/EmulatorWindowInput.cpp` (the four mouse handlers, `GetTooltipAt`, `IsMenuOpen`, `HandleKey`, input state on the cluster)
- [ ] T047 [US5] Delete `CassoEmuCore/Ui/Chrome/CommandToolbar.h/.cpp` and remove them from `CassoEmuCore/CassoEmuCore.vcxproj`; delete `Dxui/Widgets/DxuiPopupMenu.h/.cpp` and remove them from `Dxui/Dxui.vcxproj`; fix the comment in `Dxui/Window/DxuiPopupHost.h` and `Dxui/Window/DxuiWindow.h` that still refers to `DxuiPopupMenu`; in `UnitTest/Dxui/DxuiWidgetIDxuiControlTests.cpp` lines 170-172 retype the three `DxuiPopupMenu_*` conformance rows to `DxuiMenuFlyout_*` and add `DxuiToolbar_*` rows, same assertions
- [ ] T048 [US5] Build all with `-Target Rebuild`; grep `CassoEmuCore/` for `CommandToolbar`, `MainMenuCommandEntry`, `DxuiPopupMenu` and confirm no hits
- [ ] T049 [P] [US2] Create `UnitTest/Dxui/DxuiCommandSurfacesTests.cpp`: one `DxuiCommand` placed in a `DxuiMenuBar` item list, a `DxuiToolbar` entry and a `DxuiMenuFlyout` list; flip `isEnabled` to false and assert all three paint it disabled and none dispatches on activation; change `labelText` and assert all three show the new label
- [ ] T060 [P] [US5] Create `UnitTest/UiTests/ChromeToolbarPartsTests.cpp`: `PrinterStatusLed` returns the expected color for each `PrinterStatus` and nothing while idle; `InputClusterEntry` reports the mode for a click on each segment, `OnClick` returns true while expanded and false while collapsed, and `GetTooltipAt` returns the segment tip inside a segment and nullptr outside
- [ ] T050 [US2] Add `Dxui\DxuiCommandSurfacesTests.cpp` and `UiTests\ChromeToolbarPartsTests.cpp` to `UnitTest/UnitTest.vcxproj`; run the full suite `scripts/RunTests.ps1 -Configuration Debug` and `-Configuration Release`; confirm via `git diff master -- UnitTest/` that existing files changed only at construction sites and in the parity test's table accessor, with no assertion altered
- [ ] T051 [US1] Run the emulator; compare the menu bar and toolbar bands against the Phase 1 captures in each theme, re-run `scripts/CaptureScreenshotMatrix.ps1 -Configuration Release -OutDir scripts/out/screenshots/branch` and crop-compare both bands for all four monitor colors; walk quickstart §4 in full
- [ ] T052 Commit: `refactor(shell): one command table; CommandToolbar deleted`

**Checkpoint**: Every story's code is in. Remaining work is validation and gates.

---

## Phase 7: User Story 1 validation and polish

**Purpose**: Prove the emulator is unchanged and pass the merge gates.

- [ ] T053 [US1] Complete quickstart §1 through §4 end to end on a Release x64 build and record the outcome, including the two allowed differences from §3, in `specs/032-dxui-command-widgets/validation.md`
- [ ] T054 [US1] Run quickstart §7 structural checks and record them in `validation.md`
- [ ] T055 [P] Add one line under `[Unreleased]` / Changed in `CHANGELOG.md` describing the net effect: menu bar, toolbar and context menus share one command model and one dropdown; dropdowns fit their content
- [ ] T056 `git add -A` then `scripts/CheckStyle.ps1 -Mode Tree`; fix every hit
- [ ] T057 `scripts/Build.ps1 -Target Rebuild -RunCodeAnalysis` for all four configurations; zero warnings
- [ ] T058 Merge `origin/master` into the branch, rebuild, rerun the suite
- [ ] T059 Commit: `docs(changelog): 032 command widgets`; push; present the CHANGELOG line and every commit subject for approval before the master merge

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1** first; it must use a master binary.
- **Phase 2** blocks everything after it.
- **Phase 3** needs Phase 2. **Phase 4** needs Phase 3.
- **Phase 5** needs Phase 3 (pickers) and Phase 2; it does not need Phase 4.
- **Phase 6** needs Phases 4 and 5.
- **Phase 7** needs Phase 6.

### Story Dependencies

- **US2** (command) is foundational and completes in Phase 6 with the emulator table.
- **US3** (dropdown) completes across Phases 3 and 4.
- **US4** (toolbar) completes in Phase 5 and depends on US3's dropdown for pickers.
- **US5** (emulator content) completes in Phase 6 and depends on US4.
- **US1** (unchanged) is checked at T023, T027, T039, T051 and closed in Phase 7.

### Parallel Opportunities

- T006 with T004 and T005 (test file beside the header).
- T016 while T009 through T015 are in progress (test against the contract).
- T024 alongside T019 through T023.
- T036 alongside T029 through T035.
- T041 and T042 together, before T043.
- T049 alongside T043 through T048.
- T055 alongside T053 and T054.

---

## Parallel Example: Phase 6

```text
Together:  T041 PrinterStatusLed.h/.cpp
           T042 InputClusterEntry.h/.cpp
Then:      T043 EmulatorCommands.h/.cpp
Then:      T044 MainMenu, T045 shell member and builder
Then:      T046 call sites, T047 deletes, T048 build
Together:  T049 cross-surface test, T051 captures
```

---

## Implementation Strategy

### Merge after any checkpoint

Every phase leaves the tree building and the emulator identical, so the branch can merge after Phase 4 (dropdown and menu bar), after Phase 5 (toolbar behind a wrapper) or after Phase 6 (everything). The intended merge is after Phase 7.

### Minimum useful slice

Phases 1 through 4: one dropdown under the menu bar and the context menus, with `DxuiCommand` in place. That alone removes one duplicate dropdown and gives Cassque its context menus.

### Master merges

Merge `origin/master` into the branch after each phase commit, since master takes sweeping renames and the cost compounds.

---

## Notes

- Every moved function keeps its body and its metric constants; a changed constant is a defect, not a cleanup.
- `DxuiMenuBarTests.cpp` and `MainMenuDropdownTests.cpp` are the strongest behavioral oracles in the tree. Their assertions do not change; only item construction is retyped where a type was removed. No adapter keeps a removed type alive for a test's sake.
- `ChromeCommandRoutingTests.cpp` is the only automated check that each command id is declared once. It is re-pointed in the same commit that deletes the old table (T044), never left broken.
- T060 executes inside Phase 6 alongside T041 and T042 despite its number.
- The two allowed visible changes are dropdown width fitting content and the pickers' 14 dp font. Anything else that differs from the Phase 1 captures is a defect.
