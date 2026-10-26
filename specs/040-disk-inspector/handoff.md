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

## Built since the last handoff

Each item is a separate commit on the branch.

- Sector editing end to end: the edit bar, apply at a safe point on the
  emulation thread, undo and redo of an applied edit, Save edited copy, Use
  B's bytes, and confirmations before an insert, an eject, a drive switch,
  closing the window or quitting discards pending edits.
- Pending edits survive Reset, Power Cycle and a machine change, since each
  of them remounts the same file. An eject discards them, with a note in the
  edit bar and the status band.
- An eject now empties the inspector. Before this, an emptied drive's media
  id matched the window's initial request id, so the old disk stayed on screen.
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
- Guest scenario tests for SC-018 (all four cases) and SC-022.

## Waiting on the owner

- **Find hit tooltips, by mouse.** Posted mouse input does not open Casso's
  tooltips, so these were checked by unit tests only.
- **Menu bar: yes or no.** Nothing is built.
- **The walkthrough's text.** Plain wording for someone new to disk internals
  is a judgment call; it is in `CEC/Ui/DiskInspector/InspectorWalkthrough.cpp`.

## Open work

- T157, T158: the tilted view and the head carriage in it. T159's tilt half
  waits for them.
- T096: the "Slot 6 drives only" tooltip, and the drive-not-attached state.
- T120: the confirmation before a machine change whose new machine has no
  Disk II in slot 6. The merged machine config is loaded only on the CPU
  thread, inside `MachineManager::SwitchMachine`.
- T102 and T144: performance measurements.
- T142, T143, T145, T146, T147: the verification passes before merge.
- Blocked until 033 merges to master: T129 to T141 and T161.
- Blocked on 035's machine state: the state-restore parts of T094 and T114.

## Known gaps

- File map grid hit marks are not verified on screen.
- A hit in sector data is placed on the strip and platter through the nibbles
  of its bytes, exactly for 6-and-2 sectors and approximately for 5-and-3.
- `UnitTest/Fixtures/dos33.dsk` is an empty file.
- Screen captures by `PrintWindow` sometimes miss text that is drawn late in
  the frame; a second capture has it.
