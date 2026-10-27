# 040 Disk Inspector: Handoff

State as of 2026-10-26. The `040-disk-inspector` branch is pushed; see
`git log` for the last commit.

## Setting up on another machine

1. Pull `040-disk-inspector`.
2. `.specify/feature.json` is untracked, and each checkout has a separate copy. Before
   any `/speckit-*` command, run:

   ```powershell
   $env:SPECIFY_FEATURE_DIRECTORY = 'specs/040-disk-inspector'
   ```

## Try-it build

A Debug build is running minimized, titled `040-disk-inspector`, with the
DOS 3.3 master in drive 1. Close it whenever you like.

## Built since the last handoff

Each item is a separate commit on the branch.

- Sector editing end to end: the edit bar, apply at a safe point on the
  emulation thread, undo and redo of an applied edit, Save edited copy, Use
  B's bytes, and confirmations before an insert, an eject, a drive switch,
  closing the window or quitting discards pending edits.
- Pending edits survive Reset, Power Cycle and a machine change, since each
  of them remounts the same file. An eject discards them, with a note in the
  edit bar and the status band.
- Casso confirms before a switch to a machine without slot 6's drives
  discards pending edits. Every shipped machine has slot 6's drives, so this
  appears only when slot 6's Disk II is turned off in Settings.
- An eject now empties the inspector. Before this, an emptied drive's media
  id matched the window's initial request id, so the old disk stayed on screen.
- A detached drive 2 reads "Drive 2 is not attached". With a second Disk II
  in slot 5, the drive tabs' tooltip reads "Slot 6 drives only", and Casso
  now uses slot 6's card for its drives; before this it took whichever card
  was built first, which was slot 5's.
- The tilted view: drag the knob beside the platter, or drag with the right
  button anywhere on it, up to 60 degrees. "Top down" or a double-click on
  the knob returns it flat. The tilt is saved with the window. In the tilted
  view the head is a carriage on its rail with a shadow.
- The head marker over a turning platter, and the head line, fading trail and
  drive state on the strip.
- Written and read marks with visit counts in the Tracks tab, the Reads
  overlay, and a lock before a write-protected disk's name, with the causes in
  its tooltip.
- The Image tab marks values changed since the file was read, tracks the
  guest added, and a save.
- The window's state is saved, and the window reopens at launch on its last
  drive when it was open at exit.
- An empty drive is drawn as a dashed disk with the reason.
- Near-black panels behind the sector bytes and the Nibbles tab.
- The strip's timing line is a shaded waveform in Timing mode.
- The walkthrough, opened the first time the inspector opens and from the "?"
  button in the status band.
- Fixed: in Timing mode the platter column was narrower than its control row,
  and Legend covered Reads.
- Guest scenario tests for SC-018 (all four cases) and SC-022, and Release
  timing tests in `InspectorPerformanceTests`.

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
- **Bandits (SC-012).** Taken from GH #159's attachment into the scratchpad.
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
- **Menu bar: yes or no.** The mock-ups are in `mockups/`: the window with
  View open, and every menu's contents. Delete `mockups/` once the menu bar is
  decided.
- **The walkthrough's text.** Plain wording for someone new to disk internals
  is a judgment call; it is in `CEC/Ui/DiskInspector/InspectorWalkthrough.cpp`.
## Open work

- T143, T145: the Explorer halves wait for 033. Casso's theme captures
  (all three themes, every tab, a damaged NIB, grayscale) and SC-007 are done.
- T147: all gates passed 2026-10-26 (Debug 7003, Release 7001, scenarios 55,
  ARM64 Release, analysis clean); run again before the merge.
- Blocked until 033 merges to master: T129 to T141 and T161.
- Blocked on 035's machine state: the state-restore parts of T094 and T114.

## Known gaps

- File map grid hit marks are not verified on screen.
- A hit in sector data is placed on the strip and platter through the nibbles
  of its bytes, exactly for 6-and-2 sectors and approximately for 5-and-3.
- `UnitTest/Fixtures/dos33.dsk` is an empty file.
- Screen captures by `PrintWindow` sometimes miss text that is drawn late in
  the frame; a second capture has it.
