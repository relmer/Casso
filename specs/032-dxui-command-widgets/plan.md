# Implementation Plan: Dxui Command Widgets

**Branch**: `032-dxui-command-widgets` | **Date**: 2026-09-10 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/032-dxui-command-widgets/spec.md`

## Summary

Install the command model WPF and WinUI use into Dxui, and move the
emulator's chrome onto it with no visible change. Three library types:
`DxuiCommand`, one declaration per action; `DxuiDropdown`, one dropdown
evolved from `DxuiPopupMenu` with the menu bar's item model and painting;
`DxuiToolbar`, the strip extracted from the emulator's `CommandToolbar`.
`DxuiMenuBar` becomes titles over the dropdown. A one-call context menu
opens the same dropdown at a point. In the emulator, `MainMenu`'s table and
`CommandToolbar`'s table become one command table, `CommandToolbar` is
deleted, the shell holds a `DxuiToolbar`, and the two debug panels use the
context menu call. Reasoning per choice is in [research.md](research.md).

## Technical Context

**Language/Version**: C++ under stdcpplatest, MSVC v145+

**Primary Dependencies**: Dxui (Direct2D, DirectWrite, `DxuiHwndSource`
popup pool); no new third-party dependencies

**Storage**: N/A

**Testing**: CppUnitTestFramework; headless Dxui mocks in `UnitTest/Dxui/`

**Target Platform**: Windows 10/11, x64 and ARM64 (ARM64 build-only)

**Project Type**: desktop app, UI library refactor

**Performance Goals**: no change; command functors are evaluated once per
paint per visible row or entry, which is the cost the menu bar already pays

**Constraints**: menu bar band and toolbar band pixel identical to master
in three themes; every open dropdown carries master's rows and states at
the same row positions, fitted to content; no behavioral assertion in an
existing unit test changed, construction sites retyped only; `Dxui/` includes nothing from `CassoEmuCore`

**Scale/Scope**: three new library types, one rebuilt widget, one removed
widget, one removed emulator class, ~2,000 lines moved, ~40 shell call
sites edited, four new test files

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. Code Quality | PASS | Moved functions keep their bodies; new code follows EHM, VerbNoun, class-static helpers. |
| II. Testing Discipline | PASS | Four headless test files on the existing mocks; no file, registry or window access. |
| III. UX Consistency | PASS | Two deliberate visible changes, both in research R5: dropdowns fit content instead of a fixed 300 dp, and pickers gain the menu font. Nothing else moves. |
| IV. Performance | PASS | Functor evaluation per paint matches the menu bar's current cost. |
| V. Simplicity | PASS | Removes two duplicate dropdowns and one duplicate command table; adds no abstraction beyond the three types. |
| VI. Thin Executable | PASS | Executables untouched. |

Post-design re-check: unchanged. No Complexity Tracking entries.

## Project Structure

### Documentation (this feature)

```text
specs/032-dxui-command-widgets/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── dxui-command.md
│   ├── dxui-dropdown.md
│   └── dxui-toolbar.md
├── tasks.md              # /speckit-tasks output
└── validation.md         # quickstart outcomes, written during Phase 7
```

### Source Code (repository root)

```text
Dxui/
├── Core/
│   └── DxuiCommand.h                  # NEW
├── Widgets/
│   ├── DxuiDropdown.h/.cpp            # NEW, from DxuiPopupMenu + menu bar painting
│   ├── DxuiContextMenu.h/.cpp         # NEW, one static Show
│   ├── DxuiToolbar.h/.cpp             # NEW, from CommandToolbar
│   ├── DxuiMenuBar.h/.cpp             # REBUILT over DxuiDropdown; subitem type removed
│   └── DxuiPopupMenu.h/.cpp           # REMOVED
└── Dxui.vcxproj

CassoEmuCore/
├── Ui/Chrome/
│   ├── EmulatorCommands.h/.cpp        # NEW: the one command table + menu and toolbar placement
│   ├── PrinterStatusLed.h/.cpp        # NEW: status color + decoration painter
│   ├── InputClusterEntry.h/.cpp       # NEW: IDxuiToolbarCustomEntry
│   ├── MainMenu.h/.cpp                # shrinks: builds titles from placement, no id maps
│   └── CommandToolbar.h/.cpp          # REMOVED
├── Ui/
│   ├── Disk2DebugPanel.cpp            # DxuiContextMenu::Show
│   └── InputDebugPanel.cpp            # DxuiContextMenu::Show
└── Shell/
    ├── EmulatorShell.h/.cpp           # holds DxuiToolbar; sinks
    ├── EmulatorShellChrome.cpp        # plan/layout/theme calls
    ├── EmulatorShellPresent.cpp       # IsMenuOpen
    ├── EmulatorShellPrinter.cpp       # PrinterStatusLed state
    └── Window/EmulatorWindow*.cpp     # adopt, routing, input state

UnitTest/
├── Dxui/
│   ├── DxuiCommandTests.cpp           # NEW
│   ├── DxuiDropdownTests.cpp          # NEW
│   ├── DxuiToolbarTests.cpp           # NEW
│   ├── DxuiCommandSurfacesTests.cpp   # NEW: one command, three surfaces
│   ├── DxuiMenuBarTests.cpp           # item construction retyped, assertions unchanged
│   └── DxuiWidgetIDxuiControlTests.cpp # DxuiPopupMenu rows become DxuiDropdown + DxuiToolbar rows
├── UiTests/
│   ├── ChromeCommandRoutingTests.cpp  # parity walk re-pointed at EmulatorCommands
│   ├── MainMenuDropdownTests.cpp      # drives the preserved MainMenu surface, unchanged
│   └── ChromeToolbarPartsTests.cpp    # NEW: PrinterStatusLed color rule, InputClusterEntry segments
└── UnitTest.vcxproj

CHANGELOG.md                            # [Unreleased], Changed: one internal line
```

**Structure Decision**: widgets land beside their siblings in
`Dxui/Widgets`, the command struct in `Dxui/Core` beside `IDxuiControl`,
tests beside `DxuiMenuBarTests.cpp`, following
`specs/013-dxui-framework-extraction/quickstart.md` §2 and §3.

## Design

### Order of work

Each step leaves the tree building and the emulator identical, so the
branch can merge after any of them if it has to.

1. **DxuiCommand** with tests. No consumer yet.
2. **DxuiDropdown** built from `DxuiPopupMenu` plus the menu bar's item
   painting, with tests. `DxuiPopupMenu` still exists.
3. **DxuiMenuBar over DxuiDropdown.** `DxuiMenuBarSubitem` becomes
   `DxuiDropdownItem` over `DxuiCommand`. `MainMenu` adapts its `s_kEntries`
   into commands at this step so the menu bar has a consumer. Row check
   of every open dropdown. `DxuiMenuBarTests.cpp` retypes its item
   construction and must pass with no assertion changed.
4. **DxuiContextMenu** and the two debug panels moved onto it.
   `DxuiPopupMenu` survives until step 6 because the toolbar still holds
   three.
5. **DxuiToolbar** extracted from `CommandToolbar`, pickers on
   `DxuiDropdown`, with tests. `CommandToolbar` becomes a temporary thin
   wrapper for one commit so the shell compiles.
6. **Emulator command table.** `EmulatorCommands` replaces `MainMenu`'s
   table and `CommandToolbar`'s entries. `PrinterStatusLed` and
   `InputClusterEntry` extracted. Shell holds `DxuiToolbar`. Delete
   `CommandToolbar` and `DxuiPopupMenu`. Pixel check of both bands.
7. **Cross-surface test**, CHANGELOG line, style sweep, four-configuration
   rebuild with analysis.

### DxuiDropdown

Takes `DxuiPopupMenu`'s hosting, callbacks, submenu chain and
content-fitted width, and the menu bar's font, check glyph, separators,
accelerator column and disabled color. No fixed width anywhere. One
instance serves a whole submenu chain by owning its children. The reopen
guard moves here from the toolbar so titles and pickers share it.

### DxuiMenuBar

Keeps titles, mnemonics, Alt+letter, hover-swap, Left and Right, focus
return. Owns one `DxuiDropdown`. `Open` shows it under the title with that
title's items; `Close` hides it. Up, Down, Enter and Escape forward to it.
`SetDropdownColors` forwards to the dropdown.

### DxuiToolbar

Moved functions, in today's order: `WireMenus`, `SetPopupHost`,
`HideMenus`, `IsReopenSuppressed` (to the dropdown), `IsMenuOpen`,
`HandleKey`, `OpenMenuFor`, `IsPointInRect`, `HitTest`, `GetBandDp`,
`FlyoutKeepAliveRc`, `MeasureLabelPx`, `GetEntryWidthPx`, `GetTotalWidthPx`,
`PlanForWidth`, `Layout`, `GetTooltipAt`, the four `OnToolbar*` handlers,
`PaintEntryIcon`, `PaintButton`, `Paint`, `StrokeCircle`,
`PaintVolumeFlyout` as `PaintFlyout`. Input-cluster branches become the
custom-entry delegation. Three pickers by value become one dropdown plus a
per-picker item list. The reopen guard is copied into the dropdown in step
2 and the toolbar's copy deleted here, so the pickers never lose
click-to-toggle between commits.

### EmulatorCommands

A static table of `DxuiCommand` for every `IDM_*` the menu or toolbar
shows, with `dispatch` calling the shell's command handler, `isChecked`
and `isEnabled` functors bound to shell state through sinks set at
startup, and label functors for the machine-name tips. Two placement
tables: menu title to item list, and toolbar entry list with kinds and
groups. `MainMenu` builds its titles from the first; a builder fills the
`DxuiToolbar` from the second.

### Shell call sites

`EmulatorShell.cpp` sinks and popup host; `EmulatorShellChrome.cpp` plan,
layout, theme rows, machine name, fullscreen glyph; `EmulatorShellPresent.cpp`
menu-open check; `EmulatorShellPrinter.cpp` LED state; `EmulatorWindow.cpp`
adopt and sinks; `EmulatorWindowInput.cpp` routing and input state. About
forty lines.

## Risks

- **Menu bar keyboard regressions**: the most-used chrome. Mitigated by
  `DxuiMenuBarTests.cpp` and `MainMenuDropdownTests.cpp` passing with
  every assertion unchanged, and the hand checks in quickstart §4.
- **Losing the parity gate**: `ChromeCommandRoutingTests.cpp` is the only
  automated check that every command id is declared once. It is
  re-pointed at `EmulatorCommands` in the same commit that deletes the old
  table, never left broken.
- **Pixel drift from moved metrics**: every dp constant moves with the
  function reading it; band and dropdown crops catch a slip.
- **Vtable churn**: `-Target Rebuild` after steps 2, 3 and 5.
- **Banner orphaning**: splice ahead of `////`; tree-mode CheckStyle
  before merge.
- **Master renames mid-branch**: merge master after each step.

## Phase 2 Preview

`/speckit-tasks` generates tasks.md following the seven steps above, one
phase each, committed per phase.
