# Implementation Plan: Disk II Debug Window

**Branch**: `feature/006-disk-ii-debug` | **Date**: 2026-05-17 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification at `specs/006-disk-ii-debug/spec.md`
**Research basis**: [`research.md`](./research.md) — Disk II nibble-format anchors, Win32 virtual-mode ListView idioms, and SPSC ring patterns
**Constitution**: `.specify/memory/constitution.md`

## Summary

Add a modeless **Disk II Debug** window to the Casso shell that surfaces a
live, scrolling, append-only log of `DiskIIController` events for developers
diagnosing disk behavior — head-stuck-spinning, RWTS-failing-to-find-an-
address-mark, custom-protected-loaders-reading-non-standard-sectors. The
window opens from **View → Disk II Debug...** (Ctrl+Shift+D), hosts a Win32
ListView in virtual + report mode, and tails events at ~30 Hz with an
auto-tail scroll heuristic that pins newest rows to the visible bottom
unless the user has scrolled back into history.

The implementation is structured around four new components in `CassoEmuCore`
(`IDiskIIEventSink`, `IDriveAudioEventSink`, `DiskIIEventRing`,
`DiskIIAddressMarkWatcher`) and one new dialog in the `Casso` shell
(`DiskIIDebugDialog`). The `DiskIIController` gains a single
`IDiskIIEventSink*` pointer (default `nullptr`) plus fire sites at every
existing motor/head/mount call site, guarded so that a no-sink build is
100% byte-identical to the pre-feature controller. The motor lifecycle
is split across **four** events (`OnMotorCommandOn` / `OnMotorEngaged` /
`OnMotorCommandOff` / `OnMotorDisengaged`) so consumers can distinguish
strobes from logical engagement edges — see FR-006 and A-009.
`DiskIIAudioSource` gains a parallel `IDriveAudioEventSink*` pointer
that fires one of six audio-decision outcomes (Started / Restarted /
Continued / Silent / LoopStarted / LoopStopped) from inside its existing
controller-event hooks; the dialog implements **both** sinks. A lightweight
passive nibble-stream watcher decodes 4-and-4 address marks and detects
6-and-2 data-mark prologues without disturbing the existing read path.

Cross-thread delivery uses a single fixed-capacity (4,096-entry) lock-free
SPSC ring buffer shared by both sinks; each ring entry carries an
`EventCategory` tag (`Controller` / `Audio`) so the consumer's formatter
routes per category, and the events interleave chronologically by cycle
counter in one unified ListView. The CPU emulation thread is the sole
producer, the UI thread is the sole consumer (drained on a ~30 Hz timer).
Overflow drops events with a single coalesced `[N events lost]` marker on
the next drain. The UI thread maintains a
`std::deque<DiskIIEventDisplay>` of formatted entries (rolling cap
100,000) that the virtual-mode ListView pulls from via `LVN_GETDISPINFO`.

This feature is independent of #51 (interactive debugger engine) and #59
(GUI debugger panel) — it is a passive event log, not a CPU debugger.

## Technical Context

**Language/Version**: C++ stdcpplatest, MSVC v145 (VS 2026)
**Primary Dependencies**: Windows SDK only — User32 (modeless dialog +
accelerators), ComCtl32 v6 (virtual-mode ListView). No new third-party
libraries.
**Storage**: None. v1 does not persist filter / pause / column-width /
column visibility state across launches (NFR-006). Persistence via
the existing `HKCU\Software\relmer\Casso\Machines\<MachineName>\`
registry tier is a follow-up.
**Testing**: Microsoft C++ Unit Test Framework in `UnitTest/`. New
`DiskIIEventRingTests.cpp`, `DiskIIAddressMarkWatcherTests.cpp`,
`DiskIIControllerEventTests.cpp` (extends the existing controller test
surface), and `DiskIIDebugDialogTests.cpp` (projection-logic-only — no
real `HWND`, no real ListView, no real timer per NFR-002).
**Target Platform**: Windows 10/11, x64 and ARM64
**Project Type**: Existing 5-project solution; new code in `CassoEmuCore`
(library: event sink interface, ring, watcher) and `Casso` (GUI shell:
debug dialog). No new project.
**Performance Goals**: Event-fire path on CPU thread MUST add < 1 % to
total CPU-thread time at DOS 3.3 boot peak (~150 events/sec). Per-fire
cost: one `nullptr` check + (if attached) one SPSC ring push (two
unsynchronized loads, one store, one release update). Address-mark watcher
per-nibble cost: one small switch statement + at most one state-machine
transition.
**Constraints**: No regression in `DiskIIController` byte-for-byte behavior
when the sink is unattached (FR-007, FR-020, SC-007). No locks, no
condition variables, no kernel-level sync on the producer or consumer
fast paths (FR-009). Tests do not touch the filesystem or open a real
HWND (NFR-002).
**Scale/Scope**: ~8 new source files (~900–1,300 LOC: 5 controller/audio
contracts + dialog + projection + filter parser), ~6 touched files
(controller, audio source, shell, menu, resource, machine-shell for
Uptime anchor reseed), ~50–75 new test cases. New modeless dialog
(small, programmatically constructed — no .rc dialog template).

## Constitution Check

### Principle I — Code Quality (NON-NEGOTIABLE)

| Rule                                              | How this plan complies                                                                                                                  |
|---------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------|
| Pch.h-first include                               | Every new `.cpp` includes `"Pch.h"` as the first include. No angle-bracket includes outside `Pch.h`. STL/Windows headers go via `Pch.h`. |
| Formatting Preservation                           | All edits to existing files are surgical insertions at the line ranges named in `research.md` §3; no existing aligned blocks are touched. |
| EHM on fallible functions                         | Only the dialog's `WM_INITDIALOG` Win32 wiring is fallible (`CreateWindowEx`); it uses the standard `HRESULT hr = S_OK; … Error: … return hr;` pattern. Ring push/pop, watcher transitions, and event-sink fire methods are all infallible (`void`). |
| No calls inside macro arguments                   | Store results first; reviewed per PR.                                                                                                   |
| Single exit via `Error:` label                    | Standard pattern in every new fallible function (dialog setup only).                                                                    |
| Avoid Nesting (≤ 2-3 indent beyond EHM)           | Watcher state machine factored into per-state helper methods (`StepAddrMarkState`, `StepDataMarkState`), each ≤ 2 levels. Drain loop factored into `DrainRing` + `ProjectToView` helpers. |
| Variable Declarations at Top of Scope             | All new functions follow this; column-aligned per project rules (type / `*`-`&` column / name / `=` / value). |
| Function Comments in `.cpp` Only                  | Headers carry only declaration comments; doc blocks live in `.cpp` with 80-`/` delimiters.                                              |
| Function Spacing — `func()` vs `func (a, b)`      | Verified via `rg -n '\w \(\)' CassoEmuCore/Devices/ Casso/` before commit on every PR phase.                                            |
| Smart Pointers                                    | `DiskIIDebugDialog` is owned by `EmulatorShell` directly (composition); no smart pointer needed. `DiskIIEventRing` and `DiskIIAddressMarkWatcher` are direct members of `DiskIIController` / the dialog respectively. |
| PascalCase file names                             | All new source files (`IDiskIIEventSink.h`, `DiskIIEventRing.{h,cpp}`, `DiskIIAddressMarkWatcher.{h,cpp}`, `DiskIIDebugDialog.{h,cpp}`) and test files use PascalCase with no underscores. |
| No magic numbers                                  | `kEventRingCapacity = 4096`, `kDisplayDequeCap = 100000`, `kDrainTimerHz = 30`, `kDrainTimerMs = 33`, `kAddrMarkPrologue0 = 0xD5`, `kAddrMarkPrologue1 = 0xAA`, `kAddrMarkPrologue2 = 0x96`, `kDataMarkPrologue2 = 0xAD`, `kSectorEpilogue0 = 0xDE`, `kSectorEpilogue1 = 0xAA`, `kSectorEpilogue2 = 0xEB`, `kDataNibbleCount = 342`, `kFilterTextDebounceMs = 250` (Track/Sector text-input debounce per FR-014d), `kFilterRichEditClass = L"RICHEDIT50W"` and `kFilterRichEditModule = L"msftedit.dll"` (Track/Sector RichEdit class + module per FR-014e), `kFilterSquiggleUnderlineColor = CFU_UNDERLINECOLOR_RED` (per FR-014e), and column-width constants `kColWallWidth = 110`, `kColUptimeWidth = 90`, `kColCycleWidth = 110`, `kColEventWidth = 110`, `kColDetailWidth = 360` (sum ≈ 790 px), are all named constants. |
| Hungarian for file-scope statics                  | Any file-scope statics in dialog code (e.g., a `static const wchar_t* g_pszWindowClass`) use Hungarian per constitution.                |
| `std::numbers` for math constants                 | No math constants needed by this feature (ring/deque are integer-indexed; time formatting uses integer ms).                            |
| American English                                  | Verified (e.g., "behavior", not "behaviour"; "canceled", not "cancelled").                                                              |

### Principle II — Test Isolation (NON-NEGOTIABLE)

All tests run on the same headless harness as existing `CassoEmuCore`
tests. The ring buffer, watcher, and event-sink mock are pure data
structures with no Win32 / filesystem dependencies. The dialog's
projection logic (ring drain → deque projection → filtered-view computation)
is factored into a non-Win32 helper class (`DebugDialogProjection` or
equivalent) that tests instantiate directly; the Win32 wiring layer
(`WM_INITDIALOG`, `WM_TIMER`, `LVN_GETDISPINFO`, button handlers) is
exercised by manual / integration testing only and is intentionally thin.

### Principle V — Function Size & Structure

The longest behavior-carrying function is `DiskIIAddressMarkWatcher::ObserveNibble`,
which factors as:

```
ObserveNibble (nibble)
  StepAddrMarkState (nibble)                       // ≤ 30 LOC
  StepDataMarkState (nibble)                       // ≤ 30 LOC
```

`DebugDialogProjection::DrainAndProject`:

```
DrainAndProject (ring, deque, filterMask)
  while ring.TryPop (event):                       // ≤ 5 LOC
      FormatEvent (event, displayEntry)            // ≤ 25 LOC
      deque.push_back (displayEntry)
      if deque.size > kDisplayDequeCap:
          deque.pop_front()
  // filter projection is lazy at GETDISPINFO time
```

All helpers stay under ~30 LOC.

### Principle III — Concurrency Discipline

The CPU thread is the sole producer; the UI thread is the sole consumer.
The SPSC ring uses `std::atomic<uint32_t>` head/tail indices with
`memory_order_relaxed` loads on the same-side index and
`memory_order_acquire` / `memory_order_release` on the cross-side index.
No mutex, no condition variable, no critical section. The sink pointer
on `DiskIIController` is a plain `IDiskIIEventSink*` published via
`SetEventSink` only when the dialog is open and revoked (set to `nullptr`)
before the dialog tears down the ring; the publish/revoke is gated by
the dialog's create/destroy lifecycle, which happens on the UI thread
while the CPU thread is paused at slice boundaries (mirrors the existing
sink-attach pattern from Feature 005's drive-audio wiring).

## Architecture

### Components

```
                ┌──────────────────────────────────┐
                │  DiskIIController                │  (CassoEmuCore/Devices)
                │   ─ HandleSwitch                 │──┐
                │   ─ HandlePhase                  │  │ IDiskIIEventSink*
                │   ─ Tick (spindown)              │  │ (default nullptr)
                │   ─ MountImage / EjectImage      │  │
                │   ─ NibbleReadByte ──────────────│──┼─► DiskIIAddressMarkWatcher
                │   ─ NibbleWriteByte              │  │     ─ ObserveNibble (nibble)
                │   ─ SetEventSink                 │  │     ─ fires OnAddressMark,
                └──────────────────────────────────┘  │       OnDataMarkRead via
                                                      │       same m_eventSink
                                                      ▼
                ┌────────────────────────────────────────────────┐
                │  IDiskIIEventSink (abstract, 12 methods)       │  (CassoEmuCore/Devices)
                │   OnMotorCommandOn / OnMotorEngaged            │
                │   OnMotorCommandOff / OnMotorDisengaged        │
                │   OnHeadStep (prevQt, newQt)                   │
                │   OnHeadBump (atQt)                            │
                │   OnAddressMark (track, sector, volume)        │
                │   OnDataMarkRead (sector, bytesRead)           │
                │   OnDataMarkWrite (sector, bytesWritten)       │
                │   OnDriveSelect (drive)                        │
                │   OnDiskInserted (drive) / OnDiskEjected (drv) │
                │   [future: OnMotorAtSpeed — see A-009]         │
                └────────────────────────────────────────────────┘
                                       ▲
                                       │ implements (Controller side)
                                       │

                ┌──────────────────────────────────┐
                │  DiskIIAudioSource               │  (CassoEmuCore/Audio)
                │   ─ existing OnMotorEngaged hook │──┐
                │   ─ existing OnMotorDisengaged   │  │ IDriveAudioEventSink*
                │   ─ existing OnHeadStep hook     │  │ (default nullptr)
                │   ─ existing OnHeadBump hook     │  │
                │   ─ existing OnDiskInserted hook │  │
                │   ─ existing OnDiskEjected hook  │  │
                │   ─ SetAudioEventSink            │  │
                └──────────────────────────────────┘  │
                                                      ▼
                ┌────────────────────────────────────────────────┐
                │  IDriveAudioEventSink (abstract, 6 methods)    │  (CassoEmuCore/Audio)
                │   OnAudioStarted    (kind, drive)              │
                │   OnAudioRestarted  (kind, drive)              │
                │   OnAudioContinued  (kind, drive)              │
                │   OnAudioSilent     (kind, drive, reason)      │
                │   OnAudioLoopStarted (kind, drive)             │
                │   OnAudioLoopStopped (kind, drive)             │
                │     SoundKind   ∈ {MotorLoop, HeadStep,        │
                │                    HeadStop, DoorOpen,         │
                │                    DoorClose}                  │
                │     SilentReason ∈ {DriveAudioDisabled,        │
                │                     BufferMissing,             │
                │                     NoSourceRegistered,        │
                │                     ColdBootSuppression}       │
                └────────────────────────────────────────────────┘
                                       ▲
                                       │ implements (Audio side)
                                       │
                ┌──────────────────────┴─────────────────────────┐
                │  DiskIIDebugDialog                             │  (Casso)
                │  : IDiskIIEventSink, IDriveAudioEventSink      │
                │   ─ owns DiskIIEventRing (4096 entries,        │
                │     shared by both sinks; entries tagged       │
                │     with EventCategory)                        │
                │   ─ owns std::deque<DiskIIEventDisplay> (≤100k)│
                │   ─ filter state (event-type checkboxes,       │
                │     Drive radio, Track/Sector text inputs,     │
                │     Audio sub-checkboxes), pause flag,         │
                │     logical column model std::array            │
                │     <LogicalColumn, 5> (id/header/default-     │
                │     Width/savedWidth/visible) per FR-026       │
                │   ─ DebugDialogProjection (testable helper)    │
                │                                                │
                │   CPU thread → IDiskIIEventSink methods        │
                │      → ring.TryPush (DiskIIEvent[Controller])  │
                │   CPU thread → IDriveAudioEventSink methods    │
                │      → ring.TryPush (DiskIIEvent[Audio])       │
                │   UI thread → WM_TIMER (~33 ms)                │
                │      → projection.DrainAndProject              │
                │      → ListView_SetItemCountEx                 │
                │      → (if at tail) ListView_EnsureVisible     │
                └────────────────────────────────────────────────┘
                                       ▲
                                       │ owned by
                                       │
                ┌──────────────────────┴──────────────┐
                │  EmulatorShell                      │  (Casso)
                │   ─ owns DiskIIDebugDialog (opt.)   │
                │   ─ View → Disk II Debug... handler │
                │   ─ wires SetEventSink on open      │
                │     AND SetAudioEventSink (FR-024)  │
                │   ─ revokes both sinks on close     │
                │   ─ owns Uptime anchor; reseeds it  │
                │     on SoftReset / PowerCycle       │
                │     (FR-004a)                       │
                └─────────────────────────────────────┘
                                       ▲
                                       │ View menu + Ctrl+Shift+D
                                       ▼
                ┌─────────────────────────────────────┐
                │  MenuSystem + Casso.rc/resource.h   │  (Casso)
                │   IDM_VIEW_DISKII_DEBUG             │
                │   View → Disk II Debug...           │
                │   Accelerator: Ctrl+Shift+D         │
                └─────────────────────────────────────┘
```

### Data Flow Per Event

1. **CPU thread**: an emulation slice runs inside `ExecuteCpuSlices()`.
2. `DiskIIController::HandleSwitch` / `HandlePhase` / `Tick` / `MountImage` /
   `EjectImage` reaches a fire site. Each site is:
   ```cpp
   if (m_eventSink != nullptr) {
       m_eventSink->OnHeadStep (prevQt, newQt);
   }
   ```
   The motor switch (`$C0E8` / `$C0E9`) fires both the unconditional
   `OnMotorCommandOff` / `OnMotorCommandOn` AND, on transition, the
   `OnMotorDisengaged` / `OnMotorEngaged` edge events.
3. `DiskIIDebugDialog::OnHeadStep` (or any `IDiskIIEventSink` /
   `IDriveAudioEventSink` method — the dialog implements both interfaces
   per FR-024) packs a `DiskIIEvent` POD (`EventCategory` tag +
   event-type enum + cycle counter snapshot + integer payload union)
   and calls `m_ring.TryPush (event)`. On ring full, the push returns
   `false`; the dialog increments a `m_droppedSinceLastDrain` counter
   (atomic, written only on the CPU thread).
4. The nibble-read path (`DiskIIController::NibbleReadByte`) calls
   `m_addrMarkWatcher.ObserveNibble (nibble)` on every nibble that goes
   to the CPU. The watcher's state machines may fire
   `m_eventSink->OnAddressMark` or `m_eventSink->OnDataMarkRead` from
   inside `ObserveNibble`, which routes through the same SPSC ring push.
4a. Independently, the audio path: when `DiskIIAudioSource` receives a
   controller-event callback (`OnMotorEngaged`, `OnHeadStep`, etc.) it
   makes its audio decision exactly as before, then — if
   `m_audioEventSink != nullptr` — fires exactly one of the six
   `IDriveAudioEventSink` methods describing the outcome (Started /
   Restarted / Continued / Silent / LoopStarted / LoopStopped) with the
   `SoundKind` that was decided on. These events ALSO route through the
   dialog's `m_ring` via the dialog's `IDriveAudioEventSink`
   implementation; each ring entry's `EventCategory` tag is `Audio`.
5. **UI thread**: every ~33 ms, `WM_TIMER` fires. The handler calls
   `m_projection.DrainAndProject (m_ring, m_deque, m_droppedSinceLastDrain, m_uptimeAnchor)`,
   which:
   - Pops every available event from the ring into the deque, formatting
     each into a `DiskIIEventDisplay` (time string, cycle string with
     thousands separators, event-type enum, detail string).
   - If `m_droppedSinceLastDrain > 0`, inserts a single synthetic
     `[N events lost]` entry into the deque at the position where the
     loss occurred (between the last surviving pre-overflow event and the
     first survivor after the overflow).
   - If the deque size exceeds `kDisplayDequeCap` (100,000), `pop_front`-s
     entries until the cap is restored.
6. The handler captures the current "at tail" state by computing
   `(ListView_GetTopIndex + ListView_GetCountPerPage) >= prevDequeSize`,
   then calls `ListView_SetItemCountEx (lv, deque.size(), LVSICF_NOSCROLL)`.
   If "at tail" was true, it calls `ListView_EnsureVisible (lv, deque.size() - 1, FALSE)`.
7. The ListView's `LVN_GETDISPINFO` handler responds to row-fetch requests
   by indexing into a per-filter index vector — see "Filter Projection
   Architecture" below — and then into `m_deque` (O(1) random access),
   returning the appropriate string for the requested column. The
   ListView's column index is the **visible-subset ordinal** (per
   FR-026's logical column model — hidden columns are absent from the
   ListView entirely, not zero-width). The handler maps the visible
   ordinal back to the `LogicalColumn::id` via the active visible-subset
   list.

### Filter Projection Architecture

Per FR-014, the dialog supports a composed (logical AND) filter built
from event-type checkboxes, a Drive radio, free-form Track/Sector text
inputs, and Audio sub-checkboxes. The composition is applied at the
**deque-to-display projection** stage, NOT at ring-drain time, so a
filter toggled off-then-on reveals events that arrived in the off
window in their original chronological position (acceptance criterion
in User Story 3).

Implementation outline:

```cpp
struct FilterState {
    uint32_t                eventTypeMask;     // bit per event-type checkbox
    int                     driveFilter;       // 0=All, 1=Drive 1, 2=Drive 2
    TrackSectorPredicate    trackFilter;       // parsed from text input
    TrackSectorPredicate    sectorFilter;      // parsed from text input
    bool                    audioMaster;
    bool                    audioStarted;      // sub-toggles, only active when audioMaster
    bool                    audioRestarted;
    bool                    audioContinued;
    bool                    audioSilent;
    bool                    trackFilterRawQt;  // FR-014a "raw qt" toggle
};

bool MatchesFilter (const DiskIIEventDisplay& e, const FilterState& f);
```

`TrackSectorPredicate` is a parsed representation of an FR-014a/b
expression. The parser handles integers, decimal quarter-tracks
(`.0`/`.25`/`.5`/`.75`), inclusive ranges (`a-b`), and comma-separated
lists; malformed tokens are silently dropped (no exception, no UI
block) but their **byte ranges within the original expression are
recorded** on the returned `TrackSectorPredicate` (FR-014e) so the
dialog can squiggle them in the RichEdit. The predicate stores a small
`std::vector<Range>` and answers `Matches (int value)` in O(N) where
N is typically 1–4 ranges, plus a `std::vector<RejectedSpan>` (each
`{int beginUtf16, int endUtf16}`) used only for the FR-014e visual
treatment.

### Track / Sector RichEdit + squiggle (FR-014e)

The Track and Sector inputs are **RichEdit 4.1** controls — class
`RICHEDIT50W`, registered by loading `msftedit.dll` exactly once in
the dialog's `Create` path (`LoadLibraryW(kFilterRichEditModule)`).
The handle is leaked intentionally (process-lifetime cost is
negligible and standard for RichEdit usage).

At every debounce-flush moment (FR-014d) the dialog re-parses the
input via `TrackSectorPredicate::Parse`, then:

1. Saves the current selection (`EM_EXGETSEL`).
2. Selects `{0, -1}` and applies a "default formatting" `CHARFORMAT2W`
   that clears `CFM_UNDERLINETYPE | CFM_UNDERLINECOLOR` (so a token the
   user just fixed loses its squiggle).
3. For each `RejectedSpan` returned by the parser, selects that range
   and applies a `CHARFORMAT2W` with
   `dwMask = CFM_UNDERLINETYPE | CFM_UNDERLINECOLOR`,
   `bUnderlineType = CFU_UNDERLINEWAVE`,
   `bUnderlineColor = kFilterSquiggleUnderlineColor` (=
   `CFU_UNDERLINECOLOR_RED`).
4. Restores the saved selection (`EM_EXSETSEL`).
5. Updates the static label beneath the input. When the rejected list
   is empty the label text is set to the empty string; otherwise it
   reads `Ignored: <token>, <token>` (UTF-16, original-cased tokens
   joined by `, `).

The visual treatment is a polish layer over FR-014a / FR-014b's
silent-drop semantics: the parser still drops bad tokens and the
projection rebuild still runs on the valid subset.

The dialog maintains an `std::vector<uint32_t> m_filteredIndices` that
maps display-row → deque index. The vector is rebuilt:
- On any filter-state change (`WM_COMMAND` for checkboxes/radio,
  `EN_CHANGE` for text inputs).
- Incrementally on drain: newly-appended events are matched against the
  current filter and the new indices appended.

`ListView_SetItemCountEx` is called with `m_filteredIndices.size()`,
not `m_deque.size()`. `LVN_GETDISPINFO` resolves
`m_deque[m_filteredIndices[iItem]]`.

### Column Show/Hide (FR-026 / FR-027 / NFR-006)

The dialog owns a **logical column model** — `std::array<LogicalColumn, 5> m_columns` covering all five user-facing columns in fixed id order (Wall, Uptime, Cycle, Event, Detail):

```cpp
struct LogicalColumn {
    int       id;            // 0..4, matches the column-id enum
    LPCWSTR   headerText;
    int       defaultWidth;  // from kCol*Width named constants
    int       savedWidth;    // last known good width (user-dragged or auto-sized)
    bool      visible;       // user toggle, default true
    bool      autoSizedYet;  // FR-027: false until first ShowColumn
};
```

The ListView holds only the currently-visible subset of columns, in id order. Hidden columns are absent from the ListView entirely; this avoids zero-width-column edge cases and cooperates cleanly with any later auto-sizing.

**Toggle algorithm** (FR-026):

```cpp
void ToggleColumn (int id)
{
    // Capture current widths back into the logical model so a drag
    // that happened before this toggle is preserved.
    CaptureCurrentWidthsIntoModel ();

    m_columns[id].visible = !m_columns[id].visible;
    RebuildListViewColumns ();
}

void RebuildListViewColumns ()
{
    while (ListView_DeleteColumn (m_lv, 0))
    {
        // loop until no columns left
    }

    int  virtualIdx = 0;

    for (LogicalColumn & col : m_columns)
    {
        if (!col.visible)
        {
            continue;
        }

        LVCOLUMNW  lvc      = {};
        lvc.mask            = LVCF_TEXT | LVCF_WIDTH;
        lvc.pszText         = const_cast<LPWSTR> (col.headerText);
        lvc.cx              = col.savedWidth;

        ListView_InsertColumn (m_lv, virtualIdx, &lvc);

        // FR-027: first-time-shown columns get a one-time
        // auto-size-to-header pass.
        if (!col.autoSizedYet)
        {
            ListView_SetColumnWidth (m_lv, virtualIdx, LVSCW_AUTOSIZE_USEHEADER);
            col.savedWidth    = ListView_GetColumnWidth (m_lv, virtualIdx);
            col.autoSizedYet  = true;
        }

        virtualIdx++;
    }
}
```

**User-dragged width capture**: a header `HDN_ENDTRACK` notification triggers `CaptureCurrentWidthsIntoModel ()` — walks the ListView's current columns, maps each back to the corresponding `LogicalColumn` via the visible-subset order, and writes the current width into `savedWidth`. This ensures a subsequent hide/show cycle restores whatever width the user last picked.

The header right-click handler builds a `TrackPopupMenu` with five `MF_CHECKED`/`MF_UNCHECKED` items (one per `LogicalColumn`, checked iff `visible`). Selection invokes `ToggleColumn(id)`.

NFR-006 forbids cross-session persistence in v1; closing the dialog destroys `m_columns` and the next open re-seeds from defaults (and re-runs auto-sizing on first show).

### Uptime Anchor (FR-004a)

`EmulatorShell` owns `std::chrono::steady_clock::time_point m_uptimeAnchor`,
initialized on shell construction. The existing `MachineShell::SoftReset`
and `MachineShell::PowerCycle` handlers each gain a single line:

```cpp
m_shell->ResetUptimeAnchor();   // m_uptimeAnchor = steady_clock::now();
```

The debug dialog reads the anchor through a shell-provided accessor on
each event-format pass; the Uptime column shows `(now - anchor)`
formatted as `MM:SS.mmm`. No new thread synchronization is needed —
the anchor is written on the UI thread (reset is initiated by user
input) and read on the UI thread (the dialog's `WM_TIMER` drain).

### Event Trigger Specification

Normative for this feature. Controller-side events (`EventCategory::Controller`):

| Event                  | Trigger                                                                  | Call site                                                      |
|------------------------|--------------------------------------------------------------------------|----------------------------------------------------------------|
| `OnMotorCommandOn`     | Every `$C0E9` strobe, including no-op re-strobes when motor is already engaged | `HandleSwitch` case `0x9` (unconditional)                |
| `OnMotorEngaged`       | `m_motorOn` transitions false → true                                     | `HandleSwitch` case `0x9` (edge-detected)                      |
| `OnMotorCommandOff`    | Every `$C0E8` strobe (arms spindown when engaged)                        | `HandleSwitch` case `0x8` (unconditional)                      |
| `OnMotorDisengaged`    | `m_motorOn` transitions true → false (post-spindown)                     | `Tick` after spindown counter expires                          |
| `OnHeadStep`           | `qtDelta != 0` AND head NOT clamped against a stop                       | `HandlePhase` after `m_quarterTrack += qtDelta`                |
| `OnHeadBump`           | `qtDelta != 0` AND post-clamp `m_quarterTrack == 0` (from < 0) OR `== kMaxQuarterTrack` (from > kMaxQuarterTrack) | Same `HandlePhase` site |
| `OnAddressMark`        | Watcher decoded `D5 AA 96` + 4-and-4 fields + verified XOR checksum      | `DiskIIAddressMarkWatcher::StepAddrMarkState` accept terminal  |
| `OnDataMarkRead`       | Watcher decoded `D5 AA AD` + counted 342 6-and-2 nibbles + saw `DE AA EB` | `DiskIIAddressMarkWatcher::StepDataMarkState` accept terminal  |
| `OnDataMarkWrite`      | NEVER FIRES IN v1 — interface method defined for forward compatibility per A-010. A symmetric write watcher lands with issue #67 (bit-level write path through Q6/Q7). | (no v1 fire site) |
| `OnDriveSelect`        | Active drive index changes via `$C0EA` / `$C0EB`                         | `HandleSwitch` cases `0xA` / `0xB`                             |
| `OnDiskInserted`       | Disk image newly mounted (every mount fires for the debug log — unlike audio spec-005 FR-013, which suppresses the *sound* on cold-boot; the controller event still fires, and the suppression decision is logged via the audio sink — see FR-025) | `MountImage` (or shell-level mount path) |
| `OnDiskEjected`        | Disk image unmounted                                                     | `EjectImage` (or shell-level eject path)                       |

`OnHeadStep` and `OnHeadBump` are mutually exclusive for any single
`HandlePhase` invocation (matches Feature 005's audio plumbing).
`OnMotorCommandOn` and `OnMotorEngaged` are NOT mutually exclusive: a
cold-boot first strobe fires both, while subsequent re-strobes on an
already-engaged motor fire only `OnMotorCommandOn`. Symmetrically,
`OnMotorCommandOff` always fires on `$C0E8`, but `OnMotorDisengaged`
only fires later, when the spindown timer expires (typically ~1 sec).

Audio-side events (`EventCategory::Audio`), all fired from inside
`DiskIIAudioSource`'s existing controller-event hooks at the moment
each audio decision is made:

| Event                | Trigger                                                                                                            |
|----------------------|--------------------------------------------------------------------------------------------------------------------|
| `OnAudioStarted`     | New one-shot from sample 0; previous shot for this `SoundKind` was finished/idle.                                  |
| `OnAudioRestarted`   | New one-shot fired while a previous shot was still playing; previous shot cut short, new shot from sample 0.       |
| `OnAudioContinued`   | Event acknowledged but previous shot's tail deliberately preserved (spec-005 FR-005 seek-mode case for `HeadStep`).|
| `OnAudioSilent`      | Event fired but no audio played. Reason ∈ {DriveAudioDisabled, BufferMissing, NoSourceRegistered, ColdBootSuppression}. |
| `OnAudioLoopStarted` | Looping sound (motor) began.                                                                                       |
| `OnAudioLoopStopped` | Looping sound (motor) stopped.                                                                                     |

`DriveAudioMixer` may forward / aggregate as needed. The two sinks share
the same `DiskIIEventRing` instance owned by the dialog; the
`EventCategory` tag on each ring entry tells the consumer's formatter
which detail-template to apply.

### Address-Mark / Data-Mark Watcher State Machines

Two independent state machines inside `DiskIIAddressMarkWatcher`. Each
state machine processes one nibble per `ObserveNibble` call (the same
nibble is fed to both).

**Address-mark state machine** (8 states):

```
S_ADDR_IDLE  ─ on D5 → S_ADDR_D5
S_ADDR_D5    ─ on AA → S_ADDR_AA
             ─ else  → S_ADDR_IDLE (or S_ADDR_D5 if nibble is D5 — overlap)
S_ADDR_AA    ─ on 96 → S_ADDR_VOL_HI (capturing fields)
             ─ else  → S_ADDR_IDLE
S_ADDR_VOL_HI / S_ADDR_VOL_LO  ─ accumulate volume nibbles → S_ADDR_TRK_HI
S_ADDR_TRK_HI / S_ADDR_TRK_LO  ─ accumulate track nibbles → S_ADDR_SEC_HI
S_ADDR_SEC_HI / S_ADDR_SEC_LO  ─ accumulate sector nibbles → S_ADDR_CHK_HI
S_ADDR_CHK_HI / S_ADDR_CHK_LO  ─ accumulate checksum nibbles
                               → on accept: verify `chk == vol XOR trk XOR sec`
                                 if ok: fire OnAddressMark, cache sector, → S_ADDR_IDLE
                                 if bad: → S_ADDR_IDLE silently
```

4-and-4 decode: each pair (`hi`, `lo`) decodes to `((hi << 1) | 1) & lo`,
i.e., the standard `((odd << 1) | 1) & even` formula used by every
Apple II disk decoder.

**Data-mark state machine** (4 states + counter):

```
S_DATA_IDLE  ─ on D5 → S_DATA_D5
S_DATA_D5    ─ on AA → S_DATA_AA
             ─ else  → S_DATA_IDLE (with D5 overlap as above)
S_DATA_AA    ─ on AD → S_DATA_BODY (set counter = 0)
             ─ else  → S_DATA_IDLE
S_DATA_BODY  ─ on each nibble: counter++; on DE peek-ahead 3:
                if next two are AA EB → fire OnDataMarkRead (cachedSector, 256),
                                       → S_DATA_IDLE
              ─ if counter exceeds (kDataNibbleCount + small slack):
                                       → S_DATA_IDLE (giving up; corrupt frame)
```

The "peek-ahead" is implemented by buffering the last 3 nibbles in a
3-byte rolling window inside the state, not by re-reading the nibble
stream (the watcher is forbidden from doing that per FR-008).

Both state machines run on every `ObserveNibble` call. They share no
mutable state with each other beyond the cached most-recent sector
number (written by the address-mark accept terminal, read by the
data-mark accept terminal).

### SPSC Ring Buffer

```cpp
class DiskIIEventRing {
    static constexpr uint32_t        kEventRingCapacity = 4096;  // power of 2
    static constexpr uint32_t        kMask = kEventRingCapacity - 1;
    DiskIIEvent                      m_slots[kEventRingCapacity];
    std::atomic<uint32_t>            m_head{0};         // consumer reads/writes
    std::atomic<uint32_t>            m_tail{0};         // producer reads/writes

public:
    bool TryPush (const DiskIIEvent& e);   // CPU thread only
    bool TryPop  (DiskIIEvent& out);       // UI thread only
    uint32_t Drain (DiskIIEvent* out, uint32_t maxCount);  // UI thread only
    uint32_t ApproxSize() const;           // either thread (advisory only)
};
```

`TryPush` is wait-free: load tail relaxed, load head acquire, check free
slot count, write event into `m_slots[tail & kMask]`, store tail release.
`TryPop` mirrors with roles reversed. `Drain` is the UI-thread fast path
that calls `TryPop` in a loop up to `maxCount` (or until the ring is
empty). On a 64-bit Windows host, `uint32_t` atomics are lock-free; the
ring carries no spurious lock fallback.

### Auto-Tail Scroll Algorithm

Implements FR-013 exactly:

```cpp
void DiskIIDebugDialog::OnDrainTimer()
{
    bool                wasAtTail;
    uint32_t            oldCount;
    uint32_t            dropped;

    if (m_paused || !IsWindowVisible (m_hwnd))
        return;

    oldCount   = (uint32_t) m_deque.size();
    wasAtTail  = (ListView_GetTopIndex (m_listView) + ListView_GetCountPerPage (m_listView)
                  >= (int) oldCount);

    dropped    = m_droppedSinceLastDrain.exchange (0, std::memory_order_acq_rel);
    m_projection.DrainAndProject (m_ring, m_deque, dropped, m_shell.GetUptimeAnchor());

    if ((uint32_t) m_deque.size() == oldCount)
        return;    // nothing arrived; don't disturb scroll

    ListView_SetItemCountEx (m_listView, m_deque.size(), LVSICF_NOSCROLL);

    if (wasAtTail)
        ListView_EnsureVisible (m_listView, (int) m_deque.size() - 1, FALSE);
}
```

The `wasAtTail` capture is done **before** the projection mutates the
deque; this is the entire correctness condition for FR-013.

### Menu, Accelerator, Dialog Wire-Up

`Casso/resource.h` gains a new menu-item id: `IDM_VIEW_DISKII_DEBUG`.
`Casso/Casso.rc` gains:

- Under the existing `View` popup: a new "Disk II Debug...\tCtrl+Shift+D"
  item with the new id.
- In the application accelerator table: an `ACCELERATOR` entry binding
  `VK_D, IDM_VIEW_DISKII_DEBUG, VIRTKEY, CONTROL, SHIFT`.

`Casso/MenuSystem.{h,cpp}` routes `WM_COMMAND` with the new id to a
shell method `EmulatorShell::OpenDiskIIDebugDialog()`, which:

- If the dialog is not yet created: constructs it (programmatic
  `CreateWindowEx`-based dialog, no `.rc` template per A-008), attaches
  the dialog as the sink on `DiskIIController[0]`, starts the timer,
  shows the window.
- If the dialog already exists: brings it to front (`SetForegroundWindow`).

The dialog's `WM_CLOSE` handler hides the window and revokes the sink
(`controller.SetEventSink (nullptr)`); the dialog is reused on the next
open. The dialog is fully destroyed only on shell shutdown.

## Constitution Check — Post-Design Re-Validation

- ✅ All new functions stay within size and indent budgets.
- ✅ EHM applied only where fallibility exists (`CreateWindowEx`, accelerator
      registration); event-fire, ring push/pop, and watcher transitions
      are `void` and infallible.
- ✅ No new threads. Synchronization is exactly two `std::atomic<uint32_t>`
      indices on a lock-free SPSC ring + one `std::atomic<uint32_t>` lost-
      event counter; no mutex, no CV, no critical section.
- ✅ Test isolation: ring, watcher, and projection helpers are pure
      data structures testable without HWND/timer/filesystem; the dialog's
      Win32 wiring is intentionally thin.
- ✅ No external dependencies; nothing new in `vcpkg.json` or third-party
      headers.
- ✅ Sink pointer guarded at every fire site; controller behavior with
      no sink attached is byte-identical to pre-feature (SC-007, SC-010).

## Phasing (cross-reference to `tasks.md`)

The implementation follows the dependency order required by the design:
interface first, then producer-side pieces, then consumer-side pieces,
then the Win32 wiring, then polish. Phase summary:

1. **Phase 0 — Contracts**: `IDiskIIEventSink` interface (12 methods,
   four-event motor lifecycle per FR-006), `IDriveAudioEventSink`
   interface (6 methods + `SoundKind` + `SilentReason` enums per FR-021),
   `EventCategory` enum (FR-023), `DiskIIEvent` POD, `DiskIIEventDisplay`
   struct, named constants (including column widths per the No-magic-
   numbers row of the Constitution table). Header-only.
2. **Phase 1 — SPSC ring**: `DiskIIEventRing.{h,cpp}` + exhaustive
   single-threaded and two-threaded tests (push fills, pop drains,
   wrap-around, overflow returns false without corruption).
3. **Phase 2 — Address-mark watcher**: `DiskIIAddressMarkWatcher.{h,cpp}`
   with both state machines + tests against synthetic nibble streams
   (stock DOS 3.3 cadence, bad checksum rejection, mid-stream resync,
   data-mark without preceding address mark, garbage stream produces
   zero false positives).
3a. **Phase 2a — Track/Sector filter parser**: `TrackSectorPredicate.{h,cpp}`
   + tests for the FR-014a/b syntax (integers, decimals, ranges, lists,
   raw-qt mode, weird-input tolerance — `abc` and `999` accepted without
   throwing).
4. **Phase 3 — Controller integration**: `IDiskIIEventSink* m_eventSink` +
   `SetEventSink` + fire-site insertions at every existing motor/head/
   mount call site (including the **four-event motor lifecycle** —
   re-wire existing `OnMotorStart`/`OnMotorStop` fire sites in
   `DiskIIController.cpp` to the four new methods) + watcher invocation
   in the nibble read path. Tests using a recording mock sink confirm
   event ordering and that the no-sink path is byte-identical to baseline.
4a. **Phase 3a — Audio source integration**: `IDriveAudioEventSink*
   m_audioEventSink` + `SetAudioEventSink` on `DiskIIAudioSource`.
   Migrate `DiskIIAudioSource`'s existing `OnMotorStart`/`OnMotorStop`
   listeners to `OnMotorEngaged`/`OnMotorDisengaged` (the audio-relevant
   edges). Add `OnAudio*` fire sites at every audio decision moment
   inside the existing controller-event hooks, including the cold-boot
   suppression case (FR-025 → `OnAudioSilent(DoorClose, drive,
   ColdBootSuppression)`). Wire `EmulatorShell` to set both sinks on
   dialog open and revoke both on close. Tests with a recording mock
   `IDriveAudioEventSink` confirm each of the six outcomes fires under
   reproducible conditions.
5. **Phase 4 — UI-thread projection helper**: `DebugDialogProjection`
   (or equivalent) — the non-Win32 helper that drains the ring into the
   deque, formats per FR-005 (covers both `EventCategory::Controller`
   and `EventCategory::Audio` events), applies the rolling cap, and
   inserts the `[N events lost]` marker. Pure data-structure tests.
6. **Phase 5 — Dialog skeleton**: `DiskIIDebugDialog.{h,cpp}` —
   programmatic `WM_INITDIALOG` that creates the ListView (virtual mode,
   **5 columns** per FR-004: Wall, Uptime, Cycle, Event, Detail with
   widths from `kColWallWidth`/`kColUptimeWidth`/etc.), event-type
   filter checkboxes (FR-014), Drive radio, Track/Sector text inputs
   with raw-qt toggle, Audio sub-checkboxes, Pause and Clear buttons.
   No event data yet; the dialog renders an empty list. Hooks
   `WM_DESTROY` / `WM_CLOSE`.
7. **Phase 6 — Drain timer + auto-tail**: `WM_TIMER` handler invokes the
   projection helper, captures `wasAtTail`, calls
   `ListView_SetItemCountEx`, conditionally `ListView_EnsureVisible`.
   `LVN_GETDISPINFO` handler returns deque entries by index (via the
   `m_filteredIndices` vector — Phase 7 wires the real filter; for now,
   identity mapping).
8. **Phase 7 — Filter projection**: Wire the event-type checkboxes,
   Drive radio, Track/Sector inputs, and Audio sub-checkboxes into the
   `FilterState` struct and rebuild `m_filteredIndices` on any change.
   Tests confirm chronological preservation when a filter is re-checked
   and full logical-AND composition across all filter axes (SC-016).
9. **Phase 8 — Pause / Resume / Clear**: Wire buttons. Pause stops the
   drain (events keep accumulating in the ring); Clear empties the deque
   but does not flush the ring. Tests confirm in-flight events survive
   Clear and the `[N events lost]` marker appears exactly once after
   any number of pause-induced overflows.
10. **Phase 9 — Clipboard copy**: Ctrl+C selection → tab-separated text
    on the clipboard (FR-019), respecting visible-column ordering
    (hidden columns omitted per FR-026).
11. **Phase 10 — Menu / accelerator / shell wiring**: `Casso.rc`,
    `resource.h`, `MenuSystem.{h,cpp}`, `EmulatorShell` open/close
    handler with sink attach/revoke (both `SetEventSink` AND
    `SetAudioEventSink` per FR-024). Multi-controller indicator if
    `MachineConfig` reports > 1 Disk II. Wire the Uptime anchor
    reseed into `MachineShell::SoftReset` and `PowerCycle` (FR-004a).
12. **Phase 10a — Column show/hide**: Header right-click handler
    (`NM_RCLICK` / `WM_CONTEXTMENU`) builds the popup menu; toggles
    rebuild the ListView's column set from the logical column model
    (`std::array<LogicalColumn, 5>`) via delete-and-recreate per
    FR-026/FR-027 (no width-to-zero hack). No cross-session persistence
    (NFR-006).
13. **Phase 11 — Polish**: CHANGELOG, README, manual DOS 3.3 boot
    sanity, manual Karateka / Choplifter / HardHatMack inspection,
    final constitution sweep (`Pch.h` first, spacing, declarations
    aligned).

## Risks & Mitigations

| Risk                                                                                  | Mitigation                                                                                                                            |
|---------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------|
| SPSC ring's correctness depends on memory-ordering discipline                         | Implement once, test exhaustively against a 2-thread stress harness using `_InterlockedIncrement` / `std::atomic` debug-assert peer; cross-check against Folly's `ProducerConsumerQueue` and Vyukov's classic SPSC formulation (research §4). |
| Address-mark watcher emits false positives on garbage nibbles                         | Checksum verification (FR-008 step 2) rejects every accidental `D5 AA 96` lookalike that doesn't have a matching XOR. Tests include a random-nibble torture set that asserts zero `OnAddressMark` calls. |
| Watcher perturbs the existing nibble engine                                            | Watcher is `const`-on-the-engine: it only reads what `NibbleReadByte` returns to the CPU. Existing `DiskIIControllerTests` must pass byte-identically with the sink-attached-but-stubbed configuration (SC-007). |
| ListView in virtual mode chokes on the row-count-update + ensure-visible cycle         | Virtual mode is specifically designed for this; AppleWin, x64dbg, and Process Explorer all use the same pattern at higher rates. 30 Hz updates with ≤ 4,096 new rows per tick is trivially within the control's documented envelope. |
| Cross-thread torn reads of `DiskIIEvent` fields                                       | The ring's release/acquire pairing on the index publish/observe is the standard SPSC happens-before edge. The slot data itself is plain-old-data — no torn reads possible because the publish is the index update, and the index update happens after the slot write on the producer side and before the slot read on the consumer side. |
| Filter checkbox toggle stalls UI on a 100,000-entry deque                              | Filter projection is computed lazily inside `LVN_GETDISPINFO` (only visible rows are touched); the recomputation on toggle is O(visible rows), not O(deque size). If a precomputed index vector is used, the rebuild is O(deque size) but happens on a user-driven event, not in the drain timer. |
| Dialog opened mid-session shows no historical context                                  | This is by design (FR-018). README and CHANGELOG mention "open the debug window *before* the operation you want to investigate." |
| Multi-controller machine config in the future                                          | v1 surfaces a clear indicator and monitors controller #0 only. The interface and ring are per-window so a future v2 can host one window per controller without changing the underlying contract. |
