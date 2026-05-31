# Feature Specification: Native DX Dialogs Completion

**Feature Branch**: `011-native-dialogs-completion`
**Created**: 2026-05-27
**Status**: Draft
**Input**: User description: Complete the native DX UI overhaul started in spec 007 by converting the remaining Win32 UI surfaces (asset-bootstrap modals, help/about/machine-info MessageBoxes, the SettingsPanel stray MessageBox, the legacy GetOpenFileName path, the drive widget label, the DebugConsole EDIT control, and the DiskIIDebugDialog) into themed DX overlays that match the rest of the native chrome.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Unified first-run asset download (Priority: P1)

A new user launches Casso for the first time without the Apple II ROM images (and optionally without the Disk II audio WAVs). Instead of being walked through two or three separate native MessageBox / TaskDialog prompts before the emulator can boot, the user sees a single themed dialog that lists every missing asset, lets them approve or decline the downloads in one decision, and shows live progress until the downloads finish (or the user cancels and accepts a degraded boot).

**Why this priority**: This is the very first thing a brand-new user sees. Today it is also the loudest remaining departure from the native DX look introduced in spec 007 — three different Win32 prompt styles in a row before any pixel of the emulator chrome renders. Consolidating them is the highest-visibility win and unblocks removal of `MessageBoxW` / `TaskDialogIndirect` from `AssetBootstrap.cpp`.

**Independent Test**: Delete the local ROM cache (and optionally the Disk II audio cache), launch Casso, and confirm exactly one themed dialog appears that enumerates every missing asset, that approving downloads them all with visible progress before the emulator boots, and that declining boots Casso with the documented degradation (no ROMs => emulator does not start; no audio => Disk II runs silently).

**Acceptance Scenarios**:

1. **Given** no ROM images on disk and no Disk II audio WAVs, **When** the user launches Casso, **Then** a single themed download dialog is shown listing all missing ROMs and the Disk II audio assets, with one "Download" and one "Skip" decision.
2. **Given** the user approves the unified download, **When** downloads run, **Then** the dialog remains modal, shows per-asset (or aggregate) progress, and dismisses itself only when every approved download has completed or failed.
3. **Given** the user declines the unified download, **When** the dialog is dismissed, **Then** boot proceeds with the same degraded behavior the current code produces (no ROMs => boot is blocked exactly as today; missing audio => Disk II runs silently).
4. **Given** the active theme is DarkModern, Skeuomorphic, or GreenScreen, **When** the unified download dialog appears, **Then** it renders using that theme's palette, fonts, and chrome rules (matching `SettingsWindow`).

---

### User Story 2 — Boot disk picker with MRU (Priority: P1)

When the user launches a machine that has a Disk II controller in slot 6, drive 1 is empty, and the per-machine config did not pin a disk image (or pinned one that no longer exists on disk), the user sees a themed picker that lists their previously-mounted disk images at the top and the two downloadable system disks (DOS 3.3 System Master, ProDOS Users Disk) inline below. Picking an MRU entry mounts it; picking a download entry downloads then mounts; cancelling lets the machine boot with no disk just as today.

**Why this priority**: Disk II is the primary way users get software into the emulator, and the current `PromptBootDisk` is one of only two remaining native TaskDialogs. Adding the MRU also delivers visible day-to-day workflow value beyond pure cosmetic conversion — users stop re-navigating to the same `.dsk` files via the file picker.

**Independent Test**: With a fresh user-prefs file, launch a Disk II machine: only the two download entries appear. Mount three disk images over time via the file picker / drag-drop / this dialog. Relaunch with drive 1 empty: those three filenames appear above the download entries, in most-recently-mounted order, and selecting one mounts that disk.

**Acceptance Scenarios**:

1. **Given** a machine with a Disk II in slot 6, drive 1 empty, and no pinned disk image in the per-machine config, **When** the machine starts, **Then** the boot-disk picker appears.
2. **Given** the picker is shown and the user has previously mounted disk images that still exist on disk, **When** the picker renders, **Then** those images appear at the top of the list as MRU entries (most-recent first), with the two download entries below them.
3. **Given** the user selects an MRU entry, **When** the selection is confirmed, **Then** that disk image is mounted into drive 1 and the picker dismisses.
4. **Given** the user selects a download entry, **When** the selection is confirmed, **Then** the asset is downloaded (with progress UI consistent with User Story 1) and then mounted into drive 1.
5. **Given** the user cancels the picker, **When** the picker dismisses, **Then** the machine boots with drive 1 empty (same as today's Skip path).
6. **Given** the user has mounted a disk image via any path (file picker, drag-drop, or this dialog), **When** the mount succeeds, **Then** the MRU is updated and persisted to user prefs.
7. **Given** an MRU entry's underlying file no longer exists on disk, **When** the picker renders, **Then** that entry is pruned from the displayed list (and from persisted prefs).
8. **Given** the MRU is empty and no embedded boot disk applies, **When** the picker renders, **Then** only the two download entries (plus Cancel) are shown — matching today's `PromptBootDisk` behavior.

---

### User Story 3 — Themed About / Keymap / Machine Info (Priority: P2)

A user opens Help → About, Help → Keymap, or the machine-info popup. Instead of a system MessageBox, they see a themed dialog that matches the rest of the emulator chrome, with the About box now showing the Casso app icon and a clickable repository hyperlink.

**Why this priority**: Lower urgency than the boot-flow modals (users encounter these only on demand) but completes the visible Win32-removal goal of the spec and exercises the new reusable dialog primitive on the simplest content.

**Independent Test**: Trigger each menu command, confirm a themed dialog appears under each of the three themes, confirm the About hyperlink opens `https://github.com/relmer/Casso` in the default browser, and confirm the About icon shows the rendered Casso app icon rather than the generic Windows info glyph.

**Acceptance Scenarios**:

1. **Given** the user selects Help → About, **When** the dialog appears, **Then** it shows the existing About text content, the large Casso app icon, and the repository URL rendered as a clickable hyperlink.
2. **Given** the About dialog is showing, **When** the user clicks the repository hyperlink, **Then** the default browser opens to `https://github.com/relmer/Casso`.
3. **Given** the user selects Help → Keymap (F1) or the machine-info menu command, **When** the dialog appears, **Then** it shows the same content as today's MessageBox, rendered through the themed dialog primitive.
4. **Given** any of these dialogs is open, **When** the user presses Escape or activates the close button, **Then** the dialog dismisses and the previously focused window regains focus.

---

### User Story 4 — Drive widget filename label (Priority: P2)

A user mounts a disk image into a drive. The drive widget now shows the disk's filename (basename only) directly below the existing "Drive N" label. When the user ejects the disk, the filename label disappears. When the filename is too long for the widget, it is truncated with an ellipsis.

**Why this priority**: Pure UX win delivered alongside the boot-disk MRU work — once we are touching the mount path to record MRU entries, surfacing the mounted filename on the widget itself is a small additional change with high day-to-day value. P2 because the emulator is fully usable without it.

**Independent Test**: Mount a disk with a short name, observe its basename appears below "Drive N". Mount a disk with a very long basename, observe ellipsis truncation. Eject, observe the label disappears. Mount again, observe it reappears immediately.

**Acceptance Scenarios**:

1. **Given** a drive is empty, **When** the widget paints, **Then** only the existing "Drive N" label is shown.
2. **Given** a disk image is mounted in the drive, **When** the widget paints, **Then** the disk's filename basename (no path, with extension) appears below the "Drive N" label.
3. **Given** the basename is wider than the drive widget, **When** the widget paints the label, **Then** the label is truncated with a trailing ellipsis.
4. **Given** the user ejects the disk, **When** the widget repaints, **Then** the filename label disappears immediately.

---

### User Story 5 — Single disk-insert file picker (Priority: P2)

A user invokes Disk → Insert Disk 1 / Insert Disk 2. They see the modern Win11 `IFileOpenDialog` (the same picker used by all other disk-image entry points), not the legacy `GetOpenFileName` dialog.

**Why this priority**: Pure dedup. Removes one of the remaining Win32 surfaces and ensures consistent behavior across every disk-insert entry point (file picker, drag-drop, boot dialog, menu commands).

**Independent Test**: Trigger Insert Disk 1 from the menu, confirm the modern file-open dialog appears, confirm a selected `.dsk` mounts correctly, and confirm the MRU is updated.

**Acceptance Scenarios**:

1. **Given** the user selects IDM_DISK_INSERT1 or IDM_DISK_INSERT2, **When** the file picker is shown, **Then** it is the modern `IFileOpenDialog` (same one as `PromptForDiskImage`).
2. **Given** a disk is selected, **When** the mount completes, **Then** the MRU is updated and the drive widget label reflects the new disk.

---

### User Story 6 — Themed Debug Console (Priority: P3)

A developer opens the Debug Console. Instead of a Win32 EDIT child window, they see a themed DX text panel with monospace font, scrolling, and copy-to-clipboard support, hosted like the SettingsWindow.

**Why this priority**: Developer-facing, lower frequency than user-facing dialogs, and a heavier conversion. Done after the simpler dialog work to amortize the new primitives.

**Independent Test**: Open the Debug Console, write a large volume of log lines, scroll through them, select a range, copy to clipboard, verify the clipboard contents and that the panel theme matches the active emulator theme.

**Acceptance Scenarios**:

1. **Given** the user opens the Debug Console, **When** the panel appears, **Then** it is rendered through the DX painter using the active theme's palette and a monospace font.
2. **Given** log lines exceed the panel height, **When** the user scrolls, **Then** the panel supports keyboard and mouse-wheel scrolling.
3. **Given** the user selects text in the panel, **When** they invoke copy (Ctrl+C or context menu), **Then** the selected text is placed on the clipboard.

---

### User Story 7 — Themed Disk II Debug Dialog (Priority: P3)

A developer opens the Disk II Debug Dialog. They see a themed DX dialog that preserves every existing capability: event-type filter checkboxes, audio master/sub toggles, raw-quarter-track filter, drive radio buttons, track and sector text filters with validation feedback, pause and clear buttons, a sortable ListView (Time / Event / Detail) with column-header show/hide, and tooltips for filter help.

**Why this priority**: Heaviest conversion in the spec, developer-facing, and gated on the simpler primitives being in place. Headless `DiskIIDebugDialogState` must continue to drive all logic.

**Independent Test**: Open the dialog, exercise every control (each checkbox, each radio button, each text filter with valid and invalid inputs, pause, clear, sort each column, hide/show each column via the header context menu, hover for tooltips), and verify behavior is identical to the Win32 version against the same headless state.

**Acceptance Scenarios**:

1. **Given** the user opens the Disk II Debug Dialog, **When** the dialog appears, **Then** all controls listed above are present and themed.
2. **Given** the user changes any filter (checkbox, radio button, text input), **When** the change is applied, **Then** the underlying `DiskIIDebugDialogState` is updated and the ListView re-filters identically to today's behavior.
3. **Given** the user enters an invalid value into the track or sector filter, **When** validation runs, **Then** a themed validation feedback label is shown adjacent to the input.
4. **Given** the user right-clicks a ListView column header, **When** the context menu appears, **Then** the user can show or hide individual columns.
5. **Given** the user hovers a filter control, **When** the hover delay elapses, **Then** a themed tooltip explains that filter.

---

### Edge Cases

- **Download failure mid-flight**: If an asset download fails partway through the unified P1 dialog, the dialog must report which asset failed, keep any successful downloads, and let the user retry or cancel.
- **Asset cache partially populated**: If some ROMs are present but others are missing, the unified dialog must only list the missing ones (not re-download what is already on disk).
- **MRU contains a now-deleted file**: The picker must silently prune the entry (both from the displayed list and from persisted prefs).
- **MRU contains a network path that is currently unreachable**: Treat as "still exists" if we cannot prove non-existence cheaply (no long stat-blocking on the UI thread); leave pruning to the next launch where stat succeeds quickly.
- **Per-machine config pins a disk image that no longer exists**: Boot picker is shown (treat as if no pin existed) rather than failing silently or blocking boot.
- **MRU at cap (16 entries) and a new mount happens**: Oldest entry is evicted.
- **About hyperlink click with no default browser registered**: `ShellExecuteW` failure is reported through `CHRN` (themed dialog, since the painter is up by this point).
- **DPI changes while a themed dialog is open** (e.g., user drags Casso to a different monitor): The dialog re-lays out at the new DPI using the same code path that handles initial render at 125% / 150% / 200%.
- **Theme changes while a themed dialog is open**: The dialog repaints with the new palette without dismissing.
- **Debug Console copy with no selection**: No-op (consistent with standard text controls); does not crash or clear the clipboard.
- **DiskIIDebugDialog opened before any Disk II activity has occurred**: ListView is empty but all controls are operable; pause/clear are valid no-ops.
- **Drive filename with no extension** or with multiple dots: Display the literal `path.filename()` result; do not strip extensions.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001 (Unified startup download dialog)**: System MUST present a single themed DX dialog at startup that enumerates every missing asset (ROM images and, if applicable, Disk II audio WAVs), consolidating today's three separate `PromptUser` / `PromptBootDisk` / `PromptDiskAudioConsent` modals into one decision point. The dialog MUST be theme-aware (DarkModern, Skeuomorphic, GreenScreen). The dialog MUST block emulator boot until the user's decision is honored (downloads complete on approval, or boot proceeds with documented degradation on decline). The dialog MUST show download progress while downloads run.

- **FR-002 (Boot disk picker with MRU)**: System MUST present a themed DX boot-disk picker when (a) the active machine has a Disk II controller in slot 6, (b) drive 1 is empty, and (c) the per-machine config did not specify a still-existing disk image. The picker MUST list MRU entries (filenames of previously-mounted disk images that still exist on disk, most-recent first) above two download entries (DOS 3.3 System Master, ProDOS Users Disk), and MUST offer a Cancel button that lets the machine boot with no disk.

- **FR-003 (MRU persistence)**: System MUST persist the disk-image MRU across sessions in the existing `GlobalUserPrefs` JSON store. The MRU MUST be capped at 16 entries, MUST be updated whenever a disk image is mounted via any path (file picker, drag-drop, or boot picker), MUST prune entries whose files no longer exist at render time, and MUST evict the oldest entry when a new mount happens at cap.

- **FR-004 (Reusable themed dialog primitive)**: System MUST provide a reusable DX dialog overlay primitive (living under `Casso/Ui/Dialog/`) modeled on the existing `SettingsWindow` / `SettingsPanel` infrastructure. The primitive MUST support: a title, multi-line body text with embedded clickable hyperlinks, an optional resource-id-based icon (including large renderings of the app icon resources), and a row of single-line buttons. The primitive MUST NOT support TaskDialog-style multi-line command-link buttons.

- **FR-005 (About box conversion)**: System MUST convert the `IDM_HELP_ABOUT` command to use the themed dialog primitive, preserving the existing text content (product name, version, build date, description, URL, copyright, license), rendering the URL `https://github.com/relmer/Casso` as a clickable hyperlink that opens in the default browser via `ShellExecuteW`, and replacing the generic info icon with a large rendering of one of the `IDI_CASSO_*` app-icon resources (e.g., `IDI_CASSO_PHOTOREAL`).

- **FR-006 (Keymap and Machine Info conversion)**: System MUST convert `IDM_HELP_KEYMAP` and the machine-info menu command to use the themed dialog primitive, preserving today's text content verbatim.

- **FR-007 (Drive widget filename label)**: System MUST render the mounted disk image's filename (basename only, no path) below the existing "Drive N" label on the drive widget when a disk is mounted, MUST hide the filename label when the drive is empty, and MUST truncate the filename with a trailing ellipsis when it does not fit within the widget width. The filename MUST update immediately on mount and eject.

- **FR-008 (Drive widget state plumbing)**: System MUST extend `DriveWidgetState` with the information needed to paint the filename label (an `imageName` string or full `imagePath` from which the basename is derived at paint time) and MUST plumb that data from the disk-mount path through `DriveWidgetController` to the widget.

- **FR-009 (Themed Debug Console)**: System MUST convert `DebugConsole` from a Win32 `EDIT` child control into a themed DX text panel hosted as a dockable / floatable surface like `SettingsWindow`. The panel MUST support scrolling (keyboard and mouse wheel), MUST render text in a monospace font using the active theme's palette, and MUST support copy-to-clipboard for user-selected text.

- **FR-010 (Themed Disk II Debug Dialog)**: System MUST convert `DiskIIDebugDialog` from a Win32 dialog to a themed DX dialog while preserving every existing capability: event-type filter checkboxes, audio master / sub toggles, raw-quarter-track filter, drive radio buttons, track and sector text filters with validation feedback labels, pause and clear buttons, a sortable ListView with Time / Event / Detail columns, a column-header context menu for show/hide, and tooltips for filter help. The conversion MUST keep the pure logic in `DiskIIDebugDialogState` and MUST NOT re-couple state to UI.

- **FR-011 (File-open dedup)**: System MUST remove the legacy `GetOpenFileNameW` path in `WindowCommandManager::OnDiskCommand` and MUST route `IDM_DISK_INSERT1` and `IDM_DISK_INSERT2` through the existing `PromptForDiskImage` (`IFileOpenDialog`) path.

- **FR-012 (Settings panel stray cleanup)**: System MUST replace the residual `MessageBoxW` call in `Casso/Ui/Settings/SettingsPanel.cpp` with the themed dialog primitive.

- **FR-013 (Theme and DPI correctness)**: All dialogs introduced by this feature MUST render correctly under DarkModern, Skeuomorphic, and GreenScreen themes, and at 100%, 125%, 150%, and 200% DPI.

- **FR-014 (Existing accelerators preserved)**: Conversion of menu-command dialogs MUST preserve existing keyboard accelerators (Ctrl+1, Ctrl+2 for disk inserts; F1 for keymap help; etc.).

- **FR-015 (Win32 surface containment)**: After this feature ships, the only Win32 UI surfaces remaining in `Casso/` MUST be (a) `IFileOpenDialog` in `WindowCommandManager::PromptForDiskImage` and (b) the `MessageBoxW` last-resort path in the EHM `SetNotifyFunction` callback in `Main.cpp`. A repository search for `MessageBox|TaskDialog|GetOpenFileName` under `Casso/` MUST return only those two call sites (plus comments).

### Key Entities

- **Themed Dialog Definition**: title, optional icon resource id, body content (text plus zero or more hyperlinks), button row (zero or more single-line buttons each with a label and a result code), modality. Hosted by the new primitive under `Casso/Ui/Dialog/`.
- **Asset Download Request**: identifier (which ROM or which Disk II audio WAV), destination, source URL, expected size for progress reporting. Aggregated into a `Startup Download Set` consumed by the unified startup dialog.
- **Disk MRU Entry**: filesystem path of a previously-mounted disk image, timestamp (or implicit ordering position) used to display most-recent-first. Persisted as an ordered list under a new key in the `GlobalUserPrefs` JSON store, capped at 16 entries.
- **Boot Disk Choice**: discriminated value — either an MRU entry path, a download entry (DOS 3.3 / ProDOS), or Cancel.
- **Drive Widget State (extended)**: existing fields plus the mounted image path or basename used to paint the new filename label.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: After the feature ships, `rg -n "MessageBox|TaskDialog|GetOpenFileName" Casso/` returns at most the two intentional surfaces (`IFileOpenDialog` in `PromptForDiskImage`, `MessageBoxW` in the `Main.cpp` EHM notify handler) plus comments — zero other matches.
- **SC-002**: A fresh user with no cached ROMs sees exactly one themed dialog before the emulator boots (down from the current three-dialog sequence).
- **SC-003**: A user with a populated MRU sees their previously-mounted disk images at the top of the boot-disk picker, in most-recent-first order, and can mount any of them in one click without re-navigating a file dialog.
- **SC-004**: The disk MRU never exceeds 16 persisted entries and never displays an entry whose file does not currently exist.
- **SC-005**: Every dialog introduced by this feature renders correctly under all three themes (DarkModern, Skeuomorphic, GreenScreen) and at all four supported DPI scales (100%, 125%, 150%, 200%) — verified by visual inspection in each combination.
- **SC-006**: Clicking the repository hyperlink in the About dialog opens `https://github.com/relmer/Casso` in the user's default browser.
- **SC-007**: The drive widget displays the mounted disk image's basename below the "Drive N" label within one frame of a successful mount, removes the label within one frame of an eject, and truncates with an ellipsis when the basename exceeds the widget width.
- **SC-008**: Existing keyboard accelerators (Ctrl+1, Ctrl+2, F1) continue to invoke the corresponding (now-themed) dialogs.
- **SC-009**: The full existing unit-test suite continues to pass, and new unit tests cover (a) MRU pruning and cap-eviction logic, (b) drive-widget filename truncation, and (c) themed dialog primitive layout (button row sizing, hyperlink hit-testing, icon slot).
- **SC-010**: `DiskIIDebugDialogState` retains zero direct dependencies on Win32 UI types after the conversion (verified by headless unit tests continuing to build and run against it).

## Assumptions

- The existing `SettingsWindow` / `SettingsPanel` infrastructure is a sufficient foundation for the new reusable dialog primitive; no new DX painter primitives beyond those already used by `SettingsWindow` are required for the simple dialogs (About / Keymap / Machine Info / unified download / boot picker / settings stray).
- The `GlobalUserPrefs` JSON store is the right home for the disk MRU; integration follows the same pattern as the recent `fix(prefs): preserve machines section in GlobalUserPrefs::Save` change.
- Hyperlink rendering inside dialog body text is a new but bounded extension of the existing text-painting code in `DxUiPainter` — clickable hit-testing, hover styling, and `ShellExecuteW` dispatch are in scope for the primitive.
- The unified startup download dialog blocks boot when ROMs are missing because the emulator cannot run without them; this matches today's behavior on decline (just consolidated into one dialog).
- Embedded boot-disk detection (cases where the active machine ships with a built-in default disk image) is out of scope for changes; the boot picker only appears when no embedded boot disk applies, as today.
- The Debug Console and Disk II Debug Dialog conversions are the heaviest items and are sequenced as later phases (P3) so that the new dialog primitive lands and stabilizes via the simpler P1 / P2 dialogs first.
- "Per-machine config pinned a disk image that no longer exists" is treated identically to "no pin" for picker-display purposes; no separate error dialog is shown for the missing pinned file.
- Network reachability for the two downloadable system disks (DOS 3.3 / ProDOS) and the Disk II audio WAVs is assumed; failure handling reuses today's user-facing reporting via `CHRN` through the new themed primitive.
