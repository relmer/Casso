# 040 Disk Inspector: Handoff

State as of 2026-10-28. The `040-disk-inspector` branch is pushed; see
`git log` for the last commit.

## Setting up on another machine

1. Pull `040-disk-inspector`.
2. `.specify/feature.json` is untracked, and each checkout has a separate copy. Before
   any `/speckit-*` command, run:

   ```powershell
   $env:SPECIFY_FEATURE_DIRECTORY = 'specs/040-disk-inspector'
   ```

## Built since the last handoff

Each item is a separate commit on the branch.

- The window is split into component classes under `Components/` and
  `Views/` (FR-162), with a CheckStyle rule that fails a `.cpp` over 2,000
  lines. A menu bar replaces the second toolbar row (FR-158); the platter's
  controls float in its corners (FR-163); one segmented control picks the
  view (FR-160); Fields merged into Nibbles.
- For each quarter track, the drive's access log lists where the guest read
  a data field, wrote, and scanned an address field (FR-161). "Reads",
  "Writes" and "Scans" shade each sector by count on a log scale, and the
  sector buttons and the Sectors header include "Read N times", "Written N times" and "Scanned N
  times". A scenario test checks that CATALOG, SAVE and RUN mark exactly the
  sectors DOS's RWTS calls name.
- The strip brackets each sector from its address prologue to its data
  epilogue with "✓ Sector $0"; its stretches read "Address" and "Data", and
  its label reads "Track 17.25". A click on a bracket selects the sector and
  flies the platter to it; a range dragged out on the strip is flown to the
  same way (FR-165).
- Back and Forward step through the user's selections: Alt+Left and
  Alt+Right, the mouse's back and forward buttons, and the Go menu (FR-164).
  Dxui now routes `WM_XBUTTONUP` as a Back or Forward mouse button.
- The window opens on Sectors every time; the selected view is no longer
  saved (FR-080, FR-160).
- Component tests host `TrackColumn`, `PlatterCorners` and `InspectorMenus`
  on `FakeInspectorHost` (T184).

## Checked 2026-10-28 on RELMER-DESKTOP

Debug x64, by `PrintWindow` captures with posted input; nothing took the
foreground.

- **SC-034.** At 2400×1600 pixels at 150%, a 1600×1067 DIP window, the menu
  bar, the toolbar, the view selector and the 16 sector buttons each take one
  row, and all 16 rows of a sector's bytes are visible. Every menu opens and
  walks by keyboard from F10.
- The bracket click, Back and Forward, the Scans header line and the heat
  map, on the DOS 3.3 master after boot.
- **Fixed while checking:** the outline for the platter's keys followed the
  square platter, so at this size it ran through the overlay chips and the
  zoom group. It now follows the platter's column.
## Measured on RELMER-DESKTOP

Release, pinned to one core of the cache CCD where it matters:

| Target | Result |
|---|---|
| SC-003, 140 KB DSK analysis under 100 ms | 22.6 ms |
| SC-003, 160 flux quarter track WOZ under 1 s | 132 ms |
| SC-005 proxy, drive state published each frame | 0.78 us against a 149 us frame, 0.53% |
| SC-015, damaged volume map under 100 ms | 0.07 ms at most |
| SC-021, two 140 KB disks under 1 s | 0.8 ms |
| SC-021, two flux WOZ images under 5 s | 12.3 ms |
| SC-004, platter GPU time | median 0.020 ms over 240 frames |
| SC-004, platter overlays and strip paint over a zoom and pan | median 0.06 ms, at most 0.45 ms |

## Checked 2026-10-26 evening

- **Keyboard walk (SC-011).** Tab did nothing in the inspector: Dxui moves
  focus on Tab only in dialog mode. The inspector now walks its controls with
  Tab, each with a focus ring, routes Enter, Space and arrows to the focused
  control first, and returns the keys to the views on Escape, F6 or a click.
- **Bandits (SC-012).** A copy is in the scratchpad.
  34 tracks with data, 19 flux. Zoomed in, its flux tracks were noise
  rather than bands until Timing mode averaged each cell over 32 cells.
- **Machine change confirmation.** Seen on screen after turning off slot 6's
  Disk II in Settings with an edit pending. Settings saves the change before
  the confirmation, so after Cancel the saved change applies at the next
  launch.
- **The Print Shop.** It boots from a custom loader, not DOS files. Its one catalog sector has the
  title banner as 7 fake entries, all pointing at the same bogus
  track/sector list, and the VTOC marks tracks 3 to 33 used. So 496 of the
  504 findings are "marked used but no file uses it", one per sector.

## Checked 2026-10-27 on RELMER-SLS2

Debug x64, 1600x1067 screen, the inspector filling it.

- **Shift+Tab (SC-011).** Passes. Fourteen stops were walked forward with Tab
  and back with Shift+Tab, with real keys, and each backward frame was
  compared against its forward counterpart pixel by pixel with the platter and
  the strip masked. Every pair matches; the residue is 16 to 18 pixels at a
  mask edge. The order is Drive 2, Structure, Follow head, Go to head, Go to,
  Find, Compare with, two clipped toolbar buttons, the tab strip, Edit sector,
  then the views. `DxuiFocusManager::MoveFocus` walks one index list in both
  directions, so the reverse holds by construction as well.
- **Tooltips open for the real cursor.** Seen on the platter ("Track 36.5 /
  Nothing recorded / The drive reads random bits here"), on the tilt knob, and
  on the emulator's tape transport. The Find hit tooltips, the Legend entry for
  hits and "Slot 6 drives only" are still unchecked; see the note on input
  below.

### Found while checking

- **The Find bar swallows Tab and Shift+Tab.** With Find open and its field
  focused the focus walk cannot leave the bar. `DiskInspectorWindow::OnKey`
  gives the find bar first refusal on every key but the window zoom, Ctrl+F and
  Ctrl+G, and the text input claims the rest, so the Tab branch below it never
  runs. The File map tab is therefore unreachable by keyboard while a search is
  open.
- **The Tab walk passes through controls that cannot be seen.** At this window
  size the toolbar is clipped: two stops after "Compare with" are partly or
  wholly off the right edge, so the focus ring is invisible and there is no way
  to tell what is focused. The summary chips and the clipped toolbar are the
  case the menu bar proposal is meant to answer.

### A note on driving the screen from here

This machine swallows injected mouse input: `SendInput` reports success and
nothing moves, while `SetCursorPos` and injected keystrokes both work. So
hover, and every tooltip that depends on it, is genuine, and so is every key
in the walk above. Clicks are not available, and posted `WM_LBUTTONDOWN`
does not activate a control either, so the checks that need a click wait for
the owner.

## Waiting on the owner

- **Tooltips by mouse.** Tooltips open only for the real cursor, and the
  screen was locked. The Find hit tooltips and "Slot 6 drives only" are checked in code
  only.
- **SC-005's whole-program runs.** Casso has no readout of its achieved
  speed or audio underruns, so the open and closed runs cannot be compared.
  Adding an opt-in speed log is your call.
- **The Print Shop's findings.** Accept them as they are, or group "marked used but no
  file uses it" into one finding per track or per run of tracks.
- **The wording of the new strip, menu and overlay text,** listed in the
  2026-10-28 commit messages.
- **A state restore as a reload (T094, T114).** See Open work.
- **The walkthrough's text.** Plain wording for someone new to disk internals
  is a judgment call; it is in `CEC/Ui/DiskInspector/InspectorWalkthrough.cpp`.

## Open work

- T143, T145: the Explorer halves wait for 033. Casso's theme captures
  (all three themes, every tab, a damaged NIB, grayscale) and SC-007 are done.
- T147: all gates passed 2026-10-26 (Debug 7003, Release 7001, scenarios 55,
  ARM64 Release, analysis clean); run again before the merge.
- Blocked until 033 merges to master: T129 to T141 and T161.
- The state-restore part of T114 is in 040 as `DiskImageStore::NoteStateRestored`
  (T193); T194 is the one call 035's `MachineHost::LoadState` makes once both
  branches are on master. "As inserted" stays the disk as first inserted.

## Known gaps

- File map grid hit marks are not verified on screen.
- A hit in sector data is placed on the strip and platter through the nibbles
  of its bytes, exactly for 6-and-2 sectors and approximately for 5-and-3.
- `UnitTest/Fixtures/dos33.dsk` is an empty file.
- Screen captures by `PrintWindow` sometimes miss text that is drawn late in
  the frame; a second capture has it.
