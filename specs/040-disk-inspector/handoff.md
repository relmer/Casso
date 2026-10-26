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

## Waiting on the owner

- **The keyboard walk (T142, SC-011).** Dxui moves focus on a posted Tab only
  while the inspector is the active window, and activating it would take the
  keyboard from you. Someone at the keyboard has to walk it.
- **Tooltips by mouse.** Posted mouse input does not open Casso's tooltips,
  so the Find hit tooltips and "Slot 6 drives only" were checked in code
  only.
- **The machine change confirmation on screen.** To see it, turn off slot 6's
  Disk II in Settings with an edit pending.
- **SC-004's zoom and pan frame times and SC-005's whole-program runs.** Both are run with
  real input: five alternating runs at Maximum
  speed with the window open and closed, and five 60 s runs at normal speed
  for audio underruns and dropped frames.
- **Bandits (SC-012).** The image is not on this machine.
- **The Print Shop.** Its VTOC passes the DOS 3.3 check and its catalog chain
  runs through 72 sectors, so the File system category lists 504 findings.
  DOS would read the same chain. Should the map stop treating such a disk as
  DOS 3.3? See research.md R4.
- **Menu bar: yes or no.** Nothing is built.
- **The walkthrough's text.** Plain wording for someone new to disk internals
  is a judgment call; it is in `CEC/Ui/DiskInspector/InspectorWalkthrough.cpp`.

## Open work

- T143, T145: the Explorer halves wait for 033. Casso's theme captures
  (all three themes, every tab, a damaged NIB, grayscale) and SC-007 are done.
- T147: all gates passed 2026-10-26 (Debug 7001, Release 7000, scenarios 55,
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
