# Quickstart: Manual Verification — Native DX Dialogs Completion

Use these checklists to verify each priority phase after building locally.
Build via the VS Code task `Build + Test Debug` (or `Release`). Do NOT run
MSBuild directly.

## Prerequisites

- Local Casso build with the spec-011 branch checked out and built.
- Three theme settings exercisable from Settings (DarkModern,
  Skeuomorphic, GreenScreen).
- A second monitor at a different DPI is helpful (for the DPI-change
  edge case) but optional.

---

## Phase P1 — Unified startup + Boot disk MRU

### P1-A — Unified first-run download dialog (FR-001, SC-002, US1)

1. Delete the local ROM cache directory. (Optional: also delete the
   Disk II audio WAV cache.)
2. Launch Casso.
3. **Verify**: exactly ONE themed dialog appears before any emulator
   chrome renders, listing every missing asset.
4. **Verify**: dialog renders under the currently selected theme
   (palette + chrome match `SettingsWindow`).
5. Approve the download. **Verify**: progress is visible; dialog stays
   modal; dialog dismisses only after downloads finish.
6. Repeat from step 1, but this time decline.
   **Verify**: missing ROMs → boot blocked (as today); missing audio
   only → Disk II runs silently.
7. Repeat steps 1–5 under each of the three themes.

### P1-B — Boot disk picker, empty MRU (FR-002, US2 AS-1, AS-8)

1. Wipe / reset user prefs so `recentDisks` is empty.
2. Launch a machine with a Disk II in slot 6, drive 1 empty, no
   pinned disk image.
3. **Verify**: picker appears with only the two download entries
   (DOS 3.3, ProDOS) plus Cancel.
4. Cancel. **Verify**: machine boots with drive 1 empty.

### P1-C — Boot disk picker, populated MRU (FR-002, FR-003, SC-003, SC-004)

1. From any entry point (file picker, drag-drop, boot picker
   download), mount three different disk images A, B, C in that
   order. After each mount, **verify** prefs file on disk contains
   the path in `recentDisks` array, most-recent first.
2. Eject and relaunch the Disk II machine with drive 1 empty.
3. **Verify**: picker shows `C, B, A` above the two download
   entries.
4. Click `B`. **Verify**: B mounts, picker dismisses, MRU order is
   now `B, C, A`.
5. Delete `A` from disk. Relaunch the picker.
   **Verify**: `A` is silently pruned from both the displayed list
   and the persisted prefs file.
6. Mount 16 distinct disks. Mount a 17th. **Verify**: the 17th
   appears at the top, the previously-oldest is evicted, list size
   stays at 16.

### P1-D — MRU update on every mount path

1. Mount via Disk → Insert Disk 1 menu. **Verify**: MRU updated.
2. Mount via drag-drop onto the Casso window. **Verify**: MRU updated.
3. Mount via boot picker. **Verify**: MRU updated.

---

## Phase P2 — Help/About, drive label, file-open dedup

### P2-A — About dialog (FR-005, SC-006, US3)

1. Help → About.
2. **Verify**: themed dialog, large Casso app icon (not generic
   Windows info glyph), product/version/copyright text intact, URL
   `https://github.com/relmer/Casso` rendered as a clickable
   hyperlink inline in the body.
3. Click the hyperlink. **Verify**: default browser opens to the
   repository.
4. Press Escape. **Verify**: dialog dismisses, previous focus
   restored.
5. Repeat under each of the three themes and at DPI scales 100%,
   125%, 150%, 200%.

### P2-B — Keymap and Machine Info (FR-006, FR-014, SC-008)

1. Press F1. **Verify**: themed Keymap dialog with same text as
   before.
2. Open the machine-info menu command. **Verify**: themed Machine
   Info dialog with same text as before.
3. Close each with Escape.

### P2-C — Drive widget filename label (FR-007, FR-008, SC-007)

1. Mount a disk with a short basename (e.g., `Test.dsk`).
   **Verify**: basename appears below "Drive 1" label within one
   frame.
2. Eject. **Verify**: label disappears within one frame.
3. Mount a disk with a very long basename. **Verify**: ellipsis
   truncation, no overflow past widget width.
4. Mount a file with no extension. **Verify**: literal filename
   shown, no stripping.

### P2-D — File-open dedup (FR-011, US5)

1. Disk → Insert Disk 1 (Ctrl+1). **Verify**: modern Win11
   `IFileOpenDialog` appears, NOT the legacy `GetOpenFileName`.
2. Select a `.dsk`. **Verify**: mounts correctly, MRU updated,
   drive widget label updates.
3. Repeat for Insert Disk 2 (Ctrl+2).

### P2-E — Settings panel stray cleanup (FR-012)

1. Trigger whatever path in Settings used to call `MessageBoxW`
   (consult `SettingsPanel.cpp` history). **Verify**: themed dialog
   instead of system MessageBox.

---

## Phase P3 — Debug Console + Disk II Debug Dialog

### P3-A — Themed Debug Console (FR-009, US6)

1. Open Debug Console (Help → Debug). **Verify**: rendered through
   DX painter, active theme palette, monospace font (no Win32 EDIT
   control). Chrome shell title bar + close button match the
   Settings panel / Disk II Debug panel chrome.
2. Generate enough log lines to exceed the panel height.
   **Verify**: keyboard scrolling (arrows / PgUp / PgDn / Home /
   End) and mouse-wheel scrolling work. New lines auto-scroll to
   the tail when the viewport is already pinned to the bottom;
   otherwise the user's scroll position is preserved.
3. Ctrl+C with no selection. **Verify**: clipboard contains the
   full buffer joined with CRLF. (Granular text-range selection
   is intentionally deferred — see CHANGELOG "Deferred".)
4. Close the panel (X button or Escape). **Verify**: panel hides
   but the buffer survives; re-opening shows the prior log.

### P3-B — Themed Disk II Debug Panel (FR-010, SC-010, US7)

For each control family below, exercise it and verify behavior
matches the legacy Win32 dialog:

1. Static labels render correctly under each theme.
2. Event-type filter checkboxes — toggle each, verify ListView
   filters identically.
3. Audio master/sub toggles — same.
4. Raw-quarter-track filter — same.
5. Drive radio buttons — same.
6. Track filter text input with valid and invalid input —
   themed validation feedback adjacent to the input on invalid.
7. Sector filter — same.
8. Pause / Clear buttons — same.
9. Sortable ListView — sort by Time, Event, Detail; ascending
   and descending.
10. **Verify SC-010**: `UnitTest` project still builds and
    `DiskIIDebugDialogColumnTests` / `DiskIIDebugDialogTests` still
    pass — no Win32 types leaked into the state TU.

The legacy `DiskIIDebugDialog.cpp` / `.h` and the
`CASSO_LEGACY_DISKII_DEBUG_DIALOG` compile-time switch have been
deleted; the DX panel is now the only Disk II debug surface.

---

## Cross-cutting regression check (run after each phase)

- `rg -n "MessageBox|TaskDialog|GetOpenFileName" Casso/` returns
  ONLY (a) `IFileOpenDialog` in `PromptForDiskImage` and (b)
  `MessageBoxW` in `Main.cpp`'s EHM `SetNotifyFunction` callback —
  plus comments. Any other hit is an FR-015 violation.
- VS Code task `Build + Test Debug` and `Build + Test Release`
  both succeed with zero warnings.
- Code Analysis task passes with zero warnings.
- No CHANGELOG / README updates skipped for user-visible changes.
- Dormann + Harte suites are NOT required for this feature
  (no CPU/assembler changes); skip unless implementation drifted
  into `CassoCore` / `CassoEmuCore`.
