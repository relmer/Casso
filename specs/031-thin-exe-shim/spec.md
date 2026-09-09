# Feature Specification: Thin Executable, Testable Core — `Casso.exe`

**Feature Branch**: `031-thin-exe-shim`

**Created**: 2026-09-09

**Status**: Draft

**Input**: GitHub issue #85, "Reduce every executable to a trivially thin shim — move all testable logic into core". `CassoCli.exe` was already reduced this way (3,639 lines to 57) and the extraction caught two shipped defects. `Casso.exe` is the remaining offender.

## Context

Constitution Principle VI (Thin Executable, Testable Core) is NON-NEGOTIABLE and says the exe/lib line is where **testability** ends, not where the operating system begins. `Casso.exe` predates that reading and violates it at scale.

Measured on `031-thin-exe-shim` at branch point (`.cpp` + `.h`, excluding build output and vendored `External/`):

| Project | Files | Lines | Unit tests |
|---|---:|---:|---|
| `Casso` (exe) | 189 | 93,927 | **none** |
| `CassoEmuCore` | 291 | 69,244 | yes |
| `Dxui` | 130 | 45,523 | yes |
| `CassoCore` | 78 | 33,409 | yes |
| `CassoCli` (exe) | 2 | 57 | n/a — nothing left to test |

Within the exe:

| Area | Lines |
|---|---:|
| `Casso/` root (of which `EmulatorShell.cpp` 15,995 and `.h` 2,055) | 30,573 |
| `Casso/Ui/` (root) | 14,857 |
| `Casso/Ui/Settings/` | 12,666 |
| `Casso/Shell/` | 9,125 |
| `Casso/Ui/Scene/` | 9,119 |
| `Casso/Ui/Chrome/` | 7,221 |
| `Casso/Config/` | 6,431 |
| `Casso/Ui/Dialogs/` | 3,443 |
| `Casso/Print/` | 482 |

`EmulatorShell` declares 291 members and mixes the Win32 window with machine lifecycle, CPU-thread orchestration, soft-switch state, chrome layout math, frame pacing, input mapping and video-mode selection. Only the window is a genuine platform edge.

138 of the 189 exe files contain no reference to `HWND`, a D3D/DXGI type, an audio client, a window-creation call or an OS file dialog at all. They are not near the edge; they are simply on the wrong side of a project boundary.

The absence of tests and the absence of the abstraction are the same hole. This specification defines how it gets closed.

## User Scenarios & Testing *(mandatory)*

The "user" here is a Casso maintainer. Each story is an independently shippable slice: it merges to master on its own, leaves the emulator behaving identically, and adds test coverage where there was none. They are ordered so that the cheapest, least entangled code moves first and establishes the seam pattern the later slices reuse.

### User Story 1 - Persistence and preferences become testable (Priority: P1)

A maintainer changes how a user preference is read, merged, or defaulted, and proves the change by running the unit-test suite rather than by launching the emulator and clicking through Settings.

`Casso/Config/` (6,431 lines) already has an `IFileSystem` seam with a `Win32FileSystem` behind it, so it is shaped for the move and only the project boundary is in the way. It carries preference merge order, corrupt-file recovery to defaults, monitor catalog lookup, window-placement restore, per-machine input preferences, and CRT override resolution — all decisions with edge cases and none of them observable today.

**Why this priority**: highest ratio of testable decisions to extraction risk, and it is the slice that proves the pattern (move behind an existing seam, backfill tests, leave a platform implementation behind) for everything after it.

**Independent Test**: run the unit-test suite with a synthetic in-memory file system and assert preference merge, corrupt-input recovery, and placement-restore outcomes. No disk, no registry, no window.

**Acceptance Scenarios**:

1. **Given** a preferences document with a corrupt or absent block, **When** it is loaded, **Then** the affected subsystem falls back to documented defaults, the rest of the document is preserved, and nothing aborts — asserted by a test, not by a code comment.
2. **Given** a saved window placement referring to a monitor arrangement that no longer exists, **When** placement is restored, **Then** the resulting rectangle is on a currently attached work area.
3. **Given** the emulator is launched after the slice merges, **When** a maintainer changes and reopens every Settings page, **Then** behavior is indistinguishable from before the move.

---

### User Story 2 - The already-pure strays move out (Priority: P1)

A maintainer can exercise the small self-contained modules that are stranded in the exe purely by accident of where they were written.

Named in issue #85 and confirmed by audit: `TrackSectorPredicate`, `DebugDialogProjection`, `Disk2DebugDialogState`, `InputDebugDialogState`, `DiskSettings`, `InputEventDisplay`, `Disk2EventDisplay`, `PerfStats`, and the resolution/catalog half of `AssetBootstrap`. None of them own a window or a device.

**Why this priority**: no seam has to be invented and no caller has to change shape; these are file moves plus project references. It is the fastest visible reduction in exe size and it is safe to interleave with Story 1.

**Independent Test**: each moved module gets a test file in `UnitTest/` that constructs it from synthetic data and asserts its outputs, with no exe involvement.

**Acceptance Scenarios**:

1. **Given** a synthetic track/sector geometry, **When** the predicate is evaluated at its boundaries, **Then** the results match the documented rule at each boundary.
2. **Given** a synthetic event stream, **When** the debug projections format it, **Then** the produced rows match expected text exactly, including the empty and overflow cases.

---

### User Story 3 - Layout, pacing and input mapping leave `EmulatorShell` (Priority: P2)

A maintainer changes chrome layout, frame-publish pacing, or host-to-guest input mapping and gets a test failure when the math is wrong, instead of discovering it as a visual or feel regression.

This is the first cut into the god object and takes the parts that are already pure functions of their inputs: viewport and chrome-band rectangles, client-size-for-content inversion, drive-widget row placement, work-area centering; the publish-rate throttle and the dirty-render signature gate; the VK classifiers, Apple modifier mirroring, joystick axis/button staging, paddle recenter math, and the absolute guest-mouse clamp-window mapping.

**Why this priority**: it is where extraction starts costing real analysis, so it follows the two slices that establish the pattern. It is also where regressions would be felt rather than seen, which is exactly the class of bug tests are good at holding.

**Independent Test**: feed synthetic client sizes, DPI values, band thicknesses, video-mode/flash/color state and key/pointer events; assert the resulting rectangles, publish decisions and staged guest state. No `HWND`.

**Acceptance Scenarios**:

1. **Given** a client size and DPI, **When** the viewport rectangle is computed and then inverted back to a client size, **Then** the round trip returns the original size for every supported DPI.
2. **Given** an unchanged screen (same video mode, flash phase, color state and clean video RAM), **When** the render gate is evaluated, **Then** it declines to re-rasterize; changing any one of those inputs makes it re-rasterize.
3. **Given** a machine with one connected drive, **When** the drive row is laid out, **Then** the single widget is centered rather than offset with a gap where a second would sit.

---

### User Story 4 - Shell managers move behind seams (Priority: P2)

A maintainer can test what happens on machine switch, disk mount, MRU update, clipboard round-trip, and screenshot capture without running the emulator.

`Casso/Shell/` is 9,125 lines: `WindowCommandManager`, `MachineManager`, `DiskManager`, `CpuManager`, `ClipboardManager`, `ScreenshotCapture`, `DiskMru`, `WindowManager`, `ModernPrintDialog`. Under Principle VI the clipboard round-trip and the image encode are core work behind seams; only the OS-owned print and file dialogs and the window itself stay behind.

**Why this priority**: these hold real lifecycle decisions and known-fragile ordering (the debug-sink reattach after a machine switch is a shipped bug fix with no test guarding it), but they depend on the seam pattern from Stories 1–2.

**Independent Test**: drive each manager with mock sinks and a synthetic file system; assert command dispatch, mount/eject outcomes, MRU ordering and de-duplication, and the encoded screenshot bytes.

**Acceptance Scenarios**:

1. **Given** an open debug panel bound to the current machine, **When** the machine is switched, **Then** the panel is re-attached to the new controller and audio source, asserted by a test rather than by hand.
2. **Given** an MRU list at capacity, **When** an already-present entry is mounted again, **Then** it moves to the front without duplicating and without evicting an unrelated entry.
3. **Given** a synthetic framebuffer, **When** a screenshot is captured, **Then** the encoded image decodes back to the same pixels at the expected dimensions.

---

### User Story 5 - UI state logic separates from UI painting (Priority: P3)

A maintainer changes what a Settings page, chrome band, or desk-scene layout *decides* and tests that decision, leaving only the painting in the exe.

`Casso/Ui/` totals 47,306 lines across its subdirectories. Page state, validation, enable/disable rules, scene layout arithmetic and chrome state synchronization are data-in/data-out; the draw calls that consume them are not.

**Why this priority**: the largest area by line count but the lowest per-line density of consequential decisions, and it benefits from every seam the earlier slices establish.

**Independent Test**: construct each page/scene state object directly, apply changes, assert the resulting state and any emitted apply commands, with no painter and no window.

**Acceptance Scenarios**:

1. **Given** a Settings page whose control is disabled by another control's value, **When** the governing value changes, **Then** the dependent control's enablement changes accordingly, asserted without painting.
2. **Given** a client size and scene scale, **When** the desk-scene layout runs, **Then** every element rectangle falls within the scene bounds at every supported DPI.

---

### User Story 6 - `EmulatorShell` becomes a Win32 driver over a core machine (Priority: P3)

A maintainer can build a machine, run it, step it, reset it, power-cycle it, and observe its soft-switch state from a unit test, with no window and no graphics device.

This is the headline of issue #85: a core machine/system façade owning device construction, the run/step/pause lifecycle, reset and power-cycle semantics, and soft-switch state. `EmulatorShell` keeps the `HWND`, its message pump, and the marshaling between the OS thread and the core.

**Why this priority**: the largest and riskiest cut, and the one every earlier slice makes smaller. Attempting it first is the failure mode this phasing exists to avoid.

**Independent Test**: build a machine headlessly in a test, run a fixed number of cycles, assert memory and soft-switch state; power-cycle and assert DRAM re-seeding differs from a soft reset, which preserves user RAM.

**Acceptance Scenarios**:

1. **Given** a machine built headlessly, **When** it is soft-reset, **Then** user RAM is preserved and the reset vector is taken.
2. **Given** the same machine, **When** it is power-cycled, **Then** every DRAM-owning device is re-seeded before the reset sequence runs.
3. **Given** a paused machine, **When** a single instruction is stepped, **Then** exactly one instruction retires and the program counter advances accordingly.

---

### Edge Cases

- **A slice is larger than expected and cannot merge cleanly.** It splits further rather than growing; a slice that cannot ship on its own is not a slice.
- **A move surfaces a latent defect** (as the `CassoCli` extraction did, twice). The defect is fixed in the same slice, gets its own test, and is recorded in `CHANGELOG.md` as a fix — the extraction itself is not changelog material.
- **A move would change user-visible behavior.** It must not, except where the behavior is the defect above. Any deliberate behavior change is out of scope and is filed separately.
- **Code appears to need a platform API and therefore "belongs" in the exe.** That reasoning is explicitly rejected by Principle VI; the API goes behind a seam and the logic moves.
- **Genuinely untestable code is found in an area being moved.** Only the irreducible edge stays; it is left behind as a thin platform implementation of the seam, and what remains in the exe after each slice is recorded so it can be challenged.
- **Master takes a sweeping rename while a slice is in flight** (four have landed since August 2026). Slices stay short-lived and merge master early; see the hazard note in `CLAUDE.md`.

## Requirements *(mandatory)*

### Functional Requirements

**Placement**

- **FR-001**: All logic the `UnitTest` project can link and exercise MUST live in a core library that both `Casso.exe` and `UnitTest` link.
- **FR-002**: Placement decisions MUST be made on UT-reachability alone. That a piece of code calls a platform API MUST NOT be accepted as a reason to leave it in the exe.
- **FR-003**: After all slices, `Casso.exe` MUST retain only: the entry point, the `HWND` and its message pump, the graphics device / swap chain / present loop, the audio device endpoint, OS-owned dialogs, and the concrete platform implementations of injected seams.
- **FR-004**: Every decision left in the exe MUST be one that cannot be asserted — no branch on user data, no format selection, no exit status, no default resolution.

**Phasing**

- **FR-005**: The work MUST be delivered as independently shippable slices, each merging to master on its own with the emulator fully working.
- **FR-006**: Each slice MUST be behavior-preserving from a user's point of view, except where it fixes a defect the extraction exposed.
- **FR-007**: Slices MUST NOT be folded into unrelated feature branches (issue #85 non-goal).

**Testing**

- **FR-008**: Every module moved into core MUST gain unit tests in the same slice that moves it. A move without tests does not count as done.
- **FR-009**: Tests for moved code MUST obey Test Isolation: no real files, registry, network, processes, or system APIs; all such access injected behind seams and mocked.
- **FR-010**: Where a move exposes a defect, a test that fails against the pre-move behavior MUST accompany the fix.

**Evidence and regression control**

- **FR-011**: Each slice MUST record measured before/after line counts for `Casso.exe` and the receiving core library, so progress is observable rather than asserted.
- **FR-012**: Each slice's Constitution Check MUST cite what was verified for Principle VI (which project now holds the moved code, what stayed in the exe, and why) rather than recording a bare PASS. A check that only ever says PASS is not a check.
- **FR-013**: New logic added anywhere in the tree while this work is in flight MUST land in core, so the exe does not regrow behind the extraction.

**Documentation**

- **FR-014**: `CHANGELOG.md` MUST receive entries only for defects fixed and for user-visible changes, never for the extraction itself.
- **FR-015**: On completion, issue #85 MUST be closable with the final measurements and the list of what remains in the exe, each remaining item justified against FR-003.

### Key Entities

- **Slice**: one independently shippable extraction — a set of modules, their new home, their backfilled tests, and its before/after measurement.
- **Seam**: an interface that lets a test substitute for an OS service, with the concrete platform implementation left behind in the exe. `IFileSystem` / `Win32FileSystem` in `Casso/Config/` is the existing example.
- **Irreducible edge**: the enumerated set in FR-003; the only legitimate reason for code to remain in the exe.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: `Casso.exe` falls from 93,927 lines to under 6,000, and from 189 files to under 25. The floor is set by the device objects and the message pump, which today measure roughly 4,400 lines across the entry point, the renderer, the post-process chain and the audio endpoint; the target allows headroom over that and nothing more.
- **SC-002**: The exe contains zero functions that branch on user data, resolve a default, choose a format, or determine an exit status.
- **SC-003**: Every module moved out of the exe is covered by unit tests in the same slice that moves it; the count of moved modules without tests is zero at every merge point.
- **SC-004**: A maintainer can build a machine, run it, reset it, power-cycle it and observe its state from a unit test, with no window and no graphics device.
- **SC-005**: Every slice merges to master with the emulator fully working and with no user-visible behavior change other than defects fixed.
- **SC-006**: The full test suite stays green at every merge point, in both Debug and Release, on x64.
- **SC-007**: The completed work is traceable: for every slice, the recorded before/after measurement and the Principle VI evidence exist and are checkable by a later reader.

## Assumptions

- **Receiving library.** Extracted code goes to the existing `CassoCore` / `CassoEmuCore` (and `Dxui` for framework-level UI concerns). Whether any slice warrants a new library project instead is a planning decision, deferred to `/speckit-plan`.
- **`Dxui` is out of scope.** It is already a separate, unit-tested library and is the proof the pattern works, not a target.
- **Slice order is a default, not a contract.** P1 → P3 reflects risk and dependency, and a later slice may be pulled forward if it becomes cheap; the constraint that binds is that each slice ships on its own.
- **No user-visible change is intended.** Users should not be able to tell any slice shipped, apart from defect fixes.
- **ARM64 is build-only.** x64 Debug and Release green is the acceptance bar for test execution; ARM64 must compile.
- **The 15,995-line `EmulatorShell.cpp` will not be extracted in one pass.** Stories 3 and 6 both cut into it, and Story 6 may itself need further subdivision at planning time.
- **This is engineering work with no end-user feature.** Its beneficiaries are maintainers, and its payoff is defects found before release, as the `CassoCli` extraction demonstrated.
